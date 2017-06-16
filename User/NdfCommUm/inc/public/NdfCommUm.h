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

	_Check_return_
		HRESULT
		WINAPI
		NdfCommunicationSendMessage(
			_In_ HANDLE Connection,
			_In_reads_bytes_opt_(InputBufferSize) LPVOID InputBuffer,
			_In_ ULONG InputBufferSize,
			_Out_writes_bytes_to_opt_(OutputBufferSize, BytesReturned) LPVOID OutputBuffer,
			_In_ ULONG OutputBufferSize,
			_Out_opt_ PULONG BytesReturned
		);

	_Check_return_
		HRESULT
		WINAPI
		NdfCommunicationGetMessage(
			_In_ HANDLE Connection,
			_Out_writes_bytes_(MessageBufferSize) PVOID MessageBuffer,
			_In_ ULONG MessageBufferSize,
			_Inout_ LPOVERLAPPED Overlapped
		);

#ifdef __cplusplus
}
#endif
