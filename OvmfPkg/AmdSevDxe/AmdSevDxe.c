/** @file

  AMD Sev Dxe driver. The driver runs early in DXE phase and clears C-bit from
  MMIO space and installs EDKII_IOMMU_PROTOCOL to provide the support for DMA
  operations when SEV is enabled.

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD
  License which accompanies this distribution.  The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/MemEncryptSevLib.h>

#include "AmdSevMmio.h"
#include "AmdSevIommu.h"

EFI_STATUS
EFIAPI
AmdSevDxeEntryPoint (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  //
  // Do nothing when SEV is not enabled
  //
  if (!MemEncryptSevIsEnabled ()) {
    return EFI_SUCCESS;
  }

  //
  // Clear C-bit from MMIO Memory Range
  //
  AmdSevClearEncMaskMmioRange ();

  //
  // Install IOMMU protocol to provide DMA support for PCIHostBridgeIo and
  // AmdSevMemEncryptLib.
  //
  AmdSevInstallIommuProtocol ();

  return EFI_SUCCESS;
}
