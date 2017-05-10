/** @file

  Implements routines to clear C-bit from MMIO Memory Range

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AmdSevMmio.h"

/**

  Iterate through the GCD map and clear the C-bit from MMIO and NonExistent
  memory space. The NonExistent memory space will be used for mapping the MMIO
  space added later (eg PciRootBridge). By clearing both known NonExistent
  memory space can gurantee that any MMIO mapped later will have C-bit cleared.
*/
VOID
EFIAPI
AmdSevClearEncMaskMmioRange (
  VOID
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *AllDescMap;
  UINTN                            NumEntries;
  UINTN                            Index;

  Status = gDS->GetMemorySpaceMap (&NumEntries, &AllDescMap);
  if (Status == EFI_SUCCESS) {
    for (Index = 0; Index < NumEntries; Index++) {
      CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Desc;

      Desc = &AllDescMap[Index];
      if (Desc->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo ||
          Desc->GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
        Status = MemEncryptSevClearPageEncMask (0, Desc->BaseAddress, EFI_SIZE_TO_PAGES(Desc->Length), FALSE);
        ASSERT_EFI_ERROR(Status);
      }
    }
  }
}
