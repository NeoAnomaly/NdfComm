#pragma once

#include <ntddk.h>

NTSTATUS
NdfCommDeliverMessageToKm(
	_In_ PFILE_OBJECT FileObject,
	_In_reads_bytes_(InputBufferSize) const PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferSize,
	_In_ KPROCESSOR_MODE Mode
);
