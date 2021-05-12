#pragma once

using namespace Xbyak;

#ifdef _M_AMD64
// Yeah let use mips naming ;)
	#ifdef _WIN64
		#define a0 rcx
		#define a1 rdx
		#define a2 r8
		#define a3 r9
		#define t0 rdi
		#define t1 rsi
	#else
		#define a0 rdi
		#define a1 rsi
		#define a2 rdx
		#define a3 rcx
		#define t0 r8
		#define t1 r9
	#endif
#endif

