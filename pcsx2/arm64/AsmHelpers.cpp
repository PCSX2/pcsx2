// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "arm64/AsmHelpers.h"

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/HostSys.h"
#include "common/Timer.h"

#ifdef __APPLE__
#include "common/Darwin/DarwinMisc.h"
#endif

const vixl::aarch64::Register& armWRegister(int n)
{
	using namespace vixl::aarch64;
	static constexpr const Register* regs[32] = {&w0, &w1, &w2, &w3, &w4, &w5, &w6, &w7, &w8, &w9, &w10,
		&w11, &w12, &w13, &w14, &w15, &w16, &w17, &w18, &w19, &w20, &w21, &w22, &w23, &w24, &w25, &w26, &w27, &w28,
		&w29, &w30, &w31};
	pxAssert(static_cast<size_t>(n) < std::size(regs));
	return *regs[n];
}

const vixl::aarch64::Register& armXRegister(int n)
{
	using namespace vixl::aarch64;
	static constexpr const Register* regs[32] = {&x0, &x1, &x2, &x3, &x4, &x5, &x6, &x7, &x8, &x9, &x10,
		&x11, &x12, &x13, &x14, &x15, &x16, &x17, &x18, &x19, &x20, &x21, &x22, &x23, &x24, &x25, &x26, &x27, &x28,
		&x29, &x30, &x31};
	pxAssert(static_cast<size_t>(n) < std::size(regs));
	return *regs[n];
}

const vixl::aarch64::VRegister& armSRegister(int n)
{
	using namespace vixl::aarch64;
	static constexpr const VRegister* regs[32] = {&s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &vixl::aarch64::s8, &s9, &s10,
		&s11, &s12, &s13, &s14, &s15, &vixl::aarch64::s16, &s17, &s18, &s19, &s20, &s21, &s22, &s23, &s24, &s25, &s26, &s27, &s28,
		&s29, &s30, &s31};
	pxAssert(static_cast<size_t>(n) < std::size(regs));
	return *regs[n];
}

const vixl::aarch64::VRegister& armDRegister(int n)
{
	using namespace vixl::aarch64;
	static constexpr const VRegister* regs[32] = {&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10,
		&d11, &d12, &d13, &d14, &d15, &d16, &d17, &d18, &d19, &d20, &d21, &d22, &d23, &d24, &d25, &d26, &d27, &d28,
		&d29, &d30, &d31};
	pxAssert(static_cast<size_t>(n) < std::size(regs));
	return *regs[n];
}

const vixl::aarch64::VRegister& armQRegister(int n)
{
	using namespace vixl::aarch64;
	static constexpr const VRegister* regs[32] = {&q0, &q1, &q2, &q3, &q4, &q5, &q6, &q7, &q8, &q9, &q10,
		&q11, &q12, &q13, &q14, &q15, &q16, &q17, &q18, &q19, &q20, &q21, &q22, &q23, &q24, &q25, &q26, &q27, &q28,
		&q29, &q30, &q31};
	pxAssert(static_cast<size_t>(n) < std::size(regs));
	return *regs[n];
}

// Opt-in only (matches origin/master): uncomment to compile vixl's
// PrintDisassembler/Decoder for armDisassembleAndDumpCode. Off by default so
// the disassembler TUs and statics don't ship in normal builds.
//#define INCLUDE_DISASSEMBLER

#ifdef INCLUDE_DISASSEMBLER
#include "vixl/aarch64/disasm-aarch64.h"
#endif

namespace a64 = vixl::aarch64;

thread_local a64::MacroAssembler* armAsm;
thread_local u8* armAsmPtr;
thread_local size_t armAsmCapacity;
thread_local ArmConstantPool* armConstantPool;
thread_local ArmAddressRecorder* armAddressRecorder;

#ifdef INCLUDE_DISASSEMBLER
static std::mutex armDisasmMutex;
static std::unique_ptr<a64::PrintDisassembler> armDisasm;
static std::unique_ptr<a64::Decoder> armDisasmDecoder;
#endif

