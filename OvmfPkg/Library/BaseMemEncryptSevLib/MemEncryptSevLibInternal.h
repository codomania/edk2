/** @file

  Secure Encrypted Virtualization (SEV) library helper function

  Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD
  License which accompanies this distribution.  The full text of the license may
  be found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _MEM_ENCRYPT_SEV_LIB_INTERNAL_H_
#define _MEM_ENCRYPT_SEV_LIB_INTERNAL_H_

#include <Base.h>

/**
  Returns a boolean to indicate whether SEV is enabled

  @retval TRUE           SEV is active
  @retval FALSE          SEV is not enabled
  **/
BOOLEAN
EFIAPI
InternalMemEncryptSevIsEnabled (
  VOID
  );

#endif
