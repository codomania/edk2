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

#include <Library/MemoryAllocationLib.h>

#include "VirtioNet.h"

//
// The user structure for the ordered collection that will track the mapping
// info of the packets queued in TxRing
//
typedef struct {
  UINT16                DescIdx;
  VOID                  *Buffer;
  EFI_PHYSICAL_ADDRESS  DeviceAddress;
  VOID                  *BufMap;
} TX_BUF_MAP_INFO;

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
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Dev->RxBufMap);
  Dev->VirtIo->FreeSharedPages (
                 Dev->VirtIo,
                 Dev->RxBufNrPages,
                 Dev->RxBuf
                 );
}


VOID
EFIAPI
VirtioNetShutdownTx (
  IN OUT VNET_DEV *Dev
  )
{
  ORDERED_COLLECTION_ENTRY *Entry, *Entry2;
  TX_BUF_MAP_INFO          *TxBufMapInfo;

  for (Entry = OrderedCollectionMin (Dev->TxBufMapInfoCollection);
       Entry != NULL;
       Entry = Entry2) {
    Entry2 = OrderedCollectionNext (Entry);
    TxBufMapInfo = (TX_BUF_MAP_INFO *)Entry2;
    Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, TxBufMapInfo->BufMap);
    FreePool (TxBufMapInfo);
    OrderedCollectionDelete (Dev->TxBufMapInfoCollection, Entry, NULL);
  }
  OrderedCollectionUninit (Dev->TxBufMapInfoCollection);

  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Dev->TxSharedReqMap);
  Dev->VirtIo->FreeSharedPages (
                 Dev->VirtIo,
                 EFI_SIZE_TO_PAGES (sizeof *(Dev->TxSharedReq)),
                 (VOID *) Dev->TxSharedReq
                 );

  FreePool (Dev->TxFreeStack);
}

/**
  Release TX and RX VRING resources.

  @param[in,out] Dev       The VNET_DEV driver instance which was using
                           the ring.
  @param[in,out] Ring      The virtio ring to clean up.
  @param[in]     RingMap   A token return from the VirtioRingMap()
*/
VOID
EFIAPI
VirtioNetUninitRing (
  IN OUT VNET_DEV *Dev,
  IN OUT VRING    *Ring,
  IN     VOID     *RingMap
  )
{
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, RingMap);
  VirtioRingUninit (Dev->VirtIo, Ring);
}


/**
  Map Caller-supplied TxBuf buffer to the device-mapped address

  @param[in]    Dev               The VNET_DEV driver instance which wants to
                                  map the Tx packet.
  @param[in]    DescIdx           VRING descriptor index which will point to
                                  the device address
  @param[in]    Buffer            The system physical address of TxBuf
  @param[in]    NumberOfBytes     Number of bytes to map
  @param[out]   DeviceAddress     The resulting device address for the bus
                                  master access.

  @retval EFI_OUT_OF_RESOURCES    The request could not be completed due to
                                  a lack of resources.
  @retval EFI_INVALID_PARAMETER   The VRING descriptor index is already mapped.
*/
EFI_STATUS
EFIAPI
VirtioNetMapTxBuf (
  IN  VNET_DEV              *Dev,
  IN  UINT16                DescIdx,
  IN  VOID                  *Buffer,
  IN  UINTN                 NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS  *DeviceAddress
  )
{
  EFI_STATUS                Status;
  TX_BUF_MAP_INFO           *TxBufMapInfo;
  EFI_PHYSICAL_ADDRESS      Address;
  VOID                      *Mapping;
  ORDERED_COLLECTION_ENTRY  *Entry;

  TxBufMapInfo = AllocatePool (sizeof (*TxBufMapInfo));
  if (TxBufMapInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = VirtioMapAllBytesInSharedBuffer (
             Dev->VirtIo,
             VirtioOperationBusMasterRead,
             Buffer,
             NumberOfBytes,
             &Address,
             &Mapping
            );
  if (EFI_ERROR (Status)) {
    goto FreeTxBufMapInfo;
  }

  TxBufMapInfo->DescIdx = DescIdx;
  TxBufMapInfo->Buffer = Buffer;
  TxBufMapInfo->DeviceAddress = Address;
  TxBufMapInfo->BufMap = Mapping;

  Status = OrderedCollectionInsert (
             Dev->TxBufMapInfoCollection,
             &Entry,
             TxBufMapInfo
             );
  switch (Status) {
  case RETURN_OUT_OF_RESOURCES:
    Status = EFI_OUT_OF_RESOURCES;
    goto UnmapTxBufBuffer;
  case RETURN_ALREADY_STARTED:
    Status = EFI_INVALID_PARAMETER;
    goto UnmapTxBufBuffer;
  default:
    ASSERT (Status == RETURN_SUCCESS);
    break;
  }

  ASSERT (OrderedCollectionUserStruct (Entry) == TxBufMapInfo);

  *DeviceAddress = Address;

  return EFI_SUCCESS;

UnmapTxBufBuffer:
  Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, Mapping);

FreeTxBufMapInfo:
  FreePool (TxBufMapInfo);
  return Status;
}

