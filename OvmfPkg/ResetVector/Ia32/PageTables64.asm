;------------------------------------------------------------------------------
; @file
; Sets the CR3 register for 64-bit paging
;
; Copyright (c) 2008 - 2013, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

BITS    32

%define PAGE_PRESENT            0x01
%define PAGE_READ_WRITE         0x02
%define PAGE_USER_SUPERVISOR    0x04
%define PAGE_WRITE_THROUGH      0x08
%define PAGE_CACHE_DISABLE     0x010
%define PAGE_ACCESSED          0x020
%define PAGE_DIRTY             0x040
%define PAGE_PAT               0x080
%define PAGE_GLOBAL           0x0100
%define PAGE_2M_MBO            0x080
%define PAGE_2M_PAT          0x01000
%define KVM_FEATURE_SEV         0x08

%define PAGE_2M_PDE_ATTR (PAGE_2M_MBO + \
                          PAGE_ACCESSED + \
                          PAGE_DIRTY + \
                          PAGE_READ_WRITE + \
                          PAGE_PRESENT)

%define PAGE_PDP_ATTR (PAGE_ACCESSED + \
                       PAGE_READ_WRITE + \
                       PAGE_PRESENT)


;
; Check if Secure Encrypted Virtualization (SEV) feature
; is enabled in KVM
;
;  If SEV is enabled, then EAX will contain Memory encryption bit position
;
CheckKVMSEVFeature:
    xor       eax, eax

    ; Check if SEV is enabled
    mov       eax, 0x40000001
    cpuid
    bt        eax, KVM_FEATURE_SEV
    jnc       NoSev

    ; Check for memory encryption feature:
    ;  CPUID  Fn8000_001F[EAX] - Bit 0
    ;
    mov       eax,  0x8000001f
    cpuid
    bt        eax, 0
    jnc       NoSev

    ; Get memory encryption information
    ; CPUID Fn8000_001F[EBX] - Bits 5:0
    ;
    mov       eax, ebx
    and       eax, 0x3f
    jmp       SevExit

NoSev:
    xor       eax, eax

SevExit:
    OneTimeCallRet CheckKVMSEVFeature

;
; Modified:  EAX, ECX
;
SetCr3ForPageTables64:

    ;
    ; For OVMF, build some initial page tables at 0x800000-0x806000.
    ;
    ; This range should match with PcdOvmfSecPageTablesBase and
    ; PcdOvmfSecPageTablesSize which are declared in the FDF files.
    ;
    ; At the end of PEI, the pages tables will be rebuilt into a
    ; more permanent location by DxeIpl.
    ;

    mov     ecx, 6 * 0x1000 / 4
    xor     eax, eax
clearPageTablesMemoryLoop:
    mov     dword[ecx * 4 + 0x800000 - 4], eax
    loop    clearPageTablesMemoryLoop

    ;
    ; Check if it SEV-enabled Guest
    ;
    OneTimeCall   CheckKVMSEVFeature
    xor     edx, edx
    test    eax, eax
    jz      SevNotActive

    ; If SEV is enabled, Memory encryption bit is always above 31
    mov     ebx, 32
    sub     ebx, eax
    bts     edx, eax

SevNotActive:

    ;
    ; Top level Page Directory Pointers (1 * 512GB entry)
    ;
    mov     dword[0x800000], 0x801000 + PAGE_PDP_ATTR
    mov     dword[0x800004], edx

    ;
    ; Next level Page Directory Pointers (4 * 1GB entries => 4GB)
    ;
    mov     dword[0x801000], 0x802000 + PAGE_PDP_ATTR
    mov     dword[0x801004], edx
    mov     dword[0x801008], 0x803000 + PAGE_PDP_ATTR
    mov     dword[0x80100C], edx
    mov     dword[0x801010], 0x804000 + PAGE_PDP_ATTR
    mov     dword[0x801014], edx
    mov     dword[0x801018], 0x805000 + PAGE_PDP_ATTR
    mov     dword[0x80101C], edx

    ;
    ; Page Table Entries (2048 * 2MB entries => 4GB)
    ;
    mov     ecx, 0x800
pageTableEntriesLoop:
    mov     eax, ecx
    dec     eax
    shl     eax, 21
    add     eax, PAGE_2M_PDE_ATTR
    mov     [ecx * 8 + 0x802000 - 8], eax
    mov     [(ecx * 8 + 0x802000 - 8) + 4], edx
    loop    pageTableEntriesLoop

    ;
    ; Set CR3 now that the paging structures are available
    ;
    mov     eax, 0x800000
    mov     cr3, eax

    OneTimeCallRet SetCr3ForPageTables64
