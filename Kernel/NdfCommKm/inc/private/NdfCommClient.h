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

_Check_return_
NTSTATUS
NdfCommCreateClient(
	_Outptr_ PNDFCOMM_CLIENT* Client
);

VOID
NdfCommFreeClient(
	_In_ PNDFCOMM_CLIENT Client
);

BOOLEAN
NdfCommReleaseClientWaiters(
	_In_ PNDFCOMM_CLIENT Client
);

VOID
NdfCommDisconnectAllClients(
    VOID
);
