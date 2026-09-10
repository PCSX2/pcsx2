; SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
; SPDX-License-Identifier: GPL-3.0+

; -----------------------------------------
; ARM64
; -----------------------------------------
  AREA |.text|, CODE, ALIGN=4

  EXPORT fastjmp_set
  EXPORT fastjmp_jmp

; void fastjmp_set(fastjmp_buf*)
  ALIGN 16
fastjmp_set    PROC
  mov x16, sp
  stp x16, x30, [x0]
  stp x19, x20, [x0, #16]
  stp x21, x22, [x0, #32]
  stp x23, x24, [x0, #48]
  stp x25, x26, [x0, #64]
  stp x27, x28, [x0, #80]
  str x29, [x0, #96]
  stp d8, d9, [x0, #112]
  stp d10, d11, [x0, #128]
  stp d12, d13, [x0, #144]
  stp d14, d15, [x0, #160]
  mov w0, wzr
  br x30
fastjmp_set    ENDP

; void fastjmp_jmp(fastjmp_buf*, int)
  ALIGN 16
fastjmp_jmp    PROC
  ldp x16, x30, [x0]
  mov sp, x16
  ldp x19, x20, [x0, #16]
  ldp x21, x22, [x0, #32]
  ldp x23, x24, [x0, #48]
  ldp x25, x26, [x0, #64]
  ldp x27, x28, [x0, #80]
  ldr x29, [x0, #96]
  ldp d8, d9, [x0, #112]
  ldp d10, d11, [x0, #128]
  ldp d12, d13, [x0, #144]
  ldp d14, d15, [x0, #160]
  mov w0, w1
  br x30
fastjmp_jmp    ENDP

  END
