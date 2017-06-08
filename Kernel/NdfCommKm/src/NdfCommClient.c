#include "NdfCommClient.h"
#include "NdfCommShared.h"
#include "NdfCommGlobalData.h"
#include "NdfCommDebug.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommCreateClient)
#   pragma alloc_text(PAGE, NdfCommFreeClient)
#   pragma alloc_text(PAGE, NdfCommDisconnectClient)
#endif // ALLOC_PRAGMA

_Check_return_
NTSTATUS
NdfCommCreateClient(
	_Outptr_ PNDFCOMM_CLIENT* Client
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_CLIENT client = NULL;

	if (!Client)
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: Parameter Client can't be NULL"
		);

		return STATUS_INVALID_PARAMETER;
	}

	///
	/// Allocate NonPagedPool because synchronization objects required to be stored in the resident memory
	///
	client = ExAllocatePoolWithTag(NonPagedPool, sizeof(NDFCOMM_CLIENT), NDFCOMM_CLIENT_MEM_TAG);
	if (!client)
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: Can't allocate memory for Client"
		);

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(client, sizeof(NDFCOMM_CLIENT));

	ExInitializeRundownProtection(&client->RundownProtect);
	KeInitializeEvent(&client->DisconnectEvent, NotificationEvent, FALSE);
	NdfCommInitializeConcurentList(&client->ReplyWaiterList);
	status = NdfCommInitializeMessageWaiterQueue(&client->MessageQueue);
	if (!NT_SUCCESS(status))
	{
		NdfCommDebugTrace(
			TRACE_LEVEL_ERROR,
			0,
			"!ERROR: NdfCommInitializeMessageWaiterQueue failed with status: %d",
			status
		);
	}

	if (NT_SUCCESS(status))
	{
		*Client = client;
	}	

	return status;
}

VOID
NdfCommFreeClient(
	_In_ PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	if (Client)
	{
		//NdfCommConcurentListLock(&Client->MessageQueue.Waiters);
		ASSERT(IsListEmpty(&Client->MessageQueue.Waiters.ListHead));
		//NdfCommConcurentListUnlock(&Client->MessageQueue.Waiters);

		ExFreePoolWithTag(Client, NDFCOMM_CLIENT_MEM_TAG);
	}
}

VOID
NdfCommDisconnectClient(
	_Inout_ PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	if (Client)
	{
		///
		/// Check and set disconnected bit to avoid multiple wait on the RundownProtect
		///

		if (InterlockedBitTestAndSet(&Client->State, NDFCOMM_CLIENT_STATE_DISCONNECTED_SHIFT) == FALSE)
		{
			///
			/// Invalidate client
			///
			ExWaitForRundownProtectionRelease(&Client->RundownProtect);

			/*KeSetEvent(&Client->DisconnectEvent, IO_NO_INCREMENT, FALSE);*/
			/// FREE REPLY WAITERS

			ExRundownCompleted(&Client->RundownProtect);
		}
	}
}