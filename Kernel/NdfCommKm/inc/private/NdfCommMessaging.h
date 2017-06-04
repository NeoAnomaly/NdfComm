#pragma once

#include <ntddk.h>
#include "NdfCommConcurentList.h"

typedef struct _NDFCOMM_MESSAGE_WAITER_QUEUE
{
    IO_CSQ Csq;
    NDFCOMM_CONCURENT_LIST Waiters;
    INT MinimumWaiterLength;
    KSEMAPHORE Semaphore;
    KEVENT Event;
} NDFCOMM_MESSAGE_WAITER_QUEUE, *PNDFCOMM_MESSAGE_WAITER_QUEUE;

_Check_return_
NTSTATUS
NdfCommInitializeMessageWaiterQueue(
    _Inout_ PNDFCOMM_MESSAGE_WAITER_QUEUE MsgQueue
);

NTSTATUS
NdfCommDeliverMessageToKm(
	_In_ PFILE_OBJECT FileObject,
	_In_reads_bytes_(InputBufferSize) const PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferSize,
	_In_ KPROCESSOR_MODE Mode
);

NTSTATUS
NdfCommQueueGetMessageIrp(
    _In_ PFILE_OBJECT FileObject,
    _Inout_ PIRP Irp
);
