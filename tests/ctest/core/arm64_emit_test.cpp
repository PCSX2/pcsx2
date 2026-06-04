// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 recompiler-toolchain smoke test (Phase 0.6).
//
// Proves the full JIT path end-to-end on Apple Silicon: allocate executable
// (MAP_JIT) memory, emit a tiny ARM64 function through the same VIXL
// MacroAssembler lifecycle the recompilers use (armSetAsmPtr / armStartBlock /
// armEndBlock, which toggle pthread_jit_write_protect and flush the I-cache),
// then call into it and check the result. If this passes, the emission +
// execution toolchain is sound and Phase 1 (the EE skeleton) can build on it.

#include "common/Pcsx2Defs.h"

#if defined(__aarch64__)

#include "arm64/AsmHelpers.h"
#include "arm64/aR5900.h"

#include "vtlb.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <sys/mman.h>

using namespace vixl::aarch64;

namespace
{
	// RAII executable buffer, allocated exactly like PCSX2's macOS JIT regions
	// (SharedMemoryMappingArea::Create with jit=true): MAP_JIT so that
	// pthread_jit_write_protect_np can flip it between writable and executable.
	class JitBuffer
	{
	public:
		explicit JitBuffer(size_t size)
			: m_size(size)
		{
			int flags = MAP_ANONYMOUS | MAP_PRIVATE;
#ifdef __APPLE__
			flags |= MAP_JIT;
#endif
			void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
			m_ptr = (ptr == MAP_FAILED) ? nullptr : ptr;
		}

		~JitBuffer()
		{
			if (m_ptr)
				munmap(m_ptr, m_size);
		}

		JitBuffer(const JitBuffer&) = delete;
		JitBuffer& operator=(const JitBuffer&) = delete;

		void* ptr() const { return m_ptr; }
		size_t size() const { return m_size; }

	private:
		void* m_ptr = nullptr;
		size_t m_size;
	};
} // namespace

// int add(int a, int b) { return a + b; }
// Args arrive in w0/w1, return value in w0 (AAPCS64).
TEST(Arm64Emit, AddTwoArgs)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed (JIT entitlement / hardened runtime?)";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Add(w0, w0, w1);
	armAsm->Ret();
	armEndBlock();

	using AddFn = int (*)(int, int);
	AddFn fn = reinterpret_cast<AddFn>(code);
	EXPECT_EQ(fn(2, 3), 5);
	EXPECT_EQ(fn(-10, 7), -3);
	EXPECT_EQ(fn(1000000, 2345678), 3345678);
}

// u64 f() { return 0x1234567890ABCDEF; }
// Exercises 64-bit immediate materialization (multi-instruction MOV/MOVK).
TEST(Arm64Emit, ReturnConstant64)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Mov(x0, 0x1234567890ABCDEFULL);
	armAsm->Ret();
	armEndBlock();

	using Fn = u64 (*)();
	Fn fn = reinterpret_cast<Fn>(code);
	EXPECT_EQ(fn(), 0x1234567890ABCDEFULL);
}

// Loads/stores through a pointer argument, mirroring how the recompiler will
// touch guest state in memory: void store(u64* p, u64 v) { *p = v + 1; }
TEST(Arm64Emit, LoadStoreThroughPointer)
{
	JitBuffer buf(4096);
	ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

	armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
	u8* const code = armStartBlock();
	armAsm->Add(x1, x1, 1);
	armAsm->Str(x1, MemOperand(x0));
	armAsm->Ret();
	armEndBlock();

	using StoreFn = void (*)(u64*, u64);
	StoreFn fn = reinterpret_cast<StoreFn>(code);
	u64 slot = 0;
	fn(&slot, 41);
	EXPECT_EQ(slot, 42u);
}

// --------------------------------------------------------------------------------------
//  Phase 2.3: EE effective-address codegen (armEmitEffectiveAddr)
// --------------------------------------------------------------------------------------
// Validates the genuinely-new EE load/store address mode at runtime:
//   addr = GPR[rs].UL[0] + sign_extend(imm)
// reading the guest base register straight out of a cpuRegs-shaped buffer via
// RESTATEPTR (x19). The vtlb call path the full generators add on top of this is
// already proven (armEmitCall + the Phase 2.1 helpers), so this isolates the
// part that's new in 2.3 and needs no live vtlb/VM to exercise.
namespace
{
	// Emit `u32 f(void* gpr_base)` that returns armEmitEffectiveAddr(rs, imm),
	// run it against a fake GPR file, and return the computed address.
	u32 RunEffectiveAddr(u32 rs, s32 imm, const std::array<u64, 32>& gpr_lo)
	{
		// cpuRegs-shaped backing store: 32 GPRs * 16 bytes, low word at n*16.
		alignas(16) std::array<u8, 32 * 16> regfile{};
		for (u32 i = 0; i < 32; i++)
			std::memcpy(&regfile[i * 16], &gpr_lo[i], sizeof(u32));

		JitBuffer buf(4096);
		EXPECT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

		armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
		u8* const code = armStartBlock();
		// RESTATEPTR (x19) is callee-saved: preserve it, point it at the regfile.
		armAsm->Str(RESTATEPTR, MemOperand(sp, -16, PreIndex));
		armAsm->Mov(RESTATEPTR, x0);
		armEmitEffectiveAddr(w0, rs, imm); // result -> w0 (return value)
		armAsm->Ldr(RESTATEPTR, MemOperand(sp, 16, PostIndex));
		armAsm->Ret();
		armEndBlock();

		using Fn = u32 (*)(void*);
		return reinterpret_cast<Fn>(code)(regfile.data());
	}
} // namespace

