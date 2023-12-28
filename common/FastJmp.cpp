// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "FastJmp.h"

// Win32 uses Fastjmp.asm, because MSVC doesn't support inline asm.
#if !defined(_WIN32) || defined(_M_ARM64)

#if defined(__APPLE__)
#define PREFIX "_"
#else
#define PREFIX ""
#endif

#if defined(_M_X86)

asm(
	"\t.global " PREFIX "fastjmp_set\n"
	"\t.global " PREFIX "fastjmp_jmp\n"
	"\t.text\n"
	"\t" PREFIX "fastjmp_set:" R"(
	movq 0(%rsp), %rax
	movq %rsp, %rdx			# fixup stack pointer, so it doesn't include the call to fastjmp_set
	addq $8, %rdx
	movq %rax, 0(%rdi)	# actually rip
	movq %rbx, 8(%rdi)
	movq %rdx, 16(%rdi)	# actually rsp
	movq %rbp, 24(%rdi)
	movq %r12, 32(%rdi)
	movq %r13, 40(%rdi)
	movq %r14, 48(%rdi)
	movq %r15, 56(%rdi)
	xorl %eax, %eax
	ret
)"
	"\t" PREFIX "fastjmp_jmp:" R"(
	movl %esi, %eax
	movq 0(%rdi), %rdx	# actually rip
	movq 8(%rdi), %rbx
	movq 16(%rdi), %rsp	# actually rsp
	movq 24(%rdi), %rbp
	movq 32(%rdi), %r12
	movq 40(%rdi), %r13
	movq 48(%rdi), %r14
	movq 56(%rdi), %r15
	jmp *%rdx
)");

#elif defined(_M_ARM64)

asm(
	"\t.global " PREFIX "fastjmp_set\n"
	"\t.global " PREFIX "fastjmp_jmp\n"
	"\t.text\n"
	"\t.align 16\n"
	"\t" PREFIX "fastjmp_set:" R"(
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
)"
".align 16\n"
"\t" PREFIX "fastjmp_jmp:" R"(
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
)");

#endif

#endif // __WIN32
