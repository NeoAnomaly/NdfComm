#pragma once

#include "NdfCommKm.h"

#include <ntddk.h>

NTSTATUS
ConnectHandler(
    _In_ PNDFCOMM_CLIENT Client,
    _In_reads_bytes_opt_(ContextSize) PVOID ClientContext,
    _In_ ULONG ContextSize,
    _Outptr_result_maybenull_ PVOID *ConnectionCookie
);

VOID
DisconnectHandler(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS
MessageHandler(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
);