void armSetAsmPtr(void* ptr, size_t capacity, ArmConstantPool* pool)
{
	pxAssert(!armAsm);
	armAsmPtr = static_cast<u8*>(ptr);
	armAsmCapacity = capacity;
	armConstantPool = pool;
}

// Align to 16 bytes, apparently ARM likes that.
void armAlignAsmPtr()
{
	static constexpr uintptr_t ALIGNMENT = 16;
	u8* new_ptr = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(armAsmPtr) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1));
	pxAssert(static_cast<size_t>(new_ptr - armAsmPtr) <= armAsmCapacity);
	armAsmCapacity -= (new_ptr - armAsmPtr);
	armAsmPtr = new_ptr;
}

// Placement-new the per-block MacroAssembler into a thread_local buffer
// instead of heap new/delete per JIT block: one fewer alloc/free pair on
// every block compile, and it sidesteps Android scudo tag/header corruption
// seen on this exact pattern (ARMSX2, Tyler Bochard, 1c1d0b880). The
// pxAssert(!armAsm) single-active-per-thread invariant (armAsm itself is
// thread_local — MTVU compiles VU1 on its own thread) makes the single
// buffer safe. (AX-10)
alignas(vixl::aarch64::MacroAssembler) static thread_local u8 s_armAsmStorage[sizeof(vixl::aarch64::MacroAssembler)];

// iOS W^X (ported from ARMSX2 master's aR* recompilers): under the iOS 26
// dual-map JIT modes (DarwinMisc::JitMode::LuckTXM / LuckNoTXM) the code
// region is mapped twice — RX at the address we execute and hand out, RW at
// rx + g_code_rw_offset. Every byte written into the region must go through
// the RW alias; all displacement math, recorded pointers, and icache flushes
// stay in RX space. On Legacy-mode iOS and on macOS/Simulator the offset is 0
// and this is an identity (writability there comes from the mprotect toggle /
// pthread_jit_write_protect_np inside Begin/EndCodeWrite[Range]). Gated on
// __APPLE__ rather than TARGET_OS_IPHONE so a forced dual-map on macOS
// (ARMSX2_FORCE_DUAL_MAP=1, CI-only) exercises the alias paths; production
// macOS always has offset 0, so behavior there is unchanged.
u8* armGetWritableCodePtr(u8* rx_ptr)
{
#ifdef __APPLE__
	return rx_ptr + DarwinMisc::g_code_rw_offset;
#else
	return rx_ptr;
#endif
}

// Write-window bookkeeping for iOS Legacy mode (mprotect-toggle W^X):
// BeginCodeWriteRange flips only [start, start+size) to RW so the rest of the
// region — including the JIT frames we will return into — stays executable.
// A no-op on every other platform/mode. Window size mirrors ARMSX2 master.
static thread_local u8* s_arm_block_start = nullptr;
static thread_local size_t s_arm_block_write_size = 0;
static constexpr size_t ARM64_CODE_WRITE_WINDOW = 1024 * 1024;

u8* armStartBlock()
{
	armAlignAsmPtr();

	s_arm_block_start = armAsmPtr;
	s_arm_block_write_size = (armAsmCapacity < ARM64_CODE_WRITE_WINDOW) ? armAsmCapacity : ARM64_CODE_WRITE_WINDOW;
	HostSys::BeginCodeWriteRange(s_arm_block_start, s_arm_block_write_size);

	pxAssert(!armAsm);
	// The MacroAssembler's buffer is the WRITABLE alias; armAsmPtr stays the
	// RX address, so armGetCurrentCodePointer() (= armAsmPtr + cursor) and
	// every displacement computed from it remain in execute space.
	armAsm = new (s_armAsmStorage) vixl::aarch64::MacroAssembler(
		static_cast<vixl::byte*>(armGetWritableCodePtr(armAsmPtr)), armAsmCapacity);
	armAsm->GetScratchVRegisterList()->Remove(31);
	armAsm->GetScratchRegisterList()->Remove(RSCRATCHADDR.GetCode());
	return armAsmPtr;
}

