#include "NdfCommProcessing.h"
#include "NdfCommClient.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommDebug.h"
#include "NdfCommUtils.h"

#include <ntifs.h>

enum _NDFCOMMP_WAIT_OBJECTS
{
	DisconnectEvent,
	QueueEvent,

	WaitObjectsCount
};

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

NTSTATUS
NdfCommpGetUserBuffer(
	_Inout_ PIRP Irp,
	_In_ ULONG RequiredBufferLength,
	_Outptr_ PVOID* Buffer
);

FORCEINLINE
VOID
NdfCommpComputeWaitInterval(
	_Inout_opt_ PLARGE_INTEGER Timeout,
	_In_ PLARGE_INTEGER EndTime
)
{
	if (Timeout && Timeout->QuadPart < 0)
	{
		Timeout->QuadPart = KeQueryInterruptTime() - EndTime->QuadPart;

		if (Timeout->QuadPart > 0)
		{
			Timeout->QuadPart = 0;
		}
	}	
}

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommpDeliverMessageToKm)
#   pragma alloc_text(PAGE, NdfCommpEnqueueGetMessageIrp)

#   pragma alloc_text(PAGE, NdfCommpProcessCreateRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessCleanupRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessCloseRequest)
#   pragma alloc_text(PAGE, NdfCommpProcessControlRequest)

#   pragma alloc_text(PAGE, NdfCommpGetUserBuffer)
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
	
	if (METHOD_FROM_CTL_CODE(controlCode) != METHOD_NEITHER)
	{
		return STATUS_INVALID_PARAMETER;
	}

	///
	/// Т.к. мы используем NEITHER IO и юзермодные буферы напрямую,
	/// необходимо быть уверенным в контексте исполнения
	///
	ASSERT(IoGetRequestorProcessId(Irp) == IoGetCurrentProcess());

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
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    ASSERT(client);

	///
	/// Подготавливаем выходной буфер пакета в который будет записано
	/// посылаемое сообщение. Сейчас это просто эмуляция BUFFERED I/O
	///
	try {

			if () 
			{
				ProbeForWrite(
					Irp->UserBuffer, 
					irpSp->Parameters.DeviceIoControl.OutputBufferLength,
					sizeof(UCHAR)
				);
			}
			else {
				OutputBufferLength = 0;
			}
		}

		if (method != METHOD_NEITHER) {
			if (ARGUMENT_PRESENT(InputBuffer)) {
				ProbeForRead(InputBuffer,
					InputBufferLength,
					sizeof(UCHAR));
			}
			else {
				InputBufferLength = 0;
			}
		}

	} except(EXCEPTION_EXECUTE_HANDLER) {

		//
		// An exception was incurred while attempting to probe or write
		// one of the caller's parameters.  Simply return an appropriate
		// error status code.
		//

		return GetExceptionCode();

	}
}

    return IoCsqInsertIrpEx(&client->PendedIrpQueue.Csq, Irp, NULL, NULL);
}

NTSTATUS
NdfCommpGetUserBuffer(
	_Inout_ PIRP Irp,
	_In_ ULONG RequiredBufferLength,
	_Outptr_ PVOID* Buffer
)
{
	PAGED_CODE();

	PMDL mdl = NULL;
	PEPROCESS process = NULL;

	if (Irp->MdlAddress || !Buffer)
	{
		return STATUS_INVALID_PARAMETER;
	}

	mdl = IoAllocateMdl(
		Irp->UserBuffer,
		RequiredBufferLength,
		FALSE,
		FALSE,
		Irp
	);

	if (!mdl)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	process = IoGetRequestorProcess(Irp);

	MmProbeAndLockProcessPages(mdl, process, Irp->RequestorMode, IoWriteAccess);

	*Buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);

	if (*Buffer == NULL)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	return STATUS_SUCCESS;
}

