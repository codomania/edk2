/** @file

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
#include <Library/PcdLib.h>
#include <Library/MemcryptSevLib.h>

#define KVM_FEATURE_MEMORY_ENCRYPTION 0x100

RETURN_STATUS
EFIAPI
MemcryptSevInitialize (
  VOID
  )
{
  UINT32 EBX;
  UINT64 MeMask = 0;

  if (SevActive ()) {
     // CPUID Fn8000_001f[EBX] - Bit 5:0 (memory encryption bit position)
     AsmCpuid(0x8000001f, NULL, &EBX, NULL, NULL);
     MeMask = (1ULL << (EBX & 0x3f));
     DEBUG ((DEBUG_INFO, "KVM Secure Encrypted Virtualization (SEV) is enabled\n"));
     DEBUG ((DEBUG_INFO, "MemEncryptionMask 0x%lx\n", MeMask));
  }

  PcdSet64S (PcdPteMemoryEncryptionAddressOrMask, MeMask);

  return RETURN_SUCCESS;
}

BOOLEAN
EFIAPI
SevActive (
  VOID
  )
{
  UINT32 KVMFeatures, EAX;

  // Check if KVM memory encyption feature is set
  AsmCpuid(0x40000001, &KVMFeatures, NULL, NULL, NULL);
  if (KVMFeatures & KVM_FEATURE_MEMORY_ENCRYPTION) {

     // Check whether SEV is enabled
     // CPUID Fn8000_001f[EAX] - Bit 0  (SEV is enabled)
     AsmCpuid(0x8000001f, &EAX, NULL, NULL, NULL);

     return TRUE;
  }

  return FALSE;
}

