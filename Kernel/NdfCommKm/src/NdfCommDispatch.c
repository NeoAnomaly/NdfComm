#include "NdfCommDispatch.h"
#include "NdfCommGlobalData.h"
#include "NdfCommShared.h"
#include "NdfCommUtils.h"
#include "NdfCommClient.h"
#include "NdfCommMessaging.h"
#include "NdfCommDebug.h"

_Check_return_
NTSTATUS
NdfCommpDispatchCreate(
    _Inout_ PFILE_OBJECT FileObject,
    _In_ PIRP Irp
);

VOID
NdfCommpDispatchCleanup(
	_In_ PFILE_OBJECT FileObject
);

VOID
NdfCommpDispatchClose(
	_Inout_ PFILE_OBJECT FileObject
);

_Check_return_
NTSTATUS
NdfCommpDispatchControl(
	_In_ PIO_STACK_LOCATION StackLocation,
	_In_ PIRP Irp,
	_Out_ PULONG ReturnOutputBufferSize
);

#ifdef ALLOC_PRAGMA
#   pragma alloc_text(INIT, NdfCommInit)
#   pragma alloc_text(PAGE, NdfCommRelease)
#   pragma alloc_text(PAGE, NdfCommDispatch)
#   pragma alloc_text(PAGE, NdfCommpDispatchCreate)
#   pragma alloc_text(PAGE, NdfCommpDispatchCleanup)
#   pragma alloc_text(PAGE, NdfCommpDispatchClose)
#   pragma alloc_text(PAGE, NdfCommpDispatchControl)
#endif // ALLOC_PRAGMA

_Function_class_(DRIVER_DISPATCH)
NTSTATUS
NdfCommDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
)
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG returnBufferSize = 0;


    if (DeviceObject != NdfCommGlobals.MessageDeviceObject)
    {
        switch (irpSp->MajorFunction)
        {

        case IRP_MJ_CREATE:
            return NdfCommGlobals.OriginalCreateDispatch(DeviceObject, Irp);

        case IRP_MJ_CLEANUP:
            return NdfCommGlobals.OriginalCleanupDispatch(DeviceObject, Irp);

        case IRP_MJ_CLOSE:
            return NdfCommGlobals.OriginalCloseDispatch(DeviceObject, Irp);

        case IRP_MJ_DEVICE_CONTROL:
            return NdfCommGlobals.OriginalControlDispatch(DeviceObject, Irp);

        default:
            ASSERTMSG("Not implemented hook", FALSE);

            return NdfCommCompleteIrp(Irp, STATUS_INVALID_DEVICE_STATE, 0);
        }
    }

    switch (irpSp->MajorFunction)
    {
    case IRP_MJ_CREATE:

        status = NdfCommpDispatchCreate(irpSp->FileObject, Irp);
        break;

    case IRP_MJ_CLEANUP:

        NdfCommpDispatchCleanup(irpSp->FileObject);
        break;

    case IRP_MJ_CLOSE:

        NdfCommpDispatchClose(irpSp->FileObject);
        break;

    case IRP_MJ_DEVICE_CONTROL:

        status = NdfCommpDispatchControl(
            irpSp,
            Irp,
            &returnBufferSize
        );
        break;

    default:
        ASSERTMSG("Not implemented hook", FALSE);

        status = STATUS_INVALID_DEVICE_STATE;
    }

    if (status != STATUS_PENDING)
    {
        NdfCommCompleteIrp(Irp, status, (ULONG_PTR)returnBufferSize);
    }

    return status;
}