/**
  Unmap (aka reverse mapping) device mapped TxBuf buffer to the system
  physical address

  @param[in]    Dev               The VNET_DEV driver instance which wants to
                                  map the Tx packet.
  @param[in]    DescIdx           VRING descriptor index which point to
                                  the device address
  @param[out]   Buffer            The system physical address of TxBuf
  @param[out]   DeviceAddress     The device address for the TxBuf

  @retval EFI_INVALID_PARAMETER   The VRING descriptor index is not mapped
*/
EFI_STATUS
EFIAPI
VirtioNetUnmapTxBuf (
  IN  VNET_DEV              *Dev,
  IN  UINT16                DescIdx,
  OUT VOID                  **Buffer,
  IN  EFI_PHYSICAL_ADDRESS  DeviceAddress
  )
{
  TX_BUF_MAP_INFO           StandaloneKey;
  ORDERED_COLLECTION_ENTRY  *Entry;
  TX_BUF_MAP_INFO           *UserStruct;
  VOID                      *Ptr;
  EFI_STATUS                Status;

  StandaloneKey.DescIdx = DescIdx;
  Entry = OrderedCollectionFind (Dev->TxBufMapInfoCollection, &StandaloneKey);
  if (Entry == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  OrderedCollectionDelete (Dev->TxBufMapInfoCollection, Entry, &Ptr);

  UserStruct = Ptr;
  ASSERT (UserStruct->DescIdx == DescIdx);

  *Buffer =  UserStruct->Buffer;
  Status = Dev->VirtIo->UnmapSharedBuffer (Dev->VirtIo, UserStruct->BufMap);
  FreePool (UserStruct);

  return Status;
}

/**
  Comparator function for two user structures.

  @param[in] UserStruct1  Pointer to the first user structure.

  @param[in] UserStruct2  Pointer to the second user structure.

  @retval <0  If UserStruct1 compares less than UserStruct2.

  @retval  0  If UserStruct1 compares equal to UserStruct2.

  @retval >0  If UserStruct1 compares greater than UserStruct2.
*/
INTN
EFIAPI
VirtioNetTxMapInfoCompare (
  IN CONST VOID *UserStruct1,
  IN CONST VOID *UserStruct2
  )
{
  CONST TX_BUF_MAP_INFO *MapInfo1;
  CONST TX_BUF_MAP_INFO *MapInfo2;

  MapInfo1 = UserStruct1;
  MapInfo2 = UserStruct2;

  return MapInfo1->DescIdx < MapInfo2->DescIdx ? -1 :
         MapInfo1->DescIdx > MapInfo2->DescIdx ?  1 :
         0;
}
