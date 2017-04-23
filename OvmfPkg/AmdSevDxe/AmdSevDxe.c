/** @file

  The driver runs early in DXE phase and clears C-bit from MMIO memory space.

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD
  License which accompanies this distribution.  The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>

EFI_STATUS
EFIAPI
AmdSevDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *AllDescMap;
  UINTN                            NumEntries;
  UINTN                            Index;
  EFI_STATUS                       Status;

  //
  // Do nothing when SEV is not enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return EFI_SUCCESS;
  }

  //
  // Iterate through the GCD map and clear the C-bit from MMIO and NonExistent
  // memory space. The NonExistent memory space will be used for mapping the MMIO
  // space added later (eg PciRootBridge). By clearing both known NonExistent
  // memory space can gurantee that any MMIO added later will have C-bit cleared.
  //
  Status = gDS->GetMemorySpaceMap (&NumEntries, &AllDescMap);
  if (Status == EFI_SUCCESS) {
    for (Index = 0; Index < NumEntries; Index++) {
      CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Desc;

      Desc = &AllDescMap[Index];
      if (Desc->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo ||
          Desc->GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
        Status = MemEncryptSevClearPageEncMask (Desc->BaseAddress, EFI_SIZE_TO_PAGES(Desc->Length), FALSE);
        ASSERT_EFI_ERROR(Status);
      }
    }
  }

  return Status;
}
