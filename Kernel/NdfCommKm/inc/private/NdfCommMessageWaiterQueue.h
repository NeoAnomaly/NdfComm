#pragma once

#include "NdfCommConcurentList.h"

#include <ntddk.h>

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