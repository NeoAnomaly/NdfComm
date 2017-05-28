#include "NdfCommClient.h"
#include "NdfCommShared.h"
#include "NdfCommGlobalData.h"
#include "NdfCommDebug.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommCreateClient)
#   pragma alloc_text(PAGE, NdfCommFreeClient)
#   pragma alloc_text(PAGE, NdfCommReleaseClientWaiters)
#   pragma alloc_text(PAGE, NdfCommDisconnectClient)
#   pragma alloc_text(PAGE, NdfCommDisconnectAllClients)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommCreateClient(
	__deref_out PNDFCOMM_CLIENT* Client
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
	
	client = ExAllocatePoolWithTag(PagedPool, sizeof(NDFCOMM_CLIENT), NDFCOMM_CLIENT_MEM_TAG);
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

	ExInitializeRundownProtection(&client->MsgNotificationRundownRef);
	ExInitializeFastMutex(&client->Lock);
	KeInitializeEvent(&client->DisconnectEvent, NotificationEvent, FALSE);
	NdfCommInitializeConcurentList(&client->ReplyWaiterList);

	*Client = client;

	return status;
}

VOID
NdfCommFreeClient(
	__in PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	if (Client)
	{
		//NdfCommReleaseClientWaiters(Client);

		ExFreePoolWithTag(Client, NDFCOMM_CLIENT_MEM_TAG);
	}
}

BOOLEAN
NdfCommReleaseClientWaiters(
	__in PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	BOOLEAN result = FALSE;

	if (Client)
	{
		ExAcquireFastMutex(&Client->Lock);

		if (!Client->Disconnected)
		{
			/*KeSetEvent(&Client->DisconnectEvent, IO_NO_INCREMENT, FALSE);*/
			/// FREE REPLY WAITERS

			Client->Disconnected = TRUE;

			result = TRUE;
		}

		ExReleaseFastMutex(&Client->Lock);
	}
	
	return result;
}

VOID
NdfCommDisconnectClient(
    PNDFCOMM_CLIENT Client
)
{
    PAGED_CODE();

    NdfCommReleaseClientWaiters(Client);
}

VOID
NdfCommDisconnectAllClients(
    VOID
)
{
    NdfCommConcurentListLock(&NdfCommGlobals.ClientList);

    if (NdfCommGlobals.ClientList.Count > 0)
    {
        PNDFCOMM_CLIENT client = NULL;
        PNDFCOMM_CONCURENT_LIST list = &NdfCommGlobals.ClientList;

        ASSERT(!IsListEmpty(&list->ListHead));

        for (PLIST_ENTRY entry = list->ListHead.Flink; entry != &list->ListHead; entry = entry->Flink)
        {
            client = CONTAINING_RECORD(entry, NDFCOMM_CLIENT, ListEntry);

            NdfCommDisconnectClient(client);
        }
    }

    NdfCommConcurentListUnlock(&NdfCommGlobals.ClientList);
}