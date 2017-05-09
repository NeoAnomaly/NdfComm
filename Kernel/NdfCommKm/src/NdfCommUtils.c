#include "NdfCommUtils.h"
#include "NdfCommMemory.h"
#include <ntintsafe.h>

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommUnicodeStringAlloc)
#   pragma alloc_text(PAGE, NdfCommUnicodeStringFree)
#   pragma alloc_text(PAGE, NdfCommUnicodeStringAssignUnicodeString)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommUnicodeStringAlloc(
	__inout PUNICODE_STRING UnicodeString,
	__in USHORT MaxCbLength
)
{
	PAGED_CODE();

	if (!UnicodeString)
	{
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS status = RtlUnicodeStringValidate(UnicodeString);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	RtlZeroMemory(UnicodeString, sizeof(UNICODE_STRING));

	UnicodeString->Buffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, MaxCbLength, NDFCOMM_STRING_MEM_TAG);
	if (UnicodeString->Buffer == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	UnicodeString->MaximumLength = MaxCbLength;

	return status;
}

VOID
NdfCommUnicodeStringFree(
	__inout PUNICODE_STRING UnicodeString
)
{
	PAGED_CODE();

	if (!UnicodeString)
	{
		return;
	}

	if (UnicodeString->Buffer != NULL)
	{
		ExFreePoolWithTag(UnicodeString->Buffer, NDFCOMM_STRING_MEM_TAG);

		RtlZeroMemory(UnicodeString, sizeof(UNICODE_STRING));
	}
}

NTSTATUS
NdfCommUnicodeStringAssignUnicodeString(
	__inout PUNICODE_STRING UnicodeStringDestination,
	__in PCUNICODE_STRING UnicodeStringSource
)
{
	PAGED_CODE();

	NTSTATUS status;
	USHORT srcCbLength, srcCbLengthAndNull, dstMaxCbLength;

	srcCbLength = UnicodeStringSource->Length;
	dstMaxCbLength = UnicodeStringDestination->MaximumLength;

	status = RtlUShortAdd(srcCbLength, sizeof(UNICODE_NULL), &srcCbLengthAndNull);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	if (dstMaxCbLength < srcCbLengthAndNull)
	{
		dstMaxCbLength = srcCbLengthAndNull;

		NdfCommUnicodeStringFree(UnicodeStringDestination);

		status = NdfCommUnicodeStringAlloc(UnicodeStringDestination, dstMaxCbLength);
		if (!NT_SUCCESS(status))
		{
			return status;
		}
	}

	RtlCopyMemory(UnicodeStringDestination->Buffer, UnicodeStringSource->Buffer, srcCbLength);
	UnicodeStringDestination->Length = srcCbLength;

	ASSERT(UnicodeStringDestination->Length + sizeof(UNICODE_NULL) <= UnicodeStringDestination->MaximumLength);

	UnicodeStringDestination->Buffer[UnicodeStringDestination->Length / sizeof(WCHAR)] = UNICODE_NULL;

	return STATUS_SUCCESS;
}

