/** @file

  Stateless fw_cfg library implementation.

  Clients must call QemuFwCfgIsAvailable() first.

  Copyright (C) 2013, Red Hat, Inc.
  Copyright (c) 2011 - 2013, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Register/Cpuid.h>
#include <Register/AmdSevMap.h>

#include "QemuFwCfgLibInternal.h"

/**
  Returns a boolean indicating if the firmware configuration interface
  is available or not.

  This function may change fw_cfg state.

  @retval    TRUE   The interface is available
  @retval    FALSE  The interface is not available

**/
BOOLEAN
EFIAPI
QemuFwCfgIsAvailable (
  VOID
  )
{
  UINT32 Signature;
  UINT32 Revision;

  QemuFwCfgSelectItem (QemuFwCfgItemSignature);
  Signature = QemuFwCfgRead32 ();
  DEBUG ((EFI_D_INFO, "FW CFG Signature: 0x%x\n", Signature));
  QemuFwCfgSelectItem (QemuFwCfgItemInterfaceVersion);
  Revision = QemuFwCfgRead32 ();
  DEBUG ((EFI_D_INFO, "FW CFG Revision: 0x%x\n", Revision));
  if ((Signature != SIGNATURE_32 ('Q', 'E', 'M', 'U')) ||
      (Revision < 1)
     ) {
    DEBUG ((EFI_D_INFO, "QemuFwCfg interface not supported.\n"));
    return FALSE;
  }

  DEBUG ((EFI_D_INFO, "QemuFwCfg interface is supported.\n"));
  return TRUE;
}


/**
  Returns a boolean indicating if the firmware configuration interface is
  available for library-internal purposes.

  This function never changes fw_cfg state.

  @retval    TRUE   The interface is available internally.
  @retval    FALSE  The interface is not available internally.
**/
BOOLEAN
InternalQemuFwCfgIsAvailable (
  VOID
  )
{
  //
  // We always return TRUE, because the consumer of this library ought to have
  // called QemuFwCfgIsAvailable before making other calls which would hit this
  // path.
  //
  return TRUE;
}

/**
  Returns a boolean indicating whether QEMU provides the DMA-like access method
  for fw_cfg.

  @retval    TRUE   The DMA-like access method is available.
  @retval    FALSE  The DMA-like access method is unavailable.
**/
BOOLEAN
InternalQemuFwCfgDmaIsAvailable (
  VOID
  )
{
  return FALSE;
}

/**
 Returns a boolean indicating whether the SEV is enabled

 @retval    TRUE    SEV is enabled
 @retval    FALSE   SEV is not enabled
**/
BOOLEAN
InternalQemuFwCfgSevIsEnabled (
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
 Allocate a bounce buffer for SEV DMA.

  @param[in]     NumPage  Number of pages.
  @param[out]    Buffer   Allocated DMA Buffer pointer

**/
VOID
InternalQemuFwCfgSevDmaAllocateBuffer (
  IN     UINT32   NumPages,
  OUT    VOID     **Buffer
  )
{
  //
  // We should never reach here
  //
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
 Free the DMA buffer allocated using InternalQemuFwCfgSevDmaAllocateBuffer

  @param[in]     NumPage  Number of pages.
  @param[in]     Buffer   DMA Buffer pointer

**/
VOID
InternalQemuFwCfgSevDmaFreeBuffer (
  IN     VOID     *Buffer,
  IN     UINT32   NumPages
  )
{
  //
  // We should never reach here
  //
  ASSERT (FALSE);
  CpuDeadLoop ();
}
