// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
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

#define RXVIXLSCRATCH vixl::aarch64::x16  // Reserved for VIXL internal use — do NOT use in rec code
#define RWVIXLSCRATCH vixl::aarch64::w16  // Reserved for VIXL internal use — do NOT use in rec code
#define RSCRATCHADDR vixl::aarch64::x17   // Address scratch — removed from VIXL pool in armStartBlock

// General-purpose value scratch registers for recompiler use.
// These are caller-saved and NOT in VIXL's scratch pool.
#define RXSCRATCH vixl::aarch64::x8
#define RWSCRATCH vixl::aarch64::w8

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

// Address-emission observer for the on-disk VU program cache. While a
// recorder is attached (mVU code-cache episodes only — see mVUopenCodeCache),
// the emit helpers below report every host-address-bearing emission so the
// recorder can build a relocation fixup table, and let it force canonical
// fixed-width forms where the default encoding couldn't be patched after the
// code block moves:
//   - armMoveAddressToReg of a volatile (heap) target → movz+movk×3 (16 bytes,
//     patchable) instead of the shortest mov/adrp form.
//   - armEmitCondBranch to a relocatable target → inverted-cond skip + B imm26
//     (B.cond's ±1MB imm19 can't survive arbitrary replacement).
// `at` arguments are the address of the first emitted instruction of the
// reported shape. All hooks are no-ops when no recorder is attached.
class ArmAddressRecorder
{
public:
	enum class MoveForm
	{
		Default, // emit in shortest form; recorder may still log it
		CanonicalAbs, // emit fixed-width movz+movk×3 so the operand is patchable
	};

	virtual ~ArmAddressRecorder() = default;

	// armMoveAddressToReg: pick the emission form for `addr`.
	virtual MoveForm ClassifyMove(const void* addr) = 0;
	// armMoveAddressToReg emitted the canonical 16-byte movz+movk×3 at `at`.
	virtual void OnCanonicalAbsMove(u8* at, const void* addr) = 0;
	// armMoveAddressToReg emitted ADRP (+Add/Orr) at `at`; the page offset is
	// PC-relative and must be re-paged if this code moves.
	virtual void OnAdrp(u8* at, const void* addr) = 0;
	// armEmitJmp/armEmitCall/armEmitCondBranch emitted a direct B/BL imm26 at
	// `at` targeting `target`.
	virtual void OnDirectBranch(u8* at, const void* target, bool is_call) = 0;
	// armEmitCondBranch: return true to force the long (cond-skip + B) form.
	virtual bool WantsLongCondBranch(const void* target) = 0;
	// An absolute (movz/movk-materialized) target with no patch site — emitted
	// by the out-of-range paths of armEmitJmp/armEmitCall/armMoveAddressToReg.
	// Recorder uses this to verify the target is run-invariant.
	virtual void OnAbsoluteTarget(const void* target) = 0;
};

static const u32 SP_SCRATCH_OFFSET = 0;

extern thread_local vixl::aarch64::MacroAssembler* armAsm;
extern thread_local u8* armAsmPtr;
extern thread_local size_t armAsmCapacity;
extern thread_local ArmConstantPool* armConstantPool;
extern thread_local ArmAddressRecorder* armAddressRecorder;

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
// iOS dual-map W^X: RX pointer -> writable alias (rx + g_code_rw_offset).
// Identity everywhere else (and on Apple platforms where the offset is 0).
// EVERY store into the code region that bypasses armAsm's buffer must route
// its write pointer through this; displacements/flushes stay on the RX ptr.
u8* armGetWritableCodePtr(u8* rx_ptr);
u8* armStartBlock();
u8* armEndBlock();

void armDisassembleAndDumpCode(const void* ptr, size_t size);
void armEmitJmp(const void* ptr, bool force_inline = false);
void armEmitCall(const void* ptr, bool force_inline = false);
// In-place patch: overwrite the 4-byte B at `code_address` with a branch to
// `target`. Used by EE block chaining to rewrite a link site (not tied to the
// current emit cursor). `code_address` must already hold a single B instruction.
void armEmitJmpPtr(void* code_address, const void* target, bool flush_icache = true);
void armEmitCbnz(const vixl::aarch64::Register& reg, const void* ptr);
void armEmitCondBranch(vixl::aarch64::Condition cond, const void* ptr);
void armMoveAddressToReg(const vixl::aarch64::Register& reg, const void* addr);
void armLoadPtr(const vixl::aarch64::CPURegister& reg, const void* addr);
void armStorePtr(const vixl::aarch64::CPURegister& reg, const void* addr);
void armBeginStackFrame(bool save_fpr);
void armEndStackFrame(bool save_fpr);
bool armIsCalleeSavedRegister(int reg);

// Emits the EE JIT's caller-saved pin flush-before / reload-after (see
// kEEPinTable in iR5900-arm64.h). Out-of-line bridges for emission contexts
// that can't include the EE rec header — currently only mVU macro-mode emit
// bodies that emit C calls inline into EE blocks (mVUaddrFix's waitMTVU).
// The flush is a lazy-dirty-mode no-op (EE_PIN_LAZY_DIRTY).
void armEmitEEClobberedPinFlushForCOP2();
void armEmitEEClobberedPinReloadForCOP2();

vixl::aarch64::MemOperand armOffsetMemOperand(const vixl::aarch64::MemOperand& op, s64 offset);
void armGetMemOperandInRegister(const vixl::aarch64::Register& addr_reg,
	const vixl::aarch64::MemOperand& op, s64 extra_offset = 0);

void armLoadConstant128(const vixl::aarch64::VRegister& reg, const void* ptr);

// Pack 4 per-lane bool lanes (each lane is all-1s or 0 — the natural output of
// a NEON CMxx / FCMxx against zero) into a 4-bit GPR using the canonical
// AArch64 movemask idiom: AND with a per-lane weight vector, ADDV-sum across
// lanes, then UMOV to GPR.
//
// `data` is clobbered (AND in-place, ADDV writes the low S lane in-place).
// `tmp`  is loaded with the weight vector via the vixl literal pool; must
// differ from `data`. Both must be Q-form (128-bit).
//
// PS2 MAC flag bit order is bit0=W, bit3=X (reverse of NEON lane order). Pass
// reverse=true to get that mapping; reverse=false yields lane[i]→bit[i].
//
// Emits 4 insns: ldr q (literal pool) + and.16b + addv s + umov w.
__fi static void armEmitPackLaneBits(const vixl::aarch64::Register& dst,
	const vixl::aarch64::VRegister& data, const vixl::aarch64::VRegister& tmp,
	bool reverse)
{
	// Weight vector as u32 lanes [0..3]. low64 packs lanes 0+1, high64 packs 2+3.
	//   forward {1,2,4,8}: low = (2<<32)|1, high = (8<<32)|4
	//   reverse {8,4,2,1}: low = (4<<32)|8, high = (1<<32)|2
	const u64 low64  = reverse ? 0x0000000400000008ULL : 0x0000000200000001ULL;
	const u64 high64 = reverse ? 0x0000000100000002ULL : 0x0000000800000004ULL;
	armAsm->Ldr(tmp, high64, low64);
	armAsm->And(data.V16B(), data.V16B(), tmp.V16B());
	armAsm->Addv(vixl::aarch64::VRegister(data.GetCode(), 32), data.V4S());
	armAsm->Umov(dst, data.V4S(), 0);
}

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
	// Contiguous 8-aligned copy of an arbitrary-length blob (no dedup).
	// Returns nullptr when the pool is full — callers must have a fallback.
	u8* GetBlob(const u8* bytes, size_t len);

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


