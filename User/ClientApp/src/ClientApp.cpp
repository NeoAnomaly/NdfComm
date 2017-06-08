// ClientApp.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include "WinSvcSup.h"
#include "NdfCommUm.h"

#include "LegacyDrvApi.h"
#include "DrvCommunicationShared.h"

const LPWSTR LegacyDriverSvcName = L"NdfCommLegacyDrv";
const LPWSTR LegacyDriverFileName = L"LegacyDrv.sys";

bool AdjustPrivileges();
bool StartLegacyDriver();
bool StopLegacyDriver();

int main()
{
#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

	ULONG testValue = 0xfefefefe;
	HANDLE hConnection = INVALID_HANDLE_VALUE;
	HRESULT hr;

	LPWSTR lowerCase = L"êîíôèãóðàöèÿ";
	LPWSTR upperCase = L"ÊÎÍÔÈÃÓÐÀÖÈß";

	BYTE commandMessageBuffer[sizeof(COMMAND_MESSAGE) + 26 + 26];
	PCOMMAND_MESSAGE command = (PCOMMAND_MESSAGE)&commandMessageBuffer;
    BYTE getMessageBuffer[12];

	RtlCopyMemory(command->Data, lowerCase, 26);
	RtlCopyMemory(Add2Ptr(command->Data, 26), upperCase, 26);

	if (!AdjustPrivileges())
	{
		system("pause");

		return 0;
	}

	if (StartLegacyDriver())
	{
		hr = NdfCommunicationConnect(
			DRV_NAME,
			&testValue,
			sizeof(ULONG),
			&hConnection
		);

		if (SUCCEEDED(hr))
		{
			command->ApiVersion = ApiVersion;
			command->Command = CommandStart;

			hr = NdfCommunicationSendMessage(
				hConnection,
				command,
				sizeof(COMMAND_MESSAGE) + 26 + 26,
				NULL,
				0,
				NULL
			);

			if (FAILED(hr))
			{
				wprintf(L"NdfCommunicationSendMessage(CommandStart) failed with error: %d\n", hr);
			}

            OVERLAPPED ovlp = { 0 };
            ovlp.hEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);

            hr = NdfCommunicationGetMessage(
                hConnection,
                getMessageBuffer,
                sizeof(getMessageBuffer),
                &ovlp
            );

            if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
            {
                wprintf(L"NdfCommunicationGetMessage failed with error: %d\n", hr);
            }
		}
		else
		{
			wprintf(L"NdfCommunicationConnect failed with error: %d\n", hr);
		}
	}

	system("pause");

	if (hConnection != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hConnection);
		hConnection = INVALID_HANDLE_VALUE;
	}

	StopLegacyDriver();

	return 0;
}

bool AdjustPrivileges()
{
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hToken;
	LUID luid;
	TOKEN_PRIVILEGES tp;

	wprintf(L"Adjusting privilege...\n");

	if (!OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
	{
		wprintf(L"OpenProcessToken failed with error: %u. Exiting...\n", GetLastError());

		return false;
	}

	if (!LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &luid))
	{
		wprintf(L"LookupPrivilegeValue failed with error: %u. Exiting...\n", GetLastError());

		return false;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
	{
		wprintf(L"AdjustTokenPrivileges failed with error: %u. Exiting...\n", GetLastError());

		return false;
	}

	if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
	{
		wprintf(L"AdjustTokenPrivileges failed with error: %u. Exiting...\n", GetLastError());

		return false;
	}

	return true;
}

bool StartLegacyDriver()
{
	BOOL isRunning = FALSE;

	wprintf(L"Start installing legacy driver...\n");

	if (WinSvcIsRunning(LegacyDriverSvcName, &isRunning))
	{
		wprintf(L"The driver already installed. Reinstalling...\n");

		if (!WinSvcDelete(LegacyDriverSvcName))
		{
			return false;
		}
	}

	///
	/// Build driver file path
	///

	WCHAR moduleFilePath[MAX_PATH] = { 0 };
	WCHAR driveComponent[_MAX_DRIVE] = { 0 };
	WCHAR dirComponent[MAX_PATH] = { 0 };
	WCHAR driverFilePath[MAX_PATH] = { 0 };

	if (!GetModuleFileName(NULL, moduleFilePath, MAX_PATH))
	{
		wprintf(L"!ERROR GetModuleFileName failed with error: %u. Exiting...\n", GetLastError());
		return false;
	}

	int errorCode = _wsplitpath_s(
		moduleFilePath,
		driveComponent, _MAX_DRIVE,
		dirComponent, MAX_PATH,
		nullptr, 0,
		nullptr, 0
	);

	if (errorCode != 0)
	{
		wprintf(L"!ERROR _wsplitpath_s failed with error: %d. Exiting...\n", errorCode);
		return false;
	}

	errorCode = wcscat_s(driverFilePath, MAX_PATH, driveComponent);
	if (errorCode != 0)
	{
		wprintf(L"!ERROR wcscat_s failed with error: %d. Exiting...\n", errorCode);
		return false;
	}

	errorCode = wcscat_s(driverFilePath, MAX_PATH, dirComponent);
	if (errorCode != 0)
	{
		wprintf(L"!ERROR wcscat_s failed with error: %d. Exiting...\n", errorCode);
		return false;
	}

	errorCode = wcscat_s(driverFilePath, MAX_PATH, LegacyDriverFileName);
	if (errorCode != 0)
	{
		wprintf(L"!ERROR wcscat_s failed with error: %d. Exiting...\n", errorCode);
		return false;
	}

	///
	///
	///
	wprintf(L"Creating service %s\nFilePath: \"%s\"\n", LegacyDriverSvcName, driverFilePath);

	if (WinSvcCreate(LegacyDriverSvcName, driverFilePath))
	{
		if (!WinSvcStart(LegacyDriverSvcName))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	wprintf(L"Driver successfully installed.\n");

	return true;
}

bool StopLegacyDriver()
{
	BOOL isRunning = FALSE;

	wprintf(L"Start uninstalling legacy driver...\n");

	if (!WinSvcIsRunning(LegacyDriverSvcName, &isRunning))
	{
		return true;
	}

	if (!WinSvcDelete(LegacyDriverSvcName))
	{
		return false;
	}

	wprintf(L"Driver successfully uninstalled.\n");

	return true;
}

