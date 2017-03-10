;------------------------------------------------------------------------------
;
; Copyright (c) 2006 - 2012, Intel Corporation. All rights reserved.<BR>
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

    .586P
    .model  flat,C
    .code

;------------------------------------------------------------------------------
; Check whether we need to unroll the String I/O under SEV guest
;
; Return // eax   (1 - unroll, 0 - no unroll)
;------------------------------------------------------------------------------
SevNoRepIo PROC

  ; CPUID clobbers ebx, ecx and edx
  push      ebx
  push      ecx
  push      edx

  ; Check if we are running under hypervisor
  ; CPUID(1).ECX Bit 31
  mov       eax, 1
  cpuid
  bt        ecx, 31
  jnc       @UseRepIo

  ; Check if we have Memory encryption CPUID leaf
  mov       eax, 0x80000000
  cpuid
  cmp       eax, 0x8000001f
  jl        @UseRepIo

  ; Check for memory encryption feature:
  ;  CPUID  Fn8000_001F[EAX] - Bit 1
  ;
  mov       eax,  0x8000001f
  cpuid
  bt        eax, 1
  jnc       @UseRepIo

  ; Check if memory encryption is enabled
  ;  MSR_0xC0010131 - Bit 0 (SEV enabled)
  ;  MSR_0xC0010131 - Bit 1 (SEV-ES enabled)
  mov       ecx, 0xc0010131
  rdmsr

  ; Check for (SevEsEnabled == 0 && SevEnabled == 1)
  and       eax, 3
  cmp       eax, 1
  je        @SevNoRepIo_Done

@UseRepIo:
  xor       eax, eax

@SevNoRepIo_Done:
  pop       edx
  pop       ecx
  pop       ebx
  ret
SevNoRepIo ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo8 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo8 PROC
    push    edi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]

    call    SevNoRepIo            ; Check if we need to unroll the rep
    test    eax, eax
    jnz     @IoReadFifo8_NoRep

    cld
    rep     insb
    jmp     @IoReadFifo8_Done

@IoReadFifo8_NoRep:
    jecxz   @IoReadFifo8_Done

@IoReadFifo8_Loop:
    in      al, dx
    mov     byte [edi], al
    inc     edi
    loop    @IoReadFifo8_Loop

@IoReadFifo8_Done:
    pop     edi
    ret
IoReadFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo16 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo16 PROC
    push    edi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]

    call    SevNoRepIo            ; Check if we need to unroll the rep
    test    eax, eax
    jnz     @IoReadFifo16_NoRep

    cld
    rep     insw
    jmp     @IoReadFifo16_Done

@IoReadFifo16_NoRep:
    jecxz   @IoReadFifo16_Done

@IoReadFifo16_Loop:
    in      ax, dx
    mov     word [edi], ax
    add     edi, 2
    loop    @IoReadFifo16_Loop

@IoReadFifo16_Done:
    pop     edi
    ret
IoReadFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoReadFifo32 (
;    IN  UINTN                 Port,
;    IN  UINTN                 Size,
;    OUT VOID                  *Buffer
;    );
;------------------------------------------------------------------------------
IoReadFifo32 PROC
    push    edi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     edi, [esp + 16]

    call    SevNoRepIo            ; Check if we need to unroll the rep
    test    eax, eax
    jnz     @IoReadFifo32_NoRep

    cld
    rep     insd
    jmp     @IoReadFifo32_Done

@IoReadFifo32_NoRep:
    jecxz   @IoReadFifo32_Done

@IoReadFifo32_Loop:
    in      eax, dx
    mov     dword [edi], eax
    add     edi, 4
    loop    @IoReadFifo32_Loop

@IoReadFifo32_Done:
    pop     edi
    ret
IoReadFifo32 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo8 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo8 PROC
    push    esi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]

    call    SevNoRepIo     	; Check if we need to unroll String I/O
    test    eax, eax
    jnz     @IoWriteFifo8_NoRep

    cld
    rep     outsb
    jmp     @IoWriteFifo8_Done

@IoWriteFifo8_NoRep:
    jecxz   @IoWriteFifo8_Done

@IoWriteFifo8_Loop:
    mov     byte [esi], al
    out     dx, al
    inc     esi
    loop    @IoWriteFifo8_Loop

@IoWriteFifo8_Done:
    pop     esi
    ret
IoWriteFifo8 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo16 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo16 PROC
    push    esi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]

    call    SevNoRepIo     	; Check if we need to unroll String I/O
    test    eax, eax
    jnz     @IoWriteFifo16_NoRep

    cld
    rep     outsw
    jmp     @IoWriteFifo16_Done

@IoWriteFifo16_NoRep:
    jecxz   @IoWriteFifo16_Done

@IoWriteFifo16_Loop:
    mov     word [esi], ax
    out     dx, ax
    add     esi, 2
    loop    @IoWriteFifo16_Loop

@IoWriteFifo16_Done:
    pop     esi
    ret
IoWriteFifo16 ENDP

;------------------------------------------------------------------------------
;  VOID
;  EFIAPI
;  IoWriteFifo32 (
;    IN UINTN                  Port,
;    IN UINTN                  Size,
;    IN VOID                   *Buffer
;    );
;------------------------------------------------------------------------------
IoWriteFifo32 PROC
    push    esi
    mov     dx, [esp + 8]
    mov     ecx, [esp + 12]
    mov     esi, [esp + 16]

    call    SevNoRepIo     	; Check if we need to unroll String I/O
    test    eax, eax
    jnz     @IoWriteFifo32_NoRep

    cld
    rep     outsd
    jmp     @IoWriteFifo32_Done

@IoWriteFifo32_NoRep:
    jecxz   @IoWriteFifo32_Done

@IoWriteFifo32_Loop:
    mov     dword [esi], eax
    out     dx, eax
    add     esi, 4
    loop    @IoWriteFifo32_Loop

@IoWriteFifo32_Done:
    pop     esi
    ret
IoWriteFifo32 ENDP

    END

