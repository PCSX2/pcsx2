# iR5900.c assembly routines
# zerofrog(@gmail.com)
.intel_syntax

.globl Dispatcher
	.type Dispatcher, @function
Dispatcher:
	# EDX contains the current pc to jump to, stack contains the jump addr to modify
	push %edx

	# calc PC_GETBLOCK
	# ((BASEBLOCK*)(reclut[((u32)(x)) >> 16] + (sizeof(BASEBLOCK)/4)*((x) & 0xffff)))
	s_pDispatchBlock = PC_GETBLOCK(cpuRegs.pc);
	
	mov %eax, s_pDispatchBlock

	## check if startpc == cpuRegs.pc
	mov %ecx, cpuRegs.pc
	##and %ecx, 0x5fffffff ## remove higher bits
	cmp %ecx, dword ptr [%eax+BLOCKTYPE_STARTPC]
	je CheckPtr

	## recompile
	push cpuRegs.pc ## pc
	call recRecompile
	add esp, 4 ## pop old param
	mov %eax, s_pDispatchBlock
CheckPtr:
	mov %eax, dword ptr [%eax]

#ifdef _DEBUG
	test %eax, %eax
	jnz CallFn
	# throw an exception
	int 10
	
CallFn:
#endif

	and %eax, 0x0fffffff
	mov %edx, %eax
	pop %ecx ## x86Ptr to mod
	sub %edx, %ecx
	sub %edx, 4
	mov dword ptr [%ecx], %edx

	jmp %eax
}

.globl DispatcherClear
	.type DispatcherClear, @function
DisptacherClear:
	## EDX contains the current pc
	mov cpuRegs.pc, %edx
	push %edx

	## calc PC_GETBLOCK
	s_pDispatchBlock = PC_GETBLOCK(cpuRegs.pc);

	if( s_pDispatchBlock->startpc == cpuRegs.pc ) {
		assert( s_pDispatchBlock->pFnptr != 0 );

		## already modded the code, jump to the new place
		__asm {
			pop %edx
			add esp, 4 ## ignore stack
			mov %eax, s_pDispatchBlock
			mov %eax, dword ptr [%eax]
			and %eax, 0x0fffffff
			jmp %eax
		}
	}

	call recRecompile
	add esp, 4 ## pop old param
	mov %eax, s_pDispatchBlock
	mov %eax, dword ptr [%eax]

	pop %ecx ## old fnptr

	and %eax, 0x0fffffff
	mov byte ptr [%ecx], 0xe9 ## jmp32
	mov %edx, %eax
	sub %edx, %ecx
	sub %edx, 5
	mov dword ptr [%ecx+1], %edx

	jmp %eax

## called when jumping to variable pc address
.globl DispatcherReg
	.type DispatcherReg, @function
DispatcherReg:

	##s_pDispatchBlock = PC_GETBLOCK(cpuRegs.pc);
	mov %edx, cpuRegs.pc
	mov %ecx, %edx
	
	shr %edx, 14
	and %edx, 0xfffffffc
	add %edx, recLUT
	mov %edx, dword ptr [%edx]

	mov %eax, %ecx
	and %eax, 0xfffc
	## %edx += 2*%eax
	shl %eax, 1
	add %edx, %eax
	
	## check if startpc == cpuRegs.pc
	mov %eax, %ecx
	##and %eax, 0x5fffffff ## remove higher bits
	cmp %eax, dword ptr [%edx+BLOCKTYPE_STARTPC]
	jne recomp

	mov %eax, dword ptr [%edx]

#ifdef _DEBUG
	test %eax, %eax
	jnz CallFn2
	# throw an exception
	int 10
	
CallFn2:

#endif

	and %eax, 0x0fffffff
	jmp %eax ## fnptr

recomp:
	sub esp, 8
	mov dword ptr [esp+4], %edx
	mov dword ptr [esp], %ecx
	call recRecompile
	mov %edx, dword ptr [esp+4]
	add esp, 8
	
	mov %eax, dword ptr [%edx]
	and %eax, 0x0fffffff
	jmp %eax ## fnptr

.globl _StartPerfCounter
	.type _StartPerfCounter, @function
_StartPerfCounter:

	push %eax
	push %ebx
	push %ecx

	rdtsc
	mov dword ptr [lbase], %eax
	mov dword ptr [lbase + 4], %edx

	pop %ecx
	pop %ebx
	pop %eax
	ret

.globl _StopPerfCounter
	.type _StopPerfCounter, @function
_StopPerfCounter:

	push %eax
	push %ebx
	push %ecx

	rdtsc

	sub %eax, dword ptr [lbase]
	sbb %edx, dword ptr [lbase + 4]
	mov %ecx, s_pCurBlock_ltime
	add %eax, dword ptr [%ecx]
	adc %edx, dword ptr [%ecx + 4]
	mov dword ptr [%ecx], %eax
	mov dword ptr [%ecx + 4], %edx
	pop %ecx
	pop %ebx
	pop %eax
	ret
	