u8* armEndBlock()
{
	pxAssert(armAsm);

	armAsm->FinalizeCode();

	const u32 size = static_cast<u32>(armAsm->GetSizeOfCodeGenerated());
	pxAssert(size < armAsmCapacity);

	armAsm->~MacroAssembler();
	armAsm = nullptr;

	HostSys::EndCodeWriteRange(s_arm_block_start, s_arm_block_write_size);

	HostSys::FlushInstructionCache(armAsmPtr, size);

	s_arm_block_start = nullptr;
	s_arm_block_write_size = 0;
	armAsmPtr = armAsmPtr + size;
	armAsmCapacity -= size;
	return armAsmPtr;
}

void armDisassembleAndDumpCode(const void* ptr, size_t size)
{
#ifdef INCLUDE_DISASSEMBLER
	std::unique_lock lock(armDisasmMutex);
	if (!armDisasm)
	{
		std::FILE* logFile = Log::GetFileLogHandle();
		armDisasm = std::make_unique<a64::PrintDisassembler>(logFile ? logFile : stderr);
		armDisasmDecoder = std::make_unique<a64::Decoder>();
		armDisasmDecoder->AppendVisitor(armDisasm.get());
	}

	const auto* start = reinterpret_cast<const vixl::aarch64::Instruction*>(ptr);
	const auto* end = reinterpret_cast<const vixl::aarch64::Instruction*>(static_cast<const u8*>(ptr) + size);
	armDisasmDecoder->Decode(start, end);
#else
	Console.Error("Not compiled with INCLUDE_DISASSEMBLER");
#endif
}

void armEmitJmp(const void* ptr, bool force_inline)
{
	s64 displacement = GetPCDisplacement(armGetCurrentCodePointer(), ptr);
	bool use_blr = !vixl::IsInt26(displacement);
	if (use_blr && armConstantPool && !force_inline)
	{
		if (u8* trampoline = armConstantPool->GetJumpTrampoline(ptr); trampoline)
		{
			displacement = GetPCDisplacement(armGetCurrentCodePointer(), trampoline);
			use_blr = !vixl::IsInt26(displacement);
		}
	}

	if (use_blr)
	{
		if (armAddressRecorder)
			armAddressRecorder->OnAbsoluteTarget(ptr);
		armAsm->Mov(RXVIXLSCRATCH, reinterpret_cast<uintptr_t>(ptr));
		armAsm->Br(RXVIXLSCRATCH);
	}
	else
	{
		{
			a64::SingleEmissionCheckScope guard(armAsm);
			armAsm->b(displacement);
		}
		// Record after emission: the scope entry may flush a pending vixl
		// literal pool, so the insn address is only known once it's out.
		if (armAddressRecorder)
			armAddressRecorder->OnDirectBranch(armGetCurrentCodePointer() - 4, ptr, false);
	}
}

void armEmitJmpPtr(void* code_address, const void* target, bool flush_icache)
{
	// Same single-word B rewrite + cache maintenance protocol as
	// Arm64BaseBlocks::PatchAtomic / recPatchIslandB: a 4-byte aligned word
	// store is atomic on AArch64, so concurrent execution of the old branch
	// is safe.
	const intptr_t off = reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(code_address);
	pxAssertRel((off & 3) == 0, "armEmitJmpPtr: branch offset not 4-byte aligned");
	const intptr_t imm26 = off >> 2;
	pxAssertRel(imm26 >= -(1 << 25) && imm26 < (1 << 25), "armEmitJmpPtr: branch offset out of B imm26 range");
	// code_address is the RX alias; under iOS dual-mapping the store must go
	// through the RW mirror. Begin/EndCodeWrite covers the toggle modes
	// (refcounted, so it nests inside an open emit scope).
	HostSys::BeginCodeWrite();
	*reinterpret_cast<volatile u32*>(armGetWritableCodePtr(static_cast<u8*>(code_address))) =
		0x14000000u | (static_cast<u32>(imm26) & 0x03FFFFFFu);
	HostSys::EndCodeWrite();
	if (flush_icache)
		HostSys::FlushInstructionCache(code_address, 4);
}

