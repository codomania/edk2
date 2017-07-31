/** @file

  The protocol provides support to allocate, free, map and umap a DMA buffer for
  bus master (e.g PciHostBridge). When SEV is enabled, the DMA operations must
  be performed on unencrypted buffer hence we use a bounce buffer to map the guest
  buffer into an unencrypted DMA buffer.

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>
  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AmdSevIoMmu.h"

typedef struct {
  EDKII_IOMMU_OPERATION                     Operation;
  UINTN                                     NumberOfBytes;
  UINTN                                     NumberOfPages;
  EFI_PHYSICAL_ADDRESS                      HostAddress;
  EFI_PHYSICAL_ADDRESS                      DeviceAddress;
} MAP_INFO;

/**

  The function is used for mapping and unmapping the Host buffer with
  BusMasterCommonBuffer. Since the buffer can be accessed equally by the
  processor and the DMA bus master hence we can not use the bounce buffer.

  The function changes the underlying encryption mask of the pages that maps the
  host buffer. It also ensures that buffer contents are updated with the desired
  state.

**/
STATIC
EFI_STATUS
SetBufferAsEncDec (
  IN MAP_INFO *MapInfo,
  IN BOOLEAN  Enc
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  TempBuffer;

  //
  // Allocate an intermediate buffer to hold the host buffer contents
  //
  Status = gBS->AllocatePages (
                  AllocateAnyPages,
                  EfiBootServicesData,
                  MapInfo->NumberOfPages,
                  &TempBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // If the host buffer has C-bit cleared, then make sure the intermediate
  // buffer matches with same encryption mask.
  //
  if (!Enc) {
    Status = MemEncryptSevClearPageEncMask (0, TempBuffer, MapInfo->NumberOfPages, TRUE);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Copy the data from host buffer into a temporary buffer. At this
  // time both host and intermediate buffer will have same encryption
  // mask.
  //
  CopyMem (
    (VOID *) (UINTN) TempBuffer,
    (VOID *) (UINTN) MapInfo->HostAddress,
    MapInfo->NumberOfBytes);

  //
  // Now change the encryption mask of the host buffer
  //
  if (Enc) {
    Status = MemEncryptSevSetPageEncMask (0, MapInfo->HostAddress,
               MapInfo->NumberOfPages, TRUE);
    ASSERT_EFI_ERROR (Status);
  } else {
    Status = MemEncryptSevClearPageEncMask (0, MapInfo->HostAddress,
               MapInfo->NumberOfPages, TRUE);
    ASSERT_EFI_ERROR (Status);
  }

  //
  // Copy the data from intermediate buffer into host buffer. At this
  // time encryption masks will be different on host and intermediate
  // buffer and the hardware will perform encryption/decryption on
  // accesses.
  //
  CopyMem (
    (VOID *) (UINTN)MapInfo->HostAddress,
    (VOID *) (UINTN)TempBuffer,
    MapInfo->NumberOfBytes);

  //
  // Restore the encryption mask of the intermediate buffer
  //
  Status = MemEncryptSevSetPageEncMask (0, TempBuffer, MapInfo->NumberOfPages, TRUE);
  ASSERT_EFI_ERROR (Status);

  //
  // Free the intermediate buffer
  //
  gBS->FreePages (TempBuffer, MapInfo->NumberOfPages);
  return EFI_SUCCESS;
}

/**
  This function will be called by Map() when mapping the buffer buffer to
  BusMasterCommonBuffer type.

**/
STATIC
EFI_STATUS
SetHostBufferAsEncrypted (
  IN MAP_INFO *MapInfo
  )
{
  return SetBufferAsEncDec (MapInfo, TRUE);
}

/**
  This function will be called by Unmap() when unmapping host buffer
  from the BusMasterCommonBuffer type.

**/
STATIC
EFI_STATUS
SetHostBufferAsDecrypted (
  IN MAP_INFO *MapInfo
  )
{
  return SetBufferAsEncDec (MapInfo, FALSE);
}

/**
  Provides the controller-specific addresses required to access system memory from a
  DMA bus master. On SEV guest, the DMA operations must be performed on shared
  buffer hence we allocate a bounce buffer to map the HostAddress to a DeviceAddress.
  The Encryption attribute is removed from the DeviceAddress buffer.

  @param  This                  The protocol instance pointer.
  @param  Operation             Indicates if the bus master is going to read or
                                write to system memory.
  @param  HostAddress           The system memory address to map to the PCI controller.
  @param  NumberOfBytes         On input the number of bytes to map. On output
                                the number of bytes
                                that were mapped.
  @param  DeviceAddress         The resulting map address for the bus master PCI
                                controller to use to
                                access the hosts HostAddress.
  @param  Mapping               A resulting value to pass to Unmap().

  @retval EFI_SUCCESS           The range was mapped for the returned NumberOfBytes.
  @retval EFI_UNSUPPORTED       The HostAddress cannot be mapped as a common buffer.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack
                                of resources.
  @retval EFI_DEVICE_ERROR      The system hardware could not map the requested address.

**/
EFI_STATUS
EFIAPI
IoMmuMap (
  IN     EDKII_IOMMU_PROTOCOL                       *This,
  IN     EDKII_IOMMU_OPERATION                      Operation,
  IN     VOID                                       *HostAddress,
  IN OUT UINTN                                      *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS                       *DeviceAddress,
  OUT    VOID                                       **Mapping
  )
{
  EFI_STATUS                                        Status;
  EFI_PHYSICAL_ADDRESS                              PhysicalAddress;
  MAP_INFO                                          *MapInfo;
  EFI_PHYSICAL_ADDRESS                              DmaMemoryTop;
  EFI_ALLOCATE_TYPE                                 AllocateType;

  if (HostAddress == NULL || NumberOfBytes == NULL || DeviceAddress == NULL ||
      Mapping == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make sure that Operation is valid
  //
  if ((UINT32) Operation >= EdkiiIoMmuOperationMaximum) {
    return EFI_INVALID_PARAMETER;
  }
  PhysicalAddress = (EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress;

  DmaMemoryTop = (UINTN)-1;
  AllocateType = AllocateAnyPages;

  if (((Operation != EdkiiIoMmuOperationBusMasterRead64 &&
        Operation != EdkiiIoMmuOperationBusMasterWrite64 &&
        Operation != EdkiiIoMmuOperationBusMasterCommonBuffer64)) &&
      ((PhysicalAddress + *NumberOfBytes) > SIZE_4GB)) {
    //
    // If the root bridge or the device cannot handle performing DMA above
    // 4GB but any part of the DMA transfer being mapped is above 4GB, then
    // map the DMA transfer to a buffer below 4GB.
    //
    DmaMemoryTop = SIZE_4GB - 1;
    AllocateType = AllocateMaxAddress;

    if (Operation == EdkiiIoMmuOperationBusMasterCommonBuffer ||
        Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64) {
        //
        // Common Buffer operations can not be remapped.  If the common buffer
        // if above 4GB, then it is not possible to generate a mapping, so return
        // an error.
        //
        return EFI_UNSUPPORTED;
    }
  }

  //
  // Allocate a MAP_INFO structure to remember the mapping when Unmap() is
  // called later.
  //
  MapInfo = AllocatePool (sizeof (MAP_INFO));
  if (MapInfo == NULL) {
    *NumberOfBytes = 0;
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize the MAP_INFO structure
  //
  MapInfo->Operation         = Operation;
  MapInfo->NumberOfBytes     = *NumberOfBytes;
  MapInfo->NumberOfPages     = EFI_SIZE_TO_PAGES (MapInfo->NumberOfBytes);
  MapInfo->HostAddress       = PhysicalAddress;
  MapInfo->DeviceAddress     = DmaMemoryTop;

  //
  // If the requested Map() operation is BusMasterCommandBuffer then map
  // using internal function otherwise allocate a bounce buffer to map
  // the host buffer to device buffer
  //
  if (Operation == EdkiiIoMmuOperationBusMasterCommonBuffer ||
      Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64) {

    Status = SetHostBufferAsDecrypted (MapInfo);
    if (EFI_ERROR (Status)) {
      FreePool (MapInfo);
      *NumberOfBytes = 0;
      return Status;
    }

    MapInfo->DeviceAddress = MapInfo->HostAddress;
    goto Done;
  }

  //
  // Allocate a buffer to map the transfer to.
  //
  Status = gBS->AllocatePages (
                  AllocateType,
                  EfiBootServicesData,
                  MapInfo->NumberOfPages,
                  &MapInfo->DeviceAddress
                  );
  if (EFI_ERROR (Status)) {
    FreePool (MapInfo);
    *NumberOfBytes = 0;
    return Status;
  }

  //
  // Clear the memory encryption mask from the device buffer
  //
  Status = MemEncryptSevClearPageEncMask (0, MapInfo->DeviceAddress, MapInfo->NumberOfPages, TRUE);
  ASSERT_EFI_ERROR(Status);

  //
  // If this is a read operation from the Bus Master's point of view,
  // then copy the contents of the real buffer into the mapped buffer
  // so the Bus Master can read the contents of the real buffer.
  //
  if (Operation == EdkiiIoMmuOperationBusMasterRead ||
      Operation == EdkiiIoMmuOperationBusMasterRead64) {
    CopyMem (
      (VOID *) (UINTN) MapInfo->DeviceAddress,
      (VOID *) (UINTN) MapInfo->HostAddress,
      MapInfo->NumberOfBytes
      );
  }

Done:
  //
  // The DeviceAddress is the address of the maped buffer below 4GB
  //
  *DeviceAddress = MapInfo->DeviceAddress;

  //
  // Return a pointer to the MAP_INFO structure in Mapping
  //
  *Mapping       = MapInfo;

  DEBUG ((DEBUG_VERBOSE, "%a Device 0x%Lx Host 0x%Lx Pages 0x%Lx Bytes 0x%Lx\n",
        __FUNCTION__, MapInfo->DeviceAddress, MapInfo->HostAddress,
        MapInfo->NumberOfPages, MapInfo->NumberOfBytes));

  return EFI_SUCCESS;
}

/**
  Completes the Map() operation and releases any corresponding resources.

  @param  This                  The protocol instance pointer.
  @param  Mapping               The mapping value returned from Map().

  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_INVALID_PARAMETER Mapping is not a value that was returned by Map().
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.
**/
EFI_STATUS
EFIAPI
IoMmuUnmap (
  IN  EDKII_IOMMU_PROTOCOL                     *This,
  IN  VOID                                     *Mapping
  )
{
  MAP_INFO                 *MapInfo;
  EFI_STATUS               Status;

  if (Mapping == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  MapInfo = (MAP_INFO *)Mapping;

  //
  // If this is a CommonBuffer operation from the Bus Master's point of
  // view then Map() have cleared the memory encryption mask from Host
  // buffer. Lets restore the memory encryption mask before returning
  //
  if (MapInfo->Operation == EdkiiIoMmuOperationBusMasterCommonBuffer ||
      MapInfo->Operation == EdkiiIoMmuOperationBusMasterCommonBuffer64) {

    Status = SetHostBufferAsEncrypted (MapInfo);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    goto Done;
  }

  //
  // If this is a write operation from the Bus Master's point of view,
  // then copy the contents of the mapped buffer into the real buffer
  // so the processor can read the contents of the real buffer.
  //
  if (MapInfo->Operation == EdkiiIoMmuOperationBusMasterWrite ||
      MapInfo->Operation == EdkiiIoMmuOperationBusMasterWrite64) {
    CopyMem (
      (VOID *) (UINTN) MapInfo->HostAddress,
      (VOID *) (UINTN) MapInfo->DeviceAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // Restore the memory encryption mask
  //
  Status = MemEncryptSevSetPageEncMask (0, MapInfo->DeviceAddress, MapInfo->NumberOfPages, TRUE);
  ASSERT_EFI_ERROR(Status);

  //
  // Zero the shared memory so that hypervisor no longer able to get intelligentable
  // data.
  //
  SetMem ((VOID *) (UINTN)MapInfo->DeviceAddress, MapInfo->NumberOfBytes, 0);

  //
  // Free the bounce buffer
  //
  gBS->FreePages (MapInfo->DeviceAddress, MapInfo->NumberOfPages);

Done:
  DEBUG ((DEBUG_VERBOSE, "%a Device 0x%Lx Host 0x%Lx Pages 0x%Lx Bytes 0x%Lx\n",
        __FUNCTION__, MapInfo->DeviceAddress, MapInfo->HostAddress,
        MapInfo->NumberOfPages, MapInfo->NumberOfBytes));

  FreePool (Mapping);
  return EFI_SUCCESS;
}

/**
  Allocates pages that are suitable for an OperationBusMasterCommonBuffer or
  OperationBusMasterCommonBuffer64 mapping.

  @param  This                  The protocol instance pointer.
  @param  Type                  This parameter is not used and must be ignored.
  @param  MemoryType            The type of memory to allocate, EfiBootServicesData
                                or EfiRuntimeServicesData.
  @param  Pages                 The number of pages to allocate.
  @param  HostAddress           A pointer to store the base system memory address
                                of the allocated range.
  @param  Attributes            The requested bit mask of attributes for the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were allocated.
  @retval EFI_UNSUPPORTED       Attributes is unsupported. The only legal attribute
                                bits are MEMORY_WRITE_COMBINE and MEMORY_CACHED.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The memory pages could not be allocated.

**/
EFI_STATUS
EFIAPI
IoMmuAllocateBuffer (
  IN     EDKII_IOMMU_PROTOCOL                     *This,
  IN     EFI_ALLOCATE_TYPE                        Type,
  IN     EFI_MEMORY_TYPE                          MemoryType,
  IN     UINTN                                    Pages,
  IN OUT VOID                                     **HostAddress,
  IN     UINT64                                   Attributes
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      PhysicalAddress;

  //
  // Validate Attributes
  //
  if ((Attributes & EDKII_IOMMU_ATTRIBUTE_INVALID_FOR_ALLOCATE_BUFFER) != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Check for invalid inputs
  //
  if (HostAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The only valid memory types are EfiBootServicesData and
  // EfiRuntimeServicesData
  //
  if (MemoryType != EfiBootServicesData &&
      MemoryType != EfiRuntimeServicesData) {
    return EFI_INVALID_PARAMETER;
  }

  PhysicalAddress = (UINTN)-1;
  if ((Attributes & EDKII_IOMMU_ATTRIBUTE_DUAL_ADDRESS_CYCLE) == 0) {
    //
    // Limit allocations to memory below 4GB
    //
    PhysicalAddress = SIZE_4GB - 1;
  }
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  Pages,
                  &PhysicalAddress
                  );
  if (!EFI_ERROR (Status)) {
    *HostAddress = (VOID *) (UINTN) PhysicalAddress;
  }

  DEBUG ((DEBUG_VERBOSE, "%a Address 0x%Lx Pages 0x%Lx\n", __FUNCTION__, PhysicalAddress, Pages));
  return Status;
}

/**
  Frees memory that was allocated with AllocateBuffer().

  @param  This                  The protocol instance pointer.
  @param  Pages                 The number of pages to free.
  @param  HostAddress           The base system memory address of the allocated range.

  @retval EFI_SUCCESS           The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER The memory range specified by HostAddress and Pages
                                was not allocated with AllocateBuffer().

**/
EFI_STATUS
EFIAPI
IoMmuFreeBuffer (
  IN  EDKII_IOMMU_PROTOCOL                     *This,
  IN  UINTN                                    Pages,
  IN  VOID                                     *HostAddress
  )
{
  DEBUG ((DEBUG_VERBOSE, "%a Address 0x%Lx Pages 0x%Lx\n", __FUNCTION__, (UINTN)HostAddress, Pages));
  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS) (UINTN) HostAddress, Pages);
}


/**
  Set IOMMU attribute for a system memory.

  If the IOMMU protocol exists, the system memory cannot be used
  for DMA by default.

  When a device requests a DMA access for a system memory,
  the device driver need use SetAttribute() to update the IOMMU
  attribute to request DMA access (read and/or write).

  The DeviceHandle is used to identify which device submits the request.
  The IOMMU implementation need translate the device path to an IOMMU device ID,
  and set IOMMU hardware register accordingly.
  1) DeviceHandle can be a standard PCI device.
     The memory for BusMasterRead need set EDKII_IOMMU_ACCESS_READ.
     The memory for BusMasterWrite need set EDKII_IOMMU_ACCESS_WRITE.
     The memory for BusMasterCommonBuffer need set EDKII_IOMMU_ACCESS_READ|EDKII_IOMMU_ACCESS_WRITE.
     After the memory is used, the memory need set 0 to keep it being protected.
  2) DeviceHandle can be an ACPI device (ISA, I2C, SPI, etc).
     The memory for DMA access need set EDKII_IOMMU_ACCESS_READ and/or EDKII_IOMMU_ACCESS_WRITE.

  @param[in]  This              The protocol instance pointer.
  @param[in]  DeviceHandle      The device who initiates the DMA access request.
  @param[in]  Mapping           The mapping value returned from Map().
  @param[in]  IoMmuAccess       The IOMMU access.

  @retval EFI_SUCCESS            The IoMmuAccess is set for the memory range specified by DeviceAddress and Length.
  @retval EFI_INVALID_PARAMETER  DeviceHandle is an invalid handle.
  @retval EFI_INVALID_PARAMETER  Mapping is not a value that was returned by Map().
  @retval EFI_INVALID_PARAMETER  IoMmuAccess specified an illegal combination of access.
  @retval EFI_UNSUPPORTED        DeviceHandle is unknown by the IOMMU.
  @retval EFI_UNSUPPORTED        The bit mask of IoMmuAccess is not supported by the IOMMU.
  @retval EFI_UNSUPPORTED        The IOMMU does not support the memory range specified by Mapping.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources available to modify the IOMMU access.
  @retval EFI_DEVICE_ERROR       The IOMMU device reported an error while attempting the operation.

**/
EFI_STATUS
EFIAPI
IoMmuSetAttribute (
  IN EDKII_IOMMU_PROTOCOL  *This,
  IN EFI_HANDLE            DeviceHandle,
  IN VOID                  *Mapping,
  IN UINT64                IoMmuAccess
  )
{
  return EFI_UNSUPPORTED;
}

EDKII_IOMMU_PROTOCOL  mAmdSev = {
  EDKII_IOMMU_PROTOCOL_REVISION,
  IoMmuSetAttribute,
  IoMmuMap,
  IoMmuUnmap,
  IoMmuAllocateBuffer,
  IoMmuFreeBuffer,
};

/**
  Initialize Iommu Protocol.

**/
VOID
EFIAPI
AmdSevInstallIoMmuProtocol (
  VOID
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEdkiiIoMmuProtocolGuid, &mAmdSev,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
}
