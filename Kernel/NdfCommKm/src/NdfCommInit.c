#include "NdfCommKm.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommUtils.h"
#include "NdfCommDispatch.h"
#include "NdfCommDebug.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(INIT, NdfCommInit)
#   pragma alloc_text(PAGE, NdfCommRelease)
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
		"Initializing..."
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
			"!ERROR: One of the following parameters is not valid(NULL):\n"
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

	ExInitializeRundownProtection(&NdfCommGlobals.LibraryRundownRef);
	NdfCommConcurentListInitialize(&NdfCommGlobals.ClientList);

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
		NdfCommGlobals.SymbolicLinkName,
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
		NdfCommGlobals.SymbolicLinkName, 
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

		NdfCommUnicodeStringFree(NdfCommGlobals.SymbolicLinkName);
		NdfCommGlobals.SymbolicLinkName = NULL;

		return status;
	}

    status = IoCreateSymbolicLink(NdfCommGlobals.SymbolicLinkName, &deviceName);

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

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Initializing complete"
	);

    return status;
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
		"Releasing..."
	);

	///
	///
	///
	ExWaitForRundownProtectionRelease(&NdfCommGlobals.LibraryRundownRef);

	if (NdfCommGlobals.SymbolicLinkName)
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_VERBOSE,
			0,
			"Deleting symbolic link..."
		);

		IoDeleteSymbolicLink(NdfCommGlobals.SymbolicLinkName);

		NdfCommUnicodeStringFree(NdfCommGlobals.SymbolicLinkName);
		NdfCommGlobals.SymbolicLinkName = NULL;
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

	ExRundownCompleted(&NdfCommGlobals.LibraryRundownRef);

	NdfCommDebugTrace(
		TRACE_LEVEL_INFORMATION,
		0,
		"Releasing complete"
	);
}