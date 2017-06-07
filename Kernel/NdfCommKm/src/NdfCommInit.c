#include "NdfCommKm.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommUtils.h"
#include "NdfCommProcessing.h"
#include "NdfCommDebug.h"
#include "NdfCommClient.h"

_Function_class_(DRIVER_DISPATCH)
NTSTATUS
NdfCommpDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
);

VOID
NdfCommpDisconnectAllClients(
	VOID
);

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(INIT, NdfCommInit)
#   pragma alloc_text(PAGE, NdfCommRelease)
#   pragma alloc_text(PAGE, NdfCommpDispatch)
#   pragma alloc_text(PAGE, NdfCommpDisconnectAllClients)
#endif // ALLOC_PRAGMA

_Check_return_
NTSTATUS
NdfCommInit(
	_In_ PDRIVER_OBJECT DriverObject,
	_In_ PWSTR DriverName,
	_In_ PNDFCOMM_CONNECT_NOTIFY ConnectNotifyCallback,
	_In_ PNDFCOMM_DISCONNECT_NOTIFY DisconnectNotifyCallback,
	_In_ PNDFCOMM_MESSAGE_NOTIFY MessageNotifyCallback,
	_In_ ULONG MaxClients
)
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;

    DECLARE_UNICODE_STRING_SIZE(deviceName, NDFCOMM_MSG_MAX_CCH_NAME_SIZE);

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Initializing library..."
	);

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
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: One of the following parameters is not valid(can't be NULL):\n"
			"\tDriverName(%p)\n"
			"\tConnectNotifyCallback(%p)\n"
			"\tDisconnectNotifyCallback(%p)\n"
			"\tMessageNotifyCallback(%p)\n"
			"\tMaxClients(%u)",
			DriverName,
			ConnectNotifyCallback,
			DisconnectNotifyCallback,
			MessageNotifyCallback,
			MaxClients
		);

		return STATUS_INVALID_PARAMETER;
	}

    RtlZeroMemory(&NdfCommGlobals, sizeof(NdfCommGlobals));

	ExInitializeRundownProtection(&NdfCommGlobals.LibraryRundownProtect);
	NdfCommInitializeConcurentList(&NdfCommGlobals.ClientList);

	NdfCommGlobals.MaxClientsCount = MaxClients;
	NdfCommGlobals.ConnectNotifyCallback = ConnectNotifyCallback;
	NdfCommGlobals.DisconnectNotifyCallback = DisconnectNotifyCallback;
	NdfCommGlobals.MessageNotifyCallback = MessageNotifyCallback;

	status = RtlUnicodeStringPrintf(&deviceName, NDFCOMM_MSG_DEVICE_NAME_FMT, DriverName);
    if (!NT_SUCCESS(status))
    {
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: RtlUnicodeStringPrintf(NDFCOMM_MSG_DEVICE_NAME_FMT) failed with status: %d",
			status
		);

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
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: IoCreateDevice failed with status: %d",
			status
		);

        return status;
    }

	status = NdfCommUnicodeStringAlloc(
		&NdfCommGlobals.SymbolicLinkName,
		NDFCOMM_MSG_MAX_CCH_NAME_SIZE * sizeof(WCHAR)
	);
	
	if (!NT_SUCCESS(status))
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: NdfCommUnicodeStringAlloc failed with status: %d",
			status
		);

		return status;
	}

	status = RtlUnicodeStringPrintf(
		&NdfCommGlobals.SymbolicLinkName, 
		NDFCOMM_MSG_SYMLINK_NAME_FMT, 
		DriverName
	);

	if (!NT_SUCCESS(status))
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: RtlUnicodeStringPrintf(NDFCOMM_MSG_SYMLINK_NAME_FMT) failed with status: %d",
			status
		);

		NdfCommUnicodeStringFree(&NdfCommGlobals.SymbolicLinkName);

		return status;
	}

    status = IoCreateSymbolicLink(&NdfCommGlobals.SymbolicLinkName, &deviceName);

    if (!NT_SUCCESS(status))
    {
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: IoCreateSymbolicLink failed with status: %d",
			status
		);

        IoDeleteDevice(NdfCommGlobals.MessageDeviceObject);
        NdfCommGlobals.MessageDeviceObject = NULL;

		NdfCommUnicodeStringFree(&NdfCommGlobals.SymbolicLinkName);

		return status;
    }

    ///
    /// Hooking dispatch routines
    ///
    NdfCommGlobals.OriginalCreateDispatch = DriverObject->MajorFunction[IRP_MJ_CREATE];
    NdfCommGlobals.OriginalCleanupDispatch = DriverObject->MajorFunction[IRP_MJ_CLEANUP];
    NdfCommGlobals.OriginalCloseDispatch = DriverObject->MajorFunction[IRP_MJ_CLOSE];
    NdfCommGlobals.OriginalControlDispatch = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];

    DriverObject->MajorFunction[IRP_MJ_CREATE] = NdfCommpDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = NdfCommpDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = NdfCommpDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NdfCommpDispatch;

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Library initialization complete"
	);

    return status;
}

