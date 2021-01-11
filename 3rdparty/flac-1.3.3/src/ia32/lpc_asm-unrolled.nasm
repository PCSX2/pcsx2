;  vim:filetype=nasm ts=8

;  libFLAC - Free Lossless Audio Codec library
;  Copyright (C) 2001-2009  Josh Coalson
;  Copyright (C) 2011-2016  Xiph.Org Foundation
;
;  Redistribution and use in source and binary forms, with or without
;  modification, are permitted provided that the following conditions
;  are met:
;
;  - Redistributions of source code must retain the above copyright
;  notice, this list of conditions and the following disclaimer.
;
;  - Redistributions in binary form must reproduce the above copyright
;  notice, this list of conditions and the following disclaimer in the
;  documentation and/or other materials provided with the distribution.
;
;  - Neither the name of the Xiph.org Foundation nor the names of its
;  contributors may be used to endorse or promote products derived from
;  this software without specific prior written permission.
;
;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
;  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
;  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
;  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
;  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
;  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
;  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; [CR] is a note to flag that the instruction can be easily reordered

%include "nasm.h"

	data_section

cglobal FLAC__lpc_compute_autocorrelation_asm

	code_section

; **********************************************************************
;
; void FLAC__lpc_compute_autocorrelation_asm(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[])
; {
;	FLAC__real d;
;	unsigned sample, coeff;
;	const unsigned limit = data_len - lag;
;
;	assert(lag > 0);
;	assert(lag <= data_len);
;
;	for(coeff = 0; coeff < lag; coeff++)
;		autoc[coeff] = 0.0;
;	for(sample = 0; sample <= limit; sample++){
;		d = data[sample];
;		for(coeff = 0; coeff < lag; coeff++)
;			autoc[coeff] += d * data[sample+coeff];
;	}
;	for(; sample < data_len; sample++){
;		d = data[sample];
;		for(coeff = 0; coeff < data_len - sample; coeff++)
;			autoc[coeff] += d * data[sample+coeff];
;	}
; }
;
FLAC__lpc_compute_autocorrelation_asm:

	push	ebp
	lea	ebp, [esp + 8]
	push	ebx
	push	esi
	push	edi

	mov	edx, [ebp + 8]			; edx == lag
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc

	cmp	edx, 1
	ja	short .lag_above_1
.lag_eq_1:
	fldz					; will accumulate autoc[0]
	ALIGN 16
.lag_1_loop:
	fld	dword [esi]
	add	esi, byte 4			; sample++
	fmul	st0, st0
	faddp	st1, st0
	dec	ecx
	jnz	.lag_1_loop
	fstp	dword [edi]
	jmp	.end

.lag_above_1:
	cmp	edx, 2
	ja	short .lag_above_2
.lag_eq_2:
	fldz					; will accumulate autoc[1]
	dec	ecx
	fldz					; will accumulate autoc[0]
	fld	dword [esi]
	ALIGN 16
.lag_2_loop:
	add	esi, byte 4			; [CR] sample++
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi]
	fmul	st1, st0
	fxch
	faddp	st3, st0			; add to autoc[1]
	dec	ecx
	jnz	.lag_2_loop
	; clean up the leftovers
	fmul	st0, st0
	faddp	st1, st0			; add to autoc[0]
	fstp	dword [edi]
	fstp	dword [edi + 4]
	jmp	.end

.lag_above_2:
	cmp	edx, 3
	ja	short .lag_above_3
.lag_eq_3:
	fldz					; will accumulate autoc[2]
	dec	ecx
	fldz					; will accumulate autoc[1]
	dec	ecx
	fldz					; will accumulate autoc[0]
	ALIGN 16
.lag_3_loop:
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[2]
	dec	ecx
	jnz	.lag_3_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st1, st0
	fxch
	faddp	st3, st0			; add to autoc[1]
	fmul	st0, st0
	faddp	st1, st0			; add to autoc[0]
	fstp	dword [edi]
	fstp	dword [edi + 4]
	fstp	dword [edi + 8]
	jmp	.end

.lag_above_3:
	cmp	edx, 4
	ja	near .lag_above_4
.lag_eq_4:
	fldz					; will accumulate autoc[3]
	dec	ecx
	fldz					; will accumulate autoc[2]
	dec	ecx
	fldz					; will accumulate autoc[1]
	dec	ecx
	fldz					; will accumulate autoc[0]
	ALIGN 16
.lag_4_loop:
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[3]
	dec	ecx
	jnz	.lag_4_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[2]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st1, st0
	fxch
	faddp	st3, st0			; add to autoc[1]
	fmul	st0, st0
	faddp	st1, st0			; add to autoc[0]
	fstp	dword [edi]
	fstp	dword [edi + 4]
	fstp	dword [edi + 8]
	fstp	dword [edi + 12]
	jmp	.end

.lag_above_4:
	cmp	edx, 5
	ja	near .lag_above_5