void armEmitCall(const void* ptr, bool force_inline)
{
	s64 displacement = GetPCDisplacement(armGetCurrentCodePointer(), ptr);
	bool use_blr = !vixl::IsInt26(displacement);
	if (use_blr && armConstantPool && !force_inline)
	{
		if (u8* trampoline = armConstantPool->GetJumpTrampoline(ptr); trampoline)
		{
			displacement = GetPCDisplacement(armGetCurrentCodePointer(), trampoline);
			use_blr = !vixl::IsInt26(displacement);
		}
	}

	if (use_blr)
	{
		if (armAddressRecorder)
			armAddressRecorder->OnAbsoluteTarget(ptr);
		armAsm->Mov(RXVIXLSCRATCH, reinterpret_cast<uintptr_t>(ptr));
		armAsm->Blr(RXVIXLSCRATCH);
	}
	else
	{
		{
			a64::SingleEmissionCheckScope guard(armAsm);
			armAsm->bl(displacement);
		}
		if (armAddressRecorder)
			armAddressRecorder->OnDirectBranch(armGetCurrentCodePointer() - 4, ptr, true);
	}
}

void armEmitCbnz(const vixl::aarch64::Register& reg, const void* ptr)
{
	const s64 jump_distance =
		static_cast<s64>(reinterpret_cast<intptr_t>(ptr) - reinterpret_cast<intptr_t>(armGetCurrentCodePointer()));
	//pxAssert(Common::IsAligned(jump_distance, 4));
	if (a64::Instruction::IsValidImmPCOffset(a64::CompareBranchType, jump_distance >> 2))
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		armAsm->cbnz(reg, jump_distance >> 2);
	}
	else
	{
		a64::MacroEmissionCheckScope guard(armAsm);
		a64::Label branch_not_taken;
		armAsm->cbz(reg, &branch_not_taken);

		const s64 new_jump_distance =
			static_cast<s64>(reinterpret_cast<intptr_t>(ptr) - reinterpret_cast<intptr_t>(armGetCurrentCodePointer()));
		armAsm->b(new_jump_distance >> 2);
		armAsm->bind(&branch_not_taken);
	}
}

void armEmitCondBranch(a64::Condition cond, const void* ptr)
{
	const s64 jump_distance =
		static_cast<s64>(reinterpret_cast<intptr_t>(ptr) - reinterpret_cast<intptr_t>(armGetCurrentCodePointer()));
	//pxAssert(Common::IsAligned(jump_distance, 4));

	// A recorder patching this branch on relocation needs the imm26 reach of a
	// plain B — B.cond's ±1MB imm19 may not survive the move. Force the long
	// form for targets the recorder marks relocatable and record the B.
	if (armAddressRecorder && armAddressRecorder->WantsLongCondBranch(ptr))
	{
		a64::MacroEmissionCheckScope guard(armAsm);
		a64::Label branch_not_taken;
		armAsm->b(&branch_not_taken, a64::InvertCondition(cond));

		const s64 new_jump_distance =
			static_cast<s64>(reinterpret_cast<intptr_t>(ptr) - reinterpret_cast<intptr_t>(armGetCurrentCodePointer()));
		armAsm->b(new_jump_distance >> 2);
		armAddressRecorder->OnDirectBranch(armGetCurrentCodePointer() - 4, ptr, false);
		armAsm->bind(&branch_not_taken);
		return;
	}

	if (a64::Instruction::IsValidImmPCOffset(a64::CondBranchType, jump_distance >> 2))
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		armAsm->b(jump_distance >> 2, cond);
	}
	else
	{
		a64::MacroEmissionCheckScope guard(armAsm);
		a64::Label branch_not_taken;
		armAsm->b(&branch_not_taken, a64::InvertCondition(cond));

		const s64 new_jump_distance =
			static_cast<s64>(reinterpret_cast<intptr_t>(ptr) - reinterpret_cast<intptr_t>(armGetCurrentCodePointer()));
		armAsm->b(new_jump_distance >> 2);
		armAsm->bind(&branch_not_taken);
	}
}

