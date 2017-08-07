/** @file

  Utility functions used by virtio device drivers.

  Copyright (C) 2012-2016, Red Hat, Inc.
  Portion of Copyright (C) 2013, ARM Ltd.
  Copyright (C) 2017, AMD Inc, All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Library/VirtioLib.h>


/**

  Configure a virtio ring.

  This function sets up internal storage (the guest-host communication area)
  and lays out several "navigation" (ie. no-ownership) pointers to parts of
  that storage.

  Relevant sections from the virtio-0.9.5 spec:
  - 1.1 Virtqueues,
  - 2.3 Virtqueue Configuration.

  @param[in]  VirtIo            The virtio device which will use the ring.

  @param[in]                    The number of descriptors to allocate for the
                                virtio ring, as requested by the host.

  @param[out] Ring              The virtio ring to set up.

  @retval EFI_OUT_OF_RESOURCES  AllocatePages() failed to allocate contiguous
                                pages for the requested QueueSize. Fields of
                                Ring have indeterminate value.

  @retval EFI_SUCCESS           Allocation and setup successful. Ring->Base
                                (and nothing else) is responsible for
                                deallocation.

**/
EFI_STATUS
EFIAPI
VirtioRingInit (
  IN  VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN  UINT16                 QueueSize,
  OUT VRING                  *Ring
  )
{
  UINTN          RingSize;
  volatile UINT8 *RingPagesPtr;

  RingSize = ALIGN_VALUE (
               sizeof *Ring->Desc            * QueueSize +
               sizeof *Ring->Avail.Flags                 +
               sizeof *Ring->Avail.Idx                   +
               sizeof *Ring->Avail.Ring      * QueueSize +
               sizeof *Ring->Avail.UsedEvent,
               EFI_PAGE_SIZE);

  RingSize += ALIGN_VALUE (
                sizeof *Ring->Used.Flags                  +
                sizeof *Ring->Used.Idx                    +
                sizeof *Ring->Used.UsedElem   * QueueSize +
                sizeof *Ring->Used.AvailEvent,
                EFI_PAGE_SIZE);

  Ring->NumPages = EFI_SIZE_TO_PAGES (RingSize);
  Ring->Base = AllocatePages (Ring->NumPages);
  if (Ring->Base == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  SetMem (Ring->Base, RingSize, 0x00);
  RingPagesPtr = Ring->Base;

  Ring->Desc = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Desc * QueueSize;

  Ring->Avail.Flags = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Avail.Flags;

  Ring->Avail.Idx = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Avail.Idx;

  Ring->Avail.Ring = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Avail.Ring * QueueSize;

  Ring->Avail.UsedEvent = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Avail.UsedEvent;

  RingPagesPtr = (volatile UINT8 *) Ring->Base +
                 ALIGN_VALUE (RingPagesPtr - (volatile UINT8 *) Ring->Base,
                   EFI_PAGE_SIZE);

  Ring->Used.Flags = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Used.Flags;

  Ring->Used.Idx = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Used.Idx;

  Ring->Used.UsedElem = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Used.UsedElem * QueueSize;

  Ring->Used.AvailEvent = (volatile VOID *) RingPagesPtr;
  RingPagesPtr += sizeof *Ring->Used.AvailEvent;

  Ring->QueueSize = QueueSize;
  return EFI_SUCCESS;
}


/**

  Tear down the internal resources of a configured virtio ring.

  The caller is responsible to stop the host from using this ring before
  invoking this function: the VSTAT_DRIVER_OK bit must be clear in
  VhdrDeviceStatus.

  @param[in]  VirtIo  The virtio device which will was using the ring.

  @param[out] Ring    The virtio ring to clean up.

**/
VOID
EFIAPI
VirtioRingUninit (
  IN     VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN OUT VRING                  *Ring
  )
{
  FreePages (Ring->Base, Ring->NumPages);
  SetMem (Ring, sizeof *Ring, 0x00);
}


/**

  Turn off interrupt notifications from the host, and prepare for appending
  multiple descriptors to the virtio ring.

  The calling driver must be in VSTAT_DRIVER_OK state.

  @param[in,out] Ring  The virtio ring we intend to append descriptors to.

  @param[out] Indices  The DESC_INDICES structure to initialize.

**/
VOID
EFIAPI
VirtioPrepare (
  IN OUT VRING        *Ring,
  OUT    DESC_INDICES *Indices
  )
{
  //
  // Prepare for virtio-0.9.5, 2.4.2 Receiving Used Buffers From the Device.
  // We're going to poll the answer, the host should not send an interrupt.
  //
  *Ring->Avail.Flags = (UINT16) VRING_AVAIL_F_NO_INTERRUPT;

  //
  // Prepare for virtio-0.9.5, 2.4.1 Supplying Buffers to the Device.
  //
  // Since we support only one in-flight descriptor chain, we can always build
  // that chain starting at entry #0 of the descriptor table.
  //
  Indices->HeadDescIdx = 0;
  Indices->NextDescIdx = Indices->HeadDescIdx;
}


/**

  Append a contiguous buffer for transmission / reception via the virtio ring.

  This function implements the following section from virtio-0.9.5:
  - 2.4.1.1 Placing Buffers into the Descriptor Table

  Free space is taken as granted, since the individual drivers support only
  synchronous requests and host side status is processed in lock-step with
  request submission. It is the calling driver's responsibility to verify the
  ring size in advance.

  The caller is responsible for initializing *Indices with VirtioPrepare()
  first.

  @param[in,out] Ring        The virtio ring to append the buffer to, as a
                             descriptor.

  @param[in] BufferPhysAddr  (Guest pseudo-physical) start address of the
                             transmit / receive buffer.

  @param[in] BufferSize      Number of bytes to transmit or receive.

  @param[in] Flags           A bitmask of VRING_DESC_F_* flags. The caller
                             computes this mask dependent on further buffers to
                             append and transfer direction.
                             VRING_DESC_F_INDIRECT is unsupported. The
                             VRING_DESC.Next field is always set, but the host
                             only interprets it dependent on VRING_DESC_F_NEXT.

  @param[in,out] Indices     Indices->HeadDescIdx is not accessed.
                             On input, Indices->NextDescIdx identifies the next
                             descriptor to carry the buffer. On output,
                             Indices->NextDescIdx is incremented by one, modulo
                             2^16.

**/
VOID
EFIAPI
VirtioAppendDesc (
  IN OUT VRING        *Ring,
  IN     UINTN        BufferPhysAddr,
  IN     UINT32       BufferSize,
  IN     UINT16       Flags,
  IN OUT DESC_INDICES *Indices
  )
{
  volatile VRING_DESC *Desc;

  Desc        = &Ring->Desc[Indices->NextDescIdx++ % Ring->QueueSize];
  Desc->Addr  = BufferPhysAddr;
  Desc->Len   = BufferSize;
  Desc->Flags = Flags;
  Desc->Next  = Indices->NextDescIdx % Ring->QueueSize;
}


/**

  Notify the host about the descriptor chain just built, and wait until the
  host processes it.

  @param[in] VirtIo       The target virtio device to notify.

  @param[in] VirtQueueId  Identifies the queue for the target device.

  @param[in,out] Ring     The virtio ring with descriptors to submit.

  @param[in] Indices      Indices->NextDescIdx is not accessed.
                          Indices->HeadDescIdx identifies the head descriptor
                          of the descriptor chain.

  @param[out] UsedLen     On success, the total number of bytes, consecutively
                          across the buffers linked by the descriptor chain,
                          that the host wrote. May be NULL if the caller
                          doesn't care, or can compute the same information
                          from device-specific request structures linked by the
                          descriptor chain.

  @return              Error code from VirtIo->SetQueueNotify() if it fails.

  @retval EFI_SUCCESS  Otherwise, the host processed all descriptors.

**/
EFI_STATUS
EFIAPI
VirtioFlush (
  IN     VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN     UINT16                 VirtQueueId,
  IN OUT VRING                  *Ring,
  IN     DESC_INDICES           *Indices,
  OUT    UINT32                 *UsedLen    OPTIONAL
  )
{
  UINT16     NextAvailIdx;
  UINT16     LastUsedIdx;
  EFI_STATUS Status;
  UINTN      PollPeriodUsecs;

  //
  // virtio-0.9.5, 2.4.1.2 Updating the Available Ring
  //
  // It is not exactly clear from the wording of the virtio-0.9.5
  // specification, but each entry in the Available Ring references only the
  // head descriptor of any given descriptor chain.
  //
  NextAvailIdx = *Ring->Avail.Idx;
  //
  // (Due to our lock-step progress, this is where the host will produce the
  // used element with the head descriptor's index in it.)
  //
  LastUsedIdx = NextAvailIdx;
  Ring->Avail.Ring[NextAvailIdx++ % Ring->QueueSize] =
    Indices->HeadDescIdx % Ring->QueueSize;

  //
  // virtio-0.9.5, 2.4.1.3 Updating the Index Field
  //
  MemoryFence();
  *Ring->Avail.Idx = NextAvailIdx;

  //
  // virtio-0.9.5, 2.4.1.4 Notifying the Device -- gratuitous notifications are
  // OK.
  //
  MemoryFence();
  Status = VirtIo->SetQueueNotify (VirtIo, VirtQueueId);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // virtio-0.9.5, 2.4.2 Receiving Used Buffers From the Device
  // Wait until the host processes and acknowledges our descriptor chain. The
  // condition we use for polling is greatly simplified and relies on the
  // synchronous, lock-step progress.
  //
  // Keep slowing down until we reach a poll period of slightly above 1 ms.
  //
  PollPeriodUsecs = 1;
  MemoryFence();
  while (*Ring->Used.Idx != NextAvailIdx) {
    gBS->Stall (PollPeriodUsecs); // calls AcpiTimerLib::MicroSecondDelay

    if (PollPeriodUsecs < 1024) {
      PollPeriodUsecs *= 2;
    }
    MemoryFence();
  }

  MemoryFence();

  if (UsedLen != NULL) {
    volatile CONST VRING_USED_ELEM *UsedElem;

    UsedElem = &Ring->Used.UsedElem[LastUsedIdx % Ring->QueueSize];
    ASSERT (UsedElem->Id == Indices->HeadDescIdx);
    *UsedLen = UsedElem->Len;
  }

  return EFI_SUCCESS;
}


/**

  Report the feature bits to the VirtIo 1.0 device that the VirtIo 1.0 driver
  understands.

  In VirtIo 1.0, a device can reject a self-inconsistent feature bitmap through
  the new VSTAT_FEATURES_OK status bit. (For example if the driver requests a
  higher level feature but clears a prerequisite feature.) This function is a
  small wrapper around VIRTIO_DEVICE_PROTOCOL.SetGuestFeatures() that also
  verifies if the VirtIo 1.0 device accepts the feature bitmap.

  @param[in]     VirtIo        Report feature bits to this device.

  @param[in]     Features      The set of feature bits that the driver wishes
                               to report. The caller is responsible to perform
                               any masking before calling this function; the
                               value is directly written with
                               VIRTIO_DEVICE_PROTOCOL.SetGuestFeatures().

  @param[in,out] DeviceStatus  On input, the status byte most recently written
                               to the device's status register. On output (even
                               on error), DeviceStatus will be updated so that
                               it is suitable for further status bit
                               manipulation and writing to the device's status
                               register.

  @retval  EFI_SUCCESS      The device accepted the configuration in Features.

  @return  EFI_UNSUPPORTED  The device rejected the configuration in Features.

  @retval  EFI_UNSUPPORTED  VirtIo->Revision is smaller than 1.0.0.

  @return                   Error codes from the SetGuestFeatures(),
                            SetDeviceStatus(), GetDeviceStatus() member
                            functions.

**/
EFI_STATUS
EFIAPI
Virtio10WriteFeatures (
  IN     VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN     UINT64                 Features,
  IN OUT UINT8                  *DeviceStatus
  )
{
  EFI_STATUS Status;

  if (VirtIo->Revision < VIRTIO_SPEC_REVISION (1, 0, 0)) {
    return EFI_UNSUPPORTED;
  }

  Status = VirtIo->SetGuestFeatures (VirtIo, Features);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *DeviceStatus |= VSTAT_FEATURES_OK;
  Status = VirtIo->SetDeviceStatus (VirtIo, *DeviceStatus);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = VirtIo->GetDeviceStatus (VirtIo, DeviceStatus);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((*DeviceStatus & VSTAT_FEATURES_OK) == 0) {
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

/**
  Helper function to allocate pages that is suitable for sharing with
  hypervisor.

  @param[in]  VirtIo  The target virtio device to use. It must be valid.

  @param[in]  Pages   The number of pages to allocate.

  @param[out] Buffer  A pointer to store the base system memory address of
                      the allocated range.

  return              Returns error code from VirtIo->AllocateSharedPages()
**/
EFI_STATUS
EFIAPI
VirtioAllocateSharedPages (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  UINTN                   NumPages,
  OUT VOID                    **Buffer
  )
{
  return VirtIo->AllocateSharedPages (VirtIo, NumPages, Buffer);
}

/**
  Helper function to free pages allocated using VirtioAllocateSharedPages().

  @param[in]  VirtIo  The target virtio device to use. It must be valid.

  @param[in]  Pages   The number of allocated pages.

  @param[in]  Buffer  System memory address allocated from
                      VirtioAllocateSharedPages ().
**/
VOID
EFIAPI
VirtioFreeSharedPages (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  UINTN                   NumPages,
  IN  VOID                    *Buffer
  )
{
  VirtIo->FreeSharedPages (VirtIo, NumPages, Buffer);
}

STATIC
EFI_STATUS
VirtioMapSharedBuffer (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  VIRTIO_MAP_OPERATION    Operation,
  IN  VOID                    *HostAddress,
  IN  UINTN                   NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS    *DeviceAddress,
  OUT VOID                    **Mapping
  )
{
  EFI_STATUS            Status;
  VOID                  *MapInfo;
  UINTN                 Size;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;

  Size = NumberOfBytes;
  Status = VirtIo->MapSharedBuffer (
                     VirtIo,
                     Operation,
                     HostAddress,
                     &Size,
                     &PhysicalAddress,
                     &MapInfo
                     );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Size < NumberOfBytes) {
    goto Failed;
  }

  *Mapping = MapInfo;
  *DeviceAddress = PhysicalAddress;

  return EFI_SUCCESS;
Failed:
  VirtIo->UnmapSharedBuffer (VirtIo, MapInfo);
  return EFI_OUT_OF_RESOURCES;
}

/**
  A helper function to map a system memory to a shared bus master memory for
  read operation from DMA bus master.

  @param[in]  VirtIo          The target virtio device to use. It must be
                              valid.

  @param[in]  HostAddress     The system memory address to map to shared bus
                              master address.

  @param[in]  NumberOfBytes   Number of bytes to be mapped.

  @param[out] DeviceAddress   The resulting shared map address for the bus
                              master to access the hosts HostAddress.

  @param[out] Mapping         A resulting value to pass to Unmap().

  return                      Returns error code from
                              VirtIo->MapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioMapSharedBufferRead (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  VOID                    *HostAddress,
  IN  UINTN                   NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS    *DeviceAddress,
  OUT VOID                    **Mapping
  )
{
  return VirtioMapSharedBuffer (VirtIo, EfiVirtIoOperationBusMasterRead,
           HostAddress, NumberOfBytes, DeviceAddress, Mapping);
}

/**
  A helper function to map a system memory to a shared bus master memory for
  write operation from DMA bus master.

  @param[in]  VirtIo          The target virtio device to use. It must be
                              valid.

  @param[in]  HostAddress     The system memory address to map to shared bus
                              master address.

  @param[in]  NumberOfBytes   Number of bytes to be mapped.

  @param[out] DeviceAddress   The resulting shared map address for the bus
                              master to access the hosts HostAddress.

  @param[out] Mapping         A resulting value to pass to Unmap().

  return                      Returns error code from
                              VirtIo->MapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioMapSharedBufferWrite (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  VOID                    *HostAddress,
  IN  UINTN                   NumberOfBytes,
  OUT EFI_PHYSICAL_ADDRESS    *DeviceAddress,
  OUT VOID                    **Mapping
  )
{
  return VirtioMapSharedBuffer (VirtIo, EfiVirtIoOperationBusMasterWrite,
           HostAddress, NumberOfBytes, DeviceAddress, Mapping);
}

/**
  A helper function to map a system memory to a shared bus master memory for
  common operation from DMA bus master.

  @param[in]  VirtIo          The target virtio device to use. It must be
                              valid.

  @param[in]  HostAddress     The system memory address to map to shared bus
                              master address.

  @param[in]  NumberOfBytes   Number of bytes to be mapped.

  @param[out] Mapping         A resulting value to pass to Unmap().

  return                      Returns error code from
                              VirtIo->MapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioMapSharedBufferCommon (
  IN  VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN  VOID                    *HostAddress,
  IN  UINTN                   NumberOfBytes,
  OUT VOID                    **Mapping
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  DeviceAddress;

  Status = VirtioMapSharedBuffer (VirtIo,
             EfiVirtIoOperationBusMasterCommonBuffer, HostAddress,
             NumberOfBytes, &DeviceAddress, Mapping);

  //
  // On Success, lets verify that DeviceAddress same as HostAddress
  //
  if (!EFI_ERROR (Status)) {
    ASSERT (DeviceAddress == (EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress);
  }

  return Status;
}

/**
  A helper function to unmap shared bus master memory mapped using Map().

  @param[in]  VirtIo          The target virtio device to use. It must be
                              valid.

  @param[in] Mapping          A mapping value return from Map().

  return                      Returns error code from
                              VirtIo->UnmapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioUnmapSharedBuffer (
  IN VIRTIO_DEVICE_PROTOCOL  *VirtIo,
  IN VOID                    *Mapping
  )
{
  return VirtIo->UnmapSharedBuffer (VirtIo, Mapping);
}

/**

  Map the ring buffer so that it can be accessed equally by both guest
  and hypervisor.

  @param[in]      VirtIo    The virtio device instance.

  @param[in]      Ring      The virtio ring to map.

  @param[out]     Mapping   A resulting value to pass to Unmap().

  @retval         Value returned from VirtIo->MapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioRingMap (
  IN  VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN  VRING                  *Ring,
  OUT VOID                   **Mapping
  )
{
  UINTN                 NumberOfBytes;

  NumberOfBytes = Ring->NumPages * EFI_PAGE_SIZE;

  return VirtioMapSharedBufferCommon (VirtIo, Ring->Base,
           NumberOfBytes, Mapping);
}

/**

  Unmap the ring buffer mapped using VirtioRingMap()

  @param[in]      VirtIo    The virtio device instance.

  @param[in]      Ring      The virtio ring to unmap.

  @param[in]      Mapping   A value obtained through Map().

  @retval         Value returned from VirtIo->UnmapSharedBuffer()
**/
EFI_STATUS
EFIAPI
VirtioRingUnmap (
  IN  VIRTIO_DEVICE_PROTOCOL *VirtIo,
  IN  VRING                  *Ring,
  IN  VOID                   *Mapping
  )
{
  return VirtioUnmapSharedBuffer (VirtIo, Mapping);
}
