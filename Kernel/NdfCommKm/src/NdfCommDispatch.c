#include "NdfCommDispatch.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommUtils.h"
#include "NdfCommClient.h"
#include "NdfCommMessage.h"

NTSTATUS
NdfCommpDispatchCreate(
	__inout PFILE_OBJECT FileObject,
	__in PIRP Irp
);

VOID
NdfCommpDispatchCleanup(
	__in PFILE_OBJECT FileObject
);

VOID
NdfCommpDispatchClose(
	__in PFILE_OBJECT FileObject
);

NTSTATUS
NdfCommpDispatchControl(
	__in PIO_STACK_LOCATION StackLocation,
	__in PIRP Irp,
	__out PULONG ReturnOutputBufferSize
);

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(INIT, NdfCommInit)
#   pragma alloc_text(PAGE, NdfCommRelease)
#   pragma alloc_text(PAGE, NdfCommDispatch)
#   pragma alloc_text(PAGE, NdfCommpDispatchCreate)
#   pragma alloc_text(PAGE, NdfCommpDispatchCleanup)
#   pragma alloc_text(PAGE, NdfCommpDispatchClose)
#   pragma alloc_text(PAGE, NdfCommpDispatchControl)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommDispatch(
	__in struct _DEVICE_OBJECT *DeviceObject,
	__inout struct _IRP *Irp
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION currentStackLocation = IoGetCurrentIrpStackLocation(Irp);
	ULONG returnBufferSize = 0;


	if (DeviceObject != NdfCommGlobals.MessageDeviceObject)
	{
		switch (currentStackLocation->MajorFunction)
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

	switch (currentStackLocation->MajorFunction)
	{
	case IRP_MJ_CREATE:

		status = NdfCommpDispatchCreate(currentStackLocation->FileObject, Irp);
		break;

	case IRP_MJ_CLEANUP:

		NdfCommpDispatchCleanup(currentStackLocation->FileObject);
		break;

	case IRP_MJ_CLOSE:

		NdfCommpDispatchClose(currentStackLocation->FileObject);
		break;

	case IRP_MJ_DEVICE_CONTROL:

		status = NdfCommpDispatchControl(
			currentStackLocation,
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

NTSTATUS
NdfCommpDispatchCreate(
	__inout PFILE_OBJECT FileObject,
	__in PIRP Irp
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_CLIENT client = NULL;
	PVOID clientContext = NULL;
	ULONG clientContextSize = 0;
	PFILE_FULL_EA_INFORMATION eaInfo = Irp->AssociatedIrp.SystemBuffer;

	if (!eaInfo)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	if (eaInfo->EaNameLength != NDFCOMM_EA_NAME_NOT_NULL_LENGTH)
	{
		return STATUS_INVALID_EA_NAME;
	}

	if (
		RtlCompareMemory(
			eaInfo->EaName,
			NDFCOMM_EA_NAME,
			NDFCOMM_EA_NAME_NOT_NULL_LENGTH) != NDFCOMM_EA_NAME_NOT_NULL_LENGTH
		)
	{
		return STATUS_INVALID_EA_NAME;
	}

	clientContext = NdfCommAdd2Ptr(eaInfo->EaName, NDFCOMM_EA_NAME_WITH_NULL_LENGTH);
	clientContextSize = eaInfo->EaValueLength;

	///
	/// Acquire library rundown protection as we are about adding the new client.
	/// All successfully added clients will be released at library cleanup. 
	/// These processes(adding/releasing) must be synchronized.
	///
	if (ExAcquireRundownProtection(&NdfCommGlobals.LibraryRundownRef))
	{
		if (InterlockedIncrement(&NdfCommGlobals.ClientsCount) <= (LONG)NdfCommGlobals.MaxClientsCount)
		{
			status = NdfCommClientCreate(&client);

			if (NT_SUCCESS(status))
			{
				status = NdfCommGlobals.ConnectNotifyCallback(
					client,
					clientContext,
					clientContextSize,
					&client->ConnectionCookie
				);

				if (NT_SUCCESS(status))
				{
					FileObject->FsContext = client;

					NdfCommConcurentListAdd(&NdfCommGlobals.ClientList, &client->ListEntry);
				}
				else
				{
					LONG clientsCount = 0;

					NdfCommClientFree(client);
					client = NULL;

					clientsCount = InterlockedDecrement(&NdfCommGlobals.ClientsCount);

					ASSERT(clientsCount >= 0);
				}

			}
		}
		else
		{
			status = STATUS_CONNECTION_COUNT_LIMIT;
		}

		ExReleaseRundownProtection(&NdfCommGlobals.LibraryRundownRef);
	}
	else
	{
		status = STATUS_CONNECTION_ABORTED;
	}

	return status;
}

VOID
NdfCommpDispatchCleanup(
	__in PFILE_OBJECT FileObject
)
{
	PAGED_CODE();

	PNDFCOMM_CLIENT client = NULL;
	LONG clientsCount = 0;

	client = FileObject->FsContext;

	ASSERT(client);

	NdfCommClientReleaseWaiters(client);

	///
	/// Synchronize external(user app CloseHandle) and internal(at library cleanup) releasing
	///
	if (ExAcquireRundownProtection(&NdfCommGlobals.LibraryRundownRef))
	{
		clientsCount = InterlockedDecrement(&NdfCommGlobals.ClientsCount);

		ASSERT(clientsCount >= 0);

		///
		/// Stops messages delivering
		///
		ExWaitForRundownProtectionRelease(&client->MsgNotificationRundownRef);

		NdfCommGlobals.DisconnectNotifyCallback(client->ConnectionCookie);

		ExRundownCompleted(&client->MsgNotificationRundownRef);

		NdfCommConcurentListRemove(&NdfCommGlobals.ClientList, &client->ListEntry);

		ExReleaseRundownProtection(&NdfCommGlobals.LibraryRundownRef);
	}
}

VOID
NdfCommpDispatchClose(
	__in PFILE_OBJECT FileObject
)
{
	PAGED_CODE();

	PNDFCOMM_CLIENT client = FileObject->FsContext;

	ASSERT(client);

	NdfCommClientFree(client);
}

NTSTATUS
NdfCommpDispatchControl(
	__in PIO_STACK_LOCATION StackLocation,
	__in PIRP Irp,
	__out PULONG ReturnOutputBufferSize
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PVOID inputBuffer = StackLocation->Parameters.DeviceIoControl.Type3InputBuffer;
	ULONG inputBufferSize = StackLocation->Parameters.DeviceIoControl.InputBufferLength;
	PVOID outputBuffer = Irp->UserBuffer;
	ULONG outputBufferSize = StackLocation->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG controlCode = StackLocation->Parameters.DeviceIoControl.IoControlCode;
	KPROCESSOR_MODE accessMode = Irp->RequestorMode;

	if (!StackLocation->FileObject->FsContext)
	{
		return STATUS_INVALID_DEVICE_STATE;
	}

	///
	/// Discard non compatible ctl
	///
	if (METHOD_FROM_CTL_CODE(controlCode) != METHOD_NEITHER)
	{
		return STATUS_INVALID_PARAMETER;
	}

	switch (controlCode)
	{
	case NDFCOMM_GETMESSAGE:
		break;

	case NDFCOMM_SENDMESSAGE:

		status = NdfCommMessageDeliverToKm(
			StackLocation->FileObject,
			inputBuffer,
			inputBufferSize,
			outputBuffer,
			outputBufferSize,
			ReturnOutputBufferSize,
			accessMode
		);

		break;

	case NDFCOMM_REPLYMESSAGE:
		break;

	default:
		status = STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}