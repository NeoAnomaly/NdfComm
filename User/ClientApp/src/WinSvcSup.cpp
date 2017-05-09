#include "WinSvcSup.h"

#include <tchar.h>

BOOL WinSvcCreate(__in LPTSTR ServiceName, __in LPTSTR FilePath)
{
    BOOL result = TRUE;
    SC_HANDLE hSCM, hService;

    hSCM = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_CREATE_SERVICE);

    if (!hSCM)
    {
        _tprintf(_T("OpenSCManager failed with error: %u\n"), GetLastError());

        return FALSE;
    }

    hService = CreateService(hSCM,
        ServiceName,
        ServiceName,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_SYSTEM_START,
        SERVICE_ERROR_IGNORE,
        FilePath,
        NULL,
        NULL,
        NULL,
        NULL,
        L""
    );

    if (!hService)
    {
        _tprintf(_T("CreateService failed with error: %u\n"), GetLastError());

        result = FALSE;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCM);

    return result;
}

BOOL WinSvcDelete(__in LPTSTR ServiceName)
{
    BOOL result = TRUE;
    SC_HANDLE hSCM, hService;

    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!hSCM)
    {
        _tprintf(_T("OpenSCManager failed with error: %u\n"), GetLastError());

        return FALSE;
    }

    hService = OpenService(hSCM, ServiceName, SERVICE_STOP | DELETE);

    if (hService)
    {
        SERVICE_STATUS svcStatus;

        if (ControlService(hService, SERVICE_CONTROL_STOP, &svcStatus) || svcStatus.dwCurrentState == SERVICE_STOPPED)
        {
            if (!DeleteService(hService))
            {
                _tprintf(_T("DeleteService failed with error: %u\n"), GetLastError());

                result = FALSE;
            }
        }
        else
        {
            _tprintf(_T("ControlService failed with error: %u\n"), GetLastError());

            result = FALSE;
        }
    }
    else
    {
        _tprintf(_T("OpenService failed with error: %u\n"), GetLastError());

        result = FALSE;
    }

    CloseServiceHandle(hSCM);

    return result;
}

BOOL WinSvcStart(__in LPTSTR ServiceName)
{
    BOOL result = TRUE;
    SC_HANDLE hSCM, hService;

    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!hSCM)
    {
        _tprintf(_T("OpenSCManager failed with error: %u\n"), GetLastError());

        return FALSE;
    }

    hService = OpenService(hSCM, ServiceName, SERVICE_START);

    if (hService)
    {
        if (!StartService(hService, 0, NULL))
        {
            _tprintf(_T("StartService failed with error: %u\n"), GetLastError());

            result = FALSE;
        }
    }
    else
    {
        _tprintf(_T("OpenService failed with error: %u\n"), GetLastError());

        result = FALSE;
    }

    CloseServiceHandle(hSCM);

    return result;
}

BOOL WinSvcStop(__in LPTSTR ServiceName)
{
    BOOL result = TRUE;
    SC_HANDLE hSCM, hService;

    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!hSCM)
    {
        _tprintf(_T("OpenSCManager failed with error: %u\n"), GetLastError());

        return FALSE;
    }

    hService = OpenService(hSCM, ServiceName, SERVICE_STOP);

    if (hService)
    {
        SERVICE_STATUS svcStatus;

        if (!ControlService(hService, SERVICE_CONTROL_STOP, &svcStatus))
        {
            _tprintf(_T("ControlService failed with error: %u\n"), GetLastError());

            result = FALSE;
        }
    }
    else
    {
        _tprintf(_T("OpenService failed with error: %u\n"), GetLastError());

        result = FALSE;
    }

    CloseServiceHandle(hSCM);

    return result;
}

BOOL WinSvcIsRunning(__in LPTSTR ServiceName, __out PBOOL IsRunning)
{
    BOOL result = TRUE;
    SC_HANDLE hSCM, hService;
    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwBytesNeeded;

    if (!IsRunning)
    {
        SetLastError(ERROR_INVALID_PARAMETER);

        _tprintf(_T("Parameter IsRunning can't be NULL\n"));

        return FALSE;
    }

    hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!hSCM)
    {
        _tprintf(_T("OpenSCManager failed with error: %u\n"), GetLastError());

        return FALSE;
    }


    hService = OpenService(hSCM, ServiceName, SERVICE_QUERY_STATUS);

    if (hService)
    {
        if (
            QueryServiceStatusEx(
                hService, 
                SC_STATUS_PROCESS_INFO, 
                (LPBYTE)&ssStatus, 
                sizeof(SERVICE_STATUS_PROCESS), 
                &dwBytesNeeded
            )
            )
        {
            *IsRunning = (ssStatus.dwCurrentState == SERVICE_RUNNING);
        }
        else
        {
            _tprintf(_T("QueryServiceStatusEx failed with error: %u\n"), GetLastError());

            result = FALSE;
        }
    }
    else
    {
        _tprintf(_T("OpenService failed with error: %u\n"), GetLastError());

        result = FALSE;
    }

    CloseServiceHandle(hSCM);
    CloseServiceHandle(hService);

    return result;
}