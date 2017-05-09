#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

HRESULT
WINAPI
CommunicationConnect(
    __in LPCWSTR DriverName,
    __in_opt LPCVOID Context,
    __in USHORT ContextSize,
    __out HANDLE* Connection
);

#ifdef __cplusplus
}
#endif