TEST(Arm64EmitEE, EffectiveAddrBasePlusImm)
{
	std::array<u64, 32> gpr{};
	gpr[5] = 0x0010'0000; // $t... base
	EXPECT_EQ(RunEffectiveAddr(5, 0x40, gpr), 0x0010'0040u);
}

TEST(Arm64EmitEE, EffectiveAddrZeroImm)
{
	std::array<u64, 32> gpr{};
	gpr[7] = 0x1FC0'0000;
	EXPECT_EQ(RunEffectiveAddr(7, 0, gpr), 0x1FC0'0000u);
}

TEST(Arm64EmitEE, EffectiveAddrNegativeImm)
{
	std::array<u64, 32> gpr{};
	gpr[9] = 0x0000'1000;
	// imm is the sign-extended 16-bit MIPS immediate; -0x10 -> 0xFFF0.
	EXPECT_EQ(RunEffectiveAddr(9, -0x10, gpr), 0x0000'0FF0u);
}

TEST(Arm64EmitEE, EffectiveAddrR0IsImmOnly)
{
	std::array<u64, 32> gpr{};
	gpr[0] = 0xDEAD'BEEF; // must be ignored: $zero is hardwired to 0
	EXPECT_EQ(RunEffectiveAddr(0, 0x1234, gpr), 0x0000'1234u);
}

TEST(Arm64EmitEE, EffectiveAddrUsesLowWordOnly)
{
	std::array<u64, 32> gpr{};
	gpr[3] = 0xFFFF'FFFF'8000'0000ull; // upper 32 bits must not leak in
	EXPECT_EQ(RunEffectiveAddr(3, 0x10, gpr), 0x8000'0010u);
}

// --------------------------------------------------------------------------------------
//  Phase 2.3: 16-byte alignment of LQ/SQ effective addresses
// --------------------------------------------------------------------------------------
// armEmitLoadQuad/armEmitStoreQuad force the computed address to 16-byte alignment
// (`& ~0x0F`) before the 128-bit access, matching EE quad semantics. The vtlb call
// they wrap is already proven, so this isolates the new align step: emit exactly
// the same address sequence the quad generators do (effective addr, then mask).
namespace
{
	u32 RunAlignedQuadAddr(u32 rs, s32 imm, const std::array<u64, 32>& gpr_lo)
	{
		alignas(16) std::array<u8, 32 * 16> regfile{};
		for (u32 i = 0; i < 32; i++)
			std::memcpy(&regfile[i * 16], &gpr_lo[i], sizeof(u32));

		JitBuffer buf(4096);
		EXPECT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

		armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
		u8* const code = armStartBlock();
		armAsm->Str(RESTATEPTR, MemOperand(sp, -16, PreIndex));
		armAsm->Mov(RESTATEPTR, x0);
		armEmitEffectiveAddr(w0, rs, imm);
		armAsm->And(w0, w0, ~0x0F); // same mask the quad generators apply
		armAsm->Ldr(RESTATEPTR, MemOperand(sp, 16, PostIndex));
		armAsm->Ret();
		armEndBlock();

		using Fn = u32 (*)(void*);
		return reinterpret_cast<Fn>(code)(regfile.data());
	}
} // namespace

TEST(Arm64EmitEE, QuadAddrAligns16)
{
	std::array<u64, 32> gpr{};
	gpr[8] = 0x0010'0007; // unaligned base
	// (0x0010'0007 + 0x1B) = 0x0010'0022 -> aligned down to 0x0010'0020.
	EXPECT_EQ(RunAlignedQuadAddr(8, 0x1B, gpr), 0x0010'0020u);
}

TEST(Arm64EmitEE, QuadAddrAlreadyAligned)
{
	std::array<u64, 32> gpr{};
	gpr[8] = 0x0020'0000;
	EXPECT_EQ(RunAlignedQuadAddr(8, 0x10, gpr), 0x0020'0010u);
}

// --------------------------------------------------------------------------------------
//  Phase 2.4: full guest-memory round-trip (load/store/quad generators)
// --------------------------------------------------------------------------------------
// Exercises the complete armEmitLoadGpr / armEmitStoreGpr / armEmitLoadQuad /
// armEmitStoreQuad path against REAL guest memory: a host buffer mapped into the
// vtlb vmap, accessed through the same vtlb_memRead/Write helpers the interpreter
// uses (ground truth). This is the first runtime proof that the address calc, the
// AAPCS64 arg/return marshalling, and the sign/zero extension all compose into a
// correct memory access — not just the address arithmetic (already proven above).
//
// The slow path only consults vtlbdata.vmap; with the default EmuConfig
// (EnableEE=true) vtlb_memRead/Write reduce to a direct pointer access at
// vmap[addr>>12].assumePtr(addr), so a hand-built vmap entry pointing at a local
// buffer is sufficient — no SysMemory reservation, fastmem area, or page-fault
// handler required.
namespace
{
	using namespace vtlb_private;

	// Guest base address of the test page window. Arbitrary, page-aligned, and well
	// clear of address 0 so a stray unmapped access faults loudly instead of
	// silently reading vmap[0].
	constexpr u32 kTestVAddr = 0x0010'0000;

	// Maps `buffer` (size bytes, a multiple of the vtlb page size) at guest
	// `kTestVAddr` for the object's lifetime by writing direct-pointer vmap entries,
	// then restores the previous vmap pointer on destruction. The 4 GB / 4 KB vmap
	// (8 MB) is allocated per instance; cheap enough for the handful of tests here.
	class VtlbMapping
	{
	public:
		VtlbMapping(void* buffer, u32 size)
			: m_saved_vmap(vtlbdata.vmap)
			, m_vmap(VTLB_VMAP_ITEMS) // default VTLBVirtual{} => raw value 0
		{
			vtlbdata.vmap = m_vmap.data();
			const uptr host = reinterpret_cast<uptr>(buffer);
			for (u32 off = 0; off < size; off += VTLB_PAGE_SIZE)
			{
				vtlbdata.vmap[(kTestVAddr + off) >> VTLB_PAGE_BITS] =
					VTLBVirtual::fromPointer(host + off, kTestVAddr + off);
			}
		}

		~VtlbMapping() { vtlbdata.vmap = m_saved_vmap; }

		VtlbMapping(const VtlbMapping&) = delete;
		VtlbMapping& operator=(const VtlbMapping&) = delete;

	private:
		VTLBVirtual* m_saved_vmap;
		std::vector<VTLBVirtual> m_vmap;
	};

	// cpuRegs-shaped guest register file (32 * 16-byte GPR slots). RESTATEPTR points
	// at this; the generators read GPR[rs]/GPR[rt] and write GPR[rt] within it.
	struct GuestRegs
	{
		alignas(16) std::array<u8, 32 * 16> bytes{};

		void set64(u32 n, u64 v) { std::memcpy(&bytes[n * 16], &v, sizeof(v)); }
		u64 get64(u32 n) const
		{
			u64 v;
			std::memcpy(&v, &bytes[n * 16], sizeof(v));
			return v;
		}
		void set128(u32 n, const u8 v[16]) { std::memcpy(&bytes[n * 16], v, 16); }
		void get128(u32 n, u8 out[16]) const { std::memcpy(out, &bytes[n * 16], 16); }
		void* data() { return bytes.data(); }
	};

	// Emit `void f(void* regfile)` whose body is produced by `emit`, with RESTATEPTR
	// (x19, callee-saved) pointed at the regfile and LR preserved across the helper
	// calls the generators make. Then run it once against `regs`.
	template <typename EmitBody>
	void RunEEGen(GuestRegs& regs, EmitBody&& emit)
	{
		JitBuffer buf(8192);
		ASSERT_NE(buf.ptr(), nullptr) << "MAP_JIT allocation failed";

		armSetAsmPtr(buf.ptr(), buf.size(), nullptr);
		u8* const code = armStartBlock();
		// Preserve RESTATEPTR (caller's x19) and LR; sp stays 16-byte aligned.
		armAsm->Stp(RESTATEPTR, x30, MemOperand(sp, -16, PreIndex));
		armAsm->Mov(RESTATEPTR, x0); // x0 = regfile
		emit();
		armAsm->Ldp(RESTATEPTR, x30, MemOperand(sp, 16, PostIndex));
		armAsm->Ret();
		armEndBlock();

		reinterpret_cast<void (*)(void*)>(code)(regs.data());
	}
} // namespace

TEST(Arm64EmitEE, StoreThenLoadWord)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	GuestRegs regs;
	regs.set64(8, kTestVAddr);          // $t0 = base
	regs.set64(9, 0xFFFF'FFFF'DEAD'BEEF); // $t1 = value (only low 32 bits stored)

	// SW $t1, 0x40($t0)
	RunEEGen(regs, [] { armEmitStoreGpr(32, /*rt*/ 9, /*rs*/ 8, /*imm*/ 0x40); });

	u32 in_ram;
	std::memcpy(&in_ram, &ram[0x40], sizeof(in_ram));
	EXPECT_EQ(in_ram, 0xDEAD'BEEFu);

	// LW $t2, 0x40($t0)  — sign-extends the 32-bit value into the 64-bit GPR.
	RunEEGen(regs, [] { armEmitLoadGpr(32, /*sign*/ true, /*rt*/ 10, /*rs*/ 8, /*imm*/ 0x40); });
	EXPECT_EQ(regs.get64(10), 0xFFFF'FFFF'DEAD'BEEFull);
}

TEST(Arm64EmitEE, LoadByteSignAndZeroExtend)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());
	ram[0x10] = 0x80; // high bit set: sign-extends to 0xFFFF..80, zero-extends to 0x80

	GuestRegs regs;
	regs.set64(8, kTestVAddr);

	RunEEGen(regs, [] { armEmitLoadGpr(8, /*sign*/ true, /*rt*/ 9, /*rs*/ 8, 0x10); });
	EXPECT_EQ(regs.get64(9), 0xFFFF'FFFF'FFFF'FF80ull);

	RunEEGen(regs, [] { armEmitLoadGpr(8, /*sign*/ false, /*rt*/ 10, /*rs*/ 8, 0x10); });
	EXPECT_EQ(regs.get64(10), 0x0000'0000'0000'0080ull);
}

TEST(Arm64EmitEE, StoreLoadDoubleword)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	GuestRegs regs;
	regs.set64(8, kTestVAddr);
	regs.set64(9, 0x0123'4567'89AB'CDEFull);

	// SD $t1, 0x80($t0) ; LD $t2, 0x80($t0)
	RunEEGen(regs, [] { armEmitStoreGpr(64, 9, 8, 0x80); });
	u64 in_ram;
	std::memcpy(&in_ram, &ram[0x80], sizeof(in_ram));
	EXPECT_EQ(in_ram, 0x0123'4567'89AB'CDEFull);

	RunEEGen(regs, [] { armEmitLoadGpr(64, true, 10, 8, 0x80); });
	EXPECT_EQ(regs.get64(10), 0x0123'4567'89AB'CDEFull);
}

TEST(Arm64EmitEE, LoadStoreWritesToZeroRegDiscarded)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());
	u32 marker = 0xCAFEF00Du;
	std::memcpy(&ram[0x20], &marker, sizeof(marker));

	GuestRegs regs;
	regs.set64(8, kTestVAddr);

	// LW $zero, 0x20($t0): the load runs (side effects) but the result is discarded.
	RunEEGen(regs, [] { armEmitLoadGpr(32, true, /*rt*/ 0, /*rs*/ 8, 0x20); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero after a load targeting it";
}

TEST(Arm64EmitEE, StoreThenLoadQuad)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	GuestRegs regs;
	regs.set64(8, kTestVAddr);
	const u8 value[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	regs.set128(5, value);

	// SQ $5, 0x30($t0) — 0x30 is already 16-byte aligned.
	RunEEGen(regs, [] { armEmitStoreQuad(/*rt*/ 5, /*rs*/ 8, 0x30); });
	EXPECT_EQ(std::memcmp(&ram[0x30], value, 16), 0);

	// LQ $6, 0x30($t0)
	RunEEGen(regs, [] { armEmitLoadQuad(/*rt*/ 6, /*rs*/ 8, 0x30); });
	u8 loaded[16];
	regs.get128(6, loaded);
	EXPECT_EQ(std::memcmp(loaded, value, 16), 0);
}

TEST(Arm64EmitEE, QuadAccessForcesAlignment)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	GuestRegs regs;
	regs.set64(8, kTestVAddr);
	const u8 value[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
	regs.set128(5, value);

	// SQ $5, 0x37($t0): the EE silently aligns down to 0x30 (& ~0xF).
	RunEEGen(regs, [] { armEmitStoreQuad(5, 8, 0x37); });
	EXPECT_EQ(std::memcmp(&ram[0x30], value, 16), 0) << "quad store must align the address down";
}

// --------------------------------------------------------------------------------------
//  Phase 3.1: immediate arithmetic opcode generators
// --------------------------------------------------------------------------------------

TEST(Arm64EmitEE, ADDI_SignExtend32)
{
	GuestRegs regs;
	// 0x8000'0000 + 0x7FFF = 0x8000'7FFF  (32-bit wrap, then sign-extended to 64)
	regs.set64(2, 0xFFFF'FFFF'8000'0000ull);
	RunEEGen(regs, [] { armEmitADDI(/*rt*/ 8, /*rs*/ 2, /*imm*/ 0x7FFF); });
	EXPECT_EQ(regs.get64(8), 0xFFFF'FFFF'8000'7FFFull);
}

TEST(Arm64EmitEE, ADDI_ZeroImmIdentity)
{
	GuestRegs regs;
	regs.set64(4, 0x1234'5678'9ABC'DEF0ull);
	RunEEGen(regs, [] { armEmitADDI(/*rt*/ 9, /*rs*/ 4, /*imm*/ 0); });
	// A 32-bit add with zero imm still sign-extends the low word.
	EXPECT_EQ(regs.get64(9), 0xFFFF'FFFF'9ABC'DEF0ull);
}

TEST(Arm64EmitEE, ADDI_DiscardZero)
{
	GuestRegs regs;
	regs.set64(0, 0); // $zero
	regs.set64(5, 42);
	RunEEGen(regs, [] { armEmitADDI(/*rt*/ 0, /*rs*/ 5, /*imm*/ 1); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero after ADDI targeting it";
}

TEST(Arm64EmitEE, DADDI_64bitAdd)
{
	GuestRegs regs;
	regs.set64(3, 0x0000'0001'FFFF'FFFEull);
	RunEEGen(regs, [] { armEmitDADDI(/*rt*/ 10, /*rs*/ 3, /*imm*/ 5); });
	EXPECT_EQ(regs.get64(10), 0x0000'0002'0000'0003ull);
}

TEST(Arm64EmitEE, DADDI_NegativeImm)
{
	GuestRegs regs;
	regs.set64(3, 0x0000'0000'0000'0005ull);
	RunEEGen(regs, [] { armEmitDADDI(/*rt*/ 10, /*rs*/ 3, /*imm*/ -3); });
	EXPECT_EQ(regs.get64(10), 0x0000'0000'0000'0002ull);
}

TEST(Arm64EmitEE, SLTI_SignedLessThan)
{
	GuestRegs regs;
	regs.set64(4, -10); // signed negative
	RunEEGen(regs, [] { armEmitSLTI(/*rt*/ 11, /*rs*/ 4, /*imm*/ -5); });
	EXPECT_EQ(regs.get64(11), 1u) << "-10 < -5";

	RunEEGen(regs, [] { armEmitSLTI(/*rt*/ 11, /*rs*/ 4, /*imm*/ -20); });
	EXPECT_EQ(regs.get64(11), 0u) << "-10 < -20 is false";
}

TEST(Arm64EmitEE, SLTIU_UnsignedLessThan)
{
	GuestRegs regs;
	regs.set64(5, 5);
	RunEEGen(regs, [] { armEmitSLTIU(/*rt*/ 12, /*rs*/ 5, /*imm*/ 10); });
	EXPECT_EQ(regs.get64(12), 1u) << "5 < 10";

	RunEEGen(regs, [] { armEmitSLTIU(/*rt*/ 12, /*rs*/ 5, /*imm*/ 3); });
	EXPECT_EQ(regs.get64(12), 0u) << "5 < 3 is false";
}

TEST(Arm64EmitEE, ANDI_ZeroExtended)
{
	GuestRegs regs;
	regs.set64(6, 0xFFFF'FFFF'FFFF'FFFFull);
	RunEEGen(regs, [] { armEmitANDI(/*rt*/ 13, /*rs*/ 6, /*imm_u*/ 0x00FF); });
	EXPECT_EQ(regs.get64(13), 0x0000'0000'0000'00FFull);
}

TEST(Arm64EmitEE, ANDI_ZeroImm)
{
	GuestRegs regs;
	regs.set64(6, 0xDEAD'BEEF'CAFE'BABEull);
	RunEEGen(regs, [] { armEmitANDI(/*rt*/ 13, /*rs*/ 6, /*imm_u*/ 0); });
	EXPECT_EQ(regs.get64(13), 0u);
}

TEST(Arm64EmitEE, ORI_ZeroExtended)
{
	GuestRegs regs;
	regs.set64(7, 0x1234'5678'9ABC'DEF0ull);
	RunEEGen(regs, [] { armEmitORI(/*rt*/ 14, /*rs*/ 7, /*imm_u*/ 0x00FF); });
	EXPECT_EQ(regs.get64(14), 0x1234'5678'9ABC'DEFFull);
}

TEST(Arm64EmitEE, ORI_ZeroImmIdentity)
{
	GuestRegs regs;
	regs.set64(7, 0xABCD'EF01'2345'6789ull);
	RunEEGen(regs, [] { armEmitORI(/*rt*/ 14, /*rs*/ 7, /*imm_u*/ 0); });
	EXPECT_EQ(regs.get64(14), 0xABCD'EF01'2345'6789ull);
}

TEST(Arm64EmitEE, XORI_ZeroExtended)
{
	GuestRegs regs;
	regs.set64(8, 0x1234'5678'9ABC'DEF0ull);
	RunEEGen(regs, [] { armEmitXORI(/*rt*/ 15, /*rs*/ 8, /*imm_u*/ 0x00FF); });
	EXPECT_EQ(regs.get64(15), 0x1234'5678'9ABC'DE0Full);
}

TEST(Arm64EmitEE, LUI_SignExtend)
{
	GuestRegs regs;
	// LUI rt, 0x8000  => 0x8000'0000 sign-extended = 0xFFFF'FFFF'8000'0000
	RunEEGen(regs, [] { armEmitLUI(/*rt*/ 16, /*imm*/ 0x8000); });
	EXPECT_EQ(regs.get64(16), 0xFFFF'FFFF'8000'0000ull);
}

TEST(Arm64EmitEE, LUI_PositiveImm)
{
	GuestRegs regs;
	// LUI rt, 0x1234 => 0x1234'0000 sign-extended (positive) = same value
	RunEEGen(regs, [] { armEmitLUI(/*rt*/ 17, /*imm*/ 0x1234); });
	EXPECT_EQ(regs.get64(17), 0x0000'0000'1234'0000ull);
}

// Regression: LUI previously passed `(op>>16)&0x1f` (5 bits) instead of the
// full 16-bit immediate. Test with an imm that would truncate.
TEST(Arm64EmitEE, LUI_Full16BitImm)
{
	GuestRegs regs;
	RunEEGen(regs, [] { armEmitLUI(/*rt*/ 18, /*imm*/ 0xABCD); });
	// 0xABCD'0000 as a 32-bit value: bit 31 is 1 (0xA = 1010), so sign-extended.
	EXPECT_EQ(regs.get64(18), 0xFFFF'FFFF'ABCD'0000ull);
}

TEST(Arm64EmitEE, LUI_ZeroImm)
{
	GuestRegs regs;
	RunEEGen(regs, [] { armEmitLUI(/*rt*/ 19, /*imm*/ 0); });
	EXPECT_EQ(regs.get64(19), 0u);
}

// --------------------------------------------------------------------------------------
//  Phase 3.2: register-register arithmetic opcode generators
// --------------------------------------------------------------------------------------

TEST(Arm64EmitEE, ADD_SignExtend32Wrap)
{
	GuestRegs regs;
	// 0x8000'0000 + 0x7FFF'FFFF = 0xFFFF'FFFF (32-bit wrap), sign-extended to 64 = -1.
	regs.set64(2, 0xFFFF'FFFF'8000'0000ull);
	regs.set64(3, 0x0000'0000'7FFF'FFFFull);
	RunEEGen(regs, [] { armEmitADD(/*rd*/ 10, /*rs*/ 2, /*rt*/ 3); });
	EXPECT_EQ(regs.get64(10), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(Arm64EmitEE, ADD_RSsameRT)
{
	GuestRegs regs;
	regs.set64(4, 0x1234'5678ull);
	RunEEGen(regs, [] { armEmitADD(/*rd*/ 11, /*rs*/ 4, /*rt*/ 4); });
	// 0x1234'5678 + 0x1234'5678 = 0x2468'ACF0 as 32-bit, sign-extended = same positive.
	EXPECT_EQ(regs.get64(11), 0x0000'0000'2468'ACF0ull);
}

TEST(Arm64EmitEE, ADD_DiscardZeroRd)
{
	GuestRegs regs;
	regs.set64(5, 42);
	regs.set64(6, 1);
	RunEEGen(regs, [] { armEmitADD(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero";
}

TEST(Arm64EmitEE, ADDU_IsSameAsADD)
{
	GuestRegs regs;
	regs.set64(7, 0x0000'0000'FFFF'FFFEull);
	regs.set64(8, 0x0000'0000'0000'0005ull);
	RunEEGen(regs, [] { armEmitADDU(/*rd*/ 12, /*rs*/ 7, /*rt*/ 8); });
	// 32-bit wrap: 0xFFFF'FFFE + 5 = 0x0000'0003 (wraparound), sign-extended.
	EXPECT_EQ(regs.get64(12), 0x0000'0000'0000'0003ull);
}

TEST(Arm64EmitEE, DADD_64bitAdd)
{
	GuestRegs regs;
	regs.set64(1, 0x0000'0001'FFFF'FFFEull);
	regs.set64(2, 0x0000'0000'0000'0005ull);
	RunEEGen(regs, [] { armEmitDADD(/*rd*/ 13, /*rs*/ 1, /*rt*/ 2); });
	EXPECT_EQ(regs.get64(13), 0x0000'0002'0000'0003ull);
}

TEST(Arm64EmitEE, DADD_RSsameRT)
{
	GuestRegs regs;
	regs.set64(3, 0x1234'5678'9ABC'DEF0ull);
	RunEEGen(regs, [] { armEmitDADD(/*rd*/ 14, /*rs*/ 3, /*rt*/ 3); });
	EXPECT_EQ(regs.get64(14), 0x2468'ACF1'3579'BDE0ull);
}

TEST(Arm64EmitEE, SUB_SignExtend32Wrap)
{
	GuestRegs regs;
	regs.set64(2, 0x0000'0000'0000'0001ull); // 1
	regs.set64(3, 0x0000'0000'0000'0002ull); // 2
	RunEEGen(regs, [] { armEmitSUB(/*rd*/ 15, /*rs*/ 2, /*rt*/ 3); });
	// 1 - 2 = 0xFFFF'FFFF (32-bit wrap), sign-extended = -1.
	EXPECT_EQ(regs.get64(15), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(Arm64EmitEE, SUB_RSsameRT)
{
	GuestRegs regs;
	regs.set64(4, 0xDEAD'BEEF'CAFE'BABEull);
	RunEEGen(regs, [] { armEmitSUB(/*rd*/ 16, /*rs*/ 4, /*rt*/ 4); });
	EXPECT_EQ(regs.get64(16), 0u) << "rs - rs must be 0";
}

TEST(Arm64EmitEE, SUBU_IsSameAsSUB)
{
	GuestRegs regs;
	regs.set64(5, 0x0000'0000'0000'000Aull); // 10
	regs.set64(6, 0xFFFF'FFFF'FFFF'FFFFull); // -1 as 64-bit, low word 0xFFFFFFFF
	RunEEGen(regs, [] { armEmitSUBU(/*rd*/ 17, /*rs*/ 5, /*rt*/ 6); });
	// 10 - 0xFFFFFFFF = 11 (32-bit wrap), sign-extended positive.
	EXPECT_EQ(regs.get64(17), 0x0000'0000'0000'000Bull);
}

TEST(Arm64EmitEE, DSUB_64bitSub)
{
	GuestRegs regs;
	regs.set64(1, 0x0000'0002'0000'0000ull);
	regs.set64(2, 0x0000'0001'0000'0001ull);
	RunEEGen(regs, [] { armEmitDSUB(/*rd*/ 18, /*rs*/ 1, /*rt*/ 2); });
	EXPECT_EQ(regs.get64(18), 0x0000'0000'FFFF'FFFFull);
}

TEST(Arm64EmitEE, DSUB_RSsameRT)
{
	GuestRegs regs;
	regs.set64(3, 0xDEAD'BEEF'CAFE'BABEull);
	RunEEGen(regs, [] { armEmitDSUB(/*rd*/ 19, /*rs*/ 3, /*rt*/ 3); });
	EXPECT_EQ(regs.get64(19), 0u);
}

TEST(Arm64EmitEE, AND_Standard)
{
	GuestRegs regs;
	regs.set64(1, 0x0F0F'0F0F'0F0F'0F0Full);
	regs.set64(2, 0x00FF'00FF'00FF'00FFull);
	RunEEGen(regs, [] { armEmitAND(/*rd*/ 20, /*rs*/ 1, /*rt*/ 2); });
	EXPECT_EQ(regs.get64(20), 0x000F'000F'000F'000Full);
}

TEST(Arm64EmitEE, AND_RSsameRT)
{
	GuestRegs regs;
	regs.set64(3, 0xDEAD'BEEF'CAFE'BABEull);
	RunEEGen(regs, [] { armEmitAND(/*rd*/ 21, /*rs*/ 3, /*rt*/ 3); });
	EXPECT_EQ(regs.get64(21), 0xDEAD'BEEF'CAFE'BABEull);
}

TEST(Arm64EmitEE, OR_Standard)
{
	GuestRegs regs;
	regs.set64(4, 0x0F0F'0F0F'0F0F'0F0Full);
	regs.set64(5, 0xF0F0'F0F0'F0F0'F0F0ull);
	RunEEGen(regs, [] { armEmitOR(/*rd*/ 22, /*rs*/ 4, /*rt*/ 5); });
	EXPECT_EQ(regs.get64(22), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(Arm64EmitEE, OR_RSsameRT)
{
	GuestRegs regs;
	regs.set64(6, 0x1234'5678'9ABC'DEF0ull);
	RunEEGen(regs, [] { armEmitOR(/*rd*/ 23, /*rs*/ 6, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(23), 0x1234'5678'9ABC'DEF0ull);
}

TEST(Arm64EmitEE, XOR_Standard)
{
	GuestRegs regs;
	regs.set64(7, 0xAAAA'AAAA'5555'5555ull);
	regs.set64(8, 0x5555'5555'AAAA'AAAAull);
	RunEEGen(regs, [] { armEmitXOR(/*rd*/ 24, /*rs*/ 7, /*rt*/ 8); });
	EXPECT_EQ(regs.get64(24), 0xFFFF'FFFF'FFFF'FFFFull);
}

TEST(Arm64EmitEE, XOR_RSsameRT)
{
	GuestRegs regs;
	regs.set64(9, 0xDEAD'BEEF'CAFE'BABEull);
	RunEEGen(regs, [] { armEmitXOR(/*rd*/ 25, /*rs*/ 9, /*rt*/ 9); });
	EXPECT_EQ(regs.get64(25), 0u) << "rs ^ rs must be 0";
}

TEST(Arm64EmitEE, NOR_Standard)
{
	GuestRegs regs;
	regs.set64(10, 0x0F0F'0F0F'0F0F'0F0Full);
	regs.set64(11, 0x00FF'00FF'00FF'00FFull);
	RunEEGen(regs, [] { armEmitNOR(/*rd*/ 26, /*rs*/ 10, /*rt*/ 11); });
	// ~(0x0F0F... | 0x00FF...) = ~0x0FFF'0FFF'0FFF'0FFF = 0xF000'F000'F000'F000.
	EXPECT_EQ(regs.get64(26), 0xF000'F000'F000'F000ull);
}

TEST(Arm64EmitEE, NOR_RSsameRT)
{
	GuestRegs regs;
	regs.set64(12, 0xFFFF'0000'FFFF'0000ull);
	RunEEGen(regs, [] { armEmitNOR(/*rd*/ 27, /*rs*/ 12, /*rt*/ 12); });
	// ~(rs | rs) == ~rs == 0x0000'FFFF'0000'FFFF.
	EXPECT_EQ(regs.get64(27), 0x0000'FFFF'0000'FFFFull);
}

TEST(Arm64EmitEE, SLT_SignedLess)
{
	GuestRegs regs;
	regs.set64(1, -10); // signed negative
	regs.set64(2, -5);
	RunEEGen(regs, [] { armEmitSLT(/*rd*/ 28, /*rs*/ 1, /*rt*/ 2); });
	EXPECT_EQ(regs.get64(28), 1u) << "-10 < -5";
}

TEST(Arm64EmitEE, SLT_SignedGreater)
{
	GuestRegs regs;
	regs.set64(3, 100);
	regs.set64(4, 50);
	RunEEGen(regs, [] { armEmitSLT(/*rd*/ 29, /*rs*/ 3, /*rt*/ 4); });
	EXPECT_EQ(regs.get64(29), 0u) << "100 < 50 is false";
}

TEST(Arm64EmitEE, SLTU_UnsignedLess)
{
	GuestRegs regs;
	regs.set64(5, 5);
	regs.set64(6, 10);
	RunEEGen(regs, [] { armEmitSLTU(/*rd*/ 30, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(30), 1u) << "5 < 10";
}

TEST(Arm64EmitEE, SLTU_UnsignedGreaterWithSignBit)
{
	GuestRegs regs;
	// 0xFFFF'FFFF'FFFF'FFFE (as u64: very large) vs 5.
	regs.set64(7, 0xFFFF'FFFF'FFFF'FFFEull);
	regs.set64(8, 5);
	RunEEGen(regs, [] { armEmitSLTU(/*rd*/ 31, /*rs*/ 7, /*rt*/ 8); });
	EXPECT_EQ(regs.get64(31), 0u)
		<< "0xFFFF...FFFE < 5 is false in unsigned";
}

// --------------------------------------------------------------------------------------
//  Phase 3.3: shift opcode generators
// --------------------------------------------------------------------------------------

TEST(Arm64EmitEE, SLL_ShiftAndSignExtend)
{
	GuestRegs regs;
	// Low word 0x4000'0001 << 1 = 0x8000'0002. As a 32-bit value bit 31 is set,
	// so sign-extended to 64 = 0xFFFF'FFFF'8000'0002.
	regs.set64(2, 0x0000'0000'4000'0001ull);
	RunEEGen(regs, [] { armEmitSLL(/*rd*/ 10, /*rt*/ 2, /*sa*/ 1); });
	EXPECT_EQ(regs.get64(10), 0xFFFF'FFFF'8000'0002ull);
}

TEST(Arm64EmitEE, SLL_ZeroAmount)
{
	GuestRegs regs;
	// SLL by 0 should just sign-extend the low word.
	regs.set64(3, 0xFFFF'FFFF'7FFF'FFFEull); // low word 0x7FFF_FFFE => bit 31 clear => positive
	RunEEGen(regs, [] { armEmitSLL(/*rd*/ 11, /*rt*/ 3, /*sa*/ 0); });
	EXPECT_EQ(regs.get64(11), 0x0000'0000'7FFF'FFFEull);
}

TEST(Arm64EmitEE, SRL_LosesSignBit)
{
	GuestRegs regs;
	// 0x8000'0001 >> 2 = 0x2000'0000 (bit 31 clear, positive).
	regs.set64(4, 0xFFFF'FFFF'8000'0001ull);
	RunEEGen(regs, [] { armEmitSRL(/*rd*/ 12, /*rt*/ 4, /*sa*/ 2); });
	EXPECT_EQ(regs.get64(12), 0x0000'0000'2000'0000ull);
}

TEST(Arm64EmitEE, SRA_PreservesSign)
{
	GuestRegs regs;
	// (s32)0x8000'0000 >> 1 = 0xC000'0000; sign-extended = 0xFFFF'FFFF'C000'0000.
	regs.set64(5, 0x0000'0000'8000'0000ull);
	RunEEGen(regs, [] { armEmitSRA(/*rd*/ 13, /*rt*/ 5, /*sa*/ 1); });
	EXPECT_EQ(regs.get64(13), 0xFFFF'FFFF'C000'0000ull);
}

TEST(Arm64EmitEE, SLLV_VariableAmount)
{
	GuestRegs regs;
	// GPR[rs]=3, GPR[rt]=0x0000'0001 => 1 << 3 = 8, sign-extended positive.
	regs.set64(6, 3);
	regs.set64(7, 1);
	RunEEGen(regs, [] { armEmitSLLV(/*rd*/ 14, /*rt*/ 7, /*rs*/ 6); });
	EXPECT_EQ(regs.get64(14), 0x0000'0000'0000'0008ull);
}

TEST(Arm64EmitEE, SRLV_VariableAmount)
{
	GuestRegs regs;
	// GPR[rs]=4, GPR[rt]=0xFFFF'FFFF => 0x0FFF'FFFF, sign-extended positive.
	regs.set64(8, 4);
	regs.set64(9, 0xFFFF'FFFF'FFFF'FFFFull);
	RunEEGen(regs, [] { armEmitSRLV(/*rd*/ 15, /*rt*/ 9, /*rs*/ 8); });
	EXPECT_EQ(regs.get64(15), 0x0000'0000'0FFF'FFFFull);
}

TEST(Arm64EmitEE, SRAV_VariableAmount)
{
	GuestRegs regs;
	// GPR[rs]=1, GPR[rt]=(s32)0x8000'0000 => 0xC000'0000, sign-extended negative.
	regs.set64(10, 1);
	regs.set64(11, 0x0000'0000'8000'0000ull);
	RunEEGen(regs, [] { armEmitSRAV(/*rd*/ 16, /*rt*/ 11, /*rs*/ 10); });
	EXPECT_EQ(regs.get64(16), 0xFFFF'FFFF'C000'0000ull);
}

TEST(Arm64EmitEE, DSLLV_64bitVariable)
{
	GuestRegs regs;
	// GPR[rs]=8, GPR[rt]=0x0000'0001'0000'0000 => << 8 => 0x0000'0100'0000'0000.
	regs.set64(12, 8);
	regs.set64(13, 0x0000'0001'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSLLV(/*rd*/ 17, /*rt*/ 13, /*rs*/ 12); });
	EXPECT_EQ(regs.get64(17), 0x0000'0100'0000'0000ull);
}

TEST(Arm64EmitEE, DSRLV_64bitVariable)
{
	GuestRegs regs;
	// GPR[rs]=16, GPR[rt]=0x0001'0000'0000'0000 => >> 16 => 0x0000'0001'0000'0000.
	regs.set64(14, 16);
	regs.set64(15, 0x0001'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRLV(/*rd*/ 18, /*rt*/ 15, /*rs*/ 14); });
	EXPECT_EQ(regs.get64(18), 0x0000'0001'0000'0000ull);
}

TEST(Arm64EmitEE, DSRAV_64bitVariable)
{
	GuestRegs regs;
	// GPR[rs]=4, GPR[rt]=(s64)0xF000'0000'0000'0000 => >> 4 => 0xFF00'0000'0000'0000.
	regs.set64(14, 4);
	regs.set64(15, 0xF000'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRAV(/*rd*/ 18, /*rt*/ 15, /*rs*/ 14); });
	EXPECT_EQ(regs.get64(18), 0xFF00'0000'0000'0000ull);
}

TEST(Arm64EmitEE, DSLL_64bitImmediate)
{
	GuestRegs regs;
	regs.set64(16, 0x0000'0000'0000'0001ull);
	RunEEGen(regs, [] { armEmitDSLL(/*rd*/ 19, /*rt*/ 16, /*sa*/ 40); });
	EXPECT_EQ(regs.get64(19), 0x0000'0100'0000'0000ull);
}

TEST(Arm64EmitEE, DSRL_64bitImmediate)
{
	GuestRegs regs;
	regs.set64(17, 0x0000'0100'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRL(/*rd*/ 20, /*rt*/ 17, /*sa*/ 40); });
	EXPECT_EQ(regs.get64(20), 0x0000'0000'0000'0001ull);
}

TEST(Arm64EmitEE, DSRA_64bitImmediate)
{
	GuestRegs regs;
	regs.set64(18, 0xF000'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRA(/*rd*/ 21, /*rt*/ 18, /*sa*/ 4); });
	EXPECT_EQ(regs.get64(21), 0xFF00'0000'0000'0000ull);
}

TEST(Arm64EmitEE, DSLL32_Adds32ToShift)
{
	GuestRegs regs;
	// DSLL32 rt, 0, 1 => 1 << (1+32) = 1 << 33.
	regs.set64(19, 0x0000'0000'0000'0001ull);
	RunEEGen(regs, [] { armEmitDSLL32(/*rd*/ 22, /*rt*/ 19, /*sa*/ 1); });
	EXPECT_EQ(regs.get64(22), 0x0000'0002'0000'0000ull);
}

TEST(Arm64EmitEE, DSRL32_Adds32ToShift)
{
	GuestRegs regs;
	// DSRL32 rt, 0, 2 => 0x0004'0000'0000'0000 (== 2^50) >> (2+32) = 2^16 = 0x0000'0001'0000.
	regs.set64(20, 0x0004'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRL32(/*rd*/ 23, /*rt*/ 20, /*sa*/ 2); });
	EXPECT_EQ(regs.get64(23), 0x0000'0000'0001'0000ull) << "2^50 >> 34 = 2^16 = 65536";
}

TEST(Arm64EmitEE, DSRA32_Adds32ToShift)
{
	GuestRegs regs;
	// DSRA32 rt, 0, 1 => (s64)0x8000'0000'0000'0000 >> (1+32) = 0xFFFF'FFFF'C000'0000.
	regs.set64(21, 0x8000'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitDSRA32(/*rd*/ 24, /*rt*/ 21, /*sa*/ 1); });
	EXPECT_EQ(regs.get64(24), 0xFFFF'FFFF'C000'0000ull);
}

TEST(Arm64EmitEE, Shift_DiscardZeroRd)
{
	GuestRegs regs;
	regs.set64(0, 0);
	regs.set64(5, 42);
	RunEEGen(regs, [] { armEmitSLL(/*rd*/ 0, /*rt*/ 5, /*sa*/ 1); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero after a shift targeting it";
}

TEST(Arm64EmitEE, SLLV_AmountMaskedTo5Bits)
{
	GuestRegs regs;
	// ARM64 variable Lsl on W-reg uses low 5 bits of amount. 33 & 0x1f = 1.
	// GPR[rs]=33, GPR[rt]=1 => 1 << 1 = 2.
	regs.set64(10, 33);
	regs.set64(11, 1);
	RunEEGen(regs, [] { armEmitSLLV(/*rd*/ 12, /*rt*/ 11, /*rs*/ 10); });
	EXPECT_EQ(regs.get64(12), 2u);
}

TEST(Arm64EmitEE, DSLLV_AmountMaskedTo6Bits)
{
	GuestRegs regs;
	// ARM64 variable Lsl on X-reg uses low 6 bits. 65 & 0x3f = 1.
	// GPR[rs]=65, GPR[rt]=1 => 1 << 1 = 2.
	regs.set64(13, 65);
	regs.set64(14, 1);
	RunEEGen(regs, [] { armEmitDSLLV(/*rd*/ 15, /*rt*/ 14, /*rs*/ 13); });
	EXPECT_EQ(regs.get64(15), 2u);
}

#endif // __aarch64__
