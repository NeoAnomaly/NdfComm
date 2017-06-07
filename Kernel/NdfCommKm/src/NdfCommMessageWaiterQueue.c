#include "NdfCommMessageWaiterQueue.h"
#include "NdfCommClient.h"
#include "NdfCommUtils.h"

#include <ntintsafe.h>          /// for LONG_MAX

IO_CSQ_INSERT_IRP_EX NdfCommpAddMessageWaiter;
IO_CSQ_REMOVE_IRP NdfCommpRemoveMessageWaiter;
IO_CSQ_PEEK_NEXT_IRP NdfCommpGetNextMessageWaiter;
IO_CSQ_ACQUIRE_LOCK NdfCommpAcquireMessageWaiterLock;
IO_CSQ_RELEASE_LOCK NdfCommpReleaseMessageWaiterLock;
IO_CSQ_COMPLETE_CANCELED_IRP NdfCommpCancelMessageWaiter;

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommInitializeMessageWaiterQueue)
#endif // ALLOC_PRAGMA

_Check_return_
NTSTATUS
NdfCommInitializeMessageWaiterQueue(
	_Inout_ PNDFCOMM_MESSAGE_WAITER_QUEUE MsgQueue
)
{
	PAGED_CODE();

	MsgQueue->MinimumWaiterLength = -1;
	KeInitializeSemaphore(&MsgQueue->Semaphore, 0, LONG_MAX);
	KeInitializeEvent(&MsgQueue->Event, NotificationEvent, FALSE);

	NdfCommInitializeConcurentList(&MsgQueue->Waiters);

	return IoCsqInitializeEx(
		&MsgQueue->Csq,
		NdfCommpAddMessageWaiter,
		NdfCommpRemoveMessageWaiter,
		NdfCommpGetNextMessageWaiter,
		NdfCommpAcquireMessageWaiterLock,
		NdfCommpReleaseMessageWaiterLock,
		NdfCommpCancelMessageWaiter
	);
}

NTSTATUS
NdfCommpAddMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp,
	_In_ PVOID InsertContext
)
{
	UNREFERENCED_PARAMETER(InsertContext);

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);
	PNDFCOMM_CLIENT client = CONTAINING_RECORD(messageQueue, NDFCOMM_CLIENT, MessageQueue);
	PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

	if (NdfCommAcquireClient(client))
	{
		NdfCommConcurentListAdd(&messageQueue->Waiters, &Irp->Tail.Overlay.ListEntry);

		if (irpSp->Parameters.DeviceIoControl.OutputBufferLength >= messageQueue->MinimumWaiterLength)
		{
			KeSetEvent(&messageQueue->Event, IO_NO_INCREMENT, FALSE);

			messageQueue->MinimumWaiterLength = -1;
		}

		KeReleaseSemaphore(&messageQueue->Semaphore, SEMAPHORE_INCREMENT, 1, FALSE);

		status = STATUS_PENDING;

		NdfCommReleaseClient(client);
	}
	else
	{
		status = STATUS_PORT_DISCONNECTED;
	}	

	return status;
}

VOID
NdfCommpRemoveMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);

	NdfCommConcurentListRemove(&messageQueue->Waiters, &Irp->Tail.Overlay.ListEntry);	
}

PIRP
NdfCommpGetNextMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp,
	_In_ PVOID PeekContext
)
{
	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);
	PLIST_ENTRY queuedIrpEntry = NULL;
	PIRP queuedIrp = NULL;
	ULONG size = *((PULONG)PeekContext);

	if (Irp)
	{
		queuedIrpEntry = Irp->Tail.Overlay.ListEntry.Flink;
	}
	else
	{
		queuedIrpEntry = messageQueue->Waiters.ListHead.Flink;
	}

	if (queuedIrpEntry = &messageQueue->Waiters.ListHead)
	{
		if (IsListEmpty(&messageQueue->Waiters.ListHead))
		{
			queuedIrp = NULL;
		}
		else
		{
			queuedIrp = CONTAINING_RECORD(queuedIrpEntry, IRP, Tail.Overlay.ListEntry);
		}		
	}
	else
	{
		for (; queuedIrpEntry != &messageQueue->Waiters.ListHead; queuedIrpEntry = queuedIrpEntry->Flink)
		{

		}
	}

	return queuedIrp;
}

VOID
NdfCommpAcquireMessageWaiterLock(
	_In_ PIO_CSQ Csq,
	_Out_ PKIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Irql);

	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);

	NdfCommConcurentListLock(&messageQueue->Waiters);
}

VOID
NdfCommpReleaseMessageWaiterLock(
	_In_ PIO_CSQ Csq,
	_In_ KIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Irql);

	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);

	NdfCommConcurentListUnlock(&messageQueue->Waiters);
}

VOID
NdfCommpCancelMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	PNDFCOMM_MESSAGE_WAITER_QUEUE messageQueue = CONTAINING_RECORD(Csq, NDFCOMM_MESSAGE_WAITER_QUEUE, Csq);
	LARGE_INTEGER timeout = { 0 };

	KeWaitForSingleObject(&messageQueue->Semaphore, Executive, KernelMode, FALSE, &timeout);

	NdfCommCompleteIrp(Irp, STATUS_CANCELLED, 0);
}