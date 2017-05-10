/** @file

  Stateful and implicitly initialized fw_cfg library implementation.

  Copyright (C) 2013, Red Hat, Inc.
  Copyright (c) 2011 - 2013, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2017, Advanced Micro Devices. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "Uefi.h"

#include <Protocol/IoMmu.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>

#include "QemuFwCfgLibInternal.h"

STATIC BOOLEAN mQemuFwCfgSupported = FALSE;
STATIC BOOLEAN mQemuFwCfgDmaSupported;

STATIC EDKII_IOMMU_PROTOCOL        *mIoMmuProtocol;
/**

 Returns a boolean indicating whether SEV is enabled

 @retval    TRUE    SEV is enabled
 @retval    FALSE   SEV is disabled
**/
BOOLEAN
InternalQemuFwCfgSevIsEnabled (
  VOID
  )
{
  return MemEncryptSevIsEnabled ();
}

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
  return InternalQemuFwCfgIsAvailable ();
}


RETURN_STATUS
EFIAPI
QemuFwCfgInitialize (
  VOID
  )
{
  UINT32 Signature;
  UINT32 Revision;

  //
  // Enable the access routines while probing to see if it is supported.
  // For probing we always use the IO Port (IoReadFifo8()) access method.
  //
  mQemuFwCfgSupported = TRUE;
  mQemuFwCfgDmaSupported = FALSE;

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
    mQemuFwCfgSupported = FALSE;
    return RETURN_SUCCESS;
  }

  if ((Revision & FW_CFG_F_DMA) == 0) {
    DEBUG ((DEBUG_INFO, "QemuFwCfg interface (IO Port) is supported.\n"));
  } else {
    mQemuFwCfgDmaSupported = TRUE;
    DEBUG ((DEBUG_INFO, "QemuFwCfg interface (DMA) is supported.\n"));
  }

  //
  // When SEV is enabled, the AmdSevDxe driver should have installed the IoMMU
  // protocol which must be used for mapping host buffer to DMA buffer
  //
  if (mQemuFwCfgDmaSupported && MemEncryptSevIsEnabled ()) {
    EFI_STATUS   Status;

    Status = gBS->LocateProtocol (&gEdkiiIoMmuProtocolGuid, NULL, (VOID **)&mIoMmuProtocol);
    if (EFI_ERROR(Status)) {
      DEBUG ((DEBUG_WARN, "QemuwCfgSevDma: failed to locate IoMmu protocol, disabling DMA support\n"));
      mQemuFwCfgDmaSupported = FALSE;
    }
  }

  return RETURN_SUCCESS;
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
  return mQemuFwCfgSupported;
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
  return mQemuFwCfgDmaSupported;
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
  EFI_STATUS    Status;

  if (!mIoMmuProtocol) {
    //
    // We should never reach here
    //
    ASSERT (FALSE);
    CpuDeadLoop ();
  }

  Status = mIoMmuProtocol->AllocateBuffer (
                            mIoMmuProtocol,
                            0,
                            EfiBootServicesData,
                            NumPages,
                            Buffer,
                            EDKII_IOMMU_ATTRIBUTE_MEMORY_CACHED
                          );
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_VERBOSE, "QemuFwCfgSevDma allocate buffer 0x%Lx Pages %d\n", (UINTN)Buffer, NumPages));

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
  EFI_STATUS    Status;

  if (!mIoMmuProtocol) {
    //
    // We should never reach here
    //
    ASSERT (FALSE);
    CpuDeadLoop ();
  }

  Status = mIoMmuProtocol->FreeBuffer (
                            mIoMmuProtocol,
                            NumPages,
                            Buffer
                          );
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_VERBOSE, "QemuFwCfgSevDma free buffer 0x%Lx Pages %d\n", (UINTN)Buffer, NumPages));
}
