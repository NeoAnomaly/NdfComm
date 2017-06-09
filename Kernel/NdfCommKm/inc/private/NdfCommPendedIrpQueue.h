#pragma once

#include "NdfCommConcurentList.h"

#include <ntddk.h>

typedef struct _NDFCOMM_PENDED_IRP_QUEUE
{
	IO_CSQ Csq;
	NDFCOMM_CONCURENT_LIST IrpList;
	INT MinimumWaiterLength;
	KSEMAPHORE Semaphore;
	KEVENT Event;
} NDFCOMM_PENDED_IRP_QUEUE, *PNDFCOMM_PENDED_IRP_QUEUE;

_Check_return_
NTSTATUS
NdfCommInitializeMessageWaiterQueue(
	_Inout_ PNDFCOMM_PENDED_IRP_QUEUE MsgQueue
);