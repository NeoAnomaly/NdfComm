// ClientApp.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include "WinSvcSup.h"
#include "NdfCommUm.h"
#include "LegacyDrvApi.h"

const LPWSTR LegacyDriverSvcName = L"NdfCommLegacyDrv";
const LPWSTR LegacyDriverFileName = L"LegacyDrv.sys";

bool AdjustPrivileges()
{
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;

    if (!OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        return false;
    }

    if (!LookupPrivilegeValue(NULL, SE_LOAD_DRIVER_NAME, &luid))
    {
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
    {
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
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
            wprintf(L"!ERROR WinSvcDelete failed, exiting...\n");

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
            wprintf(L"!ERROR WinSvcStart failed with error: %u. Exiting...\n", GetLastError());
            return false;
        }
    }
    else
    {
        wprintf(L"!ERROR WinSvcCreate failed with error: %u. Exiting...\n", GetLastError());
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
        wprintf(L"The driver not installed. Exiting...\n");

        return true;
    }

    if (!WinSvcDelete(LegacyDriverSvcName))
    {
        wprintf(L"!ERROR WinSvcDelete failed, exiting...\n");

        return false;
    }

    wprintf(L"Driver successfully uninstalled.\n");

    return true;
}

int main()
{
    ULONG testValue = 0xfefefefe;
    HANDLE hConnection;
    HRESULT hr;

    if (!AdjustPrivileges())
    {
        wprintf(L"AdjustPrivileges failed with error: %u. Exiting...\n", GetLastError());

        system("pause");

        return 0;
    }

    StartLegacyDriver();

    hr = CommunicationConnect(
        DRV_NAME,
        &testValue,
        sizeof(ULONG),
        &hConnection
    );

    if (FAILED(hr))
    {
        wprintf(L"CommunicationConnect failed with error: %d\n", hr);
    }    

    system("pause");

    StopLegacyDriver();

    return 0;
}

