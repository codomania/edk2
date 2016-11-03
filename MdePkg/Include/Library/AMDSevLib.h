#ifndef __AMD_SEV_LIB__
#define __AMD_SEV_LIB__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

typedef enum {
   SetCBit,
   ClearCBit
} MAP_RANGE_MODE;

/**
 
  Check whether SEV is enabled

  */

BOOLEAN
EFIAPI
SevEnabled (
  VOID
  );


/**
 
  Map the buffer with either C-bit clear or C-bit set

  @param[out] PhysicalAddress   Start physical address
  @param[in]  Pages             Number of pages to allocate
  @param[in]  mode              Set or Clear C-bit
  @param[in]  flushCache        Flush the caches

  */

EFI_STATUS
EFIAPI
SevMapMemoryRange(
  IN EFI_PHYSICAL_ADDRESS     PhysicalAddress,
  IN UINT64                   Length,
  IN MAP_RANGE_MODE           Mode,
  IN BOOLEAN                  FlushCache 
  );

#endif