VOID
NdfCommpDisconnectAllClients(
	VOID
)
{
	PAGED_CODE();

	NdfCommConcurentListLock(&NdfCommGlobals.ClientList);

	if (NdfCommGlobals.ClientList.Count > 0)
	{
		PNDFCOMM_CLIENT client = NULL;
		PNDFCOMM_CONCURENT_LIST clientList = &NdfCommGlobals.ClientList;

		ASSERT(!IsListEmpty(&clientList->ListHead));

		for (PLIST_ENTRY entry = clientList->ListHead.Flink; entry != &clientList->ListHead; entry = entry->Flink)
		{
			client = CONTAINING_RECORD(entry, NDFCOMM_CLIENT, ListEntry);

			NdfCommDisconnectClient(client);

			NdfCommGlobals.DisconnectNotifyCallback(client->ConnectionCookie);
		}
	}

	NdfCommConcurentListUnlock(&NdfCommGlobals.ClientList);
}

VOID
NdfCommRelease(
    VOID
)
{
    PAGED_CODE();

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Releasing library..."
	);

    ExWaitForRundownProtectionRelease(&NdfCommGlobals.LibraryRundownProtect);

    NdfCommpDisconnectAllClients();

	if (NdfCommGlobals.SymbolicLinkName.Length)
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_VERBOSE,
			0,
			"Deleting symbolic link..."
		);

		IoDeleteSymbolicLink(&NdfCommGlobals.SymbolicLinkName);

		NdfCommUnicodeStringFree(&NdfCommGlobals.SymbolicLinkName);
	}

    if (NdfCommGlobals.MessageDeviceObject)
    {
		NdfCommDebugTrace(
			TRACE_LEVEL_VERBOSE,
			0,
			"Deleting device object..."
		);

        IoDeleteDevice(NdfCommGlobals.MessageDeviceObject);
    }

    ExRundownCompleted(&NdfCommGlobals.LibraryRundownProtect);

    ASSERT(IsListEmpty(&NdfCommGlobals.ClientList.ListHead));
    ASSERT(NdfCommGlobals.ClientList.Count == 0);

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Library releasing complete"
	);
}

_Function_class_(DRIVER_DISPATCH)
NTSTATUS
NdfCommpDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	ULONG returnBufferSize = 0;


	if (DeviceObject != NdfCommGlobals.MessageDeviceObject)
	{
		switch (irpSp->MajorFunction)
		{

		case IRP_MJ_CREATE:
			return NdfCommGlobals.OriginalCreateDispatch(DeviceObject, Irp);

		case IRP_MJ_CLEANUP:
			return NdfCommGlobals.OriginalCleanupDispatch(DeviceObject, Irp);

		case IRP_MJ_CLOSE:
			return NdfCommGlobals.OriginalCloseDispatch(DeviceObject, Irp);

		case IRP_MJ_DEVICE_CONTROL:
			return NdfCommGlobals.OriginalControlDispatch(DeviceObject, Irp);

		default:
			ASSERTMSG("Not implemented hook", FALSE);

			return NdfCommCompleteIrp(Irp, STATUS_INVALID_DEVICE_STATE, 0);
		}
	}

	switch (irpSp->MajorFunction)
	{
	case IRP_MJ_CREATE:

		status = NdfCommpProcessCreateRequest(irpSp->FileObject, Irp);
		break;

	case IRP_MJ_CLEANUP:

		NdfCommpProcessCleanupRequest(irpSp->FileObject);
		break;

	case IRP_MJ_CLOSE:

		NdfCommpProcessCloseRequest(irpSp->FileObject);
		break;

	case IRP_MJ_DEVICE_CONTROL:

		status = NdfCommpProcessControlRequest(
			irpSp,
			Irp,
			&returnBufferSize
		);
		break;

	default:
		ASSERTMSG("Not implemented hook", FALSE);

		status = STATUS_INVALID_DEVICE_STATE;
	}

	if (status != STATUS_PENDING)
	{
		NdfCommCompleteIrp(Irp, status, (ULONG_PTR)returnBufferSize);
	}

	return status;
}