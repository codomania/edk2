/** @file

  Implements
  - the SNM.WaitForPacket EVT_NOTIFY_WAIT event,
  - the EVT_SIGNAL_EXIT_BOOT_SERVICES event
  for the virtio-net driver.

  Copyright (C) 2013, Red Hat, Inc.
  Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "VirtioNet.h"

/**
  Invoke a notification event

  @param  Event                 Event whose notification function is being
                                invoked.
  @param  Context               The pointer to the notification function's
                                context, which is implementation-dependent.

**/

VOID
EFIAPI
VirtioNetIsPacketAvailable (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
{
  //
  // This callback has been enqueued by an external application and is
  // running at TPL_CALLBACK already.
  //
  // The WaitForPacket logic is similar to that of WaitForKey. The former has
  // almost no documentation in either the UEFI-2.3.1+errC spec or the
  // DWG-2.3.1, but WaitForKey does have some.
  //
  VNET_DEV *Dev;
  UINT16   RxCurUsed;

  Dev = Context;
  if (Dev->Snm.State != EfiSimpleNetworkInitialized) {
    return;
  }

  //
  // virtio-0.9.5, 2.4.2 Receiving Used Buffers From the Device
  //
  MemoryFence ();
  RxCurUsed = *Dev->RxRing.Used.Idx;
  MemoryFence ();

  if (Dev->RxLastUsed != RxCurUsed) {
    gBS->SignalEvent (&Dev->Snp.WaitForPacket);
  }
}

VOID
EFIAPI
VirtioNetExitBoot (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
{
  //
  // This callback has been enqueued by ExitBootServices() and is running at
  // TPL_CALLBACK already.
  //
  // Shut down pending transfers according to DWG-2.3.1, "25.5.1 Exit Boot
  // Services Event".
  //
  VNET_DEV *Dev;

  Dev = Context;
  if (Dev->Snm.State == EfiSimpleNetworkInitialized) {
    Dev->VirtIo->SetDeviceStatus (Dev->VirtIo, 0);
  }

  //
  // Unmap Tx and Rx rings so that hypervisor can not get readable data
  // after device is reset.
  //
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Dev->TxRingMap);
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Dev->RxRingMap);

  //
  // Unmap Rx buffer so that hypervisor can not get readable data after
  // device is reset.
  //
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Dev->RxBufMap);
}
