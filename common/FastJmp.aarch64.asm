; SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
; SPDX-License-Identifier: GPL-3.0+

; -----------------------------------------
; 64-bit ARM (AArch64) - Microsoft armasm64
; -----------------------------------------
; MSVC (unlike clang-cl) does not accept GNU inline asm, so on Windows the
; fastjmp routines are assembled from this file instead of common/FastJmp.cpp.
; The register set and buffer offsets mirror the inline AArch64 implementation
; in FastJmp.cpp. Windows AArch64 follows AAPCS64, so the same callee-saved
; registers (x19-x28, x29, x30, sp, d8-d15) must be preserved.

    AREA |.text|, CODE, READONLY

    EXPORT fastjmp_set
    EXPORT fastjmp_jmp

; int fastjmp_set(fastjmp_buf* buf)   ; buf in x0
fastjmp_set PROC
    mov     x16, sp
    stp     x16, x30, [x0]
    stp     x19, x20, [x0, #16]
    stp     x21, x22, [x0, #32]
    stp     x23, x24, [x0, #48]
    stp     x25, x26, [x0, #64]
    stp     x27, x28, [x0, #80]
    str     x29, [x0, #96]
    stp     d8, d9, [x0, #112]
    stp     d10, d11, [x0, #128]
    stp     d12, d13, [x0, #144]
    stp     d14, d15, [x0, #160]
    mov     w0, wzr
    ret
    ENDP

; void fastjmp_jmp(const fastjmp_buf* buf, int ret)   ; buf in x0, ret in w1
fastjmp_jmp PROC
    ldp     x16, x30, [x0]
    mov     sp, x16
    ldp     x19, x20, [x0, #16]
    ldp     x21, x22, [x0, #32]
    ldp     x23, x24, [x0, #48]
    ldp     x25, x26, [x0, #64]
    ldp     x27, x28, [x0, #80]
    ldr     x29, [x0, #96]
    ldp     d8, d9, [x0, #112]
    ldp     d10, d11, [x0, #128]
    ldp     d12, d13, [x0, #144]
    ldp     d14, d15, [x0, #160]
    mov     w0, w1
    ret
    ENDP

    END
