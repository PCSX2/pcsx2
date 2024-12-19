// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"

#include "vixl/aarch64/constants-aarch64.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <unordered_map>

#define RWRET vixl::aarch64::w0
#define RXRET vixl::aarch64::x0
#define RQRET vixl::aarch64::q0

#define RWARG1 vixl::aarch64::w0
#define RWARG2 vixl::aarch64::w1
#define RWARG3 vixl::aarch64::w2
#define RWARG4 vixl::aarch64::w3
#define RXARG1 vixl::aarch64::x0
#define RXARG2 vixl::aarch64::x1
#define RXARG3 vixl::aarch64::x2
#define RXARG4 vixl::aarch64::x3

#define RXVIXLSCRATCH vixl::aarch64::x16
#define RWVIXLSCRATCH vixl::aarch64::w16
#define RSCRATCHADDR vixl::aarch64::x17

#define RQSCRATCH vixl::aarch64::q30
#define RDSCRATCH vixl::aarch64::d30
#define RSSCRATCH vixl::aarch64::s30
#define RQSCRATCH2 vixl::aarch64::q31
#define RDSCRATCH2 vixl::aarch64::d31
#define RSSCRATCH2 vixl::aarch64::s31
#define RQSCRATCH3 vixl::aarch64::q29
#define RDSCRATCH3 vixl::aarch64::d29
#define RSSCRATCH3 vixl::aarch64::s29

#define RQSCRATCHI vixl::aarch64::VRegister(30, 128, 16)
#define RQSCRATCHF vixl::aarch64::VRegister(30, 128, 4)
#define RQSCRATCHD vixl::aarch64::VRegister(30, 128, 2)

#define RQSCRATCH2I vixl::aarch64::VRegister(31, 128, 16)
#define RQSCRATCH2F vixl::aarch64::VRegister(31, 128, 4)
#define RQSCRATCH2D vixl::aarch64::VRegister(31, 128, 2)

static inline s64 GetPCDisplacement(const void* current, const void* target)
{
	return static_cast<s64>((reinterpret_cast<ptrdiff_t>(target) - reinterpret_cast<ptrdiff_t>(current)) >> 2);
}

const vixl::aarch64::Register& armWRegister(int n);
const vixl::aarch64::Register& armXRegister(int n);
const vixl::aarch64::VRegister& armSRegister(int n);
const vixl::aarch64::VRegister& armDRegister(int n);
const vixl::aarch64::VRegister& armQRegister(int n);

class ArmConstantPool;

static const u32 SP_SCRATCH_OFFSET = 0;

extern thread_local vixl::aarch64::MacroAssembler* armAsm;
extern thread_local u8* armAsmPtr;
extern thread_local size_t armAsmCapacity;
extern thread_local ArmConstantPool* armConstantPool;

static __fi bool armHasBlock()
{
	return (armAsm != nullptr);
}

static __fi u8* armGetCurrentCodePointer()
{
	return static_cast<u8*>(armAsmPtr) + armAsm->GetCursorOffset();
}

__fi static u8* armGetAsmPtr()
{
	return armAsmPtr;
}

void armSetAsmPtr(void* ptr, size_t capacity, ArmConstantPool* pool);
void armAlignAsmPtr();
u8* armStartBlock();
u8* armEndBlock();

void armDisassembleAndDumpCode(const void* ptr, size_t size);
void armEmitJmp(const void* ptr, bool force_inline = false);
void armEmitCall(const void* ptr, bool force_inline = false);
void armEmitCbnz(const vixl::aarch64::Register& reg, const void* ptr);
void armEmitCondBranch(vixl::aarch64::Condition cond, const void* ptr);
void armMoveAddressToReg(const vixl::aarch64::Register& reg, const void* addr);
void armLoadPtr(const vixl::aarch64::CPURegister& reg, const void* addr);
void armStorePtr(const vixl::aarch64::CPURegister& reg, const void* addr);
void armBeginStackFrame(bool save_fpr);
void armEndStackFrame(bool save_fpr);
bool armIsCalleeSavedRegister(int reg);

vixl::aarch64::MemOperand armOffsetMemOperand(const vixl::aarch64::MemOperand& op, s64 offset);
void armGetMemOperandInRegister(const vixl::aarch64::Register& addr_reg,
	const vixl::aarch64::MemOperand& op, s64 extra_offset = 0);

void armLoadConstant128(const vixl::aarch64::VRegister& reg, const void* ptr);

// may clobber RSCRATCH/RSCRATCH2. they shouldn't be inputs.
void armEmitVTBL(const vixl::aarch64::VRegister& dst, const vixl::aarch64::VRegister& src1,
	const vixl::aarch64::VRegister& src2, const vixl::aarch64::VRegister& tbl);

//////////////////////////////////////////////////////////////////////////

class ArmConstantPool
{
public:
	void Init(void* ptr, u32 capacity);
	void Destroy();
	void Reset();

	u8* GetJumpTrampoline(const void* target);
	u8* GetLiteral(u64 value);
	u8* GetLiteral(const u128& value);
	u8* GetLiteral(const u8* bytes, size_t len);

	void EmitLoadLiteral(const vixl::aarch64::CPURegister& reg, const u8* literal) const;

private:
	__fi u32 GetRemainingCapacity() const { return m_capacity - m_used; }

	struct u128_hash
	{
		std::size_t operator()(const u128& v) const
		{
			std::size_t s = 0;
			HashCombine(s, v.lo, v.hi);
			return s;
		}
	};

	std::unordered_map<const void*, u32> m_jump_targets;
	std::unordered_map<u128, u32, u128_hash> m_literals;

	u8* m_base_ptr = nullptr;
	u32 m_capacity = 0;
	u32 m_used = 0;
};
