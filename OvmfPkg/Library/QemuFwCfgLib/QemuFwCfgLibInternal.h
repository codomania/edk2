/** @file
  Internal interfaces specific to the QemuFwCfgLib instances in OvmfPkg.

  Copyright (C) 2016, Red Hat, Inc.
  Copyright (C) 2017, Advanced Micro Devices.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __QEMU_FW_CFG_LIB_INTERNAL_H__
#define __QEMU_FW_CFG_LIB_INTERNAL_H__

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
  );


/**
  Returns a boolean indicating whether QEMU provides the DMA-like access method
  for fw_cfg.

  @retval    TRUE   The DMA-like access method is available.
  @retval    FALSE  The DMA-like access method is unavailable.
**/
BOOLEAN
InternalQemuFwCfgDmaIsAvailable (
  VOID
  );

/**
 Returns a boolean indicating whether the SEV is enabled

 @retval    TRUE    SEV is enabled
 @retval    FALSE   SEV is not enabled
**/
BOOLEAN
InternalQemuFwCfgSevIsEnabled (
  VOID
  );

/**
 Allocate a bounce buffer for SEV DMA.

  @param[in]     NumPage  Number of pages.
  @param[out]    Buffer   Allocated DMA Buffer pointer

**/
VOID
InternalQemuFwCfgSevDmaAllocateBuffer (
  IN     UINT32   NumPages,
  OUT    VOID     **Buffer
  );

/**
 Free the DMA buffer allocated using InternalQemuFwCfgSevDmaAllocateBuffer

  @param[in]     NumPage  Number of pages.
  @param[in]     Buffer   DMA Buffer pointer

**/
VOID
InternalQemuFwCfgSevDmaFreeBuffer (
  IN     VOID     *Buffer,
  IN     UINT32   NumPages
  );

#endif
