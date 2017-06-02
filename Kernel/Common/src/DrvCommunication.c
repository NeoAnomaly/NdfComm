#include "DrvCommunication.h"

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(PAGE, ConnectHandler)
#   pragma alloc_text(PAGE, DisconnectHandler)
#   pragma alloc_text(PAGE, MessageHandler)
#endif // ALLOC_PRAGMA

NTSTATUS
ConnectHandler(
    _In_ PNDFCOMM_CLIENT Client,
    _In_reads_bytes_opt_(ContextSize) PVOID ClientContext,
    _In_ ULONG ContextSize,
    _Outptr_result_maybenull_ PVOID *ConnectionCookie
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Client);
    UNREFERENCED_PARAMETER(ClientContext);
    UNREFERENCED_PARAMETER(ContextSize);
    UNREFERENCED_PARAMETER(ConnectionCookie);

	DbgPrint(
		"[ConnectHandler]\n"
		"	Client          -> %p\n"
		"	ClientContext   -> %p\n"
		"	ContextSize     -> %u\n", 
		Client, 
		ClientContext, 
		ContextSize
	);

    return STATUS_SUCCESS;
}

VOID
DisconnectHandler(
    _In_opt_ PVOID ConnectionCookie
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);

	DbgPrint(
		"[DisconnectHandler]\n"
		"	ConnectionCookie -> %p\n", 
		ConnectionCookie
	);
}

NTSTATUS
MessageHandler(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);
    UNREFERENCED_PARAMETER(InputBuffer);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(OutputBuffer);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ReturnOutputBufferLength);

	DbgPrint(
		"[MessageHandler]\n"
		"	ConnectionCookie    -> %p\n"
		"	InputBuffer         -> %p\n"
		"	InputBufferLength   -> %u\n"
		"	OutputBuffer        -> %p\n"
		"	OutputBufferLength  -> %u\n", 
		ConnectionCookie, 
		InputBuffer, 
		InputBufferLength, 
		OutputBuffer, 
		OutputBufferLength
	);

    return STATUS_SUCCESS;
}