.lag_eq_5:
	fldz					; will accumulate autoc[4]
	fldz					; will accumulate autoc[3]
	fldz					; will accumulate autoc[2]
	fldz					; will accumulate autoc[1]
	fldz					; will accumulate autoc[0]
	sub	ecx, byte 4
	ALIGN 16
.lag_5_loop:
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[3]
	fld	dword [esi + 16]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st5, st0			; add to autoc[4]
	dec	ecx
	jnz	.lag_5_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[3]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[2]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st1, st0
	fxch
	faddp	st3, st0			; add to autoc[1]
	fmul	st0, st0
	faddp	st1, st0			; add to autoc[0]
	fstp	dword [edi]
	fstp	dword [edi + 4]
	fstp	dword [edi + 8]
	fstp	dword [edi + 12]
	fstp	dword [edi + 16]
	jmp	.end

.lag_above_5:
	cmp	edx, 6
	ja	.lag_above_6
.lag_eq_6:
	fldz					; will accumulate autoc[5]
	fldz					; will accumulate autoc[4]
	fldz					; will accumulate autoc[3]
	fldz					; will accumulate autoc[2]
	fldz					; will accumulate autoc[1]
	fldz					; will accumulate autoc[0]
	sub	ecx, byte 5
	ALIGN 16
.lag_6_loop:
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[3]
	fld	dword [esi + 16]
	fmul	st0, st1
	faddp	st6, st0			; add to autoc[4]
	fld	dword [esi + 20]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st6, st0			; add to autoc[5]
	dec	ecx
	jnz	.lag_6_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[3]
	fld	dword [esi + 16]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st5, st0			; add to autoc[4]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[2]
	fld	dword [esi + 12]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[3]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[1]
	fld	dword [esi + 8]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[2]
	fld	dword [esi]
	fld	st0
	fmul	st0, st0
	faddp	st2, st0			; add to autoc[0]
	fld	dword [esi + 4]
	fmul	st1, st0
	fxch
	faddp	st3, st0			; add to autoc[1]
	fmul	st0, st0
	faddp	st1, st0			; add to autoc[0]
	fstp	dword [edi]
	fstp	dword [edi + 4]
	fstp	dword [edi + 8]
	fstp	dword [edi + 12]
	fstp	dword [edi + 16]
	fstp	dword [edi + 20]
	jmp	.end

.lag_above_6:
	;	for(coeff = 0; coeff < lag; coeff++)
	;		autoc[coeff] = 0.0;
	lea	ecx, [edx * 2]			; ecx = # of dwords of 0 to write
	xor	eax, eax
	rep	stosd
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	edi, [ebp + 12]			; edi == autoc
	;	const unsigned limit = data_len - lag;
	sub	ecx, edx
	inc	ecx				; we are looping <= limit so we add one to the counter
	;	for(sample = 0; sample <= limit; sample++){
	;		d = data[sample];
	;		for(coeff = 0; coeff < lag; coeff++)
	;			autoc[coeff] += d * data[sample+coeff];
	;	}
	xor	eax, eax			; eax == sample <- 0
	ALIGN 16
.outer_loop:
	push	eax				; save sample
	fld	dword [esi + eax * 4]		; ST = d <- data[sample]
	mov	ebx, eax			; ebx == sample+coeff <- sample
	mov	edx, [ebp + 8]			; edx <- lag
	xor	eax, eax			; eax == coeff <- 0
	ALIGN 16
.inner_loop:
	fld	st0				; ST = d d
	fmul	dword [esi + ebx * 4]		; ST = d*data[sample+coeff] d
	fadd	dword [edi + eax * 4]		; ST = autoc[coeff]+d*data[sample+coeff] d
	fstp	dword [edi + eax * 4]		; autoc[coeff]+=d*data[sample+coeff]  ST = d
	inc	ebx				; (sample+coeff)++
	inc	eax				; coeff++
	dec	edx
	jnz	.inner_loop
	pop	eax				; restore sample
	fstp	st0				; pop d, ST = empty
	inc	eax				; sample++
	loop	.outer_loop
	;	for(; sample < data_len; sample++){
	;		d = data[sample];
	;		for(coeff = 0; coeff < data_len - sample; coeff++)
	;			autoc[coeff] += d * data[sample+coeff];
	;	}
	mov	ecx, [ebp + 8]			; ecx <- lag
	dec	ecx				; ecx <- lag - 1
	jz	.outer_end			; skip loop if 0
.outer_loop2:
	push	eax				; save sample
	fld	dword [esi + eax * 4]		; ST = d <- data[sample]
	mov	ebx, eax			; ebx == sample+coeff <- sample
	mov	edx, [ebp + 4]			; edx <- data_len
	sub	edx, eax			; edx <- data_len-sample
	xor	eax, eax			; eax == coeff <- 0
