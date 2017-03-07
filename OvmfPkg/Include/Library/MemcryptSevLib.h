/** @file
  Copyright (C) 2017 Advanced Micro Devices.

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __MEMCRYPT_SEV_LIB_H__
#define __MEMCRYPT_SEV_LIB_H__

#include <Uefi/UefiBaseType.h>
#include <Base.h>

/**
 
 Initialize SEV memory encryption

**/

RETURN_STATUS
EFIAPI
MemcryptSevInitialize (
  VOID
  );

/**
 
 Return TRUE when SEV is active otherwise FALSE

 **/
BOOLEAN
EFIAPI
SevActive (
  VOID
  );

#endif
