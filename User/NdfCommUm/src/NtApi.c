#include "NdfCommpNtApi.h"
#include <assert.h>

extern BOOLEAN CommunicationpNtApiInitialized = FALSE;
extern BOOLEAN CommunicationpNtApiInitializationFailed = FALSE;

CommNtCreateFileProc NtCreateFile = NULL;

HRESULT CommunicationpInitNtApi()
{
    HMODULE ntdllModule = NULL;

    if (CommunicationpNtApiInitialized)
    {
        return S_OK;
    }

    if (CommunicationpNtApiInitializationFailed)
    {
        return E_UNEXPECTED;
    }

    assert(NtCreateFile == NULL);

    ntdllModule = GetModuleHandle(L"ntdll");
    if (!ntdllModule)
    {
        CommunicationpNtApiInitializationFailed = TRUE;

        return HRESULT_FROM_WIN32(GetLastError());
    }

    NtCreateFile = (CommNtCreateFileProc)GetProcAddress(ntdllModule, "NtCreateFile");
    if (!NtCreateFile)
    {
        CommunicationpNtApiInitializationFailed = TRUE;

        return HRESULT_FROM_WIN32(GetLastError());
    }

    CommunicationpNtApiInitialized = TRUE;

    return S_OK;
}