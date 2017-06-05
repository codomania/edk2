/** @file
DxeCore Gcd Platform Hook NULL library instance.

Copyright (c) 2017, AMD Inc. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

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
#include <Library/DxeGcdCorePlatformHookLib.h>

/**
  This function is hook point called after the GCD memory map is initialized,
  it allows the platform to perform post processing before the GCD maps are
  made available to other drivers.
**/
VOID
EFIAPI
DxeGcdCorePlatformHookReady (
  EFI_DXE_SERVICES      *gDS
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
    return;
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
}
