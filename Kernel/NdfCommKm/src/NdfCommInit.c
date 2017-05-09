#include "NdfCommKm.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommUtils.h"
#include "NdfCommDispatch.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(INIT, NdfCommInit)
#   pragma alloc_text(PAGE, NdfCommRelease)
#endif // ALLOC_PRAGMA

__checkReturn
NTSTATUS
NdfCommInit(
	__in PDRIVER_OBJECT DriverObject,
	__in PWSTR DriverName,
	__in PNDFCOMM_CONNECT_NOTIFY ConnectNotifyCallback,
	__in PNDFCOMM_DISCONNECT_NOTIFY DisconnectNotifyCallback,
	__in PNDFCOMM_MESSAGE_NOTIFY MessageNotifyCallback,
	__in LONG MaxClients
)
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;

    DECLARE_UNICODE_STRING_SIZE(deviceName, NDFCOMM_MSG_MAX_CCH_NAME_SIZE);

	if (
		!DriverName
		||
		!ConnectNotifyCallback
		||
		!DisconnectNotifyCallback
		||
		!MessageNotifyCallback
		||
		!MaxClients
		)
	{
		return STATUS_INVALID_PARAMETER;
	}

    RtlZeroMemory(&NdfCommGlobals, sizeof(NdfCommGlobals));

	NdfCommConcurentListInitialize(&NdfCommGlobals.ClientList);

	NdfCommGlobals.MaxClientsCount = MaxClients;
	NdfCommGlobals.ConnectNotifyCallback = ConnectNotifyCallback;
	NdfCommGlobals.DisconnectNotifyCallback = DisconnectNotifyCallback;
	NdfCommGlobals.MessageNotifyCallback = MessageNotifyCallback;

	status = RtlUnicodeStringPrintf(&deviceName, NDFCOMM_MSG_DEVICE_NAME_FMT, DriverName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
		NDFCOMM_DEVICE_TYPE,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &NdfCommGlobals.MessageDeviceObject
    );

    if (!NT_SUCCESS(status))
    {
        return status;
    }

	status = NdfCommUnicodeStringAlloc(
		NdfCommGlobals.SymbolicLinkName,
		NDFCOMM_MSG_MAX_CCH_NAME_SIZE * sizeof(WCHAR)
	);
	
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = RtlUnicodeStringPrintf(
		NdfCommGlobals.SymbolicLinkName, 
		NDFCOMM_MSG_SYMLINK_NAME_FMT, 
		DriverName
	);

	if (!NT_SUCCESS(status))
	{
		NdfCommUnicodeStringFree(NdfCommGlobals.SymbolicLinkName);
		NdfCommGlobals.SymbolicLinkName = NULL;

		return status;
	}

    status = IoCreateSymbolicLink(NdfCommGlobals.SymbolicLinkName, &deviceName);

    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(NdfCommGlobals.MessageDeviceObject);
        NdfCommGlobals.MessageDeviceObject = NULL;

		NdfCommUnicodeStringFree(NdfCommGlobals.SymbolicLinkName);
		NdfCommGlobals.SymbolicLinkName = NULL;

		return status;
    }

    ///
    /// Hooking dispatch routines
    ///
    NdfCommGlobals.OriginalCreateDispatch = DriverObject->MajorFunction[IRP_MJ_CREATE];
    NdfCommGlobals.OriginalCleanupDispatch = DriverObject->MajorFunction[IRP_MJ_CLEANUP];
    NdfCommGlobals.OriginalCloseDispatch = DriverObject->MajorFunction[IRP_MJ_CLOSE];
    NdfCommGlobals.OriginalControlDispatch = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];

    DriverObject->MajorFunction[IRP_MJ_CREATE] = NdfCommDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = NdfCommDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = NdfCommDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdfCommDispatch;

    return status;
}

VOID
NdfCommRelease(
    VOID
)
{
    PAGED_CODE();

	if (NdfCommGlobals.SymbolicLinkName)
	{
		IoDeleteSymbolicLink(NdfCommGlobals.SymbolicLinkName);

		NdfCommUnicodeStringFree(NdfCommGlobals.SymbolicLinkName);
		NdfCommGlobals.SymbolicLinkName = NULL;
	}

    if (NdfCommGlobals.MessageDeviceObject)
    {
        IoDeleteDevice(NdfCommGlobals.MessageDeviceObject);
    }
}