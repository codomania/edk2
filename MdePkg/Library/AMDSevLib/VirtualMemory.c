#include "VirtualMemory.h"

#define KVM_FEATURE_MEMORY_ENCRYPTION 0x100

STATIC UINT64 MemEncryptMask;

/**
  Split 2M page to 4K.

  @param[in]      PhysicalAddress       Start physical address the 2M page covered.
  @param[in, out] PageEntry2M           Pointer to 2M page entry.
  @param[in]      StackBase             Stack base address.
  @param[in]      StackSize             Stack size.

**/
STATIC
VOID
Split2MPageTo4K (
  IN EFI_PHYSICAL_ADDRESS               PhysicalAddress,
  IN OUT UINT64                         *PageEntry2M,
  IN EFI_PHYSICAL_ADDRESS               StackBase,
  IN UINTN                              StackSize
  )
{
  EFI_PHYSICAL_ADDRESS                  PhysicalAddress4K;
  UINTN                                 IndexOfPageTableEntries;
  PAGE_TABLE_4K_ENTRY                   *PageTableEntry, *PageTableEntry1;

  PageTableEntry = AllocatePages(1);
  
  PageTableEntry1 = PageTableEntry;

  ASSERT (PageTableEntry != NULL);
  ASSERT (*PageEntry2M & MemEncryptMask);

  PhysicalAddress4K = PhysicalAddress;
  for (IndexOfPageTableEntries = 0; IndexOfPageTableEntries < 512; IndexOfPageTableEntries++, PageTableEntry++, PhysicalAddress4K += SIZE_4KB) {
    //
    // Fill in the Page Table entries
    //
    PageTableEntry->Uint64 = (UINT64) PhysicalAddress4K | MemEncryptMask;
    PageTableEntry->Bits.ReadWrite = 1;
    PageTableEntry->Bits.Present = 1;
    if ((PhysicalAddress4K >= StackBase) && (PhysicalAddress4K < StackBase + StackSize)) {
      //
      // Set Nx bit for stack.
      //
      PageTableEntry->Bits.Nx = 1;
    }
  }

  //
  // Fill in 2M page entry.
  //
  *PageEntry2M = (UINT64) (UINTN) PageTableEntry1 | IA32_PG_P | IA32_PG_RW | MemEncryptMask;
}

/**
  Split 1G page to 2M.

  @param[in]      PhysicalAddress       Start physical address the 1G page covered.
  @param[in, out] PageEntry1G           Pointer to 1G page entry.
  @param[in]      StackBase             Stack base address.
  @param[in]      StackSize             Stack size.

**/
STATIC
VOID
Split1GPageTo2M (
  IN EFI_PHYSICAL_ADDRESS               PhysicalAddress,
  IN OUT UINT64                         *PageEntry1G,
  IN EFI_PHYSICAL_ADDRESS               StackBase,
  IN UINTN                              StackSize
  )
{
  EFI_PHYSICAL_ADDRESS                  PhysicalAddress2M;
  UINTN                                 IndexOfPageDirectoryEntries;
  PAGE_TABLE_ENTRY                      *PageDirectoryEntry;

  PageDirectoryEntry = AllocatePages(1);

  ASSERT (PageDirectoryEntry != NULL);
  ASSERT (*PageEntry1G & MemEncryptMask);
  //
  // Fill in 1G page entry.
  //
  *PageEntry1G = (UINT64) (UINTN) PageDirectoryEntry | IA32_PG_P | IA32_PG_RW | MemEncryptMask;

  PhysicalAddress2M = PhysicalAddress;
  for (IndexOfPageDirectoryEntries = 0; IndexOfPageDirectoryEntries < 512; IndexOfPageDirectoryEntries++, PageDirectoryEntry++, PhysicalAddress2M += SIZE_2MB) {
    if ((PhysicalAddress2M < StackBase + StackSize) && ((PhysicalAddress2M + SIZE_2MB) > StackBase)) {
      //
      // Need to split this 2M page that covers stack range.
      //
      Split2MPageTo4K (PhysicalAddress2M, (UINT64 *) PageDirectoryEntry, StackBase, StackSize);
    } else {
      //
      // Fill in the Page Directory entries
      //
      PageDirectoryEntry->Uint64 = (UINT64) PhysicalAddress2M | MemEncryptMask;
      PageDirectoryEntry->Bits.ReadWrite = 1;
      PageDirectoryEntry->Bits.Present = 1;
      PageDirectoryEntry->Bits.MustBe1 = 1;
    }
  }
}


