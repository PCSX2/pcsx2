// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <bitset>
#include <optional>

//------------------------------------------------------------------
// Micro VU - Host register instances
//------------------------------------------------------------------

const xmm& xmm::GetInstance(int i)
{
	static constexpr xmm regs[32] = {
		xmm(0), xmm(1), xmm(2), xmm(3), xmm(4), xmm(5), xmm(6), xmm(7),
		xmm(8), xmm(9), xmm(10), xmm(11), xmm(12), xmm(13), xmm(14), xmm(15),
		xmm(16), xmm(17), xmm(18), xmm(19), xmm(20), xmm(21), xmm(22), xmm(23),
		xmm(24), xmm(25), xmm(26), xmm(27), xmm(28), xmm(29), xmm(30), xmm(31)};
	pxAssert(i >= 0 && i < 32);
	return regs[i];
}

const x32& x32::GetInstance(int i)
{
	static constexpr x32 regs[32] = {
		x32(0), x32(1), x32(2), x32(3), x32(4), x32(5), x32(6), x32(7),
		x32(8), x32(9), x32(10), x32(11), x32(12), x32(13), x32(14), x32(15),
		x32(16), x32(17), x32(18), x32(19), x32(20), x32(21), x32(22), x32(23),
		x32(24), x32(25), x32(26), x32(27), x32(28), x32(29), x32(30), x32(31)};
	pxAssert(i >= 0 && i < 32);
	return regs[i];
}

static const xmm mVUScratch1(30);
static const xmm mVUScratch2(31);
static const xmm mVUScratch3(29);

//------------------------------------------------------------------
// Micro VU - Addressing
//------------------------------------------------------------------

// Returns a memory operand for a host address. Addresses inside VU state, the microVU struct or
// VU memory are relative to the pinned base registers, anything else is materialized in x17.
// The operand must be used immediately, before anything else emits code using x17.
a64::MemOperand mVUptr(u32 vuIndex, const void* addr)
{
	microVU& mVU = vuIndex ? microVU1 : microVU0;
	const uptr a = reinterpret_cast<uptr>(addr);

	const uptr vuregs_start = reinterpret_cast<uptr>(&vuRegs[0]);
	const uptr vuregs_end = reinterpret_cast<uptr>(&vuRegs[2]);
	if (a >= vuregs_start && a < vuregs_end)
		return a64::MemOperand(RMVUREGS, static_cast<s64>(a) - static_cast<s64>(reinterpret_cast<uptr>(&mVU.regs())));

	const uptr mvu_start = reinterpret_cast<uptr>(&mVU);
	if (a >= mvu_start && a < mvu_start + sizeof(microVU))
		return a64::MemOperand(RMVU, static_cast<s64>(a - mvu_start));

	const uptr mem_start = reinterpret_cast<uptr>(mVU.regs().Mem);
	if (a >= mem_start && a < mem_start + mVU.vuMemSize)
		return a64::MemOperand(RMVUMEM, static_cast<s64>(a - mem_start));

	const uptr const_start = reinterpret_cast<uptr>(&mVUglob);
	if (a >= const_start && a < const_start + sizeof(mVUglob))
		return a64::MemOperand(RMVUCONST, static_cast<s64>(a - const_start));

	armMoveAddressToReg(RSCRATCHADDR, addr);
	return a64::MemOperand(RSCRATCHADDR);
}

#define mVUmem(addr) mVUptr(mVU.index, (addr))

// Returns an address for VU memory + index register (the byte offset in a 64-bit register).
static __fi mVUAddr mVUmemIndexed(const a64::Register& index, s64 offset = 0)
{
	armAsm->Add(RSCRATCHADDR, RMVUMEM, index);
	return mVUAddr(RSCRATCHADDR, offset);
}

static __fi mVUAddr mVUmemConst(s64 offset)
{
	return mVUAddr(RMVUMEM, offset);
}

// Loads a 128-bit constant from the constant table into a scratch register.
static __fi a64::VRegister mVUloadConst(s64 offset, const xmm& scratch = mVUScratch1)
{
	armAsm->Ldr(scratch.Q(), a64::MemOperand(RMVUCONST, offset));
	return scratch.V4S();
}

