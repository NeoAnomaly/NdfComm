/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that app can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_WdfDrv,
    0x1d7f7b82,0x2047,0x46c4,0x88,0x38,0xd8,0x0d,0x9e,0xf3,0xca,0xc4);
// {1d7f7b82-2047-46c4-8838-d80d9ef3cac4}