void armMoveAddressToReg(const vixl::aarch64::Register& reg, const void* addr)
{
	// psxAsm->Mov(reg, static_cast<u64>(reinterpret_cast<uintptr_t>(addr)));
	pxAssert(reg.IsX());

	if (armAddressRecorder &&
		armAddressRecorder->ClassifyMove(addr) == ArmAddressRecorder::MoveForm::CanonicalAbs)
	{
		// Fixed-width 16-byte form: every operand bit lives in a movz/movk
		// imm16 field a relocation patcher can rewrite in place.
		const u64 v = reinterpret_cast<uintptr_t>(addr);
		{
			vixl::ExactAssemblyScope guard(armAsm, 16);
			armAsm->movz(reg, v & 0xFFFF, 0);
			armAsm->movk(reg, (v >> 16) & 0xFFFF, 16);
			armAsm->movk(reg, (v >> 32) & 0xFFFF, 32);
			armAsm->movk(reg, (v >> 48) & 0xFFFF, 48);
		}
		armAddressRecorder->OnCanonicalAbsMove(armGetCurrentCodePointer() - 16, addr);
		return;
	}

	const void* current_code_ptr_page = reinterpret_cast<const void*>(
		reinterpret_cast<uintptr_t>(armGetCurrentCodePointer()) & ~static_cast<uintptr_t>(0xFFF));
	const void* ptr_page =
		reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(addr) & ~static_cast<uintptr_t>(0xFFF));
	const s64 page_displacement = GetPCDisplacement(current_code_ptr_page, ptr_page) >> 10;
	const u32 page_offset = static_cast<u32>(reinterpret_cast<uintptr_t>(addr) & 0xFFFu);
	if (vixl::IsInt21(page_displacement) && a64::Assembler::IsImmAddSub(page_offset))
	{
		{
			a64::SingleEmissionCheckScope guard(armAsm);
			armAsm->adrp(reg, page_displacement);
		}
		if (armAddressRecorder)
			armAddressRecorder->OnAdrp(armGetCurrentCodePointer() - 4, addr);
		armAsm->Add(reg, reg, page_offset);
	}
	else if (vixl::IsInt21(page_displacement) && a64::Assembler::IsImmLogical(page_offset, 64))
	{
		{
			a64::SingleEmissionCheckScope guard(armAsm);
			armAsm->adrp(reg, page_displacement);
		}
		if (armAddressRecorder)
			armAddressRecorder->OnAdrp(armGetCurrentCodePointer() - 4, addr);
		armAsm->Orr(reg, reg, page_offset);
	}
	else
	{
		if (armAddressRecorder)
			armAddressRecorder->OnAbsoluteTarget(addr);
		armAsm->Mov(reg, reinterpret_cast<uintptr_t>(addr));
	}
}

void armLoadPtr(const vixl::aarch64::CPURegister& reg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(reg, a64::MemOperand(RSCRATCHADDR));
}

void armStorePtr(const vixl::aarch64::CPURegister& reg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(reg, a64::MemOperand(RSCRATCHADDR));
}

void armBeginStackFrame(bool save_fpr)
{
	// save x19 through x28, x29 could also be used
	armAsm->Sub(a64::sp, a64::sp, save_fpr ? 192 : 144);
	armAsm->Stp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 32));
	armAsm->Stp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 48));
	armAsm->Stp(a64::x23, a64::x24, a64::MemOperand(a64::sp, 64));
	armAsm->Stp(a64::x25, a64::x26, a64::MemOperand(a64::sp, 80));
	armAsm->Stp(a64::x27, a64::x28, a64::MemOperand(a64::sp, 96));
	armAsm->Stp(a64::x29, a64::lr, a64::MemOperand(a64::sp, 112));
	if (save_fpr)
	{
		armAsm->Stp(a64::d8, a64::d9, a64::MemOperand(a64::sp, 128));
		armAsm->Stp(a64::d10, a64::d11, a64::MemOperand(a64::sp, 144));
		armAsm->Stp(a64::d12, a64::d13, a64::MemOperand(a64::sp, 160));
		armAsm->Stp(a64::d14, a64::d15, a64::MemOperand(a64::sp, 176));
	}
}

