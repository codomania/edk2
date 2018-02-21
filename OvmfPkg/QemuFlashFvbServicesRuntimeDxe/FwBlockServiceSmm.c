/**@file
  Functions related to the Firmware Volume Block service whose
  implementation is specific to the SMM driver build.

  Copyright (C) 2015, Red Hat, Inc.
  Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SmmFirmwareVolumeBlock.h>

#include "FwBlockService.h"

VOID
InstallProtocolInterfaces (
  IN EFI_FW_VOL_BLOCK_DEVICE *FvbDevice
  )
{
  EFI_HANDLE FvbHandle;
  EFI_STATUS Status;

  ASSERT (FeaturePcdGet (PcdSmmSmramRequire));

  //
  // There is no SMM service that can install multiple protocols in the SMM
  // protocol database in one go.
  //
  // The SMM Firmware Volume Block protocol structure is the same as the
  // Firmware Volume Block protocol structure.
  //
  FvbHandle = NULL;
  DEBUG ((EFI_D_INFO, "Installing QEMU flash SMM FVB\n"));
  Status = gSmst->SmmInstallProtocolInterface (
                    &FvbHandle,
                    &gEfiSmmFirmwareVolumeBlockProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    &FvbDevice->FwVolBlockInstance
                    );
  ASSERT_EFI_ERROR (Status);

  Status = gSmst->SmmInstallProtocolInterface (
                    &FvbHandle,
                    &gEfiDevicePathProtocolGuid,
                    EFI_NATIVE_INTERFACE,
                    FvbDevice->DevicePath
                    );
  ASSERT_EFI_ERROR (Status);
}

VOID
InstallVirtualAddressChangeHandler (
  VOID
  )
{
  //
  // Nothing.
  //
}

VOID
FvbBeforeFlashProbe (
  VOID
  )
{

  ASSERT (FeaturePcdGet (PcdSmmSmramRequire));

  //
  // When SEV is enabled, AmdSevDxe runs early in PEI phase and clears the C-bit
  // from the MMIO space (including flash ranges) but the driver runs in non SMM
  // context hence it cleared the flash ranges from non SMM page table.
  // When SMM is enabled, the flash services are accessed from the SMM mode
  // hence we explicitly clear the C-bit on flash ranges from SMM page table.
  //
  if (MemEncryptSevIsEnabled ()) {
    EFI_STATUS              Status;
    EFI_PHYSICAL_ADDRESS    BaseAddress;
    UINTN                   FdBlockSize, FdBlockCount;

    BaseAddress = (EFI_PHYSICAL_ADDRESS) PcdGet32 (PcdOvmfFdBaseAddress);
    FdBlockSize = PcdGet32 (PcdOvmfFirmwareBlockSize);
    FdBlockCount = PcdGet32 (PcdOvmfFirmwareFdSize) / FdBlockSize;

    Status = MemEncryptSevClearPageEncMask (
               0,
               BaseAddress,
               EFI_SIZE_TO_PAGES (FdBlockSize * FdBlockCount),
               FALSE
              );
    ASSERT_EFI_ERROR (Status);
  }
}
