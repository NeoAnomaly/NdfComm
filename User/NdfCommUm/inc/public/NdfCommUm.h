#pragma once

#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    HRESULT
        WINAPI
        NdfCommunicationConnect(
            __in LPCWSTR DriverName,
            __in_opt LPCVOID Context,
            __in USHORT ContextSize,
            __out HANDLE* Connection
        );

    HRESULT
        WINAPI
        NdfCommunicationSendMessage(
            __in HANDLE Connection,
            __in_opt LPVOID InputBuffer,
            __in ULONG InputBufferSize,
            __out_opt LPVOID OutputBuffer,
            __in ULONG OutputBufferSize,
            __out PULONG BytesReturned
        );

#ifdef __cplusplus
}
#endif
