#include "NdfCommClient.h"
#include "NdfCommShared.h"
#include "NdfCommGlobalData.h"
#include "NdfCommDebug.h"
#include "NdfCommUtils.h"

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
	status = NdfCommInitializeMessageWaiterQueue(&client->PendedIrpQueue);
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

	ASSERT(Client);

	if (Client)
	{
		//NdfCommConcurentListLock(&Client->PendedIrpQueue.Waiters);
		ASSERT(IsListEmpty(&Client->PendedIrpQueue.IrpList.ListHead));
		//NdfCommConcurentListUnlock(&Client->PendedIrpQueue.Waiters);

		ExFreePoolWithTag(Client, NDFCOMM_CLIENT_MEM_TAG);
	}
}

VOID
NdfCommDisconnectClient(
	_Inout_ PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	PIRP irp = NULL;
	ULONG peekContext = 0;

	ASSERT(Client);

	if (Client)
	{
		///
		/// Check and set disconnected bit to avoid multiple wait on the RundownProtect
		///

		if (InterlockedBitTestAndSet(&Client->State, NDFCOMM_CLIENT_STATE_CONNECTION_BROKEN_BIT) == FALSE)
		{
			///
			///
			///

			KeSetEvent(&Client->DisconnectEvent, IO_NO_INCREMENT, FALSE);			

			///
			/// Invalidate client
			///
			ExWaitForRundownProtectionRelease(&Client->RundownProtect);

			///
			/// remove all pended IRPs
			///
			while (TRUE)
			{
				irp = IoCsqRemoveNextIrp(&Client->PendedIrpQueue.Csq, &peekContext);

				if (!irp)
				{
					break;
				}

				NdfCommCompleteIrp(irp, STATUS_PORT_DISCONNECTED, 0);
			}


			ExRundownCompleted(&Client->RundownProtect);
		}
	}
}