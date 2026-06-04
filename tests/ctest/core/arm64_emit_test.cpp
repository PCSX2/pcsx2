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
#include <cmath>
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

	// cpuRegs-shaped guest register file (32 * 16-byte GPR slots + HI/LO).
	// RESTATEPTR points at this; the generators read GPR[rs]/GPR[rt] and write
	// GPR[rt] within it. HI/LO are at indices 32 and 33 (see EE_HI_OFFSET/EE_LO_OFFSET).
	struct GuestRegs
	{
		// Sized to reach the FPU register file (fpuRegs), which lives in the same
		// cpuRegistersPack as cpuRegs at EE_FPU_BASE. That offset is past cpuRegs.pc,
		// so this also covers the GPR/HI/LO/pc range the integer+branch tests use.
		// 32 GPRs + HI + LO live in the first 34*16 bytes; CP0/sa/IsDelaySlot sit
		// between LO and pc; fpr[]/fprc[]/ACC follow at EE_FPU_BASE.
		alignas(16) std::array<u8, EE_FPU_BASE + sizeof(fpuRegisters)> bytes{};

		void set64(u32 n, u64 v) { std::memcpy(&bytes[n * 16], &v, sizeof(v)); }
		u64 get64(u32 n) const
		{
			u64 v;
			std::memcpy(&v, &bytes[n * 16], sizeof(v));
			return v;
		}
		void setHI(u64 v) { std::memcpy(&bytes[32 * 16], &v, sizeof(v)); }
		u64 getHI() const
		{
			u64 v;
			std::memcpy(&v, &bytes[32 * 16], sizeof(v));
			return v;
		}
		void setLO(u64 v) { std::memcpy(&bytes[33 * 16], &v, sizeof(v)); }
		u64 getLO() const
		{
			u64 v;
			std::memcpy(&v, &bytes[33 * 16], sizeof(v));
			return v;
		}
		// Second-pipeline results (MULT1/DIV1 family): HI.UD[1]/LO.UD[1] at +8.
		u64 getHI1() const
		{
			u64 v;
			std::memcpy(&v, &bytes[32 * 16 + 8], sizeof(v));
			return v;
		}
		u64 getLO1() const
		{
			u64 v;
			std::memcpy(&v, &bytes[33 * 16 + 8], sizeof(v));
			return v;
		}
		void set128(u32 n, const u8 v[16]) { std::memcpy(&bytes[n * 16], v, 16); }
		void get128(u32 n, u8 out[16]) const { std::memcpy(out, &bytes[n * 16], 16); }
		// FPU register file: fpr[n] (32-bit) and fprc[n] (control regs, [31]=FCR31).
		void setFPR(u32 n, u32 v) { std::memcpy(&bytes[EE_FPR_OFFSET(n)], &v, sizeof(v)); }
		u32 getFPR(u32 n) const
		{
			u32 v;
			std::memcpy(&v, &bytes[EE_FPR_OFFSET(n)], sizeof(v));
			return v;
		}
		void setFPRC(u32 n, u32 v) { std::memcpy(&bytes[EE_FPRC_OFFSET(n)], &v, sizeof(v)); }
		u32 getFPRC(u32 n) const
		{
			u32 v;
			std::memcpy(&v, &bytes[EE_FPRC_OFFSET(n)], sizeof(v));
			return v;
		}
		void setACC(u32 v) { std::memcpy(&bytes[EE_ACC_OFFSET], &v, sizeof(v)); }
		u32 getACC() const
		{
			u32 v;
			std::memcpy(&v, &bytes[EE_ACC_OFFSET], sizeof(v));
			return v;
		}
		void setPc(u32 v) { std::memcpy(&bytes[EE_PC_OFFSET], &v, sizeof(v)); }
		u32 getPc() const
		{
			u32 v;
			std::memcpy(&v, &bytes[EE_PC_OFFSET], sizeof(v));
			return v;
		}
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

// --------------------------------------------------------------------------------------
//  Phase 3.4: Move opcode generators
// --------------------------------------------------------------------------------------

TEST(Arm64EmitEE, MOVZ_ConditionTrue)
{
	GuestRegs regs;
	// Rt == 0, so Rd should become Rs.
	regs.set64(5, 0xDEADBEEFCAFEBABEull);  // Rs
	regs.set64(6, 0ull);                    // Rt (condition: zero)
	regs.set64(7, 0x1111111111111111ull);  // Rd (original value, will be overwritten)
	RunEEGen(regs, [] { armEmitMOVZ(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(7), 0xDEADBEEFCAFEBABEull);
}

TEST(Arm64EmitEE, MOVZ_ConditionFalse)
{
	GuestRegs regs;
	// Rt != 0, so Rd should stay unchanged.
	regs.set64(5, 0xDEADBEEFCAFEBABEull);  // Rs
	regs.set64(6, 1ull);                    // Rt (condition: non-zero)
	regs.set64(7, 0x1111111111111111ull);  // Rd (should stay)
	RunEEGen(regs, [] { armEmitMOVZ(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(7), 0x1111111111111111ull);
}

TEST(Arm64EmitEE, MOVZ_DiscardZeroRd)
{
	GuestRegs regs;
	regs.set64(5, 42);
	regs.set64(6, 0);
	RunEEGen(regs, [] { armEmitMOVZ(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero";
}

TEST(Arm64EmitEE, MOVZ_RsSameAsRd)
{
	GuestRegs regs;
	// If Rs == Rd, the operation is a no-op.
	regs.set64(5, 0x1234567890ABCDEFull);
	regs.set64(6, 0ull);
	RunEEGen(regs, [] { armEmitMOVZ(/*rd*/ 5, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(5), 0x1234567890ABCDEFull) << "Rs==Rd should be a no-op";
}

TEST(Arm64EmitEE, MOVN_ConditionTrue)
{
	GuestRegs regs;
	// Rt != 0, so Rd should become Rs.
	regs.set64(5, 0xCAFEBABE12345678ull);  // Rs
	regs.set64(6, 42ull);                   // Rt (condition: non-zero)
	regs.set64(7, 0x2222222222222222ull);  // Rd (original value, will be overwritten)
	RunEEGen(regs, [] { armEmitMOVN(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(7), 0xCAFEBABE12345678ull);
}

TEST(Arm64EmitEE, MOVN_ConditionFalse)
{
	GuestRegs regs;
	// Rt == 0, so Rd should stay unchanged.
	regs.set64(5, 0xCAFEBABE12345678ull);  // Rs
	regs.set64(6, 0ull);                    // Rt (condition: zero)
	regs.set64(7, 0x2222222222222222ull);  // Rd (should stay)
	RunEEGen(regs, [] { armEmitMOVN(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(7), 0x2222222222222222ull);
}

TEST(Arm64EmitEE, MOVN_DiscardZeroRd)
{
	GuestRegs regs;
	regs.set64(5, 42);
	regs.set64(6, 1);
	RunEEGen(regs, [] { armEmitMOVN(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero";
}

TEST(Arm64EmitEE, MOVN_RsSameAsRd)
{
	GuestRegs regs;
	// If Rs == Rd, the operation is a no-op.
	regs.set64(5, 0xFEDCBA9876543210ull);
	regs.set64(6, 1ull);
	RunEEGen(regs, [] { armEmitMOVN(/*rd*/ 5, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.get64(5), 0xFEDCBA9876543210ull) << "Rs==Rd should be a no-op";
}

TEST(Arm64EmitEE, MFHI_MoveFromHI)
{
	GuestRegs regs;
	regs.setHI(0xABCDEF0123456789ull);
	RunEEGen(regs, [] { armEmitMFHI(/*rd*/ 10); });
	EXPECT_EQ(regs.get64(10), 0xABCDEF0123456789ull);
}

TEST(Arm64EmitEE, MFHI_DiscardZeroRd)
{
	GuestRegs regs;
	regs.setHI(0x1234567890ABCDEFull);
	RunEEGen(regs, [] { armEmitMFHI(/*rd*/ 0); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero";
}

TEST(Arm64EmitEE, MTHI_MoveToHI)
{
	GuestRegs regs;
	regs.set64(5, 0x9876543210FEDCBAull);
	RunEEGen(regs, [] { armEmitMTHI(/*rs*/ 5); });
	EXPECT_EQ(regs.getHI(), 0x9876543210FEDCBAull);
}

TEST(Arm64EmitEE, MFLO_MoveFromLO)
{
	GuestRegs regs;
	regs.setLO(0x1111222233334444ull);
	RunEEGen(regs, [] { armEmitMFLO(/*rd*/ 11); });
	EXPECT_EQ(regs.get64(11), 0x1111222233334444ull);
}

TEST(Arm64EmitEE, MFLO_DiscardZeroRd)
{
	GuestRegs regs;
	regs.setLO(0x5555666677778888ull);
	RunEEGen(regs, [] { armEmitMFLO(/*rd*/ 0); });
	EXPECT_EQ(regs.get64(0), 0u) << "$zero must stay zero";
}

TEST(Arm64EmitEE, MTLO_MoveToLO)
{
	GuestRegs regs;
	regs.set64(6, 0xAABBCCDDEEFF0011ull);
	RunEEGen(regs, [] { armEmitMTLO(/*rs*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xAABBCCDDEEFF0011ull);
}

// --------------------------------------------------------------------------------------
//  Phase 3.5: multiply/divide opcode generators
// --------------------------------------------------------------------------------------

TEST(Arm64EmitEE, MULT_Signed32x32Positive)
{
	GuestRegs regs;
	// 100 * 50 = 5000
	regs.set64(5, 100);
	regs.set64(6, 50);
	RunEEGen(regs, [] { armEmitMULT(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 5000ull);
	EXPECT_EQ(regs.getHI(), 0ull);
}

TEST(Arm64EmitEE, MULT_Signed32x32Negative)
{
	GuestRegs regs;
	// (-100) * 50 = -5000
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100 as s64
	regs.set64(6, 50);
	RunEEGen(regs, [] { armEmitMULT(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'EC78ull); // -5000 sign-extended
	EXPECT_EQ(regs.getHI(), 0xFFFF'FFFF'FFFF'FFFFull); // -1 sign-extended
}

TEST(Arm64EmitEE, MULT_Signed32x32LargePositive)
{
	GuestRegs regs;
	// 0x7FFF'FFFF * 2 = 0xFFFF'FFFE (positive, fits in 32-bit signed)
	regs.set64(5, 0x0000'0000'7FFF'FFFFull);
	regs.set64(6, 2);
	RunEEGen(regs, [] { armEmitMULT(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'FFFEull); // -2 sign-extended (overflow to negative)
	EXPECT_EQ(regs.getHI(), 0ull);
}

TEST(Arm64EmitEE, MULT_WritesRdWhenNonZero)
{
	GuestRegs regs;
	// R5900 3-operand form: GPR[rd] = LO (the sign-extended low 32 bits).
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100
	regs.set64(6, 50);
	RunEEGen(regs, [] { armEmitMULT(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'EC78ull); // -5000
	EXPECT_EQ(regs.get64(7), 0xFFFF'FFFF'FFFF'EC78ull); // Rd = LO
}

TEST(Arm64EmitEE, MULTU_Unsigned32x32)
{
	GuestRegs regs;
	// 0xFFFF'FFFF * 2 = 0x1'FFFF'FFFE (33-bit result).
	// LO = (s32)(low 32) = (s32)0xFFFF'FFFE = -2 → sign-extended (interpreter
	// sign-extends even for MULTU). HI = (s32)(high 32) = 1.
	regs.set64(5, 0x0000'0000'FFFF'FFFFull);
	regs.set64(6, 2);
	RunEEGen(regs, [] { armEmitMULTU(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'FFFEull);
	EXPECT_EQ(regs.getHI(), 0x0000'0000'0000'0001ull);
}

TEST(Arm64EmitEE, MULTU_Unsigned32x32Small)
{
	GuestRegs regs;
	// 100 * 50 = 5000
	regs.set64(5, 100);
	regs.set64(6, 50);
	RunEEGen(regs, [] { armEmitMULTU(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 5000ull);
	EXPECT_EQ(regs.getHI(), 0ull);
}

TEST(Arm64EmitEE, DIV_Signed32Positive)
{
	GuestRegs regs;
	// 100 / 7 = 14 remainder 2
	regs.set64(5, 100);
	regs.set64(6, 7);
	RunEEGen(regs, [] { armEmitDIV(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 14ull);
	EXPECT_EQ(regs.getHI(), 2ull);
}

TEST(Arm64EmitEE, DIV_Signed32Negative)
{
	GuestRegs regs;
	// (-100) / 7 = -14 remainder -2
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100
	regs.set64(6, 7);
	RunEEGen(regs, [] { armEmitDIV(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'FFF2ull); // -14
	EXPECT_EQ(regs.getHI(), 0xFFFF'FFFF'FFFF'FFFEull); // -2
}

TEST(Arm64EmitEE, DIV_OverflowCase)
{
	GuestRegs regs;
	// 0x80000000 / -1 = 0x80000000 (overflow), remainder 0 — ARM SDIV reproduces this.
	regs.set64(5, 0xFFFF'FFFF'8000'0000ull); // -2147483648
	regs.set64(6, 0xFFFF'FFFF'FFFF'FFFFull); // -1
	RunEEGen(regs, [] { armEmitDIV(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'8000'0000ull);
	EXPECT_EQ(regs.getHI(), 0ull);
}

TEST(Arm64EmitEE, DIV_DivideByZero)
{
	GuestRegs regs;
	// 100 / 0 → quotient -1 (rs >= 0), remainder = rs = 100
	regs.set64(5, 100);
	regs.set64(6, 0);
	RunEEGen(regs, [] { armEmitDIV(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'FFFFull); // -1
	EXPECT_EQ(regs.getHI(), 100ull);
}

TEST(Arm64EmitEE, DIV_DivideByZeroNegative)
{
	GuestRegs regs;
	// (-100) / 0 → quotient 1 (rs < 0), remainder = rs = -100
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100
	regs.set64(6, 0);
	RunEEGen(regs, [] { armEmitDIV(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 1ull);
	EXPECT_EQ(regs.getHI(), 0xFFFF'FFFF'FFFF'FF9Cull);
}

TEST(Arm64EmitEE, DIVU_Unsigned32Positive)
{
	GuestRegs regs;
	// 100 / 7 = 14 remainder 2
	regs.set64(5, 100);
	regs.set64(6, 7);
	RunEEGen(regs, [] { armEmitDIVU(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 14ull);
	EXPECT_EQ(regs.getHI(), 2ull);
}

TEST(Arm64EmitEE, DIVU_UnsignedLarge)
{
	GuestRegs regs;
	// 0xFFFF'FFFF / 2 = 0x7FFF'FFFF remainder 1 (quotient sign-extends to itself)
	regs.set64(5, 0x0000'0000'FFFF'FFFFull);
	regs.set64(6, 2);
	RunEEGen(regs, [] { armEmitDIVU(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0x0000'0000'7FFF'FFFFull);
	EXPECT_EQ(regs.getHI(), 1ull);
}

TEST(Arm64EmitEE, DIVU_DivideByZero)
{
	GuestRegs regs;
	// 100 / 0 → quotient -1 (full 64-bit per interpreter), remainder = rs = 100
	regs.set64(5, 100);
	regs.set64(6, 0);
	RunEEGen(regs, [] { armEmitDIVU(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO(), 0xFFFF'FFFF'FFFF'FFFFull);
	EXPECT_EQ(regs.getHI(), 100ull);
}

// The "1" variants run on the second multiplier pipeline: results land in
// HI1/LO1 (HI.UD[1]/LO.UD[1]); the base HI/LO[0] must be left untouched.

TEST(Arm64EmitEE, MULT1_Pipeline1)
{
	GuestRegs regs;
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100
	regs.set64(6, 50);
	RunEEGen(regs, [] { armEmitMULT1(/*rd*/ 7, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO1(), 0xFFFF'FFFF'FFFF'EC78ull); // -5000 → LO1
	EXPECT_EQ(regs.getHI1(), 0xFFFF'FFFF'FFFF'FFFFull); // -1   → HI1
	EXPECT_EQ(regs.get64(7), 0xFFFF'FFFF'FFFF'EC78ull); // Rd = LO.UD[1]
	EXPECT_EQ(regs.getLO(), 0ull);                      // base LO untouched
	EXPECT_EQ(regs.getHI(), 0ull);                      // base HI untouched
}

TEST(Arm64EmitEE, MULTU1_Pipeline1)
{
	GuestRegs regs;
	regs.set64(5, 0x0000'0000'FFFF'FFFFull);
	regs.set64(6, 2);
	RunEEGen(regs, [] { armEmitMULTU1(/*rd*/ 0, /*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO1(), 0xFFFF'FFFF'FFFF'FFFEull);
	EXPECT_EQ(regs.getHI1(), 0x0000'0000'0000'0001ull);
}

TEST(Arm64EmitEE, DIV1_Pipeline1)
{
	GuestRegs regs;
	regs.set64(5, 0xFFFF'FFFF'FFFF'FF9Cull); // -100
	regs.set64(6, 7);
	RunEEGen(regs, [] { armEmitDIV1(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO1(), 0xFFFF'FFFF'FFFF'FFF2ull); // -14
	EXPECT_EQ(regs.getHI1(), 0xFFFF'FFFF'FFFF'FFFEull); // -2
	EXPECT_EQ(regs.getLO(), 0ull);                      // base untouched
}

TEST(Arm64EmitEE, DIVU1_Pipeline1)
{
	GuestRegs regs;
	regs.set64(5, 100);
	regs.set64(6, 0); // divide-by-zero on pipeline 1
	RunEEGen(regs, [] { armEmitDIVU1(/*rs*/ 5, /*rt*/ 6); });
	EXPECT_EQ(regs.getLO1(), 0xFFFF'FFFF'FFFF'FFFFull); // -1
	EXPECT_EQ(regs.getHI1(), 100ull);
}

// ---------------------------------------------------------------------------
// Jumps (Phase 4.1). The generators write only cpuRegs.pc (+ the link GPR);
// the block compiler handles the delay slot separately.
// ---------------------------------------------------------------------------

TEST(Arm64EmitEE, J_SetsPc)
{
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	RunEEGen(regs, [] { armEmitJ(0x0012'3456); });
	EXPECT_EQ(regs.getPc(), 0x0012'3456u);
}

TEST(Arm64EmitEE, JAL_SetsPcAndLink)
{
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	regs.set64(31, 0xDEAD'BEEF'DEAD'BEEFull);
	RunEEGen(regs, [] { armEmitJAL(/*target*/ 0x0012'3456, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), 0x0012'3456u);
	EXPECT_EQ(regs.get64(31), 0x0000'0000'0010'0008ull); // zero-extended return addr
}

TEST(Arm64EmitEE, JR_TargetFromRegLowWord)
{
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	regs.set64(9, 0xFFFF'FFFF'0020'0000ull); // only the low word is the target
	RunEEGen(regs, [] { armEmitJR(/*rs*/ 9); });
	EXPECT_EQ(regs.getPc(), 0x0020'0000u);
}

TEST(Arm64EmitEE, JALR_TargetFromRegAndLink)
{
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	regs.set64(9, 0x0020'0000);
	regs.set64(8, 0xDEAD'BEEF'DEAD'BEEFull);
	RunEEGen(regs, [] { armEmitJALR(/*rd*/ 8, /*rs*/ 9, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), 0x0020'0000u);
	EXPECT_EQ(regs.get64(8), 0x0000'0000'0010'0008ull);
}

TEST(Arm64EmitEE, JALR_RdEqualsRsJumpsToOriginal)
{
	// rd == rs: the jump must target the original GPR[rs], not the link we write.
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	regs.set64(9, 0x0020'0000);
	RunEEGen(regs, [] { armEmitJALR(/*rd*/ 9, /*rs*/ 9, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), 0x0020'0000u);          // jumped to the original value
	EXPECT_EQ(regs.get64(9), 0x0000'0000'0010'0008ull); // and the reg now holds the link
}

TEST(Arm64EmitEE, JALR_DiscardLinkToZeroReg)
{
	GuestRegs regs;
	regs.setPc(0x0010'0000);
	regs.set64(9, 0x0020'0000);
	RunEEGen(regs, [] { armEmitJALR(/*rd*/ 0, /*rs*/ 9, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), 0x0020'0000u);
	EXPECT_EQ(regs.get64(0), 0ull); // $zero stays zero
}

// ---------------------------------------------------------------------------
// Conditional branches (Phase 4.2). target = taken, fallthrough = not taken.
// ---------------------------------------------------------------------------
static constexpr u32 kTaken = 0x0020'0000;
static constexpr u32 kFall = 0x0010'0008;

TEST(Arm64EmitEE, BEQ_TakenAndNotTaken)
{
	GuestRegs regs;
	regs.set64(5, 0x1111'2222'3333'4444ull);
	regs.set64(6, 0x1111'2222'3333'4444ull);
	RunEEGen(regs, [] { armEmitBEQ(5, 6, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(6, 0x1111'2222'3333'4445ull); // differ in low bit
	RunEEGen(regs, [] { armEmitBEQ(5, 6, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BEQ_ComparesFull64Bits)
{
	// Differ only in the high word -> not equal (must be a 64-bit compare).
	GuestRegs regs;
	regs.set64(5, 0x0000'0001'0000'0000ull);
	regs.set64(6, 0x0000'0000'0000'0000ull);
	RunEEGen(regs, [] { armEmitBEQ(5, 6, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BNE_TakenAndNotTaken)
{
	GuestRegs regs;
	regs.set64(5, 7);
	regs.set64(6, 8);
	RunEEGen(regs, [] { armEmitBNE(5, 6, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(6, 7);
	RunEEGen(regs, [] { armEmitBNE(5, 6, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BLTZ_SignedAgainstZero)
{
	GuestRegs regs;
	regs.set64(5, 0xFFFF'FFFF'FFFF'FFFFull); // -1
	RunEEGen(regs, [] { armEmitBLTZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(5, 0); // not < 0
	RunEEGen(regs, [] { armEmitBLTZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BGEZ_SignedAgainstZero)
{
	GuestRegs regs;
	regs.set64(5, 0); // >= 0 taken
	RunEEGen(regs, [] { armEmitBGEZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(5, 0xFFFF'FFFF'FFFF'FFFFull); // -1, not taken
	RunEEGen(regs, [] { armEmitBGEZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BLEZ_SignedAgainstZero)
{
	GuestRegs regs;
	regs.set64(5, 0); // <= 0 taken
	RunEEGen(regs, [] { armEmitBLEZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(5, 1); // not taken
	RunEEGen(regs, [] { armEmitBLEZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BGTZ_SignedAgainstZero)
{
	GuestRegs regs;
	regs.set64(5, 1); // > 0 taken
	RunEEGen(regs, [] { armEmitBGTZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kTaken);

	regs.set64(5, 0); // not taken
	RunEEGen(regs, [] { armEmitBGTZ(5, kTaken, kFall); });
	EXPECT_EQ(regs.getPc(), kFall);
}

TEST(Arm64EmitEE, BLTZAL_LinksUnconditionally)
{
	GuestRegs regs;
	regs.set64(5, 0); // condition false -> not taken, but link still written
	regs.set64(31, 0xDEAD'BEEF'DEAD'BEEFull);
	RunEEGen(regs, [] { armEmitBLTZAL(5, kTaken, kFall, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), kFall);
	EXPECT_EQ(regs.get64(31), 0x0000'0000'0010'0008ull);
}

TEST(Arm64EmitEE, BGEZAL_TakenAndLinks)
{
	GuestRegs regs;
	regs.set64(5, 5); // >= 0 -> taken
	RunEEGen(regs, [] { armEmitBGEZAL(5, kTaken, kFall, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.getPc(), kTaken);
	EXPECT_EQ(regs.get64(31), 0x0000'0000'0010'0008ull);
}

TEST(Arm64EmitEE, BGEZAL_RsIs31ComparesLinkedValue)
{
	// Degenerate rs==31: link is written first, so the (now non-negative) link
	// value is what the >= 0 test sees -> taken. Mirrors the interpreter.
	GuestRegs regs;
	regs.set64(31, 0xFFFF'FFFF'FFFF'FFFFull); // -1 before linking
	RunEEGen(regs, [] { armEmitBGEZAL(31, kTaken, kFall, /*linkpc*/ 0x0010'0008); });
	EXPECT_EQ(regs.get64(31), 0x0000'0000'0010'0008ull); // linked (positive)
	EXPECT_EQ(regs.getPc(), kTaken);                     // so branch taken
}

// ============================================================================
//  FPU (COP1) exact-semantics ops (Phase 5.2a)
// ============================================================================

TEST(Arm64EmitEE, MFC1_SignExtendIntoGpr)
{
	GuestRegs regs;
	regs.setFPR(5, 0x8000'0001);                  // fpr[5], sign bit set
	const u8 hi[16] = {0,0,0,0,0,0,0,0, 0xEE,0xEE,0xEE,0xEE,0xEE,0xEE,0xEE,0xEE};
	regs.set128(7, hi);                           // poison GPR[7].UD[1]
	// MFC1 $7, $f5
	RunEEGen(regs, [] { armEmitMFC1(/*rt*/ 7, /*fs*/ 5); });
	EXPECT_EQ(regs.get64(7), 0xFFFF'FFFF'8000'0001ull); // sign-extended into UD[0]
	u8 out[16];
	regs.get128(7, out);
	EXPECT_EQ(out[8], 0xEEu);                      // UD[1] untouched
}

TEST(Arm64EmitEE, MFC1_DiscardZeroRt)
{
	GuestRegs regs;
	regs.setFPR(5, 0xDEAD'BEEF);
	// MFC1 $0, $f5 — write to $zero must be discarded.
	RunEEGen(regs, [] { armEmitMFC1(/*rt*/ 0, /*fs*/ 5); });
	EXPECT_EQ(regs.get64(0), 0u);
}

TEST(Arm64EmitEE, MTC1_MoveGprToFpr)
{
	GuestRegs regs;
	regs.set64(6, 0xFFFF'FFFF'CAFE'F00Dull); // only low 32 bits transfer
	// MTC1 $6, $f4
	RunEEGen(regs, [] { armEmitMTC1(/*fs*/ 4, /*rt*/ 6); });
	EXPECT_EQ(regs.getFPR(4), 0xCAFE'F00Du);
}

TEST(Arm64EmitEE, CFC1_Fcr31SignExtended)
{
	GuestRegs regs;
	regs.setFPRC(31, 0x8000'0040);
	// CFC1 $7, $31
	RunEEGen(regs, [] { armEmitCFC1(/*rt*/ 7, /*fs*/ 31); });
	EXPECT_EQ(regs.get64(7), 0xFFFF'FFFF'8000'0040ull);
}

TEST(Arm64EmitEE, CFC1_Fs0IsConstant)
{
	GuestRegs regs;
	regs.set64(7, 0x1111'1111'1111'1111ull);
	// CFC1 $7, $0 -> 0x2E00
	RunEEGen(regs, [] { armEmitCFC1(/*rt*/ 7, /*fs*/ 0); });
	EXPECT_EQ(regs.get64(7), 0x2E00ull);
}

TEST(Arm64EmitEE, CFC1_OtherFsIsZero)
{
	GuestRegs regs;
	regs.set64(7, 0x1111'1111'1111'1111ull);
	// CFC1 $7, $2 -> 0
	RunEEGen(regs, [] { armEmitCFC1(/*rt*/ 7, /*fs*/ 2); });
	EXPECT_EQ(regs.get64(7), 0u);
}

TEST(Arm64EmitEE, CTC1_WritesFcr31Only)
{
	GuestRegs regs;
	regs.set64(6, 0x0000'0000'0000'1234ull);
	regs.setFPRC(31, 0xFFFF'FFFF);
	regs.setFPRC(2, 0xAAAA'AAAA);
	// CTC1 $6, $31 -> fprc[31] = low word; CTC1 $6, $2 is a no-op.
	RunEEGen(regs, [] { armEmitCTC1(/*fs*/ 31, /*rt*/ 6); });
	EXPECT_EQ(regs.getFPRC(31), 0x1234u);
	RunEEGen(regs, [] { armEmitCTC1(/*fs*/ 2, /*rt*/ 6); });
	EXPECT_EQ(regs.getFPRC(2), 0xAAAA'AAAAu); // unchanged
}

TEST(Arm64EmitEE, MOV_S_Copies)
{
	GuestRegs regs;
	regs.setFPR(3, 0x1234'5678);
	// MOV.S $f8, $f3
	RunEEGen(regs, [] { armEmitMOV_S(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), 0x1234'5678u);
}

TEST(Arm64EmitEE, ABS_S_ClearsSignAndFlags)
{
	GuestRegs regs;
	regs.setFPR(3, 0xC080'0000);   // negative float
	regs.setFPRC(31, 0xFFFF'FFFF); // all flags set
	// ABS.S $f8, $f3
	RunEEGen(regs, [] { armEmitABS_S(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), 0x4080'0000u);     // sign bit cleared
	EXPECT_EQ(regs.getFPRC(31), 0xFFFF'3FFFu);   // O|U (0xC000) cleared
}

TEST(Arm64EmitEE, NEG_S_FlipsSignAndClearsFlags)
{
	GuestRegs regs;
	regs.setFPR(3, 0x4080'0000);   // positive float
	regs.setFPRC(31, 0x0000'C000); // only O|U set
	// NEG.S $f8, $f3
	RunEEGen(regs, [] { armEmitNEG_S(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), 0xC080'0000u);   // sign bit flipped
	EXPECT_EQ(regs.getFPRC(31), 0u);           // O|U cleared
}

TEST(Arm64EmitEE, LWC1_LoadsIntoFpr)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	const u32 val = 0x3F80'0000; // 1.0f
	std::memcpy(&ram[0x50], &val, sizeof(val));

	GuestRegs regs;
	regs.set64(8, kTestVAddr); // $t0 = base
	// LWC1 $f4, 0x50($t0)
	RunEEGen(regs, [] { armEmitLWC1(/*ft*/ 4, /*rs*/ 8, /*imm*/ 0x50); });
	EXPECT_EQ(regs.getFPR(4), 0x3F80'0000u);
}

TEST(Arm64EmitEE, SWC1_StoresFprToMemory)
{
	alignas(16) std::array<u8, VTLB_PAGE_SIZE> ram{};
	VtlbMapping map(ram.data(), ram.size());

	GuestRegs regs;
	regs.set64(8, kTestVAddr);
	regs.setFPR(4, 0x4049'0FDB); // ~pi
	// SWC1 $f4, 0x60($t0)
	RunEEGen(regs, [] { armEmitSWC1(/*ft*/ 4, /*rs*/ 8, /*imm*/ 0x60); });
	u32 in_ram;
	std::memcpy(&in_ram, &ram[0x60], sizeof(in_ram));
	EXPECT_EQ(in_ram, 0x4049'0FDBu);
}

// ----- FPU float arithmetic (Phase 5.2b) ---------------------------------
// Reference model mirroring pcsx2/FPU.cpp (the interpreter, ground truth). The JIT
// uses the same host FPU, so comparing against this C++ replica makes the tests
// independent of the host FPCR (e.g. flush-to-zero) state: both paths see the same
// post-op bits and apply the identical clamp logic.
namespace fpuref {
constexpr u32 kI = 0x00020000, kD = 0x00010000, kO = 0x00008000, kU = 0x00004000;
constexpr u32 kSI = 0x00000040, kSD = 0x00000020, kSO = 0x00000010, kSU = 0x00000008;
constexpr u32 kC = 0x00800000;

u32 bits(float f)
{
	u32 out;
	std::memcpy(&out, &f, sizeof(out));
	return out;
}

float fpuDouble(u32 f)
{
	u32 r;
	switch (f & 0x7f800000)
	{
		case 0x0: r = f & 0x80000000; break;
		case 0x7f800000: r = (f & 0x80000000) | 0x7f7fffff; break;
		default: r = f; break;
	}
	float out;
	std::memcpy(&out, &r, sizeof(out));
	return out;
}
bool checkOverflow(u32& x, u32 flags, u32& fcr)
{
	if ((x & ~0x80000000u) == 0x7f800000u)
	{
		x = (x & 0x80000000u) | 0x7f7fffffu;
		fcr |= flags;
		return true;
	}
	else if (flags & kO)
		fcr &= ~kO;
	return false;
}
void checkUnderflow(u32& x, u32 flags, u32& fcr)
{
	if (((x & 0x7f800000u) == 0) && ((x & 0x007fffffu) != 0))
	{
		x &= 0x80000000u;
		fcr |= flags;
	}
	else if (flags & kU)
		fcr &= ~kU;
}
// Returns {result bits, fcr31} for the ADD/SUB/MUL family.
struct Result { u32 val; u32 fcr; };
Result binop(char op, u32 fs, u32 ft, u32 fcr_in)
{
	const float a = fpuDouble(fs), b = fpuDouble(ft);
	const float r = (op == '+') ? a + b : (op == '-') ? a - b : a * b;
	u32 bits;
	std::memcpy(&bits, &r, sizeof(bits));
	u32 fcr = fcr_in;
	if (!checkOverflow(bits, kO | kSO, fcr))
		checkUnderflow(bits, kU | kSU, fcr);
	return {bits, fcr};
}
bool checkDivideByZero(u32& x, u32 divisor, u32 dividend, u32 flags_nonzero, u32 flags_zero, u32& fcr)
{
	if ((divisor & 0x7f800000u) == 0)
	{
		fcr |= ((dividend & 0x7f800000u) == 0) ? flags_zero : flags_nonzero;
		x = ((divisor ^ dividend) & 0x80000000u) | 0x7f7fffffu;
		return true;
	}
	return false;
}
Result div(u32 fs, u32 ft, u32 fcr_in)
{
	u32 out = 0;
	u32 fcr = fcr_in;
	if (checkDivideByZero(out, ft, fs, kD | kSD, kI | kSI, fcr))
		return {out, fcr};
	out = bits(fpuDouble(fs) / fpuDouble(ft));
	if (!checkOverflow(out, 0, fcr))
		checkUnderflow(out, 0, fcr);
	return {out, fcr};
}
Result sqrt(u32 ft, u32 fcr_in)
{
	u32 fcr = fcr_in & ~(kI | kD);
	if ((ft & 0x7f800000u) == 0)
		return {ft & 0x80000000u, fcr};
	if (ft & 0x80000000u)
		fcr |= kI | kSI;

	u32 out = bits(std::sqrt(std::fabs(fpuDouble(ft))));
	if (!checkOverflow(out, 0, fcr))
		checkUnderflow(out, 0, fcr);
	return {out, fcr};
}
Result rsqrt(u32 fs, u32 ft, u32 fcr_in)
{
	u32 fcr = fcr_in & ~(kI | kD);
	if ((ft & 0x7f800000u) == 0)
		return {(ft & 0x80000000u) | 0x7f7fffffu, fcr | kD | kSD};
	if (ft & 0x80000000u)
		fcr |= kI | kSI;

	u32 out = bits(fpuDouble(fs) / std::sqrt(std::fabs(fpuDouble(ft))));
	if (!checkOverflow(out, 0, fcr))
		checkUnderflow(out, 0, fcr);
	return {out, fcr};
}
// MADD/MSUB -> fd: re-clamps the accumulator and the (single-precision) product.
Result madd(bool subtract, u32 acc, u32 fs, u32 ft, u32 fcr_in)
{
	const u32 prod = bits(fpuDouble(fs) * fpuDouble(ft));
	const float a = fpuDouble(acc), p = fpuDouble(prod);
	u32 out = bits(subtract ? a - p : a + p);
	u32 fcr = fcr_in;
	if (!checkOverflow(out, kO | kSO, fcr))
		checkUnderflow(out, kU | kSU, fcr);
	return {out, fcr};
}
// MADDA/MSUBA -> ACC: uses the raw stored accumulator float and unclamped product.
Result maddacc(bool subtract, u32 acc, u32 fs, u32 ft, u32 fcr_in)
{
	float accf;
	std::memcpy(&accf, &acc, sizeof(accf));
	const float p = fpuDouble(fs) * fpuDouble(ft);
	u32 out = bits(subtract ? accf - p : accf + p);
	u32 fcr = fcr_in;
	if (!checkOverflow(out, kO | kSO, fcr))
		checkUnderflow(out, kU | kSU, fcr);
	return {out, fcr};
}
u32 fp_max(u32 a, u32 b)
{
	const std::int32_t sa = static_cast<std::int32_t>(a), sb = static_cast<std::int32_t>(b);
	if (sa < 0 && sb < 0)
		return (sa < sb) ? a : b; // both negative -> min
	return (sa > sb) ? a : b;     // -> max
}
u32 fp_min(u32 a, u32 b)
{
	const std::int32_t sa = static_cast<std::int32_t>(a), sb = static_cast<std::int32_t>(b);
	if (sa < 0 && sb < 0)
		return (sa > sb) ? a : b; // both negative -> max
	return (sa < sb) ? a : b;     // -> min
}
// C.* compare: returns the updated FCR31. 'F' always clears the C bit.
u32 ccond(char which, u32 fs, u32 ft, u32 fcr_in)
{
	if (which == 'F')
		return fcr_in & ~kC;
	const float a = fpuDouble(fs), b = fpuDouble(ft);
	const bool c = (which == '=') ? (a == b) : (which == '<') ? (a < b) : (a <= b);
	return c ? (fcr_in | kC) : (fcr_in & ~kC);
}
u32 cvtw(u32 fs)
{
	if ((fs & 0x7f800000u) <= 0x4e800000u)
	{
		float f;
		std::memcpy(&f, &fs, sizeof(f));
		return static_cast<u32>(static_cast<std::int32_t>(f));
	}
	return (fs & 0x80000000u) ? 0x80000000u : 0x7fffffffu;
}
u32 cvts(u32 fs) { return bits(static_cast<float>(static_cast<std::int32_t>(fs))); }
} // namespace fpuref

// fs op ft -> fpr[fd], with FCR31 flag side-effects, checked against the reference.
static void CheckBinop(char op, u32 fs_bits, u32 ft_bits, u32 fcr_in)
{
	GuestRegs regs;
	regs.setFPR(3, fs_bits);
	regs.setFPR(4, ft_bits);
	regs.setFPRC(31, fcr_in);
	switch (op)
	{
		case '+': RunEEGen(regs, [] { armEmitADD_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); }); break;
		case '-': RunEEGen(regs, [] { armEmitSUB_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); }); break;
		case '*': RunEEGen(regs, [] { armEmitMUL_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); }); break;
	}
	const fpuref::Result expect = fpuref::binop(op, fs_bits, ft_bits, fcr_in);
	EXPECT_EQ(regs.getFPR(8), expect.val) << "op=" << op << std::hex << " fs=" << fs_bits << " ft=" << ft_bits;
	EXPECT_EQ(regs.getFPRC(31), expect.fcr) << "op=" << op << " (flags)";
}

TEST(Arm64EmitEE, ADD_S_Basic)
{
	CheckBinop('+', 0x3f800000 /*1.0*/, 0x40000000 /*2.0*/, 0); // -> 3.0 = 0x40400000
}

TEST(Arm64EmitEE, SUB_S_Basic)
{
	CheckBinop('-', 0x40a00000 /*5.0*/, 0x40000000 /*2.0*/, 0); // -> 3.0
}

TEST(Arm64EmitEE, MUL_S_Basic)
{
	CheckBinop('*', 0x40400000 /*3.0*/, 0x40000000 /*2.0*/, 0); // -> 6.0 = 0x40c00000
}

TEST(Arm64EmitEE, ADD_S_OverflowClampsToFmaxAndSetsFlags)
{
	// fmax + fmax overflows to +inf -> clamped to +fmax, O|SO set.
	CheckBinop('+', 0x7f7fffff, 0x7f7fffff, 0);
}

TEST(Arm64EmitEE, ADD_S_InputInfinityClampedToFmax)
{
	// fpuDouble(+inf) = +fmax; +fmax + 0 = +fmax (finite, no overflow flag).
	CheckBinop('+', 0x7f800000 /*+inf*/, 0x00000000, 0);
}

TEST(Arm64EmitEE, ADD_S_ClearsStaleOverflowFlagWhenNoOverflow)
{
	// A normal result must clear the O cause flag that was set on entry (kept SO).
	CheckBinop('+', 0x3f800000, 0x3f800000, fpuref::kO | fpuref::kSO);
}

TEST(Arm64EmitEE, MUL_S_DenormalResultUnderflowsToZero)
{
	// smallest-normal * 0.5 underflows; result flushed to signed zero, U|SU set
	// (matches whatever the host FPU produced — see the reference-model note above).
	CheckBinop('*', 0x00800000, 0x3f000000, 0);
}

TEST(Arm64EmitEE, ADDA_S_WritesAccumulator)
{
	GuestRegs regs;
	regs.setFPR(3, 0x3f800000); // 1.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitADDA_S(/*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::binop('+', 0x3f800000, 0x40000000, 0);
	EXPECT_EQ(regs.getACC(), expect.val); // -> 3.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MULA_S_WritesAccumulator)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMULA_S(/*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::binop('*', 0x40400000, 0x40000000, 0);
	EXPECT_EQ(regs.getACC(), expect.val); // -> 6.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, DIV_S_Basic)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40c00000); // 6.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, fpuref::kD | fpuref::kI | fpuref::kSD | fpuref::kSI);
	RunEEGen(regs, [] { armEmitDIV_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::div(0x40c00000, 0x40000000, fpuref::kD | fpuref::kI | fpuref::kSD | fpuref::kSI);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr); // normal DIV does not clear stale I/D in the interpreter
}

TEST(Arm64EmitEE, DIV_S_DivideByZeroSetsDAndFmax)
{
	GuestRegs regs;
	regs.setFPR(3, 0xc0800000); // -4.0 dividend
	regs.setFPR(4, 0x00000000); // +0 divisor
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitDIV_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::div(0xc0800000, 0x00000000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val); // -fmax: sign(divisor ^ dividend)
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, DIV_S_ZeroOverZeroSetsInvalid)
{
	GuestRegs regs;
	regs.setFPR(3, 0x80000000); // -0 dividend
	regs.setFPR(4, 0x00000000); // +0 divisor
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitDIV_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::div(0x80000000, 0x00000000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, SQRT_S_Positive)
{
	GuestRegs regs;
	regs.setFPR(4, 0x40800000); // 4.0
	regs.setFPRC(31, fpuref::kI | fpuref::kD | fpuref::kSI | fpuref::kSD);
	RunEEGen(regs, [] { armEmitSQRT_S(/*fd*/ 8, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::sqrt(0x40800000, fpuref::kI | fpuref::kD | fpuref::kSI | fpuref::kSD);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr); // clears I/D causes, keeps sticky bits
}

TEST(Arm64EmitEE, SQRT_S_NegativeSetsInvalidAndUsesAbs)
{
	GuestRegs regs;
	regs.setFPR(4, 0xc0800000); // -4.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitSQRT_S(/*fd*/ 8, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::sqrt(0xc0800000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val); // sqrt(abs(-4)) = 2
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, SQRT_S_NegativeZeroReturnsNegativeZero)
{
	GuestRegs regs;
	regs.setFPR(4, 0x80000000); // -0
	regs.setFPRC(31, fpuref::kI | fpuref::kD);
	RunEEGen(regs, [] { armEmitSQRT_S(/*fd*/ 8, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::sqrt(0x80000000, fpuref::kI | fpuref::kD);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, RSQRT_S_Positive)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40800000); // 4.0
	regs.setFPR(4, 0x40800000); // 4.0
	regs.setFPRC(31, fpuref::kI | fpuref::kD);
	RunEEGen(regs, [] { armEmitRSQRT_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::rsqrt(0x40800000, 0x40800000, fpuref::kI | fpuref::kD);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, RSQRT_S_ZeroFtSetsDivideAndFmax)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40800000); // 4.0
	regs.setFPR(4, 0x80000000); // -0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitRSQRT_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::rsqrt(0x40800000, 0x80000000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val); // sign comes from ft in the interpreter zero path
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, RSQRT_S_NegativeFtSetsInvalid)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40800000); // 4.0
	regs.setFPR(4, 0xc0800000); // -4.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitRSQRT_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::rsqrt(0x40800000, 0xc0800000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MADD_S_AccPlusProduct)
{
	GuestRegs regs;
	regs.setACC(0x40000000); // 2.0
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMADD_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::madd(/*sub*/ false, 0x40000000, 0x40400000, 0x40000000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val); // 2 + 3*2 = 8.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MSUB_S_AccMinusProduct)
{
	GuestRegs regs;
	regs.setACC(0x41200000); // 10.0
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMSUB_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::madd(/*sub*/ true, 0x41200000, 0x40400000, 0x40000000, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val); // 10 - 3*2 = 4.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MADD_S_OverflowClampsAndSetsFlags)
{
	GuestRegs regs;
	regs.setACC(0x7f7fffff); // +fmax
	regs.setFPR(3, 0x7f7fffff); // +fmax
	regs.setFPR(4, 0x7f7fffff); // +fmax -> product clamps to +fmax, sum overflows
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMADD_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::madd(/*sub*/ false, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0);
	EXPECT_EQ(regs.getFPR(8), expect.val);
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MADDA_S_WritesAccumulatorRaw)
{
	GuestRegs regs;
	regs.setACC(0x3f800000); // 1.0 (used raw, not re-clamped)
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMADDA_S(/*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::maddacc(/*sub*/ false, 0x3f800000, 0x40400000, 0x40000000, 0);
	EXPECT_EQ(regs.getACC(), expect.val); // 1 + 3*2 = 7.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MSUBA_S_WritesAccumulatorRaw)
{
	GuestRegs regs;
	regs.setACC(0x41200000); // 10.0
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMSUBA_S(/*fs*/ 3, /*ft*/ 4); });
	const fpuref::Result expect = fpuref::maddacc(/*sub*/ true, 0x41200000, 0x40400000, 0x40000000, 0);
	EXPECT_EQ(regs.getACC(), expect.val); // 10 - 3*2 = 4.0
	EXPECT_EQ(regs.getFPRC(31), expect.fcr);
}

TEST(Arm64EmitEE, MAX_S_PositivePicksLarger)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, fpuref::kO | fpuref::kU);
	RunEEGen(regs, [] { armEmitMAX_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPR(8), fpuref::fp_max(0x40400000, 0x40000000)); // 3.0
	EXPECT_EQ(regs.getFPRC(31), 0u); // O|U cleared
}

TEST(Arm64EmitEE, MAX_S_BothNegativePicksCloserToZero)
{
	GuestRegs regs;
	regs.setFPR(3, 0xc0400000); // -3.0
	regs.setFPR(4, 0xc0000000); // -2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMAX_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPR(8), fpuref::fp_max(0xc0400000, 0xc0000000)); // -2.0
	EXPECT_EQ(regs.getFPRC(31), 0u);
}

TEST(Arm64EmitEE, MIN_S_PositivePicksSmaller)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMIN_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPR(8), fpuref::fp_min(0x40400000, 0x40000000)); // 2.0
	EXPECT_EQ(regs.getFPRC(31), 0u);
}

TEST(Arm64EmitEE, MIN_S_BothNegativePicksFartherFromZero)
{
	GuestRegs regs;
	regs.setFPR(3, 0xc0400000); // -3.0
	regs.setFPR(4, 0xc0000000); // -2.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitMIN_S(/*fd*/ 8, /*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPR(8), fpuref::fp_min(0xc0400000, 0xc0000000)); // -3.0
	EXPECT_EQ(regs.getFPRC(31), 0u);
}

TEST(Arm64EmitEE, C_LT_TrueSetsConditionBit)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40000000); // 2.0
	regs.setFPR(4, 0x40400000); // 3.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitC_LT(/*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPRC(31), fpuref::ccond('<', 0x40000000, 0x40400000, 0)); // C set
}

TEST(Arm64EmitEE, C_LT_FalseClearsConditionBit)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40000000); // 2.0
	regs.setFPRC(31, fpuref::kC); // stale C must be cleared
	RunEEGen(regs, [] { armEmitC_LT(/*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPRC(31), fpuref::ccond('<', 0x40400000, 0x40000000, fpuref::kC)); // C cleared
}

TEST(Arm64EmitEE, C_EQ_EqualSetsConditionBit)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40400000); // 3.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitC_EQ(/*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPRC(31), fpuref::ccond('=', 0x40400000, 0x40400000, 0));
}

TEST(Arm64EmitEE, C_LE_EqualSetsConditionBit)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000); // 3.0
	regs.setFPR(4, 0x40400000); // 3.0
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitC_LE(/*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPRC(31), fpuref::ccond('L', 0x40400000, 0x40400000, 0));
}

TEST(Arm64EmitEE, C_F_AlwaysClearsConditionBit)
{
	GuestRegs regs;
	regs.setFPR(3, 0x40400000);
	regs.setFPR(4, 0x40000000);
	regs.setFPRC(31, fpuref::kC | fpuref::kO);
	RunEEGen(regs, [] { armEmitC_F(/*fs*/ 3, /*ft*/ 4); });
	EXPECT_EQ(regs.getFPRC(31), fpuref::ccond('F', 0, 0, fpuref::kC | fpuref::kO)); // only C cleared
}

TEST(Arm64EmitEE, CVT_W_InRangeTruncatesTowardZero)
{
	GuestRegs regs;
	regs.setFPR(3, 0x402df3b6); // 2.7185 -> truncates to 2
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_W(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvtw(0x402df3b6));
}

TEST(Arm64EmitEE, CVT_W_NegativeTruncatesTowardZero)
{
	GuestRegs regs;
	regs.setFPR(3, 0xc02df3b6); // -2.7185 -> -2
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_W(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvtw(0xc02df3b6));
}

TEST(Arm64EmitEE, CVT_W_OutOfRangePositiveSaturates)
{
	GuestRegs regs;
	regs.setFPR(3, 0x4f000000); // 2^31, beyond the in-range exponent -> 0x7fffffff
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_W(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvtw(0x4f000000));
	EXPECT_EQ(regs.getFPR(8), 0x7fffffffu);
}

TEST(Arm64EmitEE, CVT_W_OutOfRangeNegativeSaturates)
{
	GuestRegs regs;
	regs.setFPR(3, 0xcf000000); // -2^31 magnitude out of range -> 0x80000000
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_W(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvtw(0xcf000000));
	EXPECT_EQ(regs.getFPR(8), 0x80000000u);
}

TEST(Arm64EmitEE, CVT_S_IntToFloat)
{
	GuestRegs regs;
	regs.setFPR(3, 100); // integer 100 -> 100.0f
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_S(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvts(100));
	EXPECT_EQ(regs.getFPR(8), 0x42c80000u); // 100.0f
}

TEST(Arm64EmitEE, CVT_S_NegativeIntToFloat)
{
	GuestRegs regs;
	regs.setFPR(3, static_cast<u32>(-5)); // -5 -> -5.0f
	regs.setFPRC(31, 0);
	RunEEGen(regs, [] { armEmitCVT_S(/*fd*/ 8, /*fs*/ 3); });
	EXPECT_EQ(regs.getFPR(8), fpuref::cvts(static_cast<u32>(-5)));
}

TEST(Arm64EmitEE, BC1F_BranchesWhenConditionClear)
{
	GuestRegs regs;
	regs.setFPRC(31, 0); // C clear -> BC1F taken
	RunEEGen(regs, [] { armEmitBC1F(/*target*/ 0x1000, /*fallthrough*/ 0x2000); });
	EXPECT_EQ(regs.getPc(), 0x1000u);
}

TEST(Arm64EmitEE, BC1F_FallsThroughWhenConditionSet)
{
	GuestRegs regs;
	regs.setFPRC(31, fpuref::kC); // C set -> BC1F not taken
	RunEEGen(regs, [] { armEmitBC1F(/*target*/ 0x1000, /*fallthrough*/ 0x2000); });
	EXPECT_EQ(regs.getPc(), 0x2000u);
}

TEST(Arm64EmitEE, BC1T_BranchesWhenConditionSet)
{
	GuestRegs regs;
	regs.setFPRC(31, fpuref::kC); // C set -> BC1T taken
	RunEEGen(regs, [] { armEmitBC1T(/*target*/ 0x1000, /*fallthrough*/ 0x2000); });
	EXPECT_EQ(regs.getPc(), 0x1000u);
}

TEST(Arm64EmitEE, BC1T_FallsThroughWhenConditionClear)
{
	GuestRegs regs;
	regs.setFPRC(31, 0); // C clear -> BC1T not taken
	RunEEGen(regs, [] { armEmitBC1T(/*target*/ 0x1000, /*fallthrough*/ 0x2000); });
	EXPECT_EQ(regs.getPc(), 0x2000u);
}

// --------------------------------------------------------------------------------------
//  Phase 5.4: MMI 128-bit SIMD generators
// --------------------------------------------------------------------------------------
// Each generator is checked against a scalar C++ replica of the interpreter
// (pcsx2/MMI.cpp). We feed a couple of 128-bit input vectors per op — including
// saturation / sign / equality edge cases — and assert byte-exact agreement.
namespace mmiref
{
	// A 128-bit guest GPR viewed as packed lanes (host is little-endian arm64, so
	// these alias the same byte order the JIT load/store sees).
	// Use stdint types here: a `using namespace vixl::aarch64;` is in scope at file
	// level and defines register objects named s8/s16/etc. that shadow the pcsx2
	// integer aliases, so spell the lane types explicitly.
	union MQ
	{
		uint8_t uc[16];
		int8_t sc[16];
		uint16_t us[8];
		int16_t ss[8];
		uint32_t ul[4];
		int32_t sl[4];
		uint64_t ud[2];
	};

	static MQ make(std::initializer_list<u8> bytes)
	{
		MQ q{};
		u32 i = 0;
		for (u8 b : bytes)
			q.uc[i++] = b;
		return q;
	}

	// Reference replicas (mirror MMI.cpp exactly).
	static MQ refPADDW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.ul[n] + t.ul[n]; return d; }
	static MQ refPADDH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = u16(s.us[n] + t.us[n]); return d; }
	static MQ refPADDB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.sc[n] = int8_t(s.sc[n] + t.sc[n]); return d; }
	static MQ refPSUBW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.ul[n] - t.ul[n]; return d; }
	static MQ refPSUBH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = u16(s.us[n] - t.us[n]); return d; }
	static MQ refPSUBB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.sc[n] = int8_t(s.sc[n] - t.sc[n]); return d; }

	static u32 satSW(s64 v) { return v > 0x7FFFFFFF ? 0x7FFFFFFF : (v < (s32)0x80000000 ? 0x80000000u : (u32)(s32)v); }
	static u16 satSH(s32 v) { return v > 0x7FFF ? 0x7FFF : (v < (s32)0xFFFF8000 ? 0x8000 : (u16)(int16_t)v); }
	static u8 satSB(int16_t v) { return v > 0x7F ? 0x7F : (v < (int16_t)-128 ? 0x80 : (u8)(int8_t)v); }
	static MQ refPADDSW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = satSW((s64)s.sl[n] + (s64)t.sl[n]); return d; }
	static MQ refPADDSH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = satSH((s32)s.ss[n] + (s32)t.ss[n]); return d; }
	static MQ refPADDSB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.uc[n] = satSB((int16_t)s.sc[n] + (int16_t)t.sc[n]); return d; }
	static MQ refPSUBSW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = satSW((s64)s.sl[n] - (s64)t.sl[n]); return d; }
	static MQ refPSUBSH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = satSH((s32)s.ss[n] - (s32)t.ss[n]); return d; }
	static MQ refPSUBSB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.uc[n] = satSB((int16_t)s.sc[n] - (int16_t)t.sc[n]); return d; }

	static MQ refPADDUW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { s64 v = (s64)s.ul[n] + (s64)t.ul[n]; d.ul[n] = v > 0xFFFFFFFF ? 0xFFFFFFFFu : (u32)v; } return d; }
	static MQ refPADDUH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) { s32 v = (s32)s.us[n] + (s32)t.us[n]; d.us[n] = v > 0xFFFF ? 0xFFFF : (u16)v; } return d; }
	static MQ refPADDUB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) { u16 v = (u16)s.uc[n] + (u16)t.uc[n]; d.uc[n] = v > 0xFF ? 0xFF : (u8)v; } return d; }
	static MQ refPSUBUW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { s64 v = (s64)s.ul[n] - (s64)t.ul[n]; d.ul[n] = v <= 0 ? 0u : (u32)v; } return d; }
	static MQ refPSUBUH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) { s32 v = (s32)s.us[n] - (s32)t.us[n]; d.us[n] = v <= 0 ? 0 : (u16)v; } return d; }
	static MQ refPSUBUB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) { int16_t v = (int16_t)s.uc[n] - (int16_t)t.uc[n]; d.uc[n] = v <= 0 ? 0 : (u8)v; } return d; }

	static MQ refPCGTW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.sl[n] > t.sl[n] ? 0xFFFFFFFFu : 0; return d; }
	static MQ refPCGTH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = s.ss[n] > t.ss[n] ? 0xFFFF : 0; return d; }
	static MQ refPCGTB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.uc[n] = s.sc[n] > t.sc[n] ? 0xFF : 0; return d; }
	static MQ refPCEQW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.ul[n] == t.ul[n] ? 0xFFFFFFFFu : 0; return d; }
	static MQ refPCEQH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = s.us[n] == t.us[n] ? 0xFFFF : 0; return d; }
	static MQ refPCEQB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 16; n++) d.uc[n] = s.uc[n] == t.uc[n] ? 0xFF : 0; return d; }
	static MQ refPMAXW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.sl[n] > t.sl[n] ? s.ul[n] : t.ul[n]; return d; }
	static MQ refPMAXH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = s.ss[n] > t.ss[n] ? s.us[n] : t.us[n]; return d; }
	static MQ refPMINW(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = s.sl[n] < t.sl[n] ? s.ul[n] : t.ul[n]; return d; }
	static MQ refPMINH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = s.ss[n] < t.ss[n] ? s.us[n] : t.us[n]; return d; }

	static MQ refPAND(MQ s, MQ t) { MQ d{}; d.ud[0] = s.ud[0] & t.ud[0]; d.ud[1] = s.ud[1] & t.ud[1]; return d; }
	static MQ refPOR(MQ s, MQ t) { MQ d{}; d.ud[0] = s.ud[0] | t.ud[0]; d.ud[1] = s.ud[1] | t.ud[1]; return d; }
	static MQ refPXOR(MQ s, MQ t) { MQ d{}; d.ud[0] = s.ud[0] ^ t.ud[0]; d.ud[1] = s.ud[1] ^ t.ud[1]; return d; }
	static MQ refPNOR(MQ s, MQ t) { MQ d{}; d.ud[0] = ~(s.ud[0] | t.ud[0]); d.ud[1] = ~(s.ud[1] | t.ud[1]); return d; }

	static MQ refPEXTLW(MQ s, MQ t) { MQ d{}; d.ul[0] = t.ul[0]; d.ul[1] = s.ul[0]; d.ul[2] = t.ul[1]; d.ul[3] = s.ul[1]; return d; }
	static MQ refPEXTUW(MQ s, MQ t) { MQ d{}; d.ul[0] = t.ul[2]; d.ul[1] = s.ul[2]; d.ul[2] = t.ul[3]; d.ul[3] = s.ul[3]; return d; }
	static MQ refPEXTLH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[2 * n] = t.us[n]; d.us[2 * n + 1] = s.us[n]; } return d; }
	static MQ refPEXTUH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[2 * n] = t.us[n + 4]; d.us[2 * n + 1] = s.us[n + 4]; } return d; }
	static MQ refPEXTLB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) { d.uc[2 * n] = t.uc[n]; d.uc[2 * n + 1] = s.uc[n]; } return d; }
	static MQ refPEXTUB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) { d.uc[2 * n] = t.uc[n + 8]; d.uc[2 * n + 1] = s.uc[n + 8]; } return d; }
	static MQ refPPACW(MQ s, MQ t) { MQ d{}; d.ul[0] = t.ul[0]; d.ul[1] = t.ul[2]; d.ul[2] = s.ul[0]; d.ul[3] = s.ul[2]; return d; }
	static MQ refPPACH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[n] = t.us[2 * n]; d.us[n + 4] = s.us[2 * n]; } return d; }
	static MQ refPPACB(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 8; n++) { d.uc[n] = t.uc[2 * n]; d.uc[n + 8] = s.uc[2 * n]; } return d; }
	static MQ refPCPYLD(MQ s, MQ t) { MQ d{}; d.ud[0] = t.ud[0]; d.ud[1] = s.ud[0]; return d; }
	static MQ refPCPYUD(MQ s, MQ t) { MQ d{}; d.ud[0] = s.ud[1]; d.ud[1] = t.ud[1]; return d; }

	static MQ refPABSW(MQ t) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = t.ul[n] == 0x80000000 ? 0x7FFFFFFF : (t.sl[n] < 0 ? (u32)(-t.sl[n]) : (u32)t.sl[n]); return d; }
	static MQ refPABSH(MQ t) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = t.us[n] == 0x8000 ? 0x7FFF : (t.ss[n] < 0 ? (u16)(-t.ss[n]) : (u16)t.ss[n]); return d; }
	static MQ refPCPYH(MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[n] = t.us[0]; d.us[n + 4] = t.us[4]; } return d; }

	// Parallel shifts by immediate (sa masked by lane width).
	static MQ refPSLLH(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = t.us[n] << (sa & 0x0F); return d; }
	static MQ refPSLLW(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = t.ul[n] << (sa & 0x1F); return d; }
	static MQ refPSRLH(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = t.us[n] >> (sa & 0x0F); return d; }
	static MQ refPSRLW(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = t.ul[n] >> (sa & 0x1F); return d; }
	static MQ refPSRAH(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 8; n++) d.us[n] = (u16)(t.ss[n] >> (sa & 0x0F)); return d; }
	static MQ refPSRAW(MQ t, u32 sa) { MQ d{}; for (int n = 0; n < 4; n++) d.ul[n] = (u32)(t.sl[n] >> (sa & 0x1F)); return d; }

	// Lane permutes (Phase 5.4 continuation).
	// PINTH: interleave low half of Rt with high half of Rs (halfwords)
	// [Rt[0], Rs[4], Rt[1], Rs[5], Rt[2], Rs[6], Rt[3], Rs[7]]
	static MQ refPINTH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[2 * n] = t.us[n]; d.us[2 * n + 1] = s.us[n + 4]; } return d; }
	// PINTEH: interleave even-indexed halfwords of Rt and Rs
	// [Rt[0], Rs[0], Rt[2], Rs[2], Rt[4], Rs[4], Rt[6], Rs[6]]
	static MQ refPINTEH(MQ s, MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[2 * n] = t.us[2 * n]; d.us[2 * n + 1] = s.us[2 * n]; } return d; }
	// PEXEH: extract even halfwords (reverse within each 64-bit half, swapping 0<->2)
	// [Rt[2], Rt[1], Rt[0], Rt[3], Rt[6], Rt[5], Rt[4], Rt[7]]
	static MQ refPEXEH(MQ t) { MQ d{}; d.us[0] = t.us[2]; d.us[1] = t.us[1]; d.us[2] = t.us[0]; d.us[3] = t.us[3]; d.us[4] = t.us[6]; d.us[5] = t.us[5]; d.us[6] = t.us[4]; d.us[7] = t.us[7]; return d; }
	// PEXEW: extract even words (swap word pairs)
	// [Rt[2], Rt[1], Rt[0], Rt[3]] in 32-bit lanes
	static MQ refPEXEW(MQ t) { MQ d{}; d.ul[0] = t.ul[2]; d.ul[1] = t.ul[1]; d.ul[2] = t.ul[0]; d.ul[3] = t.ul[3]; return d; }
	// PREVH: reverse halfwords within each 64-bit half
	// [Rt[3], Rt[2], Rt[1], Rt[0], Rt[7], Rt[6], Rt[5], Rt[4]]
	static MQ refPREVH(MQ t) { MQ d{}; for (int n = 0; n < 4; n++) { d.us[n] = t.us[3 - n]; d.us[n + 4] = t.us[7 - n]; } return d; }

	// Two reusable input vectors with a spread of sign / saturation edge values.
	static MQ inA() { return make({0x01, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0xFF, 0x7F, 0x05, 0xF0, 0x34, 0x12, 0x00, 0x00, 0x00, 0x80}); }
	static MQ inB() { return make({0xFF, 0xFF, 0xFF, 0x7F, 0x01, 0x00, 0x00, 0x80, 0x05, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}); }
} // namespace mmiref

namespace
{
	using mmiref::MQ;

	MQ runMMIBin(void (*gen)(u32, u32, u32), MQ s, MQ t)
	{
		GuestRegs regs;
		regs.set128(8, s.uc);
		regs.set128(9, t.uc);
		RunEEGen(regs, [gen] { gen(/*rd*/ 10, /*rs*/ 8, /*rt*/ 9); });
		MQ out{};
		regs.get128(10, out.uc);
		return out;
	}

	MQ runMMIUn(void (*gen)(u32, u32), MQ t)
	{
		GuestRegs regs;
		regs.set128(9, t.uc);
		RunEEGen(regs, [gen] { gen(/*rd*/ 10, /*rt*/ 9); });
		MQ out{};
		regs.get128(10, out.uc);
		return out;
	}

	MQ runMMIShift(void (*gen)(u32, u32, u32), MQ t, u32 sa)
	{
		GuestRegs regs;
		regs.set128(9, t.uc);
		RunEEGen(regs, [gen, sa] { gen(/*rd*/ 10, /*rt*/ 9, sa); });
		MQ out{};
		regs.get128(10, out.uc);
		return out;
	}

	::testing::AssertionResult eqMQ(const MQ& got, const MQ& want)
	{
		if (std::memcmp(got.uc, want.uc, 16) == 0)
			return ::testing::AssertionSuccess();
		auto hex = [](const MQ& q) {
			char buf[64];
			for (int i = 0; i < 16; i++)
				std::snprintf(buf + i * 3, 4, "%02x ", q.uc[i]);
			return std::string(buf);
		};
		return ::testing::AssertionFailure() << "\n  got:  " << hex(got) << "\n  want: " << hex(want);
	}
} // namespace

#define MMI_BIN_TEST(NAME)                                                      \
	TEST(Arm64EmitEE, MMI_##NAME)                                               \
	{                                                                          \
		MQ a = mmiref::inA(), b = mmiref::inB();                               \
		EXPECT_TRUE(eqMQ(runMMIBin(armEmit##NAME, a, b), mmiref::ref##NAME(a, b))); \
		EXPECT_TRUE(eqMQ(runMMIBin(armEmit##NAME, b, a), mmiref::ref##NAME(b, a))); \
	}

#define MMI_UN_TEST(NAME)                                                       \
	TEST(Arm64EmitEE, MMI_##NAME)                                               \
	{                                                                          \
		MQ a = mmiref::inA(), b = mmiref::inB();                               \
		EXPECT_TRUE(eqMQ(runMMIUn(armEmit##NAME, a), mmiref::ref##NAME(a)));   \
		EXPECT_TRUE(eqMQ(runMMIUn(armEmit##NAME, b), mmiref::ref##NAME(b)));   \
	}

// Macro for shift ops that take an immediate `sa` parameter.
#define MMI_SHIFT_TEST(NAME)                                                    \
	TEST(Arm64EmitEE, MMI_##NAME##_sa0)                                         \
	{                                                                          \
		MQ a = mmiref::inA();                                                   \
		EXPECT_TRUE(eqMQ(runMMIShift(armEmit##NAME, a, 0), mmiref::ref##NAME(a, 0))); \
	}                                                                         \
	TEST(Arm64EmitEE, MMI_##NAME##_saMax)                                       \
	{                                                                          \
		MQ a = mmiref::inA();                                                   \
		constexpr u32 kMaxShift = (sizeof(a.ul[0]) == 2) ? 15 : 31;             \
		EXPECT_TRUE(eqMQ(runMMIShift(armEmit##NAME, a, kMaxShift), mmiref::ref##NAME(a, kMaxShift))); \
	}                                                                         \
	TEST(Arm64EmitEE, MMI_##NAME##_saMasked)                                    \
	{                                                                          \
		MQ a = mmiref::inA();                                                   \
		constexpr u32 kOvershift = (sizeof(a.ul[0]) == 2) ? 17 : 33;            \
		EXPECT_TRUE(eqMQ(runMMIShift(armEmit##NAME, a, kOvershift), mmiref::ref##NAME(a, kOvershift))); \
	}

MMI_BIN_TEST(PADDW) MMI_BIN_TEST(PADDH) MMI_BIN_TEST(PADDB)
MMI_BIN_TEST(PSUBW) MMI_BIN_TEST(PSUBH) MMI_BIN_TEST(PSUBB)
MMI_BIN_TEST(PADDSW) MMI_BIN_TEST(PADDSH) MMI_BIN_TEST(PADDSB)
MMI_BIN_TEST(PSUBSW) MMI_BIN_TEST(PSUBSH) MMI_BIN_TEST(PSUBSB)
MMI_BIN_TEST(PADDUW) MMI_BIN_TEST(PADDUH) MMI_BIN_TEST(PADDUB)
MMI_BIN_TEST(PSUBUW) MMI_BIN_TEST(PSUBUH) MMI_BIN_TEST(PSUBUB)
MMI_BIN_TEST(PCGTW) MMI_BIN_TEST(PCGTH) MMI_BIN_TEST(PCGTB)
MMI_BIN_TEST(PCEQW) MMI_BIN_TEST(PCEQH) MMI_BIN_TEST(PCEQB)
MMI_BIN_TEST(PMAXW) MMI_BIN_TEST(PMAXH) MMI_BIN_TEST(PMINW) MMI_BIN_TEST(PMINH)
MMI_BIN_TEST(PAND) MMI_BIN_TEST(POR) MMI_BIN_TEST(PXOR) MMI_BIN_TEST(PNOR)
MMI_BIN_TEST(PEXTLW) MMI_BIN_TEST(PEXTLH) MMI_BIN_TEST(PEXTLB)
MMI_BIN_TEST(PEXTUW) MMI_BIN_TEST(PEXTUH) MMI_BIN_TEST(PEXTUB)
MMI_BIN_TEST(PPACW) MMI_BIN_TEST(PPACH) MMI_BIN_TEST(PPACB)
MMI_BIN_TEST(PCPYLD) MMI_BIN_TEST(PCPYUD)
MMI_UN_TEST(PABSW) MMI_UN_TEST(PABSH) MMI_UN_TEST(PCPYH)

// Parallel shifts by immediate (Phase 5.4 continuation).
MMI_SHIFT_TEST(PSLLH) MMI_SHIFT_TEST(PSLLW)
MMI_SHIFT_TEST(PSRLH) MMI_SHIFT_TEST(PSRLW)
MMI_SHIFT_TEST(PSRAH) MMI_SHIFT_TEST(PSRAW)

// Lane permutes (Phase 5.4 continuation).
#define MMI_PERM_TEST(NAME)                                                     \
	TEST(Arm64EmitEE, MMI_##NAME)                                               \
	{                                                                          \
		MQ a = mmiref::inA(), b = mmiref::inB();                               \
		EXPECT_TRUE(eqMQ(runMMIBin(armEmit##NAME, a, b), mmiref::ref##NAME(a, b))); \
		EXPECT_TRUE(eqMQ(runMMIBin(armEmit##NAME, b, a), mmiref::ref##NAME(b, a))); \
	}

#define MMI_PERM_UN_TEST(NAME)                                                  \
	TEST(Arm64EmitEE, MMI_##NAME)                                               \
	{                                                                          \
		MQ a = mmiref::inA(), b = mmiref::inB();                               \
		EXPECT_TRUE(eqMQ(runMMIUn(armEmit##NAME, a), mmiref::ref##NAME(a)));   \
		EXPECT_TRUE(eqMQ(runMMIUn(armEmit##NAME, b), mmiref::ref##NAME(b)));   \
	}

MMI_PERM_TEST(PINTH) MMI_PERM_TEST(PINTEH)
MMI_PERM_UN_TEST(PEXEH) MMI_PERM_UN_TEST(PEXEW) MMI_PERM_UN_TEST(PREVH)

// rd == 0 must discard the write (matching the interpreter's `if (!_Rd_) return`).
TEST(Arm64EmitEE, MMI_DiscardZeroDest)
{
	GuestRegs regs;
	MQ a = mmiref::inA();
	regs.set128(8, a.uc);
	regs.set128(9, a.uc);
	RunEEGen(regs, [] { armEmitPADDW(/*rd*/ 0, /*rs*/ 8, /*rt*/ 9); });
	EXPECT_EQ(regs.get64(0), 0u); // $zero stays zero
}

#endif // __aarch64__
