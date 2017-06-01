#pragma once

#include "NdfCommConcurentList.h"
#include <ntddk.h>

typedef struct _NDFCOMM_CLIENT
{
	LIST_ENTRY ListEntry;
	PVOID ConnectionCookie;
	EX_RUNDOWN_REF MsgNotificationRundownRef;
	FAST_MUTEX Lock;

	UINT64 MessageIdCounter;
	
	KEVENT DisconnectEvent;
	BOOLEAN Disconnected;
	//_FLT_MESSAGE_WAITER_QUEUE MsgQ;
	NDFCOMM_CONCURENT_LIST ReplyWaiterList;

} NDFCOMM_CLIENT, *PNDFCOMM_CLIENT;

NTSTATUS
NdfCommCreateClient(
	__deref_out PNDFCOMM_CLIENT* Client
);

VOID
NdfCommFreeClient(
	__in PNDFCOMM_CLIENT Client
);

BOOLEAN
NdfCommReleaseClientWaiters(
	__in PNDFCOMM_CLIENT Client
);

VOID
NdfCommDisconnectAllClients(
    VOID
);
