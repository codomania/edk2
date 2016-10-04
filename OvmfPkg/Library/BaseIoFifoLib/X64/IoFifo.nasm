;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
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

%define KVM_FEATURE_SEV         0x100

    DEFAULT REL
    SECTION .text

;------------------------------------------------------------------------------
;
; Check if Secure Encrypted Virtualization (SEV) feature is enabled
;
;------------------------------------------------------------------------------
SevActive:
    push    rax
    push    rbx
    push    rcx
    push    rdx
    xor     eax, eax
    mov     eax, 0x40000001
    cpuid
    test    eax, KVM_FEATURE_SEV
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    ret

;------------------------------------------------------------------------------
;  If SEV is enabled then unroll the String I/O instructions
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
SevIoReadFifo8:
    call    SevActive           ; Check if SEV is enabled
    jz     .1
.0:
    cmp     ecx, 0
    jz      .1
    in      al, dx
    mov     [edi], al
    dec     ecx
    inc     edi
    jmp     .0
.1:
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo8)
ASM_PFX(IoReadFifo8):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
    call    SevIoReadFifo8
    cmp     ecx, 0
    jz      .2
rep insb
.2:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  If SEV is enabled then unroll the String I/O instructions
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
SevIoReadFifo16:
    call    SevActive           ; Check if SEV is enabled
    jz     .4
.3: 
    cmp     ecx, 0
    jz      .4
    in      ax, dx
    mov     [edi], ax
    dec     ecx
    add     edi, 2
    jmp     .3
.4: 
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo16)
ASM_PFX(IoReadFifo16):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
    call    SevIoReadFifo16
    cmp     ecx, 0
    jz      .5
rep insw
.5:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  If SEV is enabled then unroll the String I/O instructions
;
;  Port    // dx
;  Size    // ecx
;  Buffer  // rdi
;
;------------------------------------------------------------------------------
SevIoReadFifo32:
    call    SevActive           ; Check if SEV is enabled
    jz     .6
.7: 
    cmp     ecx, 0
    jz      .6
    in      ax, dx
    mov     [edi], ax
    add     edi, 2
    in      ax, dx
    mov     [edi], ax
    dec     ecx
    add     edi, 2
    jmp     .7
.6: 
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoReadFifo32)
ASM_PFX(IoReadFifo32):
    cld
    xchg    rcx, rdx
    xchg    rdi, r8             ; rdi: buffer address; r8: save rdi
    call    SevIoReadFifo32
    cmp     ecx, 0
    jz      .8
rep insd
.8:
    mov     rdi, r8             ; restore rdi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo8)
ASM_PFX(IoWriteFifo8):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsb
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo16)
ASM_PFX(IoWriteFifo16):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsw
    mov     rsi, r8             ; restore rsi
    ret

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,              // rcx
;    IN UINTN                  Size,              // rdx
;    IN VOID                   *Buffer            // r8
;    );
;------------------------------------------------------------------------------
global ASM_PFX(IoWriteFifo32)
ASM_PFX(IoWriteFifo32):
    cld
    xchg    rcx, rdx
    xchg    rsi, r8             ; rsi: buffer address; r8: save rsi
rep outsd
    mov     rsi, r8             ; restore rsi
    ret

