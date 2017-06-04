#include "NdfCommMessaging.h"
#include "NdfCommClient.h"
#include "NdfCommGlobalData.h"

#include <ntintsafe.h>          /// for LONG_MAX

IO_CSQ_INSERT_IRP_EX NdfCommpAddMessageWaiter;
IO_CSQ_REMOVE_IRP NdfCommpRemoveMessageWaiter;
IO_CSQ_PEEK_NEXT_IRP NdfCommpGetNextMessageWaiter;
IO_CSQ_ACQUIRE_LOCK NdfCommpAcquireMessageWaiterLock;
IO_CSQ_RELEASE_LOCK NdfCommpReleaseMessageWaiterLock;
IO_CSQ_COMPLETE_CANCELED_IRP NdfCommpCancelMessageWaiter;

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommDeliverMessageToKm)
#   pragma alloc_text(PAGE, NdfCommQueueGetMessageIrp)
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
NdfCommDeliverMessageToKm(
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

	if (!ExAcquireRundownProtection(&client->MsgNotificationRundownRef))
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

	ExReleaseRundownProtection(&client->MsgNotificationRundownRef);

	return status;
}

NTSTATUS
NdfCommQueueGetMessageIrp(
    _In_ PFILE_OBJECT FileObject,
    _Inout_ PIRP Irp
)
{
    PAGED_CODE();

    PNDFCOMM_CLIENT client = FileObject->FsContext;

    ASSERT(client);

    return IoCsqInsertIrpEx(&client->MessageQueue.Csq, Irp, NULL, NULL);
}