static __fi a64::VRegister mVUloadConstS(s64 offset, const xmm& scratch = mVUScratch1)
{
	armAsm->Ldr(scratch.S(), a64::MemOperand(RMVUCONST, offset));
	return scratch.S();
}

// Read-modify-write of a 32-bit memory location.
template <typename F>
static __fi void mVUrmw32(const a64::MemOperand& mem, F&& op)
{
	armAsm->Ldr(a64::w8, mem);
	op(a64::w8);
	armAsm->Str(a64::w8, mem);
}

static __fi void mVUstoreImm32(const a64::MemOperand& mem, u32 value)
{
	if (value == 0)
	{
		armAsm->Str(a64::wzr, mem);
	}
	else
	{
		armAsm->Mov(a64::w8, value);
		armAsm->Str(a64::w8, mem);
	}
}

//------------------------------------------------------------------
// Micro VU - SSE-like helpers
//------------------------------------------------------------------

void mVUmovReg(const xmm& dst, const xmm& src)
{
	if (dst != src)
		armAsm->Mov(dst.V16B(), src.V16B());
}

// Inserts lane src_lane of src into lane dst_lane of dst.
static __fi void mVUinsLane(const xmm& dst, int dst_lane, const xmm& src, int src_lane)
{
	armAsm->Mov(dst.V4S(), dst_lane, src.V4S(), src_lane);
}

// Equivalent of pshufd dst, src, imm.
void mVUshufD(const xmm& dst, const xmm& src, int imm)
{
	imm &= 0xff;
	const int s0 = imm & 3, s1 = (imm >> 2) & 3, s2 = (imm >> 4) & 3, s3 = (imm >> 6) & 3;

	if (s0 == s1 && s1 == s2 && s2 == s3)
	{
		armAsm->Dup(dst.V4S(), src.V4S(), s0);
		return;
	}

	switch (imm)
	{
		case 0xe4: mVUmovReg(dst, src); return; // xyzw
		case 0x39: armAsm->Ext(dst.V16B(), src.V16B(), src.V16B(), 4); return; // yzwx
		case 0x4e: armAsm->Ext(dst.V16B(), src.V16B(), src.V16B(), 8); return; // zwxy
		case 0x93: armAsm->Ext(dst.V16B(), src.V16B(), src.V16B(), 12); return; // wxyz
		case 0xb1: armAsm->Rev64(dst.V4S(), src.V4S()); return; // yxwz
		case 0x1b: // wzyx
			armAsm->Rev64(dst.V4S(), src.V4S());
			armAsm->Ext(dst.V16B(), dst.V16B(), dst.V16B(), 8);
			return;
		default:
			break;
	}

	const int sel[4] = {s0, s1, s2, s3};
	if (dst == src)
	{
		mVUmovReg(mVUScratch1, src);
		for (int i = 0; i < 4; i++)
		{
			if (sel[i] != i)
				mVUinsLane(dst, i, mVUScratch1, sel[i]);
		}
	}
	else
	{
		mVUmovReg(dst, src);
		for (int i = 0; i < 4; i++)
		{
			if (sel[i] != i)
				mVUinsLane(dst, i, src, sel[i]);
		}
	}
}

// Equivalent of shufps dst, src, imm (lanes 0/1 from dst, lanes 2/3 from src).
static void mVUshufPS(const xmm& dst, const xmm& src, int imm)
{
	if (dst == src)
	{
		mVUshufD(dst, src, imm);
		return;
	}

	const int s0 = imm & 3, s1 = (imm >> 2) & 3, s2 = (imm >> 4) & 3, s3 = (imm >> 6) & 3;
	mVUmovReg(mVUScratch1, dst);
	mVUinsLane(dst, 0, mVUScratch1, s0);
	mVUinsLane(dst, 1, mVUScratch1, s1);
	mVUinsLane(dst, 2, src, s2);
	mVUinsLane(dst, 3, src, s3);
}

// Equivalent of movmskps: bit i of dst is the sign bit of lane i (or lane 3 - i if reversed).
static void mVUmovmsk(const a64::Register& dst, const xmm& src, bool reversed)
{
	armAsm->Cmlt(mVUVScratch1(4), src.V4S(), 0);
	armAsm->Ldr(mVUQScratch2, a64::MemOperand(RMVUCONST, reversed ? mVUconst(maskbits_rev) : mVUconst(maskbits)));
	armAsm->And(mVUVScratch1(16), mVUVScratch1(16), mVUVScratch2(16));
	armAsm->Addv(a64::s30, mVUVScratch1(4));
	armAsm->Fmov(dst.W(), a64::s30);
}

