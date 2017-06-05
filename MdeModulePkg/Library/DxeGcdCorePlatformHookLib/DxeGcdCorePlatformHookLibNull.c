/** @file
DxeCore Gcd Platform Hook NULL library instance.

Copyright (c) 2017, AMD Inc. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/DxeGcdCorePlatformHookLib.h>

/**
  This function is hook point called after the GCD memory map is initialized,
  it allows the platform to perform post processing before the GCD maps are
  made available to other drivers.
**/
VOID
EFIAPI
DxeGcdCorePlatformHookReady (
  EFI_DXE_SERVICES    *gDS
  )
{
}
