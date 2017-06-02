#include "NdfCommMessage.h"
#include "NdfCommClient.h"
#include "NdfCommGlobalData.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, NdfCommDeliverMessageToKm)
#endif // ALLOC_PRAGMA

NTSTATUS
NdfCommDeliverMessageToKm(
	_In_ PFILE_OBJECT FileObject,
	_In_reads_bytes_(InputBufferSize) const PVOID InputBuffer,
	_In_ ULONG InputBufferSize,
	_Out_writes_bytes_to_(OutputBufferSize, *ReturnOutputBufferSize) PVOID OutputBuffer,
	_In_ ULONG OutputBufferSize,
	_Out_ PULONG ReturnOutputBufferSize,
	_In_ KPROCESSOR_MODE Mode
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