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
NdfCommInitializeConcurentList(
	__in PNDFCOMM_CONCURENT_LIST List
)
{
	List->Count = 0;

	ExInitializeFastMutex(&List->Lock);

	InitializeListHead(&List->ListHead);	
}

FORCEINLINE
VOID
NdfCommConcurentListLock(
    __in PNDFCOMM_CONCURENT_LIST List
)
{
    ExAcquireFastMutex(&List->Lock);
}

FORCEINLINE
VOID
NdfCommConcurentListUnlock(
    __in PNDFCOMM_CONCURENT_LIST List
)
{
    ExReleaseFastMutex(&List->Lock);
}

FORCEINLINE
VOID
NdfCommConcurentListInterlockedAdd(
	__in PNDFCOMM_CONCURENT_LIST List,
	__in PLIST_ENTRY Entry
)
{
	if (List && Entry)
	{
        NdfCommConcurentListLock(List);

		InsertTailList(&List->ListHead, Entry);

		List->Count++;

        NdfCommConcurentListUnlock(List);
	}
}

FORCEINLINE
VOID
NdfCommConcurentListInterlockedRemove(
	__in PNDFCOMM_CONCURENT_LIST List,
	__in PLIST_ENTRY Entry
)
{
	if (List && Entry)
	{
        NdfCommConcurentListLock(List);

		ASSERT(!IsListEmpty(&List->ListHead));
		ASSERT(List->Count > 0);
		
		RemoveEntryList(Entry);

		List->Count--;

        NdfCommConcurentListUnlock(List);
	}
}
