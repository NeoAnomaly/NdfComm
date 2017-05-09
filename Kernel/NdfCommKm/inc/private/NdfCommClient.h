#pragma once

#include "NdfCommConcurentList.h"
#include <ntddk.h>

typedef struct _NDFCOMM_CLIENT
{
	LIST_ENTRY ListEntry;
	PVOID ConnectionCookie;
	EX_RUNDOWN_REF MsgNotifyRundownRef;
	FAST_MUTEX Lock;

	UINT64 MessageIdCounter;
	
	KEVENT DisconnectEvent;
	BOOLEAN Disconnected;
	//_FLT_MESSAGE_WAITER_QUEUE MsgQ;
	NDFCOMM_CONCURENT_LIST ReplyWaiterList;

} NDFCOMM_CLIENT, *PNDFCOMM_CLIENT;

NTSTATUS
NdfCommClientCreate(
	__deref_out PNDFCOMM_CLIENT* Client
);

VOID
NdfCommClientFree(
	__in PNDFCOMM_CLIENT Client
);

BOOLEAN
NdfCommClientReleaseWaiters(
	__in PNDFCOMM_CLIENT Client
);
