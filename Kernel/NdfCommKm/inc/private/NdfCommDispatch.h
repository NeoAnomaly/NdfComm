#pragma once

#include <ntddk.h>

_Function_class_(DRIVER_DISPATCH)
NTSTATUS
NdfCommDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
);
