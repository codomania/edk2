;------------------------------------------------------------------------------
;
; Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>
;
; This program and the accompanying materials are licensed and made available
; under the terms and conditions of the BSD License which accompanies this
; distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
;------------------------------------------------------------------------------

%define KVM_FEATURE_SEV         8

    SECTION .data
SevCheckedOnce    db            0
SevStatus         db            0

    SECTION .text

;------------------------------------------------------------------------------
; Check if Secure Encrypted Virtualization (SEV) feature
; is enabled in KVM
;
; Return // eax   (1 - active, 0 - not active)
;------------------------------------------------------------------------------
global ASM_PFX(CheckSevFeature)
ASM_PFX(CheckSevFeature):
  ; Check CPUID once, if its already checked then return SevStatus
  mov       eax, 1
  cmp       [SevCheckedOnce], eax
  jz        SevFeatureCheckExit

  ; Start the SEV feature check
  mov       [SevCheckedOnce], eax

  ; CPUID clobbers ebx, ecx and edx
  push      ebx
  push      ecx
  push      edx

  mov       eax, 0x40000001
  cpuid

  bt        eax, KVM_FEATURE_SEV
  jnc       SevCheckExit

  ; Check for memory encryption feature:
  ;  CPUID  Fn8000_001F[EAX] - Bit 0
  ;
  mov       eax,  0x8000001f
  cpuid
  bt        eax, 0
  jnc       SevCheckExit
  mov       eax, 1
  mov       [SevStatus], eax

SevCheckExit:
  pop       edx
  pop       ecx
  pop       ebx

SevFeatureCheckExit:
  mov       eax, [SevStatus]
  ret

;------------------------------------------------------------------------------
;  unroll 'rep ins' String I/O instructions when SEV is active
;  nothing
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo8)
ASM_PFX(SevIoReadFifo8):
  call      CheckSevFeature
  cmp       eax, 1
  jnz       ReadFifo8Exit
ReadFifo8Loop:
    cmp       ecx, 0
    jz        ReadFifo8Exit
    in        al, dx
    mov       [edi], al
    dec       ecx
    inc       edi
    jmp       ReadFifo8Loop
ReadFifo8Exit:
  ret

;------------------------------------------------------------------------------
;  unroll 'rep insw' String I/O instructions when SEV is active
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo16)
ASM_PFX(SevIoReadFifo16):
  call      CheckSevFeature
  cmp       eax, 1
  jnz       ReadFifo16Exit
ReadFifo16Loop:
    cmp       ecx, 0
    jz        ReadFifo16Exit
    in        ax, dx
    mov       [edi], ax
    dec       ecx
    add       edi, 2
    jmp       ReadFifo16Loop
ReadFifo16Exit:
  ret

;------------------------------------------------------------------------------
;  unroll 'rep insl' String I/O instructions when SEV is active
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
global ASM_PFX(SevIoReadFifo32)
ASM_PFX(SevIoReadFifo32):
  call      CheckSevFeature
  cmp       eax, 1
  jnz       ReadFifo32Exit
ReadFifo32Loop:
    cmp       ecx, 0
    jz        ReadFifo32Exit
    in        eax, dx
    mov       [edi], eax
    dec       ecx
    add       edi, 4
    jmp       ReadFifo32Loop
ReadFifo32Exit:
  ret