_Check_return_
NTSTATUS
NdfCommpDispatchCreate(
	_Inout_ PFILE_OBJECT FileObject,
	_In_ PIRP Irp
)
{
    PAGED_CODE();

    NTSTATUS status = STATUS_SUCCESS;
    PNDFCOMM_CLIENT client = NULL;
    PVOID clientContext = NULL;
    ULONG clientContextSize = 0;
    PFILE_FULL_EA_INFORMATION eaInfo = Irp->AssociatedIrp.SystemBuffer;

    if (!eaInfo)
    {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (eaInfo->EaNameLength != NDFCOMM_EA_NAME_NOT_NULL_LENGTH)
    {
        return STATUS_INVALID_EA_NAME;
    }

    if (
        RtlCompareMemory(
            eaInfo->EaName,
            NDFCOMM_EA_NAME,
            NDFCOMM_EA_NAME_NOT_NULL_LENGTH) != NDFCOMM_EA_NAME_NOT_NULL_LENGTH
        )
    {
        return STATUS_INVALID_EA_NAME;
    }

    clientContext = NdfCommAdd2Ptr(eaInfo->EaName, NDFCOMM_EA_NAME_WITH_NULL_LENGTH);
    clientContextSize = eaInfo->EaValueLength;

    ///
    /// Check that library is not in tearing down state
    ///
    if (ExAcquireRundownProtection(&NdfCommGlobals.LibraryRundownRef))
    {
        if (InterlockedIncrement(&NdfCommGlobals.ActiveClientsCount) <= (LONG)NdfCommGlobals.MaxClientsCount)
        {
            status = NdfCommCreateClient(&client);

            if (NT_SUCCESS(status))
            {
                status = NdfCommGlobals.ConnectNotifyCallback(
                    client,
                    clientContext,
                    clientContextSize,
                    &client->ConnectionCookie
                );

                if (NT_SUCCESS(status))
                {
                    FileObject->FsContext = client;

                    NdfCommConcurentListInterlockedAdd(&NdfCommGlobals.ClientList, &client->ListEntry);
                }
                else
                {
                    LONG clientsCount = 0;

                    NdfCommFreeClient(client);
                    client = NULL;

                    clientsCount = InterlockedDecrement(&NdfCommGlobals.ActiveClientsCount);

                    ASSERT(clientsCount >= 0);
                }

            }
        }
        else
        {
            status = STATUS_CONNECTION_COUNT_LIMIT;
        }

        ExReleaseRundownProtection(&NdfCommGlobals.LibraryRundownRef);
    }
    else
    {
        status = STATUS_CONNECTION_REFUSED;
    }

    return status;
}

VOID
NdfCommpDispatchCleanup(
	_In_ PFILE_OBJECT FileObject
)
{
    PAGED_CODE();

    PNDFCOMM_CLIENT client = NULL;
    LONG clientsCount = 0;

    client = FileObject->FsContext;

    ASSERT(client);

    NdfCommReleaseClientWaiters(client);

    clientsCount = InterlockedDecrement(&NdfCommGlobals.ActiveClientsCount);

    ASSERT(clientsCount >= 0);

    ///
    /// Stops message delivering
    ///
    ExWaitForRundownProtectionRelease(&client->MsgNotificationRundownRef);

    NdfCommGlobals.DisconnectNotifyCallback(client->ConnectionCookie);

    ExRundownCompleted(&client->MsgNotificationRundownRef);
}

VOID
NdfCommpDispatchClose(
	_Inout_ PFILE_OBJECT FileObject
)
{
    PAGED_CODE();

    PNDFCOMM_CLIENT client = FileObject->FsContext;

	ASSERT(client);

	FileObject->FsContext = NULL;

    NdfCommConcurentListInterlockedRemove(&NdfCommGlobals.ClientList, &client->ListEntry);

    NdfCommFreeClient(client);
}

_Check_return_
NTSTATUS
NdfCommpDispatchControl(
	_In_ PIO_STACK_LOCATION IrpSp,
	_In_ PIRP Irp,
	_Out_ PULONG ReturnOutputBufferSize
)
{
    PAGED_CODE();
	
    NTSTATUS status = STATUS_SUCCESS;
    PVOID inputBuffer = IrpSp->Parameters.DeviceIoControl.Type3InputBuffer;
    ULONG inputBufferSize = IrpSp->Parameters.DeviceIoControl.InputBufferLength;
    PVOID outputBuffer = Irp->UserBuffer;
    ULONG outputBufferSize = IrpSp->Parameters.DeviceIoControl.OutputBufferLength;
    ULONG controlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
    KPROCESSOR_MODE accessMode = Irp->RequestorMode;

    if (!IrpSp->FileObject->FsContext)
    {
        return STATUS_INVALID_DEVICE_STATE;
    }

    ///
    /// Discard non compatible ctl
    ///
    if (METHOD_FROM_CTL_CODE(controlCode) != METHOD_NEITHER)
    {
        return STATUS_INVALID_PARAMETER;
    }

    switch (controlCode)
    {
    case NDFCOMM_GETMESSAGE:
		status = STATUS_NOT_IMPLEMENTED;
        break;

    case NDFCOMM_SENDMESSAGE:

        status = NdfCommDeliverMessageToKm(
            IrpSp->FileObject,
            inputBuffer,
            inputBufferSize,
            outputBuffer,
            outputBufferSize,
            ReturnOutputBufferSize,
            accessMode
        );

        break;

    case NDFCOMM_REPLYMESSAGE:
		status = STATUS_NOT_IMPLEMENTED;
        break;

    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    return status;
}