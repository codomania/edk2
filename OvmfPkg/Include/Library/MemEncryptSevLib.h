/** @file

  Define Secure Encrypted Virtualization (SEV) base library helper function

  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __MEM_ENCRYPT_SEV_LIB_H_
#define __MEM_ENCRYPT_SEV_LIB_H_

#include <Base.h>

/**
  Returns a boolean to indicate whether SEV is enabled

  @retval TRUE           When SEV is active
  @retval FALSE          When SEV is not enabled
  **/
BOOLEAN
EFIAPI
MemEncryptSevIsEnabled (
  VOID
  );

/**
  This function clears memory encryption bit for the memory region specified by BaseAddress and
  Number of pages from the current page table context.

  @param[in]  BaseAddress           The physical address that is the start address of a memory region.
  @param[in]  NumberOfPages         The number of pages from start memory region.

  @retval RETURN_SUCCESS            The attributes were cleared for the memory region.
  @retval RETURN_INVALID_PARAMETER  Number of pages is zero.
  @retval RETURN_UNSUPPORTED        Clearing memory encryption attribute is not supported
  **/
RETURN_STATUS
EFIAPI
MemEncryptSevClearPageEncMask (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINT32                   NumberOfPages
  );

/**
  This function sets memory encryption bit for the memory region specified by BaseAddress and
  Number of pages from the current page table context.

  @param[in]  BaseAddress           The physical address that is the start address of a memory region.
  @param[in]  NumberOfPages         The number of pages from start memory region.

  @retval RETURN_SUCCESS            The attributes were set for the memory region.
  @retval RETURN_INVALID_PARAMETER  Number of pages is zero.
  @retval RETURN_UNSUPPORTED        Clearing memory encryption attribute is not supported
  **/
RETURN_STATUS
EFIAPI
MemEncryptSevSetPageEncMask (
  IN PHYSICAL_ADDRESS         BaseAddress,
  IN UINT32                   NumberOfPages
  );
#endif // __MEM_ENCRYPT_SEV_LIB_H_
