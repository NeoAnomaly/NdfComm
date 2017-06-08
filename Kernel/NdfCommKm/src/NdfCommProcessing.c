#include "NdfCommProcessing.h"
#include "NdfCommClient.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"

NTSTATUS
NdfCommpDeliverMessageToKm(
	_In_ PFILE_OBJECT FileObject,
	_In_reads_bytes_(InputBufferSize) const PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferSize,
	_In_ KPROCESSOR_MODE Mode
);

NTSTATUS
NdfCommpEnqueueGetMessageIrp(
	_In_ PFILE_OBJECT FileObject,
	_Inout_ PIRP Irp
);

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommpDeliverMessageToKm)
#   pragma alloc_text(PAGE, NdfCommpEnqueueGetMessageIrp)

#   pragma alloc_text(PAGE, NdfCommpProcessCreateRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessCleanupRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessCloseRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessControlRequest)

#   pragma alloc_text(PAGE, NdfCommSendMessage)
#endif // ALLOC_PRAGMA

_Check_return_
NTSTATUS
NdfCommpProcessCreateRequest(
	_Inout_ PFILE_OBJECT FileObject,
	_In_ PIRP Irp
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
	/// Check that library is not in tearing down state
	///
	if (ExAcquireRundownProtection(&NdfCommGlobals.LibraryRundownProtect))
	{
		if (InterlockedIncrement(&NdfCommGlobals.ActiveClientsCount) <= (LONG)NdfCommGlobals.MaxClientsCount)
		{
			status = NdfCommCreateClient(&client);

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

					NdfCommConcurentListInterlockedAdd(&NdfCommGlobals.ClientList, &client->ListEntry);
				}
				else
				{
					LONG clientsCount = 0;

					NdfCommFreeClient(client);
					client = NULL;

					clientsCount = InterlockedDecrement(&NdfCommGlobals.ActiveClientsCount);

					ASSERT(clientsCount >= 0);
				}

			}
		}
		else
		{
			status = STATUS_CONNECTION_COUNT_LIMIT;
		}

		ExReleaseRundownProtection(&NdfCommGlobals.LibraryRundownProtect);
	}
	else
	{
		status = STATUS_CONNECTION_REFUSED;
	}

	return status;
}

VOID
NdfCommpProcessCleanupRequest(
	_In_ PFILE_OBJECT FileObject
)
{
	PAGED_CODE();

	PNDFCOMM_CLIENT client = NULL;
	LONG clientsCount = 0;

	client = FileObject->FsContext;

	ASSERT(client);

	NdfCommDisconnectClient(client);

	if (ExAcquireRundownProtection(&NdfCommGlobals.LibraryRundownProtect))
	{
		NdfCommGlobals.DisconnectNotifyCallback(client->ConnectionCookie);

		ExReleaseRundownProtection(&NdfCommGlobals.LibraryRundownProtect);
	}

	clientsCount = InterlockedDecrement(&NdfCommGlobals.ActiveClientsCount);

	ASSERT(clientsCount >= 0);
}

VOID
NdfCommpProcessCloseRequest(
	_Inout_ PFILE_OBJECT FileObject
)
{
	PAGED_CODE();

	PNDFCOMM_CLIENT client = FileObject->FsContext;

	ASSERT(client);

	FileObject->FsContext = NULL;

	NdfCommConcurentListInterlockedRemove(&NdfCommGlobals.ClientList, &client->ListEntry);

	NdfCommFreeClient(client);
}

_Check_return_
NTSTATUS
NdfCommpProcessControlRequest(
	_In_ PIO_STACK_LOCATION IrpSp,
	_In_ PIRP Irp,
	_Out_ PULONG ReturnOutputBufferSize
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PVOID inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	ULONG inputBufferSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
	PVOID outputBuffer = Irp->UserBuffer;
	ULONG outputBufferSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG controlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
	KPROCESSOR_MODE accessMode = Irp->RequestorMode;

	if (!IrpSp->FileObject->FsContext)
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

		status = NdfCommpEnqueueGetMessageIrp(IrpSp->FileObject, Irp);
		break;



	case NDFCOMM_SENDMESSAGE:

		status = NdfCommpDeliverMessageToKm(
			IrpSp->FileObject,
			inputBuffer,
			inputBufferSize,
			outputBuffer,
			outputBufferSize,
			ReturnOutputBufferSize,
			accessMode
		);

		break;



	case NDFCOMM_REPLYMESSAGE:

		status = STATUS_NOT_IMPLEMENTED;
		break;



	default:
		status = STATUS_INVALID_PARAMETER;
		break;
	}

	return status;
}

NTSTATUS
NdfCommpDeliverMessageToKm(
	_In_ PFILE_OBJECT FileObject,
	_In_reads_bytes_(InputBufferSize) const PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferSize,
	_In_ KPROCESSOR_MODE Mode
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_CLIENT client = FileObject->FsContext;

	ASSERT(client);

	if (!InputBuffer || !InputBufferSize)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (Mode != KernelMode)
	{
		try
		{
			ProbeForRead(InputBuffer, (SIZE_T)InputBufferSize, sizeof(UCHAR));
			ProbeForWrite(OutputBuffer, (SIZE_T)OutputBufferSize, sizeof(UCHAR));
		}
		except(EXCEPTION_EXECUTE_HANDLER)
		{
			return GetExceptionCode();
		}		
	}

	if (!NdfCommAcquireClient(client))
	{
		return STATUS_PORT_DISCONNECTED;
	}

	status = NdfCommGlobals.MessageNotifyCallback(
		client->ConnectionCookie,
		InputBuffer,
		InputBufferSize,
		OutputBuffer,
		OutputBufferSize,
		ReturnOutputBufferSize
	);

	NdfCommReleaseClient(client);

	return status;
}

NTSTATUS
NdfCommpEnqueueGetMessageIrp(
    _In_ PFILE_OBJECT FileObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();

    PNDFCOMM_CLIENT client = FileObject->FsContext;

    ASSERT(client);

    return IoCsqInsertIrpEx(&client->PendedIrpQueue.Csq, Irp, NULL, NULL);
}

_Check_return_
NTSTATUS
NdfCommSendMessage(
	_In_ PNDFCOMM_CLIENT Client,
	_In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_Out_writes_bytes_opt_(*ReplyBufferLength) PVOID ReplyBuffer,
	_Inout_opt_ PULONG ReplyBufferLength,
	_In_opt_ PLARGE_INTEGER Timeout
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(ReplyBuffer);
	UNREFERENCED_PARAMETER(ReplyBufferLength);
	UNREFERENCED_PARAMETER(Timeout);

	PNDFCOMM_PENDED_IRP_QUEUE pendedIrpQueue = NULL;
//	ULONG requiredBufferLength = InputBufferLength;

	if (NdfCommAcquireClient(Client))
	{
		pendedIrpQueue = &Client->PendedIrpQueue;



		NdfCommReleaseClient(Client);
	}

	return STATUS_NOT_IMPLEMENTED;
}