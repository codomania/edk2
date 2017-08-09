/** @file

  Helper functions used by at least two Simple Network Protocol methods.

  Copyright (C) 2013, Red Hat, Inc.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>

#include "VirtioNet.h"

#define PRIVATE_TXBUF_SIGNATURE  SIGNATURE_32 ('t', 'x', 'b', 'f')
typedef struct {
  UINT32                Signature;
  LIST_ENTRY            Link;
  EFI_PHYSICAL_ADDRESS  HostAddress;
  EFI_PHYSICAL_ADDRESS  DeviceAddress;
  VOID                  *Mapping;
} PRIVATE_TXBUF_ENTRY;
#define PRIVATE_TXBUF_FROM_LINK(a) CR (a, PRIVATE_TXBUF_ENTRY, Link, \
                                       PRIVATE_TXBUF_SIGNATURE)

//
// List of Txbuf queued
//
STATIC LIST_ENTRY mTxBufMapList = INITIALIZE_LIST_HEAD_VARIABLE (
                                    mTxBufMapList
                                    );

/**
  Release RX and TX resources on the boundary of the
  EfiSimpleNetworkInitialized state.

  These functions contribute to rolling back a partial, failed initialization
  of the virtio-net SNP driver instance, or to shutting down a fully
  initialized, running instance.

  They are only callable by the VirtioNetInitialize() and the
  VirtioNetShutdown() SNP methods. See the state diagram in "VirtioNet.h".

  @param[in,out] Dev  The VNET_DEV driver instance being shut down, or whose
                      partial, failed initialization is being rolled back.
*/

VOID
EFIAPI
VirtioNetShutdownRx (
  IN OUT VNET_DEV *Dev
  )
{
  Dev->VirtIo->FreeSharedPages (
                 Dev->VirtIo,
                 Dev->RxBufNoPages,
                 (VOID *) Dev->RxBuf
                 );
}


VOID
EFIAPI
VirtioNetShutdownTx (
  IN OUT VNET_DEV *Dev
  )
{
  Dev->VirtIo->FreeSharedPages (
                 Dev->VirtIo,
                 EFI_SIZE_TO_PAGES (sizeof *(Dev->TxSharedReq)),
                 (VOID *) Dev->TxSharedReq
                 );

  FreePool (Dev->TxFreeStack);
}

EFI_STATUS
EFIAPI
VirtioMapTxBuf (
  IN  VNET_DEV               *Dev,
  IN  EFI_PHYSICAL_ADDRESS   HostAddress,
  IN  UINTN                  NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS   *DeviceAddress
  )
{
  EFI_STATUS             Status;
  PRIVATE_TXBUF_ENTRY   *Private;

  Private = AllocatePool (sizeof (*Private));
  if (Private == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Private->Signature = PRIVATE_TXBUF_SIGNATURE;
  Private->HostAddress = HostAddress;

  Status = VirtioMapAllBytesInSharedBuffer (
             Dev->VirtIo,
             VirtioOperationBusMasterRead,
             (VOID *) (UINTN) Private->HostAddress,
             NumberOfBytes,
             &Private->DeviceAddress,
             &Private->Mapping
             );
  if (EFI_ERROR (Status)) {
    goto FreePool;
  }

  *DeviceAddress = Private->DeviceAddress;

  //
  // Add the mapping information into internal list so that we can retrieve
  // the HostAddress from Unmap().
  //
  InsertTailList (&mTxBufMapList, &Private->Link);

  return EFI_SUCCESS;

FreePool:
  FreePool (Private);
  return Status;
}

EFI_STATUS
EFIAPI
VirtioUnmapTxBuf (
  IN  VNET_DEV               *Dev,
  OUT EFI_PHYSICAL_ADDRESS   *HostAddress,
  IN  EFI_PHYSICAL_ADDRESS   DeviceAddress
  )
{
  EFI_STATUS             Status;
  PRIVATE_TXBUF_ENTRY   *Private;
  LIST_ENTRY            *Link;
  BOOLEAN                Found;

  Found = FALSE;

  //
  // Iterate through internal txbuf list to find mapping for a given
  // DeviceAddress.
  //
  for (Link = GetFirstNode (&mTxBufMapList)
       ; !IsNull (&mTxBufMapList, Link)
       ; Link = GetNextNode (&mTxBufMapList, Link)
      ) {
    Private = PRIVATE_TXBUF_FROM_LINK (Link);
    if (Private->DeviceAddress == DeviceAddress) {
      Found = TRUE;
      break;
    }
  }

  //
  // We failed to find mapping for the given DeviceAddress
  // (this should never happen)
  //
  ASSERT (Found);

  Status = Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Private->Mapping);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *HostAddress = Private->HostAddress;

  RemoveEntryList (&Private->Link);
  FreePool (Private);

  return EFI_SUCCESS;
}
