/** @file

  Implements routines to clear C-bit from MMIO Memory Range

  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __AMDSEVMMIO_H_
#define __AMDSEVMMIO_H

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemEncryptSevLib.h>

/**

  Iterate through the GCD map and clear the C-bit from MMIO and NonExistent
  memory space. The NonExistent memory space will be used for mapping the MMIO
  space added later (eg PciRootBridge). By clearing both known NonExistent
  memory space can gurantee that any MMIO mapped later will have C-bit cleared.
*/
VOID
EFIAPI
AmdSevClearEncMaskMmioRange (
  VOID
  );

#endif
