#include "NdfCommMessageWaiterQueue.h"

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
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irp);
	UNREFERENCED_PARAMETER(InsertContext);

	return STATUS_NOT_IMPLEMENTED;
}

VOID
NdfCommpRemoveMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irp);
}

PIRP
NdfCommpGetNextMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp,
	_In_ PVOID PeekContext
)
{
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irp);
	UNREFERENCED_PARAMETER(PeekContext);

	return NULL;
}

VOID
NdfCommpAcquireMessageWaiterLock(
	_In_ PIO_CSQ Csq,
	_Out_ PKIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irql);
}

VOID
NdfCommpReleaseMessageWaiterLock(
	_In_ PIO_CSQ Csq,
	_In_ KIRQL Irql
)
{
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irql);
}

VOID
NdfCommpCancelMessageWaiter(
	_In_ PIO_CSQ Csq,
	_In_ PIRP Irp
)
{
	UNREFERENCED_PARAMETER(Csq);
	UNREFERENCED_PARAMETER(Irp);
}