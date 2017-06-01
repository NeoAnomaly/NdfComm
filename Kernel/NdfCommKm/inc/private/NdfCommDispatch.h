#pragma once

#include <ntddk.h>

NTSTATUS
NdfCommDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
);
