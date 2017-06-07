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
	_In_ PNDFCOMM_CONCURENT_LIST List
)
{
	List->Count = 0;

	ExInitializeFastMutex(&List->Lock);

	InitializeListHead(&List->ListHead);	
}

FORCEINLINE
VOID
NdfCommConcurentListLock(
    _In_ _Requires_lock_not_held_(List->Lock) _Acquires_lock_(List->Lock) 
	PNDFCOMM_CONCURENT_LIST List
)
{
    ExAcquireFastMutex(&List->Lock);
}

FORCEINLINE
VOID
NdfCommConcurentListUnlock(
	_In_ _Requires_lock_held_(List->Lock) _Releases_lock_(List->Lock) 
	PNDFCOMM_CONCURENT_LIST List
)
{
    ExReleaseFastMutex(&List->Lock);
}

FORCEINLINE
VOID
NdfCommConcurentListAdd(
	_Inout_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	InsertTailList(&List->ListHead, Entry);

	List->Count++;
}

FORCEINLINE
VOID
NdfCommConcurentListRemove(
	_Inout_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	ASSERT(!IsListEmpty(&List->ListHead));
	ASSERT(List->Count > 0);

	RemoveEntryList(Entry);

	List->Count--;
}

FORCEINLINE
VOID
NdfCommConcurentListInterlockedAdd(
	_Inout_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	NdfCommConcurentListLock(List);

	NdfCommConcurentListAdd(List, Entry);

	NdfCommConcurentListUnlock(List);
}

FORCEINLINE
VOID
NdfCommConcurentListInterlockedRemove(
	_Inout_ PNDFCOMM_CONCURENT_LIST List,
	_In_ PLIST_ENTRY Entry
)
{
	NdfCommConcurentListLock(List);

	NdfCommConcurentListRemove(List, Entry);

	NdfCommConcurentListUnlock(List);
}