// Bitmask of lanes which compare equal to 0.0 (like cmpeqps with zero + movmskps).
static void mVUzeromsk(const a64::Register& dst, const xmm& src, bool reversed)
{
	armAsm->Fcmeq(mVUVScratch1(4), src.V4S(), 0.0);
	armAsm->Ldr(mVUQScratch2, a64::MemOperand(RMVUCONST, reversed ? mVUconst(maskbits_rev) : mVUconst(maskbits)));
	armAsm->And(mVUVScratch1(16), mVUVScratch1(16), mVUVScratch2(16));
	armAsm->Addv(a64::s30, mVUVScratch1(4));
	armAsm->Fmov(dst.W(), a64::s30);
}

//------------------------------------------------------------------
// Micro VU - Reg Loading/Saving/Shuffling/Unpacking/Merging...
//------------------------------------------------------------------

void mVUunpack_xyzw(const xmm& dstreg, const xmm& srcreg, int xyzw)
{
	armAsm->Dup(dstreg.V4S(), srcreg.V4S(), xyzw & 3);
}

void mVUloadReg(const xmm& reg, const mVUAddr& ptr, int xyzw)
{
	switch (xyzw)
	{
		case 8:  armAsm->Ldr(reg.S(), ptr.mem(0)); break; // X
		case 4:  armAsm->Ldr(reg.S(), ptr.mem(4)); break; // Y
		case 2:  armAsm->Ldr(reg.S(), ptr.mem(8)); break; // Z
		case 1:  armAsm->Ldr(reg.S(), ptr.mem(12)); break; // W
		default: armAsm->Ldr(reg.Q(), ptr.mem()); break;
	}
}

void mVUloadIreg(const xmm& reg, int xyzw, VURegs* vuRegs)
{
	armAsm->Ldr(reg.S(), mVUptr(vuRegs == &::vuRegs[1] ? 1 : 0, &vuRegs->VI[REG_I].UL));
	if (!_XYZWss(xyzw))
		armAsm->Dup(reg.V4S(), reg.V4S(), 0);
}

// Stores a single lane.
static __fi void mVUstoreLane(const xmm& reg, int lane, const mVUAddr& ptr, s64 offset)
{
	if (lane == 0)
	{
		armAsm->Str(reg.S(), ptr.mem(offset));
	}
	else
	{
		armAsm->Mov(a64::s30, reg.V4S(), lane);
		armAsm->Str(a64::s30, ptr.mem(offset));
	}
}

// Stores the upper 64 bits.
static __fi void mVUstoreHigh(const xmm& reg, const mVUAddr& ptr, s64 offset)
{
	armAsm->Mov(a64::d30, reg.V2D(), 1);
	armAsm->Str(a64::d30, ptr.mem(offset));
}

// Unlike x86, this doesn't modify the source register.
void mVUsaveReg(const xmm& reg, const mVUAddr& ptr, int xyzw, bool modXYZW)
{
	switch (xyzw)
	{
		case 5: // YW
			mVUstoreLane(reg, 1, ptr, 4);
			mVUstoreLane(reg, 3, ptr, 12);
			break;
		case 6: // YZ
			armAsm->Ext(mVUVScratch1(16), reg.V16B(), reg.V16B(), 4);
			armAsm->Str(a64::d30, ptr.mem(4));
			break;
		case 7: // YZW
			mVUstoreHigh(reg, ptr, 8);
			mVUstoreLane(reg, 1, ptr, 4);
			break;
		case 9: // XW
			mVUstoreLane(reg, 0, ptr, 0);
			mVUstoreLane(reg, 3, ptr, 12);
			break;
		case 10: // XZ
			mVUstoreLane(reg, 0, ptr, 0);
			mVUstoreLane(reg, 2, ptr, 8);
			break;
		case 11: // XZW
			mVUstoreLane(reg, 0, ptr, 0);
			mVUstoreHigh(reg, ptr, 8);
			break;
		case 13: // XYW
			armAsm->Str(reg.D(), ptr.mem(0));
			mVUstoreLane(reg, 3, ptr, 12);
			break;
		case 14: // XYZ
			armAsm->Str(reg.D(), ptr.mem(0));
			mVUstoreLane(reg, 2, ptr, 8);
			break;
		case 4: // Y
			mVUstoreLane(reg, modXYZW ? 0 : 1, ptr, 4);
			break;
		case 2: // Z
			mVUstoreLane(reg, modXYZW ? 0 : 2, ptr, 8);
			break;
		case 1: // W
			mVUstoreLane(reg, modXYZW ? 0 : 3, ptr, 12);
			break;
		case 8: // X
			mVUstoreLane(reg, 0, ptr, 0);
			break;
		case 12: // XY
			armAsm->Str(reg.D(), ptr.mem(0));
			break;
		case 3: // ZW
			mVUstoreHigh(reg, ptr, 8);
			break;
		default: // XYZW
			armAsm->Str(reg.Q(), ptr.mem());
			break;
	}
}

