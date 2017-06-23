#include "NdfCommPendedIrpQueue.h"
#include "NdfCommClient.h"
#include "NdfCommUtils.h"
#include "NdfCommDebug.h"

#include <ntintsafe.h>          /// for LONG_MAX

IO_CSQ_INSERT_IRP_EX NdfCommpInsertIrp;
IO_CSQ_REMOVE_IRP NdfCommpRemoveIrp;
IO_CSQ_PEEK_NEXT_IRP NdfCommpGetNextIrp;
IO_CSQ_ACQUIRE_LOCK NdfCommpAcquireQueueLock;
IO_CSQ_RELEASE_LOCK NdfCommpReleaseQueueLock;
IO_CSQ_COMPLETE_CANCELED_IRP NdfCommpCompleteCanceledIrp;

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommInitializeMessageWaiterQueue)
#endif // ALLOC_PRAGMA

_Check_return_
NTSTATUS
NdfCommInitializeMessageWaiterQueue(
	_Inout_ PNDFCOMM_PENDED_IRP_QUEUE MsgQueue
)
{
	PAGED_CODE();

	MsgQueue->MinimumWaiterLength = -1;
	KeInitializeSemaphore(&MsgQueue->Semaphore, 0, LONG_MAX);
	KeInitializeEvent(&MsgQueue->Event, NotificationEvent, FALSE);

	NdfCommInitializeConcurentList(&MsgQueue->IrpList);

	return IoCsqInitializeEx(
		&MsgQueue->Csq,
		NdfCommpInsertIrp,
		NdfCommpRemoveIrp,
		NdfCommpGetNextIrp,
		NdfCommpAcquireQueueLock,
		NdfCommpReleaseQueueLock,
		NdfCommpCompleteCanceledIrp
	);
}

NTSTATUS
NdfCommpInsertIrp(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp,
	_In_ PVOID InsertContext
)
{
	UNREFERENCED_PARAMETER(InsertContext);

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_PENDED_IRP_QUEUE queue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);
	PNDFCOMM_CLIENT client = CONTAINING_RECORD(queue, NDFCOMM_CLIENT, PendedIrpQueue);
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
	INT outputBufferLength = 0;

	NdfCommDebugTrace(
		TRACE_LEVEL_VERBOSE,
		0,
		"Enqueuing IRP(%p)...\n"
		"	Client                    -> %p\n"
		"	Client->PendedIrpQueue    -> %p\n"
		"	IRP output buffer length  -> %u",
		Irp,
		client,
		queue,
		irpSp->Parameters.DeviceIoControl.OutputBufferLength
	);

	if (NdfCommAcquireClient(client))
	{
		NdfCommConcurentListAdd(&queue->IrpList, &Irp->Tail.Overlay.ListEntry);

		status = RtlULongToInt(irpSp->Parameters.DeviceIoControl.OutputBufferLength, &outputBufferLength);
		if (!NT_SUCCESS(status))
		{
			outputBufferLength = INT_MAX;
		}

		if (outputBufferLength >= queue->MinimumWaiterLength)
		{
			KeSetEvent(&queue->Event, IO_NO_INCREMENT, FALSE);

			queue->MinimumWaiterLength = -1;
		}

		NdfCommDebugTrace(
			TRACE_LEVEL_VERBOSE,
			0,
			"IRP(%p) enqueued. Signaling semaphore...",
			Irp
		);

		KeReleaseSemaphore(&queue->Semaphore, SEMAPHORE_INCREMENT, 1, FALSE);

		status = STATUS_PENDING;

		NdfCommReleaseClient(client);
	}
	else
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_VERBOSE,
			0,
			"IRP(%p) can't be enqued, because client's state is running down",
			Irp
		);

		status = STATUS_PORT_DISCONNECTED;
	}	

	return status;
}

VOID
NdfCommpRemoveIrp(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	PNDFCOMM_PENDED_IRP_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);

	NdfCommDebugTrace(
		TRACE_LEVEL_VERBOSE,
		0,
		"Removing IRP(%p)...",
		Irp
	);

	NdfCommConcurentListRemove(&messageQueue->IrpList, &Irp->Tail.Overlay.ListEntry);
}

PIRP
NdfCommpGetNextIrp(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp,
	_In_ PVOID PeekContext
)
{
	PNDFCOMM_PENDED_IRP_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);
	PLIST_ENTRY listHead = &messageQueue->IrpList.ListHead;
	PLIST_ENTRY nextEntry = NULL;
	PIRP nextIrp = NULL;
	PIO_STACK_LOCATION nextIrpSp = NULL;
	ULONG requiredOutputBufferLength = 0;

	ASSERT(PeekContext);

	requiredOutputBufferLength = *((PULONG)PeekContext);

	NdfCommDebugTrace(
		TRACE_LEVEL_VERBOSE,
		0,
		"Getting next IRP...\n"
		"	Start IRP                 -> %p\n"
		"	Required buffer length    -> %u",
		Irp,
		requiredOutputBufferLength
	);

	if (Irp)
	{
		nextEntry = Irp->Tail.Overlay.ListEntry.Flink;
	}
	else
	{
		nextEntry = listHead->Flink;
	}

	for (; nextEntry != listHead; nextEntry = nextEntry->Flink)
	{
		nextIrp = CONTAINING_RECORD(nextEntry, IRP, Tail.Overlay.ListEntry);
		nextIrpSp = IoGetCurrentIrpStackLocation(nextIrp);

		if (nextIrpSp->Parameters.DeviceIoControl.OutputBufferLength >= requiredOutputBufferLength)
		{
			break;
		}

		nextIrp = NULL;
	}

	return nextIrp;
}

VOID
NdfCommpAcquireQueueLock(
	_In_ PIO_CSQ Csq,
	_Out_ PKIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Irql);

	PNDFCOMM_PENDED_IRP_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);

	NdfCommConcurentListLock(&messageQueue->IrpList);
}

VOID
NdfCommpReleaseQueueLock(
	_In_ PIO_CSQ Csq,
	_In_ KIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Irql);

	PNDFCOMM_PENDED_IRP_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);

	NdfCommConcurentListUnlock(&messageQueue->IrpList);
}

VOID
NdfCommpCompleteCanceledIrp(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	PNDFCOMM_PENDED_IRP_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_PENDED_IRP_QUEUE, Csq);
	LARGE_INTEGER timeout = { 0 };

	KeWaitForSingleObject(&messageQueue->Semaphore, Executive, KernelMode, FALSE, &timeout);

	NdfCommCompleteIrp(Irp, STATUS_CANCELLED, 0);
}