#include "NdfCommDebug.h"

#include <ntstrsafe.h>
#include <stdarg.h>         // for va_start, etc.

#define NDFCOMMP_PREFIX_BUFFER_LENGTH (50)

VOID
NdfCommDbgPrint(
	__in ULONG Level,
	__in PCSTR Fn,
	__in PCSTR MessageFmt,
	...
)
{
	CHAR prefix[NDFCOMMP_PREFIX_BUFFER_LENGTH];

	va_list arglist;

	va_start(arglist, MessageFmt);

	if (!NT_SUCCESS(
		RtlStringCbPrintfA(
			prefix,
			NDFCOMMP_PREFIX_BUFFER_LENGTH,
			"NdfComm!%s",
			Fn
		)
	)
		)
	{
		RtlStringCbCopyA(prefix, NDFCOMMP_PREFIX_BUFFER_LENGTH, "NdfComm:");
	}

	vDbgPrintExWithPrefix(
		prefix,
		(ULONG)-1,
		Level,
		MessageFmt,
		arglist
	);
}