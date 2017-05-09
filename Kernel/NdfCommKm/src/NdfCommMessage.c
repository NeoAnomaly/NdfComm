#include "NdfCommMessage.h"
#include "NdfCommClient.h"
#include "NdfCommGlobalData.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommMessageDeliverToKm)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommMessageDeliverToKm(
	__in PFILE_OBJECT FileObject,
	__in const PVOID InputBuffer,
	__in ULONG InputBufferSize,
	__out PVOID OutputBuffer,
	__in ULONG OutputBufferSize,
	__out PULONG ReturnOutputBufferSize,
	__in KPROCESSOR_MODE Mode
)
{
	PAGED_CODE();

	NTSTATUS status = STATUS_SUCCESS;
	PNDFCOMM_CLIENT client = FileObject->FsContext;

	ASSERT(client);

	if (!InputBuffer || !InputBufferSize)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (Mode != KernelMode)
	{
		try
		{
			ProbeForRead(InputBuffer, (SIZE_T)InputBufferSize, sizeof(UCHAR));
			ProbeForWrite(OutputBuffer, (SIZE_T)OutputBufferSize, sizeof(UCHAR));
		}
		except(EXCEPTION_EXECUTE_HANDLER)
		{
			return GetExceptionCode();
		}		
	}

	if (!ExAcquireRundownProtection(&client->MsgNotificationRundownRef))
	{
		return STATUS_PORT_DISCONNECTED;
	}

	status = NdfCommGlobals.MessageNotifyCallback(
		client->ConnectionCookie,
		InputBuffer,
		InputBufferSize,
		OutputBuffer,
		OutputBufferSize,
		ReturnOutputBufferSize
	);

	ExReleaseRundownProtection(&client->MsgNotificationRundownRef);

	return status;
}