void armEndStackFrame(bool save_fpr)
{
	if (save_fpr)
	{
		armAsm->Ldp(a64::d14, a64::d15, a64::MemOperand(a64::sp, 176));
		armAsm->Ldp(a64::d12, a64::d13, a64::MemOperand(a64::sp, 160));
		armAsm->Ldp(a64::d10, a64::d11, a64::MemOperand(a64::sp, 144));
		armAsm->Ldp(a64::d8, a64::d9, a64::MemOperand(a64::sp, 128));
	}
	armAsm->Ldp(a64::x29, a64::lr, a64::MemOperand(a64::sp, 112));
	armAsm->Ldp(a64::x27, a64::x28, a64::MemOperand(a64::sp, 96));
	armAsm->Ldp(a64::x25, a64::x26, a64::MemOperand(a64::sp, 80));
	armAsm->Ldp(a64::x23, a64::x24, a64::MemOperand(a64::sp, 64));
	armAsm->Ldp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 48));
	armAsm->Ldp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 32));
	armAsm->Add(a64::sp, a64::sp, save_fpr ? 192 : 144);
}

bool armIsCalleeSavedRegister(int reg)
{
	// same on both linux and windows
	return (reg >= 19);
}

vixl::aarch64::MemOperand armOffsetMemOperand(const vixl::aarch64::MemOperand& op, s64 offset)
{
	pxAssert(op.GetBaseRegister().IsValid() && op.GetAddrMode() == vixl::aarch64::Offset && op.GetShift() == vixl::aarch64::NO_SHIFT);
	return vixl::aarch64::MemOperand(op.GetBaseRegister(), op.GetOffset() + offset, op.GetAddrMode());
}

void armGetMemOperandInRegister(const vixl::aarch64::Register& addr_reg, const vixl::aarch64::MemOperand& op, s64 extra_offset /*= 0*/)
{
	pxAssert(addr_reg.IsX());
	pxAssert(op.GetBaseRegister().IsValid() && op.GetAddrMode() == vixl::aarch64::Offset && op.GetShift() == vixl::aarch64::NO_SHIFT);
	armAsm->Add(addr_reg, op.GetBaseRegister(), op.GetOffset() + extra_offset);
}

void armLoadConstant128(const vixl::aarch64::VRegister& reg, const void* ptr)
{
	u64 low, high;
	memcpy(&low, ptr, sizeof(low));
	memcpy(&high, static_cast<const u8*>(ptr) + sizeof(low), sizeof(high));
	armAsm->Ldr(reg, high, low);
}

void armEmitVTBL(const vixl::aarch64::VRegister& dst, const vixl::aarch64::VRegister& src1, const vixl::aarch64::VRegister& src2, const vixl::aarch64::VRegister& tbl)
{
	pxAssert(src1.GetCode() != RQSCRATCH.GetCode() && src2.GetCode() != RQSCRATCH2.GetCode());
	pxAssert(tbl.GetCode() != RQSCRATCH.GetCode() && tbl.GetCode() != RQSCRATCH2.GetCode());

	// must be consecutive
	if (src2.GetCode() == (src1.GetCode() + 1))
	{
		armAsm->Tbl(dst.V16B(), src1.V16B(), src2.V16B(), tbl.V16B());
		return;
	}

	armAsm->Mov(RQSCRATCH.Q(), src1.Q());
	armAsm->Mov(RQSCRATCH2.Q(), src2.Q());
	armAsm->Tbl(dst.V16B(), RQSCRATCH.V16B(), RQSCRATCH2.V16B(), tbl.V16B());
}

