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

#include "MemEncryptSevLibInternal.h"

STATIC BOOLEAN mSevStatus = FALSE;
STATIC BOOLEAN mSevStatusChecked = FALSE;

/**

  Returns a boolean to indicate whether SEV is enabled

  @retval TRUE           SEV is enabled
  @retval FALSE          SEV is not enabled
  **/
BOOLEAN
EFIAPI
MemEncryptSevIsEnabled (
  VOID
  )
{
  if (mSevStatusChecked) {
    return mSevStatus;
  }

  mSevStatus = InternalMemEncryptSevIsEnabled();
  mSevStatusChecked = TRUE;

  return mSevStatus;
}