.inner_loop2:
	fld	st0				; ST = d d
	fmul	dword [esi + ebx * 4]		; ST = d*data[sample+coeff] d
	fadd	dword [edi + eax * 4]		; ST = autoc[coeff]+d*data[sample+coeff] d
	fstp	dword [edi + eax * 4]		; autoc[coeff]+=d*data[sample+coeff]  ST = d
	inc	ebx				; (sample+coeff)++
	inc	eax				; coeff++
	dec	edx
	jnz	.inner_loop2
	pop	eax				; restore sample
	fstp	st0				; pop d, ST = empty
	inc	eax				; sample++
	loop	.outer_loop2
.outer_end:
	jmp	.end

.lag_eq_6_plus_1:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 6
	ALIGN 16
.lag_6_1_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st1, st0			; add to autoc[6]
	dec	ecx
	jnz	.lag_6_1_loop
	fstp	dword [edi + 24]
	jmp	.end

.lag_eq_6_plus_2:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[7]
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 7
	ALIGN 16
.lag_6_2_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st2, st0			; add to autoc[7]
	dec	ecx
	jnz	.lag_6_2_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	faddp	st1, st0			; add to autoc[6]
	fstp	dword [edi + 24]
	fstp	dword [edi + 28]
	jmp	.end

.lag_eq_6_plus_3:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[8]
	fldz					; will accumulate autoc[7]
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 8
	ALIGN 16
.lag_6_3_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[8]
	dec	ecx
	jnz	.lag_6_3_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st2, st0			; add to autoc[7]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	faddp	st1, st0			; add to autoc[6]
	fstp	dword [edi + 24]
	fstp	dword [edi + 28]
	fstp	dword [edi + 32]
	jmp	.end

.lag_eq_6_plus_4:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[9]
	fldz					; will accumulate autoc[8]
	fldz					; will accumulate autoc[7]
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 9
	ALIGN 16
.lag_6_4_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[9]
	dec	ecx
	jnz	.lag_6_4_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[8]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st2, st0			; add to autoc[7]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	faddp	st1, st0			; add to autoc[6]
	fstp	dword [edi + 24]
	fstp	dword [edi + 28]
	fstp	dword [edi + 32]
	fstp	dword [edi + 36]
	jmp	.end

.lag_eq_6_plus_5:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[10]
	fldz					; will accumulate autoc[9]
	fldz					; will accumulate autoc[8]
	fldz					; will accumulate autoc[7]
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 10
	ALIGN 16
.lag_6_5_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[9]
	fld	dword [esi + 40]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st5, st0			; add to autoc[10]
	dec	ecx
	jnz	.lag_6_5_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[9]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[8]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st2, st0			; add to autoc[7]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	faddp	st1, st0			; add to autoc[6]
	fstp	dword [edi + 24]
	fstp	dword [edi + 28]
	fstp	dword [edi + 32]
	fstp	dword [edi + 36]
	fstp	dword [edi + 40]
	jmp	.end

.lag_eq_6_plus_6:
	mov	ecx, [ebp + 4]			; ecx == data_len
	mov	esi, [ebp]			; esi == data
	mov	edi, [ebp + 12]			; edi == autoc
	fldz					; will accumulate autoc[11]
	fldz					; will accumulate autoc[10]
	fldz					; will accumulate autoc[9]
	fldz					; will accumulate autoc[8]
	fldz					; will accumulate autoc[7]
	fldz					; will accumulate autoc[6]
	sub	ecx, byte 11
	ALIGN 16
.lag_6_6_loop:
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[9]
	fld	dword [esi + 40]
	fmul	st0, st1
	faddp	st6, st0			; add to autoc[10]
	fld	dword [esi + 44]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st6, st0			; add to autoc[11]
	dec	ecx
	jnz	.lag_6_6_loop
	; clean up the leftovers
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmul	st0, st1
	faddp	st5, st0			; add to autoc[9]
	fld	dword [esi + 40]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st5, st0			; add to autoc[10]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmul	st0, st1
	faddp	st4, st0			; add to autoc[8]
	fld	dword [esi + 36]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st4, st0			; add to autoc[9]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmul	st0, st1
	faddp	st3, st0			; add to autoc[7]
	fld	dword [esi + 32]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st3, st0			; add to autoc[8]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmul	st0, st1
	faddp	st2, st0			; add to autoc[6]
	fld	dword [esi + 28]
	fmulp	st1, st0
	add	esi, byte 4			; [CR] sample++
	faddp	st2, st0			; add to autoc[7]
	fld	dword [esi]
	fld	dword [esi + 24]
	fmulp	st1, st0
	faddp	st1, st0			; add to autoc[6]
	fstp	dword [edi + 24]
	fstp	dword [edi + 28]
	fstp	dword [edi + 32]
	fstp	dword [edi + 36]
	fstp	dword [edi + 40]
	fstp	dword [edi + 44]
	jmp	.end

.end:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret

; end
