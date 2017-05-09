#include "LegacyDrvApi.h"
#include "NdfCommKm.h"

#include <ntddk.h>

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;
NTSTATUS DriverDispatch(IN PDEVICE_OBJECT fdo, IN PIRP Irp);

NTSTATUS
ConnectHandler(
    _In_ PNDFCOMM_CLIENT Client,
    _In_reads_bytes_opt_(ContextSize) PVOID ClientContext,
    _In_ ULONG ContextSize,
    _Outptr_result_maybenull_ PVOID *ConnectionCookie
);

VOID
DisconnectHandler(
    _In_opt_ PVOID ConnectionCookie
);

NTSTATUS
MessageHandler(
    _In_opt_ PVOID ConnectionCookie,
    _In_reads_bytes_opt_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_to_opt_(OutputBufferLength, *ReturnOutputBufferLength) PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength,
    _Out_ PULONG ReturnOutputBufferLength
);

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#pragma alloc_text(PAGE, ConnectHandler)
#pragma alloc_text(PAGE, DisconnectHandler)
#pragma alloc_text(PAGE, MessageHandler)

NTSTATUS 
DriverDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("DriverDispatch called\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

VOID
DriverUnload(
    IN PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symLink;
    PDEVICE_OBJECT fdo;

    PAGED_CODE();

    NdfCommRelease();

    RtlInitUnicodeString(&symLink, DRV_SYMBOLIC_LINK_NAME);
    IoDeleteSymbolicLink(&symLink);

    fdo = DriverObject->DeviceObject;
    IoDeleteDevice(fdo);

    DbgPrint("Driver unloaded\n");
}

NTSTATUS 
DriverEntry(
    IN PDRIVER_OBJECT DriverObject, 
    IN PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    PDEVICE_OBJECT fdo;
    UNICODE_STRING deviceName;
    UNICODE_STRING symLink;

    DbgPrint("Loading driver...\n");

    RtlInitUnicodeString(&deviceName, DRV_DEVICE_NAME);

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &fdo);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("IoCreateDevice failed with status: %d\n", status);

        return status;
    }

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i)
    {
        DriverObject->MajorFunction[i] = DriverDispatch;
    }

    DriverObject->DriverUnload = DriverUnload;

    fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    // Create symbolic link for user mode
    RtlInitUnicodeString(&symLink, DRV_SYMBOLIC_LINK_NAME);

    status = IoCreateSymbolicLink(&symLink, &deviceName);

    if (!NT_SUCCESS(status))
    {
        DbgPrint("IoCreateSymbolicLink failed with status: %d\n", status);

        IoDeleteDevice(fdo);
        return status;
    };

    status = NdfCommInit(
        DriverObject, 
        DRV_NAME,
        ConnectHandler,
        DisconnectHandler,
        MessageHandler,
        1
    );

    if (!NT_SUCCESS(status))
    {
        DbgPrint("NdfCommInit failed with status: %d\n", status);

        IoDeleteSymbolicLink(&symLink);
        IoDeleteDevice(fdo);

        return status;
    }

    DbgPrint("Driver loaded\n");
    
    return status;
}

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

    return STATUS_SUCCESS;
}

VOID
DisconnectHandler(
    _In_opt_ PVOID ConnectionCookie
)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(ConnectionCookie);
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

    return STATUS_SUCCESS;
}