STATIC VOID
SetOrClearCBit(
  IN UINT64*  PageTablePointer,
  IN MAP_RANGE_MODE Mode
  )
{
  if (Mode == SetCBit)
    *PageTablePointer |= MemEncryptMask;
  else
    *PageTablePointer &= ~MemEncryptMask;
}

STATIC UINT64
GetCBitMask (
  VOID
  )
{
  UINT32 EAX, EBX;
  UINT64 Mask = 0;

   // Check whether SEV is enabled
   // CPUID Fn8000_001f[EAX] - Bit 0  (SEV is enabled)
   // CPUID Fn8000_001f[EBX] - Bit 5:0 (memory encryption bit position)
   AsmCpuid(0x8000001f, &EAX, &EBX, NULL, NULL);
   if (EAX & 1) {
     Mask = (1UL << (EBX & 0x3f));
   }

   return Mask;
}

EFI_STATUS
EFIAPI
SevMapMemoryRange(
  IN EFI_PHYSICAL_ADDRESS     PhysicalAddress,
  IN UINT64                   Length,
  IN MAP_RANGE_MODE           Mode,
  IN BOOLEAN                  FlushCache 
  )
{

  UINT64 Cr3 = AsmReadCr3();

  PAGE_MAP_AND_DIRECTORY_POINTER *PageMapLevel4Entry;
  PAGE_MAP_AND_DIRECTORY_POINTER *PageUpperDirectoryPointerEntry;
  PAGE_MAP_AND_DIRECTORY_POINTER *PageDirectoryPointerEntry;
  PAGE_TABLE_1G_ENTRY            *PageDirectory1GEntry;
  PAGE_TABLE_ENTRY               *PageDirectory2MEntry;
  PAGE_TABLE_4K_ENTRY            *PageTableEntry;
  UINT64                         PgTableMask;

  MemEncryptMask = GetCBitMask ();

  //
  // If memory encryption is not enabled then do nothing
  //
  if (!MemEncryptMask) {
    return EFI_SUCCESS;
  }

  PgTableMask = MemEncryptMask | EFI_PAGE_MASK;

  if (Mode == ClearCBit) {
    DEBUG ((DEBUG_INFO, "MapRangeAs Unencrypted = 0x%Lx+0x%Lx\n", PhysicalAddress, Length));
  } else {
    DEBUG ((DEBUG_INFO, "MapRangeAs Encrypted = 0x%Lx+0x%x\n", PhysicalAddress, Length));
  }

  if (FlushCache) {
    WriteBackInvalidateDataCacheRange((VOID*) PhysicalAddress, Length);
  }

  if (Length & EFI_PAGE_MASK) {
    DEBUG((DEBUG_WARN, " ****  Bad Length %lx\n", Length));
    return EFI_INVALID_PARAMETER;
  }

  while (Length)
  {

    PageMapLevel4Entry = (VOID*) (AsmReadCr3() & ~PgTableMask);
    PageMapLevel4Entry += PML4_OFFSET(PhysicalAddress);
    if (!PageMapLevel4Entry->Bits.Present) {
      DEBUG((DEBUG_WARN, "ERROR bad PML4 for %lx\n", PhysicalAddress));
      return EFI_NO_MAPPING;
    }

    PageDirectory1GEntry = (VOID*) (PageMapLevel4Entry->Bits.PageTableBaseAddress<<12 & ~PgTableMask);
    PageDirectory1GEntry += PDP_OFFSET(PhysicalAddress);
    if (!PageDirectory1GEntry->Bits.Present) {
       DEBUG((DEBUG_WARN, "ERROR bad PDPE for %lx\n", PhysicalAddress));
       return EFI_NO_MAPPING;
    }

    // If the MustBe1 bit is not 1, it's not actually a 1GB entry
    if (PageDirectory1GEntry->Bits.MustBe1) {
      // Valid 1GB page
      // If we have at least 1GB to go, we can just update this entry
      if (!(PhysicalAddress & ((1<<30) - 1)) && Length >= (1<<30)) {
        SetOrClearCBit(&PageDirectory1GEntry->Uint64, Mode);
        DEBUG((DEBUG_VERBOSE, "Updated 1GB entry for %lx\n", PhysicalAddress));
        PhysicalAddress += 1<<30;
        Length -= 1<<30;
      } else {
        // We must split the page
        DEBUG((DEBUG_VERBOSE, "Spliting 1GB page\n"));
        Split1GPageTo2M(((UINT64)PageDirectory1GEntry->Bits.PageTableBaseAddress)<<30, (UINT64*) PageDirectory1GEntry, 0, 0);
        continue;
      }
    } else {
      // Actually a PDP
      PageUpperDirectoryPointerEntry = (PAGE_MAP_AND_DIRECTORY_POINTER*) PageDirectory1GEntry;
      PageDirectory2MEntry = (VOID*) (PageUpperDirectoryPointerEntry->Bits.PageTableBaseAddress<<12 & ~PgTableMask);
      PageDirectory2MEntry += PDE_OFFSET(PhysicalAddress);
      if (!PageDirectory2MEntry->Bits.Present) {
        DEBUG((DEBUG_WARN, "ERROR bad PDE for %lx\n", PhysicalAddress));
        return EFI_NO_MAPPING;
      }
      // If the MustBe1 bit is not a 1, it's not a 2MB entry
      if (PageDirectory2MEntry->Bits.MustBe1) {
        // Valid 2MB page
        // If we have at least 2MB left to go, we can just update this entry
        if (!(PhysicalAddress & ((1<<21)-1)) && Length >= (1<<21)) {
          SetOrClearCBit(&PageDirectory2MEntry->Uint64, Mode);
          DEBUG((DEBUG_VERBOSE, "Updated 2MB entry for %lx\n", PhysicalAddress));
          PhysicalAddress += 1<<21;
          Length -= 1<<21;
        } else {
          // We must split up this page into 4K pages
          DEBUG((DEBUG_VERBOSE, "Spliting 2MB page at %lx\n", PhysicalAddress));
          Split2MPageTo4K(((UINT64)PageDirectory2MEntry->Bits.PageTableBaseAddress) << 21, (UINT64*) PageDirectory2MEntry, 0, 0);
          continue;
        }
      } else {
        PageDirectoryPointerEntry = (PAGE_MAP_AND_DIRECTORY_POINTER*) PageDirectory2MEntry;
        PageTableEntry = (VOID*) (PageDirectoryPointerEntry->Bits.PageTableBaseAddress<<12 & ~PgTableMask);
        PageTableEntry += PTE_OFFSET(PhysicalAddress);
        if (!PageTableEntry->Bits.Present) {
          DEBUG((DEBUG_WARN, "ERROR bad PTE for %lx\n", PhysicalAddress));
          return EFI_NO_MAPPING;
        }
        SetOrClearCBit(&PageTableEntry->Uint64, Mode);
        DEBUG((DEBUG_VERBOSE, "Updated 4KB entry for %lx\n", PhysicalAddress));
        PhysicalAddress += EFI_PAGE_SIZE;
        Length -= EFI_PAGE_SIZE;
      }
    }
  }

  AsmWriteCr3(Cr3);
  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
SevEnabled (
  VOID
  )
{
  UINT32 KVMFeatures;

  // Check if KVM memory encyption feature is set
  AsmCpuid(0x40000001, &KVMFeatures, NULL, NULL, NULL);
  if (KVMFeatures & KVM_FEATURE_MEMORY_ENCRYPTION) {
    return TRUE;
  }

  return FALSE;
}

