#pragma once

#include <ntddk.h>

NTSTATUS
NdfCommDispatch(
	__in struct _DEVICE_OBJECT *DeviceObject,
	__inout struct _IRP *Irp
);
