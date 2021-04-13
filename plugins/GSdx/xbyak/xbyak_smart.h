/* Copyright (c) 2020 PCSX2 Dev Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the copyright owner nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "xbyak.h"
#include "xbyak_util.h"

namespace Xbyak
{

	namespace SSEVersion
	{
		enum SSEVersion
		{
			AVX2  = 0x501,
			AVX   = 0x500,
			SSE41 = 0x401,
			SSE3  = 0x301,
			SSE2  = 0x200,
		};
	}

	/// Similar to Xbyak::util::cpu but more open to us putting in extra flags (e.g. "vpgatherdd is fast"), as well as making it easier to test other configurations by artifically limiting features
	struct CPUInfo
	{
		bool hasFMA = false;
		SSEVersion::SSEVersion sseVersion = SSEVersion::SSE2;

		CPUInfo() = default;
		CPUInfo(const util::Cpu& cpu)
		{
			auto version = SSEVersion::SSE2;
			if (cpu.has(util::Cpu::tSSE3))
				version = SSEVersion::SSE3;
			if (cpu.has(util::Cpu::tSSE41))
				version = SSEVersion::SSE41;
			if (cpu.has(util::Cpu::tAVX))
				version = SSEVersion::AVX;
			if (cpu.has(util::Cpu::tAVX2))
				version = SSEVersion::AVX2;

			hasFMA = cpu.has(util::Cpu::tFMA);
			sseVersion = version;
		}
	};

	/// Code generator that automatically selects between SSE and AVX, x86 and x64 so you don't have to
	/// Should make combined SSE and AVX codegen much easier
	class SmartCodeGenerator
	{
		/// Make sure the register is okay to use
		void validateRegister(const Operand& op)
		{
			if (is64)
				return;
			if (op.isREG() && (op.isExtIdx() || op.isExt8bit()))
				throw Error(ERR_64_BIT_REG_IN_32);
			if (op.isMEM())
			{
				auto e = static_cast<const Address&>(op).getRegExp();
				validateRegister(e.getIndex());
				validateRegister(e.getBase());
			}
		}
		/// For easier macro-ing
		void validateRegister(int imm)
		{
		}

		void require64()
		{
			if (!is64)
				throw Error(ERR_64_INSTR_IN_32);
		}
		void requireAVX()
		{
			if (!hasAVX)
				throw Error(ERR_AVX_INSTR_IN_SSE);
		}
	public:
		CodeGenerator& actual;

#if defined(_M_AMD64) || defined(_WIN64)
		constexpr static bool is32 = false;
		constexpr static bool is64 = true;
		using AddressReg = Reg64;
		using RipType = RegRip;

		template <typename T32, typename T64>
		struct Choose3264 { using type = T64; };

		template <typename T32, typename T64>
		static T64 choose3264(T32 t32, T64 t64) { return t64; }
#else
		constexpr static bool is32 = true;
		constexpr static bool is64 = false;
		using AddressReg = Reg32;
		using RipType = int;

		template <typename T32, typename T64>
		struct Choose3264 { using type = T32; };

		template <typename T32, typename T64>
		static T32 choose3264(T32 t32, T64 t64) { return t32; }
#endif

		const bool hasSSE2, hasSSE3, hasSSE41, hasAVX, hasAVX2, hasFMA;

		const Xmm xmm0{0}, xmm1{1}, xmm2{2}, xmm3{3}, xmm4{4}, xmm5{5}, xmm6{6}, xmm7{7}, xmm8{8}, xmm9{9}, xmm10{10}, xmm11{11}, xmm12{12}, xmm13{13}, xmm14{14}, xmm15{15};
		const Ymm ymm0{0}, ymm1{1}, ymm2{2}, ymm3{3}, ymm4{4}, ymm5{5}, ymm6{6}, ymm7{7}, ymm8{8}, ymm9{9}, ymm10{10}, ymm11{11}, ymm12{12}, ymm13{13}, ymm14{14}, ymm15{15};
		const AddressReg rax{Operand::RAX}, rcx{Operand::RCX}, rdx{Operand::RDX}, rbx{Operand::RBX}, rsp{Operand::RSP}, rbp{Operand::RBP}, rsi{Operand::RSI}, rdi{Operand::RDI}, r8{8},  r9{9},  r10{10},  r11{11},  r12{12},  r13{13},  r14{14},  r15{15};
		const Reg32      eax{Operand::EAX}, ecx{Operand::ECX}, edx{Operand::EDX}, ebx{Operand::EBX}, esp{Operand::ESP}, ebp{Operand::EBP}, esi{Operand::ESI}, edi{Operand::EDI}, r8d{8}, r9d{9}, r10d{10}, r11d{11}, r12d{12}, r13d{13}, r14d{14}, r15d{15};
		const Reg16       ax{Operand::AX},   cx{Operand::CX},   dx{Operand::DX},   bx{Operand::BX},   sp{Operand::SP},   bp{Operand::BP},   si{Operand::SI},   di{Operand::DI};
		const Reg8        al{Operand::AL},   cl{Operand::CL},   dl{Operand::DL},   bl{Operand::BL},   ah{Operand::AH},   ch{Operand::CH},   dh{Operand::DH},   bh{Operand::BH};

		const RipType rip{};
		const AddressFrame ptr{0}, byte{8}, word{16}, dword{32}, qword{64}, xword{128}, yword{256}, zword{512};

		SmartCodeGenerator(CodeGenerator* actual, CPUInfo cpu)
			: actual(*actual)
			, hasSSE2(cpu.sseVersion >= SSEVersion::SSE2)
			, hasSSE3(cpu.sseVersion >= SSEVersion::SSE3)
			, hasSSE41(cpu.sseVersion >= SSEVersion::SSE41)
			, hasAVX(cpu.sseVersion >= SSEVersion::AVX)
			, hasAVX2(cpu.sseVersion >= SSEVersion::AVX2)
			, hasFMA(cpu.hasFMA)
		{
		}

// ------------ Forwarding instructions ------------
// Note: Only instructions used by codegen were added here, so if you're modifying codegen, you may need to add instructions here

// For instructions available in SSE and AVX, functions with the SSE name and arguments that forward to SSE or AVX depending on the target, as well as functions with the AVX name and arguments that forward to the AVX version or assert on SSE

// ARGS_* macros are provided for shorter argument lists.  The following single-letter abbreviations are used: X=Xmm, Y=Ymm, O=Operand, A=Address, I=Immediate
// FORWARD(argcount, category, instrname, argtypes...) forwards an instruction.  The following categories are available:
//   BASE:    non-SSE
//   SSE:     available on SSE and v-prefixed on AVX
//   SSEONLY: available only on SSE (exception on AVX)
//   AVX:     available only on AVX (exception on SSE)
//   AVX2:    available only on AVX2 (exception on AVX/SSE)
//   FMA:     available only with FMA
// SFORWARD forwards an SSE instruction whose AVX variant takes the same number of registers
// AFORWARD forwards an SSE instruction whose AVX variant takes an extra destination register

// Implementation details:
// ACTUAL_FORWARD_*: Actually forward the function of the given type
// FORWARD#: First validates the arguments (e.g. make sure you're not passing registers over 7 on x86), then forwards to an ACTUAL_FORWARD_*

// Big thanks to https://stackoverflow.com/a/24028231 for helping me figure out how to work around MSVC's terrible macro expander
// Of course GCC/Clang don't like the workaround so enjoy the ifdefs
#define EXPAND_ARGS(macro, args) macro args

#define ACTUAL_FORWARD_BASE(name, ...) \
	actual.name(__VA_ARGS__);

#define ACTUAL_FORWARD_SSE(name, ...) \
	if (hasAVX) \
		actual.v##name(__VA_ARGS__); \
	else \
		actual.name(__VA_ARGS__); \

#define ACTUAL_FORWARD_SSEONLY(name, ...) \
	if (hasAVX) \
		throw Error(ERR_SSE_INSTR_IN_AVX); \
	else \
		actual.name(__VA_ARGS__); \

#define ACTUAL_FORWARD_AVX(name, ...) \
	if (hasAVX) \
		actual.name(__VA_ARGS__); \
	else \
		throw Error(ERR_AVX_INSTR_IN_SSE); \

#define ACTUAL_FORWARD_AVX2(name, ...) \
	if (hasAVX2) \
		actual.name(__VA_ARGS__); \
	else \
		throw Error(ERR_AVX_INSTR_IN_SSE); \

#define ACTUAL_FORWARD_FMA(name, ...) \
	if (hasFMA) \
		actual.name(__VA_ARGS__); \
	else \
		throw Error(ERR_AVX_INSTR_IN_SSE); \

#define FORWARD1(category, name, type) \
	void name(type a) \
	{ \
		validateRegister(a); \
		ACTUAL_FORWARD_##category(name, a) \
	}

#define FORWARD2(category, name, type1, type2) \
	void name(type1 a, type2 b) \
	{ \
		validateRegister(a); \
		validateRegister(b); \
		ACTUAL_FORWARD_##category(name, a, b) \
	}

#define FORWARD3(category, name, type1, type2, type3) \
	void name(type1 a, type2 b, type3 c) \
	{ \
		validateRegister(a); \
		validateRegister(b); \
		validateRegister(c); \
		ACTUAL_FORWARD_##category(name, a, b, c) \
	}

#define FORWARD4(category, name, type1, type2, type3, type4) \
	void name(type1 a, type2 b, type3 c, type4 d) \
	{ \
		validateRegister(a); \
		validateRegister(b); \
		validateRegister(c); \
		validateRegister(d); \
		ACTUAL_FORWARD_##category(name, a, b, c, d) \
	}

#ifdef __GNUC__
# define FORWARD_(argcount, ...) FORWARD##argcount(__VA_ARGS__)
// Gets the macro evaluator to evaluate in the right order
# define FORWARD(...) FORWARD_(__VA_ARGS__)
#else
# define FORWARD_(argcount, ...) EXPAND_ARGS(FORWARD##argcount, (__VA_ARGS__))
// Gets the macro evaluator to evaluate in the right order
# define FORWARD(...) EXPAND_ARGS(FORWARD_, (__VA_ARGS__))
#endif

#define FORWARD_SSE_XMM0(name) \
	void name(const Xmm& a, const Operand& b) \
	{ \
		validateRegister(a); \
		validateRegister(b); \
		if (hasAVX) \
			actual.v##name(a, b, Xmm(0)); \
		else \
			actual.name(a, b); \
	} \
	FORWARD(4, AVX, v##name, const Xmm&, const Xmm&, const Operand&, const Xmm&)

#define FORWARD_JUMP(name) \
		void name(const void *addr) { actual.name(addr); } \
		void name(const Label& label, CodeGenerator::LabelType type = CodeGenerator::T_AUTO) { actual.name(label, type); } \
		void name(const char *label, CodeGenerator::LabelType type = CodeGenerator::T_AUTO) { actual.name(label, type); }

#define ADD_ONE_2 3
#define ADD_ONE_3 4

#ifdef __GNUC__
# define SFORWARD(argcount, name, ...) FORWARD(argcount, SSE, name, __VA_ARGS__)
# define AFORWARD_(argcount, name, arg1, ...)\
	SFORWARD(argcount, name, arg1, __VA_ARGS__)\
	FORWARD(ADD_ONE_##argcount, AVX, v##name, arg1, arg1, __VA_ARGS__)
// Gets the macro evaluator to evaluate in the right order
# define AFORWARD(...) EXPAND_ARGS(AFORWARD_, (__VA_ARGS__))
#else
# define SFORWARD(argcount, name, ...) EXPAND_ARGS(FORWARD, (argcount, SSE, name, __VA_ARGS__))
# define AFORWARD_(argcount, name, arg1, ...)\
	EXPAND_ARGS(SFORWARD, (argcount, name, arg1, __VA_ARGS__))\
	EXPAND_ARGS(FORWARD, (ADD_ONE_##argcount, AVX, v##name, arg1, arg1, __VA_ARGS__))
// Gets the macro evaluator to evaluate in the right order
# define AFORWARD(...) EXPAND_ARGS(AFORWARD_, (__VA_ARGS__))
#endif

#define FORWARD_OO_OI(name) \
	FORWARD(2, BASE, name, ARGS_OO) \
	FORWARD(2, BASE, name, ARGS_OI)

#define ARGS_OI const Operand&, uint32
#define ARGS_OO const Operand&, const Operand&
#define ARGS_XI const Xmm&, int
#define ARGS_XO const Xmm&, const Operand&
#define ARGS_XOI const Xmm&, const Operand&, uint8
#define ARGS_XXO const Xmm&, const Xmm&, const Operand&
#define ARGS_YOI const Ymm&, const Operand&, uint8

// For instructions that are ifdef'd out without XBYAK64
#ifdef XBYAK64
# define REQUIRE64(action) require64(); action
#else
# define REQUIRE64(action) require64()
#endif

		const uint8 *getCurr() { return actual.getCurr(); }
		void align(int x = 16) { return actual.align(x); }
		void db(int code) { actual.db(code); }
		void L(const std::string& label) { actual.L(label); }

		void cdqe() { REQUIRE64(actual.cdqe()); }
		void ret(int imm = 0) { actual.ret(imm); }
		void vzeroupper() { requireAVX(); actual.vzeroupper(); }
		void vzeroall() { requireAVX(); actual.vzeroall(); }

		FORWARD_OO_OI(add)
		FORWARD_OO_OI(and)
		FORWARD_OO_OI(cmp)
		FORWARD_OO_OI(or)
		FORWARD_OO_OI(sub)
		FORWARD_OO_OI(xor)
		FORWARD(2, BASE, lea,   const Reg&, const Address&)
		FORWARD(2, BASE, mov,   const Operand&, size_t)
		FORWARD(2, BASE, mov,   ARGS_OO)
		FORWARD(2, BASE, movzx, const Reg&, const Operand&)
		FORWARD(1, BASE, not,   const Operand&)
		FORWARD(1, BASE, pop,   const Operand&)
		FORWARD(1, BASE, push,  const Operand&)
		FORWARD(2, BASE, sar,   const Operand&, const Reg8&)
		FORWARD(2, BASE, sar,   ARGS_OI)
		FORWARD(2, BASE, shl,   const Operand&, const Reg8&)
		FORWARD(2, BASE, shl,   ARGS_OI)
		FORWARD(2, BASE, shr,   const Operand&, const Reg8&)
		FORWARD(2, BASE, shr,   ARGS_OI)
		FORWARD(2, BASE, test,  const Operand&, const Reg&);
		FORWARD(2, BASE, test,  ARGS_OI);

		FORWARD_JUMP(je)
		FORWARD_JUMP(jle)
		FORWARD_JUMP(jmp)

		AFORWARD(2, addps,     ARGS_XO)
		SFORWARD(2, cvtdq2ps,  ARGS_XO)
		SFORWARD(2, cvtps2dq,  ARGS_XO)
		SFORWARD(2, cvttps2dq, ARGS_XO)
		SFORWARD(3, extractps, const Operand&, const Xmm&, uint8)
		AFORWARD(2, maxps,     ARGS_XO)
		AFORWARD(2, minps,     ARGS_XO)
		SFORWARD(2, movaps,    ARGS_XO)
		SFORWARD(2, movaps,    const Address&, const Xmm&)
		SFORWARD(2, movd,      const Address&, const Xmm&)
		SFORWARD(2, movd,      const Reg32&, const Xmm&)
		SFORWARD(2, movd,      const Xmm&, const Address&)
		SFORWARD(2, movd,      const Xmm&, const Reg32&)
		SFORWARD(2, movdqa,    ARGS_XO)
		SFORWARD(2, movdqa,    const Address&, const Xmm&)
		SFORWARD(2, movhps,    ARGS_XO)
		SFORWARD(2, movhps,    const Address&, const Xmm&)
		SFORWARD(2, movq,      const Address&, const Xmm&)
		SFORWARD(2, movq,      const Xmm&, const Address&)
		AFORWARD(2, mulps,     ARGS_XO)
		AFORWARD(2, orps,      ARGS_XO)
		AFORWARD(2, packssdw,  ARGS_XO)
		AFORWARD(2, packusdw,  ARGS_XO)
		AFORWARD(2, packuswb,  ARGS_XO)
		AFORWARD(2, paddd,     ARGS_XO)
		AFORWARD(2, paddusb,   ARGS_XO)
		AFORWARD(2, paddw,     ARGS_XO)
		AFORWARD(2, pand,      ARGS_XO)
		AFORWARD(2, pandn,     ARGS_XO)
		AFORWARD(3, pblendw,   ARGS_XOI)
		AFORWARD(2, pcmpeqd,   ARGS_XO)
		AFORWARD(2, pcmpeqw,   ARGS_XO)
		AFORWARD(2, pcmpgtd,   ARGS_XO)
		SFORWARD(3, pextrd,    const Operand&, const Xmm&, uint8)
		SFORWARD(3, pextrw,    const Operand&, const Xmm&, uint8)
		AFORWARD(3, pinsrd,    ARGS_XOI)
		AFORWARD(2, pmaxsw,    ARGS_XO)
		AFORWARD(2, pminsd,    ARGS_XO)
		AFORWARD(2, pminsw,    ARGS_XO)
		SFORWARD(2, pmovsxbd,  ARGS_XO)
		SFORWARD(2, pmovmskb,  const Reg32e&, const Xmm&)
		SFORWARD(2, pmovzxbw,  ARGS_XO)
		AFORWARD(2, pmulhrsw,  ARGS_XO)
		AFORWARD(2, pmulhw,    ARGS_XO)
		AFORWARD(2, pmullw,    ARGS_XO)
		AFORWARD(2, por,       ARGS_XO)
		SFORWARD(3, pshufd,    ARGS_XOI)
		SFORWARD(3, pshufhw,   ARGS_XOI)
		SFORWARD(3, pshuflw,   ARGS_XOI)
		AFORWARD(2, pslld,     ARGS_XI)
		AFORWARD(2, psllw,     ARGS_XI)
		AFORWARD(2, psrad,     ARGS_XI)
		AFORWARD(2, psrad,     ARGS_XO)
		AFORWARD(2, psraw,     ARGS_XI)
		AFORWARD(2, psrld,     ARGS_XI)
		AFORWARD(2, psrldq,    ARGS_XI)
		AFORWARD(2, psrlw,     ARGS_XI)
		AFORWARD(2, psrlw,     ARGS_XO)
		AFORWARD(2, psubd,     ARGS_XO)
		AFORWARD(2, psubw,     ARGS_XO)
		AFORWARD(2, punpckhdq, ARGS_XO)
		AFORWARD(2, punpckhwd, ARGS_XO)
		AFORWARD(2, punpcklbw, ARGS_XO)
		AFORWARD(2, punpckldq, ARGS_XO)
		AFORWARD(2, punpcklqdq,ARGS_XO)
		AFORWARD(2, punpcklwd, ARGS_XO)
		AFORWARD(2, pxor,      ARGS_XO)
		SFORWARD(2, rcpps,     ARGS_XO)
		AFORWARD(3, shufps,    ARGS_XOI)
		AFORWARD(2, subps,     ARGS_XO)
		AFORWARD(2, xorps,     ARGS_XO)

		FORWARD_SSE_XMM0(pblendvb)

		FORWARD(2, AVX,  vbroadcastss,   ARGS_XO)
		FORWARD(2, AVX2, vbroadcasti128, const Ymm&, const Address&)
		FORWARD(2, AVX,  vbroadcastf128, const Ymm&, const Address&)
		FORWARD(3, FMA,  vfmadd213ps,    ARGS_XXO)
		FORWARD(3, AVX2, vextracti128,   const Operand&, const Ymm&, uint8)
		FORWARD(4, AVX2, vinserti128,    const Ymm&, const Ymm&, const Operand&, uint8);
		FORWARD(2, AVX2, vpbroadcastd,   ARGS_XO)
		FORWARD(2, AVX2, vpbroadcastq,   ARGS_XO)
		FORWARD(2, AVX2, vpbroadcastw,   ARGS_XO)
		FORWARD(3, AVX2, vpermq,         ARGS_YOI)
		FORWARD(3, AVX2, vpgatherdd,     const Xmm&, const Address&, const Xmm&);
		FORWARD(3, AVX2, vpsravd,        ARGS_XXO)
		FORWARD(3, AVX2, vpsrlvd,        ARGS_XXO)

#undef REQUIRE64
#undef ARGS_OI
#undef ARGS_OO
#undef ARGS_XI
#undef ARGS_XO
#undef ARGS_XOI
#undef ARGS_XXO
#undef ARGS_YOI
#undef FORWARD_OO_OI
#undef AFORWARD
#undef AFORWARD_
#undef SFORWARD
#undef ADD_ONE_2
#undef ADD_ONE_3
#undef FORWARD_SSE_XMM0
#undef FORWARD_JUMP
#undef FORWARD
#undef FORWARD_
#undef FORWARD4
#undef FORWARD3
#undef FORWARD2
#undef FORWARD1
#undef ACTUAL_FORWARD_FMA
#undef ACTUAL_FORWARD_AVX2
#undef ACTUAL_FORWARD_AVX
#undef ACTUAL_FORWARD_SSE
#undef ACTUAL_FORWARD_SSEONLY
#undef ACTUAL_FORWARD_BASE
#undef EXPAND_ARGS
	};

}
