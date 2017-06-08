#pragma once

#include <ntddk.h>
#include "NdfCommConcurentList.h"

_Check_return_
NTSTATUS
NdfCommpProcessCreateRequest(
	_Inout_ PFILE_OBJECT FileObject,
	_In_ PIRP Irp
);

VOID
NdfCommpProcessCleanupRequest(
	_In_ PFILE_OBJECT FileObject
);

VOID
NdfCommpProcessCloseRequest(
	_Inout_ PFILE_OBJECT FileObject
);

_Check_return_
NTSTATUS
NdfCommpProcessControlRequest(
	_In_ PIO_STACK_LOCATION StackLocation,
	_In_ PIRP Irp,
	_Out_ PULONG ReturnOutputBufferSize
);
