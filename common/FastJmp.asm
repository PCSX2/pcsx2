; PCSX2 - PS2 Emulator for PCs
; Copyright (C) 2002-2021  PCSX2 Dev Team
;
; PCSX2 is free software: you can redistribute it and/or modify it under the terms
; of the GNU Lesser General Public License as published by the Free Software Found-
; ation, either version 3 of the License, or (at your option) any later version.
;
; PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
; PURPOSE.  See the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along with PCSX2.
; If not, see <http://www.gnu.org/licenses/>.

IFDEF _M_X86_32

; -----------------------------------------
; 32-bit X86
; -----------------------------------------
  .386
  .model flat

_TEXT         SEGMENT

PUBLIC @fastjmp_set@4
PUBLIC @fastjmp_jmp@8

; void fastjmp_set(fastjmp_buf*)
@fastjmp_set@4   PROC
  mov eax, dword ptr [esp]
  mov edx, esp                              ; fixup stack pointer, so it doesn't include the call to fastjmp_set
  add edx, 4
  mov dword ptr [ecx], eax                  ; actually eip
  mov dword ptr [ecx + 4], ebx
  mov dword ptr [ecx + 8], edx              ; actually esp
  mov dword ptr [ecx + 12], ebp
  mov dword ptr [ecx + 16], esi
  mov dword ptr [ecx + 20], edi
  xor eax, eax
  ret
@fastjmp_set@4   ENDP

; void __fastcall fastjmp_jmp(fastjmp_buf*, int)
@fastjmp_jmp@8   PROC
  mov eax, edx                              ; return code
  mov edx, dword ptr [ecx + 0]
  mov ebx, dword ptr [ecx + 4]
  mov esp, dword ptr [ecx + 8]
  mov ebp, dword ptr [ecx + 12]
  mov esi, dword ptr [ecx + 16]
  mov edi, dword ptr [ecx + 20]
  jmp edx
@fastjmp_jmp@8   ENDP

_TEXT         ENDS

ENDIF      ; _M_X86_32

IFDEF _M_X86_64
; -----------------------------------------
; 64-bit X86
; -----------------------------------------
_TEXT         SEGMENT

PUBLIC fastjmp_set
PUBLIC fastjmp_jmp

; void fastjmp_set(fastjmp_buf*)
fastjmp_set   PROC
  mov rax, qword ptr [rsp]
  mov rdx, rsp                              ; fixup stack pointer, so it doesn't include the call to fastjmp_set
  add rdx, 8
  mov qword ptr [rcx], rax                  ; actually rip
  mov qword ptr [rcx + 8], rbx
  mov qword ptr [rcx + 16], rdx             ; actually rsp
  mov qword ptr [rcx + 24], rbp
  mov qword ptr [rcx + 32], rsi
  mov qword ptr [rcx + 40], rdi
  mov qword ptr [rcx + 48], r12
  mov qword ptr [rcx + 56], r13
  mov qword ptr [rcx + 64], r14
  mov qword ptr [rcx + 72], r15
  movaps xmmword ptr [rcx + 80], xmm6
  movaps xmmword ptr [rcx + 96], xmm7
  movaps xmmword ptr [rcx + 112], xmm8
  add rcx, 112                              ; split to two batches to fit displacement in a single byte
  movaps xmmword ptr [rcx + 16], xmm9
  movaps xmmword ptr [rcx + 32], xmm10
  movaps xmmword ptr [rcx + 48], xmm11
  movaps xmmword ptr [rcx + 64], xmm12
  movaps xmmword ptr [rcx + 80], xmm13
  movaps xmmword ptr [rcx + 96], xmm14
  movaps xmmword ptr [rcx + 112], xmm15
  xor eax, eax
  ret
fastjmp_set   ENDP

; void fastjmp_jmp(fastjmp_buf*, int)
fastjmp_jmp   PROC
  mov eax, edx                              ; return code
  mov rdx, qword ptr [rcx + 0]              ; actually rip
  mov rbx, qword ptr [rcx + 8]
  mov rsp, qword ptr [rcx + 16]
  mov rbp, qword ptr [rcx + 24]
  mov rsi, qword ptr [rcx + 32]
  mov rdi, qword ptr [rcx + 40]
  mov r12, qword ptr [rcx + 48]
  mov r13, qword ptr [rcx + 56]
  mov r14, qword ptr [rcx + 64]
  mov r15, qword ptr [rcx + 72]
  movaps xmm6, xmmword ptr [rcx + 80]
  movaps xmm7, xmmword ptr [rcx + 96]
  movaps xmm8, xmmword ptr [rcx + 112]
  add rcx, 112                              ; split to two batches to fit displacement in a single byte
  movaps xmm9, xmmword ptr [rcx + 16]
  movaps xmm10, xmmword ptr [rcx + 32]
  movaps xmm11, xmmword ptr [rcx + 48]
  movaps xmm12, xmmword ptr [rcx + 64]
  movaps xmm13, xmmword ptr [rcx + 80]
  movaps xmm14, xmmword ptr [rcx + 96]
  movaps xmm15, xmmword ptr [rcx + 112]
  jmp rdx
fastjmp_jmp   ENDP

_TEXT         ENDS

ENDIF     ; _M_X86_64

END
