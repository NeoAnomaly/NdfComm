#pragma once

#include <ntddk.h>

typedef struct _NDFCOMM_CONCURENT_LIST
{
	ULONG Count;
	FAST_MUTEX Lock;
	LIST_ENTRY ListHead;

} NDFCOMM_CONCURENT_LIST, *PNDFCOMM_CONCURENT_LIST;

FORCEINLINE
VOID
NdfCommConcurentListInitialize(
	_In_ PNDFCOMM_CONCURENT_LIST List
)
{
	List->Count = 0;

	ExInitializeFastMutex(&List->Lock);

	InitializeListHead(&List->ListHead);	
}

FORCEINLINE
VOID
NdfCommConcurentListAdd(
	_In_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	if (List && Entry)
	{
		ExAcquireFastMutex(&List->Lock);

		InsertTailList(&List->ListHead, Entry);

		List->Count++;

		ExReleaseFastMutex(&List->Lock);
	}
}

FORCEINLINE
VOID
NdfCommConcurentListRemove(
	_In_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	if (List && Entry)
	{
		ExAcquireFastMutex(&List->Lock);

		ASSERT(!IsListEmpty(&List->ListHead));
		ASSERT(List->Count > 0);
		
		RemoveEntryList(Entry);

		List->Count--;

		ExReleaseFastMutex(&List->Lock);
	}
}