// Merges the xyzw lanes of src into dest. With modXYZW, single lane writes come from lane 0 of src.
void mVUmergeRegs(const xmm& dest, const xmm& src, int xyzw, bool modXYZW)
{
	xyzw &= 0xf;
	if ((dest != src) && (xyzw != 0))
	{
		if (xyzw == 0x8)
			mVUinsLane(dest, 0, src, 0);
		else if (xyzw == 0xf)
			mVUmovReg(dest, src);
		else
		{
			if (modXYZW)
			{
				if      (xyzw == 1) { mVUinsLane(dest, 3, src, 0); return; }
				else if (xyzw == 2) { mVUinsLane(dest, 2, src, 0); return; }
				else if (xyzw == 4) { mVUinsLane(dest, 1, src, 0); return; }
			}
			for (int i = 0; i < 4; i++)
			{
				if (xyzw & (8 >> i))
					mVUinsLane(dest, i, src, i);
			}
		}
	}
}

//------------------------------------------------------------------
// Micro VU - Misc Functions
//------------------------------------------------------------------

// Registers which need to be preserved across a call into C code.
struct mVUSavedRegs
{
	std::bitset<32> gprs;
	std::bitset<32> vecs;

	u32 stackSize() const
	{
		const u32 ngprs = static_cast<u32>(gprs.count());
		return static_cast<u32>(Common::AlignUpPow2(ngprs * 8, 16) + vecs.count() * 16);
	}
};

static mVUSavedRegs mVUgetSavedRegs(microVU& mVU, bool onlyNeeded)
{
	mVUSavedRegs saved;
	for (int i = 0; i < 16; i++)
	{
		if (!onlyNeeded || mVU.regAlloc->checkCachedGPR(i))
			saved.gprs[i] = true;
	}
	for (int i = 0; i < 29; i++)
	{
		if (!onlyNeeded || mVU.regAlloc->checkCachedReg(i) || xmmPQ.Id == i)
			saved.vecs[i] = true;
	}
	return saved;
}

static void mVUpushRegs(const mVUSavedRegs& saved)
{
	const u32 size = saved.stackSize();
	if (size == 0)
		return;

	armAsm->Sub(a64::sp, a64::sp, size);
	u32 offset = 0;
	for (int i = 0; i < 32; i++)
	{
		if (!saved.gprs[i])
			continue;
		armAsm->Str(armXRegister(i), a64::MemOperand(a64::sp, offset));
		offset += 8;
	}
	offset = Common::AlignUpPow2(offset, 16);
	for (int i = 0; i < 32; i++)
	{
		if (!saved.vecs[i])
			continue;
		armAsm->Str(armQRegister(i), a64::MemOperand(a64::sp, offset));
		offset += 16;
	}
}

static void mVUpopRegs(const mVUSavedRegs& saved)
{
	const u32 size = saved.stackSize();
	if (size == 0)
		return;

	u32 offset = 0;
	for (int i = 0; i < 32; i++)
	{
		if (!saved.gprs[i])
			continue;
		armAsm->Ldr(armXRegister(i), a64::MemOperand(a64::sp, offset));
		offset += 8;
	}
	offset = Common::AlignUpPow2(offset, 16);
	for (int i = 0; i < 32; i++)
	{
		if (!saved.vecs[i])
			continue;
		armAsm->Ldr(armQRegister(i), a64::MemOperand(a64::sp, offset));
		offset += 16;
	}
	armAsm->Add(a64::sp, a64::sp, size);
}

