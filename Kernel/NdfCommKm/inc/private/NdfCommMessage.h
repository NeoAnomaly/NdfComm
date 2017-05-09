#pragma once

#include <ntddk.h>

NTSTATUS
NdfCommMessageDeliverToKm(
	__in PFILE_OBJECT FileObject,
	__in const PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG ReturnOutputBufferSize,
	__in KPROCESSOR_MODE Mode
);
