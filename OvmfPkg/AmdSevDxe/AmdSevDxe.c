/** @file

  AMD Sev Dxe driver. This driver is dispatched early in DXE, due to being list
  in APRIORI. It clears C-bit from MMIO and NonExistent Memory space when SEV is
  enabled.

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
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Register/SmramSaveStateMap.h>
#include <Register/QemuSmramSaveStateMap.h>

EFI_STATUS
EFIAPI
AmdSevDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  *AllDescMap;
  UINTN                            NumEntries;
  UINTN                            Index;

  //
  // Do nothing when SEV is not enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return EFI_UNSUPPORTED;
  }

  //
  // Iterate through the GCD map and clear the C-bit from MMIO and NonExistent
  // memory space. The NonExistent memory space will be used for mapping the MMIO
  // space added later (eg PciRootBridge). By clearing both known MMIO and
  // NonExistent memory space can gurantee that current and furture MMIO adds
  // will have C-bit cleared.
  //
  Status = gDS->GetMemorySpaceMap (&NumEntries, &AllDescMap);
  if (!EFI_ERROR (Status)) {
    for (Index = 0; Index < NumEntries; Index++) {
      CONST EFI_GCD_MEMORY_SPACE_DESCRIPTOR *Desc;

      Desc = &AllDescMap[Index];
      if (Desc->GcdMemoryType == EfiGcdMemoryTypeMemoryMappedIo ||
          Desc->GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
        Status = MemEncryptSevClearPageEncMask (0,
                                                Desc->BaseAddress,
                                                EFI_SIZE_TO_PAGES(Desc->Length),
                                                FALSE);
        ASSERT_EFI_ERROR (Status);
      }
    }

    FreePool (AllDescMap);
  }

  //
  // When SMM is enabled, clear the C-bit from SMM Saved State Area
  //
  // NOTES: The SavedStateArea address cleared here is before SMBASE
  // relocation. Currently, we do not clear the SavedStateArea address after
  // SMBASE is relocated due to the following reasons:
  //
  // 1) Guest BIOS never access the relocated SavedStateArea.
  //
  // 2) The C-bit works on page-aligned address, but the SavedStateArea
  // address is not a page-aligned. Theoretically, we could roundup the address
  // and clear the C-bit of aligned address but looking carefully we found
  // that some portion of the page contains code -- which will causes a bigger
  // issues for SEV guest. When SEV is enabled, all the code must be encrypted
  // otherwise hardware will cause trap.
  //
  // We restore the C-bit for this SMM Saved State Area after SMBASE relocation
  // is completed (See OvmfPkg/Library/SmmCpuFeaturesLib/SmmCpuFeaturesLib.c).
  //
  if (FeaturePcdGet (PcdSmmSmramRequire)) {
    EFI_PHYSICAL_ADDRESS  SmmSavedStateAreaAddress;

    SmmSavedStateAreaAddress = SMM_DEFAULT_SMBASE + SMRAM_SAVE_STATE_MAP_OFFSET;

    Status = MemEncryptSevClearPageEncMask (
               0,
               SmmSavedStateAreaAddress,
               EFI_SIZE_TO_PAGES (sizeof(QEMU_SMRAM_SAVE_STATE_MAP)),
               FALSE
               );
    ASSERT_EFI_ERROR (Status);
  }

  return EFI_SUCCESS;
}