// Backup Volatile Regs
__fi void mVUbackupRegs(microVU& mVU, bool toMemory = false, bool onlyNeeded = false)
{
	if (toMemory)
	{
		mVUpushRegs(mVUgetSavedRegs(mVU, onlyNeeded));
	}
	else
	{
		mVU.regAlloc->flushAll(); // Flush Regalloc
		armAsm->Str(xmmPQ.Q(), mVUmem(&mVU.pqBackup[0]));
	}
}

// Restore Volatile Regs
__fi void mVUrestoreRegs(microVU& mVU, bool fromMemory = false, bool onlyNeeded = false)
{
	if (fromMemory)
		mVUpopRegs(mVUgetSavedRegs(mVU, onlyNeeded));
	else
		armAsm->Ldr(xmmPQ.Q(), mVUmem(&mVU.pqBackup[0]));
}

static void mVUTBit()
{
	u32 old = vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUTBit, std::memory_order_release);
	if (old & VU_Thread::InterruptFlagVUTBit)
		DevCon.Warning("Old TBit not registered");
}

static void mVUEBit()
{
	vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUEBit, std::memory_order_release);
}

static inline u32 branchAddr(const mV)
{
	pxAssumeMsg(islowerOP, "MicroVU: Expected Lower OP code for valid branch addr.");
	return ((((iPC + 2) + (_Imm11_ * 2)) & mVU.progMemMask) * 4);
}

static void mVUwaitMTVU()
{
	if (IsDevBuild)
		DevCon.WriteLn("microVU0: Waiting on VU1 thread to access VU1 regs!");
	vu1Thread.WaitVU();
}

// Transforms the Address in gprReg to a valid VU0/VU1 byte offset from VU memory.
__fi void mVUaddrFix(mV, const a64::Register& gprReg, const a64::Register& tmpReg)
{
	const a64::Register wreg = gprReg.W();
	if (isVU1)
	{
		armAsm->And(wreg, wreg, 0x3ff); // wrap around
		armAsm->Lsl(wreg, wreg, 4);
	}
	else
	{
		a64::Label jmpA, jmpB;
		armAsm->Tst(wreg, 0x400);
		armAsm->B(&jmpA, a64::ne); // if addr & 0x4000, reads VU1's VF regs and VI regs
			armAsm->And(wreg, wreg, 0xff); // if !(addr & 0x4000), wrap around
			armAsm->B(&jmpB);
		armAsm->Bind(&jmpA);
			if (THREAD_VU1)
				armEmitCall(mVU.waitMTVU);
			armAsm->And(wreg, wreg, 0x3f); // ToDo: theres a potential problem if VU0 overrides VU1's VF0/VI0 regs!
			const sptr offset = (u128*)VU1.VF - (u128*)VU0.Mem;
			armAsm->Mov(tmpReg.X(), static_cast<u64>(offset));
			armAsm->Add(gprReg.X(), gprReg.X(), tmpReg.X());
		armAsm->Bind(&jmpB);
		armAsm->Lsl(gprReg.X(), gprReg.X(), 4); // multiply by 16 (shift left by 4)
	}
}

__fi std::optional<mVUAddr> mVUoptimizeConstantAddr(mV, u32 srcreg, s32 offset, s32 offsetSS_)
{
	// if we had const prop for VIs, we could do that here..
	if (srcreg != 0)
		return std::nullopt;

	const s32 addr = 0 + offset;
	if (isVU1)
	{
		return mVUmemConst(((addr & 0x3FFu) << 4) + offsetSS_);
	}
	else
	{
		if (addr & 0x400)
			return std::nullopt;

		return mVUmemConst(((addr & 0xFFu) << 4) + offsetSS_);
	}
}

//------------------------------------------------------------------
// Micro VU - Custom SSE Instructions
//------------------------------------------------------------------

