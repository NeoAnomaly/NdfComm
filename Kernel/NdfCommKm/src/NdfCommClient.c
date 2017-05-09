#include "NdfCommClient.h"
#include "NdfCommShared.h"
#include "NdfCommGlobalData.h"
#include "NdfCommMemory.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommClientCreate)
#   pragma alloc_text(PAGE, NdfCommClientFree)
#   pragma alloc_text(PAGE, NdfCommClientReleaseWaiters)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommClientCreate(
	__deref_out PNDFCOMM_CLIENT* Client
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_CLIENT client = NULL;

	if (!Client)
	{
		return STATUS_INVALID_PARAMETER;
	}	
	
	client = ExAllocatePoolWithTag(PagedPool, sizeof(NDFCOMM_CLIENT), NDFCOMM_CLIENT_MEM_TAG);
	if (!client)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	RtlZeroMemory(client, sizeof(NDFCOMM_CLIENT));

	ExInitializeRundownProtection(&client->MsgNotifyRundownRef);
	ExInitializeFastMutex(&client->Lock);
	KeInitializeEvent(&client->DisconnectEvent, NotificationEvent, FALSE);
	NdfCommConcurentListInitialize(&client->ReplyWaiterList);

	*Client = client;

	return status;
}

VOID
NdfCommClientFree(
	__in PNDFCOMM_CLIENT Client
)
{
	PAGED_CODE();

	if (Client)
	{
		NdfCommClientReleaseWaiters(Client);

		ExFreePoolWithTag(Client, NDFCOMM_CLIENT_MEM_TAG);
	}
}

BOOLEAN
NdfCommClientReleaseWaiters(
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