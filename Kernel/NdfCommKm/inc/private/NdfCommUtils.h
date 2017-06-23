#pragma once

#include <ntifs.h>
#include <ntstrsafe.h>

FORCEINLINE
NTSTATUS
NdfCommCompleteIrp(
    _Inout_ PIRP Irp,
    _In_ NTSTATUS Status,
    _In_ ULONG_PTR Info
)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

NTSTATUS
NdfCommUnicodeStringAlloc(
	__inout PUNICODE_STRING UnicodeString,
	__in USHORT MaxCbLength
);

VOID
NdfCommUnicodeStringFree(
	__inout PUNICODE_STRING UnicodeString
);

NTSTATUS
NdfCommUnicodeStringAssignUnicodeString(
	__inout PUNICODE_STRING UnicodeStringDestination,
	__in PCUNICODE_STRING UnicodeStringSource
);

FORCEINLINE
NTSTATUS
NdfCommUnicodeStringAssignString(
	__inout PUNICODE_STRING UnicodeStringDestination, 
	__in PCWSTR SourceString
)
{
	PAGED_CODE();

	UNICODE_STRING string;

	NTSTATUS status = RtlUnicodeStringInit(&string, SourceString);
	if (NT_SUCCESS(status))
	{
		status = NdfCommUnicodeStringAssignUnicodeString(UnicodeStringDestination, &string);
	}

	return status;
}