// Integer comparison based min/max, treating the floats as sign-magnitude integers (same as x86).
// Warning: Modifies t1 and t2
void MIN_MAX_PS(microVU& mVU, const xmm& to, const xmm& from, const xmm& t1in, const xmm& t2in, bool min)
{
	const xmm& t1 = t1in.IsEmpty() ? mVU.regAlloc->allocReg() : t1in;
	const xmm& t2 = t2in.IsEmpty() ? mVU.regAlloc->allocReg() : t2in;

	const xmm& c1 = min ? t2 : t1;
	const xmm& c2 = min ? t1 : t2;

	armAsm->Sshr(t1.V4S(), to.V4S(), 31);
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);
	armAsm->Eor(t1.V16B(), t1.V16B(), to.V16B());

	armAsm->Sshr(t2.V4S(), from.V4S(), 31);
	armAsm->Ushr(t2.V4S(), t2.V4S(), 1);
	armAsm->Eor(t2.V16B(), t2.V16B(), from.V16B());

	// to = (c1 > c2) ? to : from
	armAsm->Cmgt(c1.V4S(), c1.V4S(), c2.V4S());
	armAsm->Bif(to.V16B(), from.V16B(), c1.V16B());

	if (t1 != t1in) mVU.regAlloc->clearNeeded(t1);
	if (t2 != t2in) mVU.regAlloc->clearNeeded(t2);
}

// Warning: Modifies to's upper 3 vectors, and t1
// The x86 version compares doubles built from the floats, which orders them the same way as the
// integer comparison above, so the same code is used.
void MIN_MAX_SS(mV, const xmm& to, const xmm& from, const xmm& t1in, bool min)
{
	MIN_MAX_PS(mVU, to, from, t1in, xEmptyReg, min);
}

// Turns out only this is needed to get TriAce games booting with mVU
// Modifies from's lower vector
void ADD_SS_TriAceHack(microVU& mVU, const xmm& to, const xmm& from)
{
	armAsm->Fmov(a64::w0, to.S());
	armAsm->Fmov(a64::w1, from.S());
	armAsm->Ubfx(a64::w0, a64::w0, 23, 8);
	armAsm->Ubfx(a64::w1, a64::w1, 23, 8);
	armAsm->Sub(a64::w1, a64::w1, a64::w0); // Exponent Difference

	a64::Label case_neg_big, case_end;
	armAsm->Cmp(a64::w1, -25);
	armAsm->B(&case_neg_big, a64::le);
	armAsm->Cmp(a64::w1, 25);
	armAsm->B(&case_end, a64::lt);

	// case_pos_big:
	armAsm->And(to.V16B(), to.V16B(), mVUloadConst(mVUconst(add_ss_mask)).V16B());
	armAsm->B(&case_end);

	armAsm->Bind(&case_neg_big);
	armAsm->And(from.V16B(), from.V16B(), mVUloadConst(mVUconst(add_ss_mask)).V16B());

	armAsm->Bind(&case_end);
	armAsm->Fadd(a64::s30, to.S(), from.S());
	mVUinsLane(to, 0, mVUScratch1, 0);
}

enum class mVUArithOp
{
	Add,
	Sub,
	Mul,
	Div,
};

static void mVUarithPS(mVUArithOp op, const xmm& to, const xmm& from)
{
	switch (op)
	{
		case mVUArithOp::Add: armAsm->Fadd(to.V4S(), to.V4S(), from.V4S()); break;
		case mVUArithOp::Sub: armAsm->Fsub(to.V4S(), to.V4S(), from.V4S()); break;
		case mVUArithOp::Mul: armAsm->Fmul(to.V4S(), to.V4S(), from.V4S()); break;
		case mVUArithOp::Div: armAsm->Fdiv(to.V4S(), to.V4S(), from.V4S()); break;
	}
}

// Scalar ops only modify lane 0, like the SSE ss instructions (NEON scalar ops clear the upper lanes).
static void mVUarithSS(mVUArithOp op, const xmm& to, const xmm& from)
{
	switch (op)
	{
		case mVUArithOp::Add: armAsm->Fadd(a64::s30, to.S(), from.S()); break;
		case mVUArithOp::Sub: armAsm->Fsub(a64::s30, to.S(), from.S()); break;
		case mVUArithOp::Mul: armAsm->Fmul(a64::s30, to.S(), from.S()); break;
		case mVUArithOp::Div: armAsm->Fdiv(a64::s30, to.S(), from.S()); break;
	}
	mVUinsLane(to, 0, mVUScratch1, 0);
}

