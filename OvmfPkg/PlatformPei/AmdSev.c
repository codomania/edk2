/**@file
  Initialize Secure Encrypted Virtualization (SEV) support

  Copyright (c) 2017, Advanced Micro Devices. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
//
// The package level header files this module uses
//
#include <PiPei.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Register/Cpuid.h>
#include <Register/AmdSevMap.h>

/**

  Function returns 'TRUE' when SEV is enabled otherwise FALSE

  **/
STATIC
BOOLEAN
SevIsEnabled (
  VOID
  )
{
  UINT32 RegEax;
  MSR_SEV_STATUS_REGISTER Msr;
  CPUID_MEMORY_ENCRYPTION_INFO_EAX  Eax;

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

  return FALSE;
}

/**
  Function checks if SEV support is available, if present then it updates
  the dynamic PcdPteMemoryEncryptionAddressOrMask with memory encryption mask.

  **/
VOID
EFIAPI
AmdSevInitialize (
  VOID
  )
{
  UINT64 MeMask;
  CPUID_MEMORY_ENCRYPTION_INFO_EBX  Ebx;

  //
  // Check if SEV is enabled
  //
  if (!SevIsEnabled ()) {
    return;
  }

  //
  // CPUID Fn8000_001F[EBX] Bit 0:5 (memory encryption bit position)
  //
  AsmCpuid (CPUID_MEMORY_ENCRYPTION_INFO, NULL, &Ebx.Uint32, NULL, NULL);
  MeMask = LShiftU64 (1, Ebx.Bits.PtePosBits);

  //
  // Set Memory Encryption Mask PCD
  //
  PcdSet64S (PcdPteMemoryEncryptionAddressOrMask, MeMask);

  DEBUG ((EFI_D_INFO, "SEV support is enabled (mask 0x%lx)\n", MeMask));
}