void ArmConstantPool::Init(void* ptr, u32 capacity)
{
	m_base_ptr = static_cast<u8*>(ptr);
	m_capacity = capacity;
	m_used = 0;
	m_jump_targets.clear();
	m_literals.clear();
}

void ArmConstantPool::Destroy()
{
	m_base_ptr = nullptr;
	m_capacity = 0;
	m_used = 0;
	m_jump_targets.clear();
	m_literals.clear();
}

void ArmConstantPool::Reset()
{
	m_used = 0;
	m_jump_targets.clear();
	m_literals.clear();
}

u8* ArmConstantPool::GetJumpTrampoline(const void* target)
{
	auto it = m_jump_targets.find(target);
	if (it != m_jump_targets.end())
		return m_base_ptr + it->second;

	// align to 16 bytes?
	const u32 offset = Common::AlignUpPow2(m_used, 16);

	// 4 movs plus a jump
	if ((m_capacity - offset) < 20)
	{
		Console.Error("Ran out of space in constant pool");
		return nullptr;
	}

	u8* const trampoline_ptr = m_base_ptr + offset;
	static constexpr size_t TRAMPOLINE_WRITE_WINDOW = 64;
	HostSys::BeginCodeWriteRange(trampoline_ptr, TRAMPOLINE_WRITE_WINDOW);
	// Emit into the RW alias; trampoline_ptr (RX) is what callers branch to.
	a64::MacroAssembler masm(static_cast<vixl::byte*>(armGetWritableCodePtr(trampoline_ptr)), m_capacity - offset);
	masm.Mov(RXVIXLSCRATCH, reinterpret_cast<intptr_t>(target));
	masm.Br(RXVIXLSCRATCH);
	masm.FinalizeCode();

	pxAssert(masm.GetSizeOfCodeGenerated() < 20);
	m_jump_targets.emplace(target, offset);
	m_used = offset + static_cast<u32>(masm.GetSizeOfCodeGenerated());

	HostSys::EndCodeWriteRange(trampoline_ptr, m_used - offset);
	HostSys::FlushInstructionCache(reinterpret_cast<void*>(trampoline_ptr), m_used - offset);

	return trampoline_ptr;
}

u8* ArmConstantPool::GetLiteral(u64 value)
{
	return GetLiteral(u128::From64(value));
}

u8* ArmConstantPool::GetLiteral(const u128& value)
{
	auto it = m_literals.find(value);
	if (it != m_literals.end())
		return m_base_ptr + it->second;

	if (GetRemainingCapacity() < 8)
		return nullptr;

	const u32 offset = Common::AlignUpPow2(m_used, 16);
	u8* const literal_ptr = &m_base_ptr[offset];
	HostSys::BeginCodeWriteRange(literal_ptr, sizeof(value));
	std::memcpy(armGetWritableCodePtr(literal_ptr), &value, sizeof(value));
	HostSys::EndCodeWriteRange(literal_ptr, sizeof(value));
	m_used = offset + sizeof(value);
	return literal_ptr;
}

u8* ArmConstantPool::GetLiteral(const u8* bytes, size_t len)
{
	pxAssertMsg(len <= 16, "literal length is less than 16 bytes");
	u128 table_u128 = {};
	std::memcpy(table_u128._u8, bytes, len);
	return GetLiteral(table_u128);
}

u8* ArmConstantPool::GetBlob(const u8* bytes, size_t len)
{
	const u32 offset = Common::AlignUpPow2(m_used, 8);
	if (offset + len > m_capacity)
		return nullptr;

	std::memcpy(&m_base_ptr[offset], bytes, len);
	m_used = offset + static_cast<u32>(len);
	return m_base_ptr + offset;
}

void ArmConstantPool::EmitLoadLiteral(const vixl::aarch64::CPURegister& reg, const u8* literal) const
{
	armMoveAddressToReg(RXVIXLSCRATCH, literal);
	armAsm->Ldr(reg, a64::MemOperand(RXVIXLSCRATCH));
}