// to.x = to.x op constant.x
static void mVUarithSSConst(mVUArithOp op, const xmm& to, s64 const_offset)
{
	armAsm->Ldr(a64::s31, a64::MemOperand(RMVUCONST, const_offset));
	switch (op)
	{
		case mVUArithOp::Add: armAsm->Fadd(a64::s30, to.S(), a64::s31); break;
		case mVUArithOp::Sub: armAsm->Fsub(a64::s30, to.S(), a64::s31); break;
		case mVUArithOp::Mul: armAsm->Fmul(a64::s30, to.S(), a64::s31); break;
		case mVUArithOp::Div: armAsm->Fdiv(a64::s30, to.S(), a64::s31); break;
	}
	mVUinsLane(to, 0, mVUScratch1, 0);
}

// dst.x = src.x op constant.x (dst's upper lanes are preserved)
static void mVUarithSSConst3(mVUArithOp op, const xmm& dst, const xmm& src, s64 const_offset)
{
	armAsm->Ldr(a64::s31, a64::MemOperand(RMVUCONST, const_offset));
	switch (op)
	{
		case mVUArithOp::Add: armAsm->Fadd(a64::s30, src.S(), a64::s31); break;
		case mVUArithOp::Sub: armAsm->Fsub(a64::s30, src.S(), a64::s31); break;
		case mVUArithOp::Mul: armAsm->Fmul(a64::s30, src.S(), a64::s31); break;
		case mVUArithOp::Div: armAsm->Fdiv(a64::s30, src.S(), a64::s31); break;
	}
	mVUinsLane(dst, 0, mVUScratch1, 0);
}

static void mVUsqrtSS(const xmm& to, const xmm& from)
{
	armAsm->Fsqrt(a64::s30, from.S());
	mVUinsLane(to, 0, mVUScratch1, 0);
}

// Clamp helpers are in microVU_Clamp.inl, but used by the ops below.
void mVUclamp3(microVU& mVU, const xmm& reg, const xmm& regT1, int xyzw);
void mVUclamp4(microVU& mVU, const xmm& reg, const xmm& regT1, int xyzw);

static __fi void clampOp(microVU& mVU, mVUArithOp op, bool isPS, const xmm& to, const xmm& from, const xmm& t1)
{
	mVUclamp3(mVU, to, t1, isPS ? 0xf : 0x8);
	mVUclamp3(mVU, from, t1, isPS ? 0xf : 0x8);
	if (isPS)
		mVUarithPS(op, to, from);
	else
		mVUarithSS(op, to, from);
	mVUclamp4(mVU, to, t1, isPS ? 0xf : 0x8);
}

void SSE_MAXPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	MIN_MAX_PS(mVU, to, from, t1, t2, false);
}
void SSE_MINPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	MIN_MAX_PS(mVU, to, from, t1, t2, true);
}
void SSE_MAXSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	MIN_MAX_SS(mVU, to, from, t1, false);
}
void SSE_MINSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	MIN_MAX_SS(mVU, to, from, t1, true);
}
void SSE_ADD2SS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	if (!CHECK_VUADDSUBHACK)
		clampOp(mVU, mVUArithOp::Add, false, to, from, t1);
	else
		ADD_SS_TriAceHack(mVU, to, from);
}

// Does same as SSE_ADDPS since tri-ace games only need SS implementation of VUADDSUBHACK...
void SSE_ADD2PS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Add, true, to, from, t1);
}
void SSE_ADDPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Add, true, to, from, t1);
}
void SSE_ADDSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Add, false, to, from, t1);
}
void SSE_SUBPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Sub, true, to, from, t1);
}
void SSE_SUBSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Sub, false, to, from, t1);
}
void SSE_MULPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Mul, true, to, from, t1);
}
void SSE_MULSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Mul, false, to, from, t1);
}
void SSE_DIVPS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Div, true, to, from, t1);
}
void SSE_DIVSS(mV, const xmm& to, const xmm& from, const xmm& t1 = xEmptyReg, const xmm& t2 = xEmptyReg)
{
	clampOp(mVU, mVUArithOp::Div, false, to, from, t1);
}
