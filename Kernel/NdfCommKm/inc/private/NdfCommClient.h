#pragma once

#include "NdfCommConcurentList.h"
#include "NdfCommPendedIrpQueue.h"

#include <ntddk.h>

enum _NDFCOMM_CLIENT_STATE_BITS
{
	NDFCOMM_CLIENT_STATE_CONNECTION_BROKEN_BIT
};

typedef struct _NDFCOMM_CLIENT
{
	LIST_ENTRY ListEntry;

	EX_RUNDOWN_REF RundownProtect;	

	///
	/// ������������, ��� ���� �� �������� �������� ��� ������� ��������� ������
	///
	KEVENT DisconnectEvent;

	UINT64 MessageIdGenerator;

	///
	/// ������� ��������� IRP
	///
    NDFCOMM_PENDED_IRP_QUEUE PendedIrpQueue;

	NDFCOMM_CONCURENT_LIST ReplyWaiterList;
	

	PVOID ConnectionCookie;

	///
	/// ����, ������������, ��� ��� ��������� IRP �������
	///
	LONG State;

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

VOID
NdfCommDisconnectClient(
	_Inout_ PNDFCOMM_CLIENT Client
);

_Check_return_
FORCEINLINE
BOOLEAN
NdfCommAcquireClient(
	_In_ PNDFCOMM_CLIENT Client
)
{
	return ExAcquireRundownProtection(&Client->RundownProtect);
}

FORCEINLINE
VOID
NdfCommReleaseClient(
	_In_ PNDFCOMM_CLIENT Client
)
{
	ExReleaseRundownProtection(&Client->RundownProtect);
}