#pragma once

#include <ntddk.h>

///
/// Opaque public types
///
typedef struct _NDFCOMM_CLIENT* PNDFCOMM_CLIENT;

typedef NTSTATUS
(NTAPI *PNDFCOMM_CONNECT_NOTIFY) (
	_In_ PNDFCOMM_CLIENT Client,
	_In_reads_bytes_opt_(ContextSize) PVOID ClientContext,
	_In_ ULONG ContextSize,
	_Outptr_result_maybenull_ PVOID *ConnectionCookie
	);

typedef VOID
(NTAPI *PNDFCOMM_DISCONNECT_NOTIFY) (
	_In_opt_ PVOID ConnectionCookie
	);

typedef NTSTATUS
(NTAPI *PNDFCOMM_MESSAGE_NOTIFY) (
	_In_opt_ PVOID ConnectionCookie,
	_In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
	_In_ ULONG OutputBufferLength,
	_Out_ PULONG ReturnOutputBufferLength
	);

_Check_return_
NTSTATUS
NdfCommInit(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PWSTR DriverName,
	_In_ PNDFCOMM_CONNECT_NOTIFY ConnectNotifyCallback,
	_In_ PNDFCOMM_DISCONNECT_NOTIFY DisconnectNotifyCallback,
	_In_ PNDFCOMM_MESSAGE_NOTIFY MessageNotifyCallback,
	_In_ ULONG MaxClients
);

VOID
NdfCommRelease(
    VOID
);

_Check_return_
NTSTATUS
NdfCommSendMessage(
	_In_ PNDFCOMM_CLIENT Client,
	_In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_In_opt_ PLARGE_INTEGER Timeout
);