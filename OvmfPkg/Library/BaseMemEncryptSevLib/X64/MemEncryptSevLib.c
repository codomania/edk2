/** @file

  Secure Encrypted Virtualization (SEV) library helper function

  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Uefi.h"
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Register/Cpuid.h>
#include <Register/AmdSevMap.h>
#include <Library/MemEncryptSevLib.h>

#include "VirtualMemory.h"

STATIC BOOLEAN SevStatus = FALSE;
STATIC BOOLEAN SevStatusChecked = FALSE;

/**
  Returns a boolean to indicate whether SEV is enabled

  @retval TRUE           When SEV is active
  @retval FALSE          When SEV is not enabled
  **/
BOOLEAN
EFIAPI
MemEncryptSevIsEnabled (
  VOID
  )
{
  UINT32 RegEax;
  MSR_SEV_STATUS_REGISTER Msr;
  CPUID_MEMORY_ENCRYPTION_INFO_EAX  Eax;

  //
  // If Status is already checked then return it
  //
  if (SevStatusChecked) {
    return SevStatus;
  }

  //
  // Check if memory encryption leaf exist
  //
  AsmCpuid (CPUID_EXTENDED_FUNCTION, &RegEax, NULL, NULL, NULL);
  if (RegEax >= CPUID_MEMORY_ENCRYPTION_INFO) {
    //
    // CPUID Fn8000_001F[EAX] Bit 1 (Sev supported)
    //
    AsmCpuid (CPUID_MEMORY_ENCRYPTION_INFO, &Eax.Uint32, NULL, NULL, NULL);

    if (Eax.Bits.SevBit) {
      //
      // Check MSR_0xC0010131 Bit 0 (Sev Enabled)
      //
      Msr.Uint32 = AsmReadMsr32 (MSR_SEV_STATUS);
      if (Msr.Bits.SevBit) {
        return TRUE;
      }
    }
  }

  SevStatusChecked = TRUE;

  return FALSE;
}

/**
 
  This function clears memory encryption bit for the memory region specified by BaseAddress and
  Number of pages from the current page table context.

  @param[in]  BaseAddress             The physical address that is the start address of a memory region.
  @param[in]  NumberOfPages           The number of pages from start memory region.

  @retval RETURN_SUCCESS              The attributes were cleared for the memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
  @retval RETURN_UNSUPPORTED          Clearing the memory encryption attribute is not supported
  **/
RETURN_STATUS
EFIAPI
MemEncryptSevClearPageEncMask (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINT32                   NumPages
  )
{
  return Set_Memory_Decrypted (BaseAddress, NumPages * EFI_PAGE_SIZE);
}

/**
 
  This function clears memory encryption bit for the memory region specified by BaseAddress and
  Number of pages from the current page table context.

  @param[in]  BaseAddress             The physical address that is the start address of a memory region.
  @param[in]  NumberOfPages           The number of pages from start memory region.

  @retval RETURN_SUCCESS              The attributes were cleared for the memory region.
  @retval RETURN_INVALID_PARAMETER    Number of pages is zero.
  @retval RETURN_UNSUPPORTED          Clearing the memory encryption attribute is not supported
  **/
RETURN_STATUS
EFIAPI
MemEncryptSevSetPageEncMask (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINT32                   NumPages
  )
{
  return Set_Memory_Encrypted (BaseAddress, NumPages * EFI_PAGE_SIZE);
}