_Check_return_
NTSTATUS
NdfCommSendMessage(
	_In_ PNDFCOMM_CLIENT Client,
	_In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
	_In_ ULONG InputBufferLength,
	_In_opt_ PLARGE_INTEGER Timeout
)
{
	PAGED_CODE();

	UNREFERENCED_PARAMETER(InputBuffer);

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_PENDED_IRP_QUEUE pendedIrpQueue = NULL;
	PIRP irp = NULL;
	PIO_STACK_LOCATION irpSp = NULL;
	PVOID buffer = NULL;
	PVOID waitObjects[WaitObjectsCount] = { 0 };
	
	LARGE_INTEGER remainingTime = { 0 };
	PLARGE_INTEGER timeout = NULL;
	LARGE_INTEGER endTime = { 0 };

	if (NdfCommAcquireClient(Client))
	{
		pendedIrpQueue = &Client->PendedIrpQueue;

		if (Timeout)
		{
			timeout = &remainingTime;

			remainingTime.QuadPart = Timeout->QuadPart;

			if (Timeout->QuadPart < 0)
			{
				endTime.QuadPart = KeQueryInterruptTime() - Timeout->QuadPart;
			}
		}
		else
		{
			timeout = NULL;
		}

		waitObjects[DisconnectEvent] = &Client->DisconnectEvent;

		do
		{
			waitObjects[QueueEvent] = &pendedIrpQueue->Semaphore;

			status = KeWaitForMultipleObjects(
				WaitObjectsCount,
				waitObjects, 
				WaitAny,
				Executive,
				KernelMode,
				TRUE,
				timeout,
				NULL
			);

			if (status == STATUS_TIMEOUT)
			{
				NdfCommDebugTrace(
					TRACE_LEVEL_INFORMATION,
					0,
					"Waiting for listeners failed(timeout)"
				);

				break;
			}

			if (status == (NTSTATUS)DisconnectEvent)
			{
				NdfCommDebugTrace(
					TRACE_LEVEL_INFORMATION,
					0,
					"Waiting for listeners failed(client disconnected)"
				);

				status = STATUS_PORT_DISCONNECTED;
				break;
			}

			///
			///
			///

			irp = IoCsqRemoveNextIrp(&pendedIrpQueue->Csq, &InputBufferLength);

			if (irp)
			{
				NdfCommDebugTrace(
					TRACE_LEVEL_INFORMATION,
					0,
					"Waiting for listeners complete, dequeued IRP(%p)",
					irp
				);

				irpSp = IoGetCurrentIrpStackLocation(irp);

				ASSERT(irpSp->Parameters.DeviceIoControl.OutputBufferLength >= InputBufferLength);

				status = NdfCommpGetUserBuffer(irp, InputBufferLength, &buffer);

				if (!NT_SUCCESS(status))
				{
					NdfCommDebugTrace(
						TRACE_LEVEL_ERROR,
						0,
						"!ERROR: NdfCommpGetUserBuffer for IRP(%p) failed with status: %d",
						irp,
						status
					);

					if (status == STATUS_INSUFFICIENT_RESOURCES)
					{
						NdfCommDebugTrace(
							TRACE_LEVEL_INFORMATION,
							0,
							"Can't map user buffer due to insufficient resources, return IRP(%p) back to the queue",
							irp
						);

						status = IoCsqInsertIrpEx(&pendedIrpQueue->Csq, irp, NULL, NULL);

						if (NT_SUCCESS(status))
						{
							status = STATUS_INSUFFICIENT_RESOURCES;
						}
						else
						{
							NdfCommCompleteIrp(irp, status, 0);
						}

						break;
					}
				}

				///
				///
				///

				RtlCopyMemory(buffer, InputBuffer, InputBufferLength);

				NdfCommCompleteIrp(irp, STATUS_SUCCESS, (ULONG_PTR)InputBufferLength);

				break;
			}
			else
			{
				NdfCommDebugTrace(
					TRACE_LEVEL_INFORMATION,
					0,
					"Waiting for listeners complete, but suitable IRP not found, waiting again..."
				);

				NdfCommpComputeWaitInterval(timeout, &endTime);
			}			

		} while (TRUE);


		NdfCommReleaseClient(Client);
	}
	else
	{
		status = STATUS_PORT_DISCONNECTED;
	}

	return status;
}