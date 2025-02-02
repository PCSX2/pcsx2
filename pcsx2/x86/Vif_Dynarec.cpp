// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Vif_UnpackSSE.h"
#include "MTVU.h"
#include "common/Perf.h"
#include "common/StringUtil.h"

void dVifReset(int idx)
{
	nVif[idx].vifBlocks.reset();

	const size_t offset = idx ? HostMemoryMap::VIF1recOffset : HostMemoryMap::VIF0recOffset;
	const size_t size = idx ? HostMemoryMap::VIF1recSize : HostMemoryMap::VIF0recSize;
	nVif[idx].recWritePtr = SysMemory::GetCodePtr(offset);
	nVif[idx].recEndPtr = nVif[idx].recWritePtr + (size - _256kb);
}

void dVifRelease(int idx)
{
	nVif[idx].vifBlocks.clear();
}

VifUnpackSSE_Dynarec::VifUnpackSSE_Dynarec(const nVifStruct& vif_, const nVifBlock& vifBlock_)
	: v(vif_)
	, vB(vifBlock_)
{
	const int wl = vB.wl ? vB.wl : 256; //0 is taken as 256 (KH2)
	isFill    = (vB.cl < wl);
	usn       = (vB.upkType>>5) & 1;
	doMask    = (vB.upkType>>4) & 1;
	doMode    = vB.mode & 3;
	IsAligned = vB.aligned;
	vCL       = 0;
}

__fi void makeMergeMask(u32& x)
{
	x = ((x & 0x40) >> 6) | ((x & 0x10) >> 3) | (x & 4) | ((x & 1) << 3);
}

__fi void VifUnpackSSE_Dynarec::SetMasks(int cS) const
{
	const int idx = v.idx;
	const vifStruct& vif = MTVU_VifX;

	//This could have ended up copying the row when there was no row to write.1810080
	const u32 m0 = vB.mask; //The actual mask example 0x03020100
	const u32 m3 = ((m0 & 0xaaaaaaaa) >> 1) & ~m0; //all the upper bits, so our example 0x01010000 & 0xFCFDFEFF = 0x00010000 just the cols (shifted right for maskmerge)
	const u32 m2 = (m0 & 0x55555555) & (~m0 >> 1); // 0x1000100 & 0xFE7EFF7F = 0x00000100 Just the row

	if ((doMask && m2) || doMode)
	{
		xMOVAPS(rowReg, ptr128[&vif.MaskRow]);
		MSKPATH3_LOG("Moving row");
	}

	if (doMask && m3)
	{
		VIF_LOG("Merging Cols");
		xMOVAPS(colRegs[0], ptr128[&vif.MaskCol]);
		if ((cS >= 2) && (m3 & 0x0000ff00)) xPSHUF.D(colRegs[1], colRegs[0], _v1);
		if ((cS >= 3) && (m3 & 0x00ff0000)) xPSHUF.D(colRegs[2], colRegs[0], _v2);
		if ((cS >= 4) && (m3 & 0xff000000)) xPSHUF.D(colRegs[3], colRegs[0], _v3);
		if ((cS >= 1) && (m3 & 0x000000ff)) xPSHUF.D(colRegs[0], colRegs[0], _v0);
	}
	//if (doMask||doMode) loadRowCol((nVifStruct&)v);
}

void VifUnpackSSE_Dynarec::doMaskWrite(const xRegisterSSE& regX) const
{
	pxAssertMsg(regX.Id <= 1, "Reg Overflow! XMM2 thru XMM6 are reserved for masking.");

	const int cc = std::min(vCL, 3);
	const u32 m0 = (vB.mask >> (cc * 8)) & 0xff; //The actual mask example 0xE4 (protect, col, row, clear)
	u32 m3 = ((m0 & 0xaa) >> 1) & ~m0; //all the upper bits (cols shifted right) cancelling out any write protects 0x10
	u32 m2 = (m0 & 0x55) & (~m0 >> 1); // all the lower bits (rows)cancelling out any write protects 0x04
	u32 m4 = (m0 & ~((m3 << 1) | m2)) & 0x55; //  = 0xC0 & 0x55 = 0x40 (for merge mask)

	makeMergeMask(m2);
	makeMergeMask(m3);
	makeMergeMask(m4);

	if (doMask && m2) // Merge MaskRow
	{
		mVUmergeRegs(regX, rowReg, m2);
	}

	if (doMask && m3) // Merge MaskCol
	{
		mVUmergeRegs(regX, colRegs[cc], m3);
	}
	
	if (doMode)
	{
		u32 m5 = ~(m2 | m3 | m4) & 0xf;

		if (!doMask)
			m5 = 0xf;

		if (m5 < 0xf)
		{
			if (doMode == 3)
			{
				mVUmergeRegs(rowReg, regX, m5);
			}
			else
			{
				xPXOR(tmpReg, tmpReg);
				mVUmergeRegs(tmpReg, rowReg, m5);
				xPADD.D(regX, tmpReg);
				if (doMode == 2)
					mVUmergeRegs(rowReg, regX, m5);
			}
		}
		else
		{
			if (doMode == 3)
			{
				xMOVAPS(rowReg, regX);
			}
			else
			{
				xPADD.D(regX, rowReg);
				if (doMode == 2)
					xMOVAPS(rowReg, regX);
			}
		}
	}

	if (doMask && m4) // Merge Write Protect
		mVUsaveReg(regX, ptr32[dstIndirect], m4 ^ 0xf, false);
	else
		xMOVAPS(ptr32[dstIndirect], regX);
}

void VifUnpackSSE_Dynarec::writeBackRow() const
{
	const int idx = v.idx;
	xMOVAPS(ptr128[&(MTVU_VifX.MaskRow)], rowReg);

	VIF_LOG("nVif: writing back row reg! [doMode = %d]", doMode);
}

static void ShiftDisplacementWindow(xAddressVoid& addr, const xRegisterLong& modReg)
{
	// Shifts the displacement factor of a given indirect address, so that the address
	// remains in the optimal 0xf0 range (which allows for byte-form displacements when
	// generating instructions).

	int addImm = 0;
	while (addr.Displacement >= 0x80)
	{
		addImm += 0xf0;
		addr -= 0xf0;
	}
	if (addImm)
		xADD(modReg, addImm);
}

void VifUnpackSSE_Dynarec::ModUnpack(int upknum, bool PostOp)
{

	switch (upknum)
	{
		case 0:
		case 1:
		case 2:
			if (PostOp)
			{
				UnpkLoopIteration++;
				UnpkLoopIteration = UnpkLoopIteration & 0x3;
			}
			break;

		case 4:
		case 5:
		case 6:
			if (PostOp)
			{
				UnpkLoopIteration++;
				UnpkLoopIteration = UnpkLoopIteration & 0x1;
			}
			break;

		case 8:
			if (PostOp)
			{
				UnpkLoopIteration++;
				UnpkLoopIteration = UnpkLoopIteration & 0x1;
			}
			break;
		case 9:
			if (!PostOp)
			{
				UnpkLoopIteration++;
			}
			break;
		case 10:
			if (!PostOp)
			{
				UnpkLoopIteration++;
			}
			break;

		case 12:
		case 13:
		case 14:
		case 15:
			break;

		case 3:
		case 7:
		case 11:
			// TODO: Needs hardware testing.
			// Dynasty Warriors 5: Empire  - Player 2 chose a character menu.
			Console.Warning("Vpu/Vif: Invalid Unpack %d", upknum);
			break;
	}
}

void VifUnpackSSE_Dynarec::ProcessMasks()
{
	skipProcessing = false;
	inputMasked = false;

	if (!doMask)
		return;

	const int cc = std::min(vCL, 3);
	const u32 full_mask = (vB.mask >> (cc * 8)) & 0xff;
	const u32 rowcol_mask = ((full_mask >> 1) | full_mask) & 0x55; // Rows or Cols being written instead of data, or protected.

	// Every channel is write protected for this cycle, no need to process anything.
	skipProcessing = full_mask == 0xff;

	// All channels are masked, no reason to process anything here.
	inputMasked = rowcol_mask == 0x55;
}

void VifUnpackSSE_Dynarec::CompileRoutine()
{
	const int wl        = vB.wl ? vB.wl : 256; // 0 is taken as 256 (KH2)
	const int upkNum    = vB.upkType & 0xf;
	const u8& vift      = nVifT[upkNum];
	const int cycleSize = isFill ? vB.cl : wl;
	const int blockSize = isFill ? wl : vB.cl;
	const int skipSize  = blockSize - cycleSize;

	uint vNum = vB.num ? vB.num : 256;
	doMode    = (upkNum == 0xf) ? 0 : doMode; // V4_5 has no mode feature.
	UnpkNoOfIterations = 0;
	VIF_LOG("Compiling new block, unpack number %x, mode %x, masking %x, vNum %x", upkNum, doMode, doMask, vNum);

	pxAssume(vCL == 0);

	// Need a zero register for V2_32/V3 unpacks.
	const bool needXmmZero = (upkNum >= 8 && upkNum <= 10) || upkNum == 4;

#ifdef _WIN32
	// See SetMasks()
	const u32 m0 = vB.mask;
	const u32 m3 = ((m0 & 0xaaaaaaaa) >> 1) & ~m0;
	const u32 m2 = (m0 & 0x55555555) & (~m0 >> 1);
	// see doMaskWrite()
	const u32 m4 = (m0 & ~((m3 << 1) | m2)) & 0x55555555;
	const u32 m5 = ~(m2 | m3 | m4) & 0x0f0f0f0f;

	int regsUsed = 2;
	// Allocate column registers
	if (doMask && m3)
	{
		colRegs[0] = xRegisterSSE(regsUsed++);

		const int cS = isFill ? blockSize : cycleSize;
		if ((cS >= 2) && (m3 & 0x0000ff00))
			colRegs[1] = xRegisterSSE(regsUsed++);
		if ((cS >= 3) && (m3 & 0x00ff0000))
			colRegs[2] = xRegisterSSE(regsUsed++);
		if ((cS >= 4) && (m3 & 0xff000000))
			colRegs[3] = xRegisterSSE(regsUsed++);
		// Column 0 already accounted for
	}

	std::array<xRegisterSSE, 3> nonVolatileRegs;

	// Allocate row register
	if ((doMask && m2) || doMode)
	{
		if (regsUsed - 6 >= 0)
			nonVolatileRegs[regsUsed - 6] = rowReg;
		rowReg = xRegisterSSE(regsUsed++);
	}

	// Allocate temp register
	if (doMode && (doMode != 3) &&
		doMask && m5 != 0x0f0f0f0f)
	{
		if (regsUsed - 6 >= 0)
			nonVolatileRegs[regsUsed - 6] = tmpReg;
		tmpReg = xRegisterSSE(regsUsed++);
	}

	// Allocate zero register
	if (needXmmZero)
	{
		if (regsUsed - 6 >= 0)
			nonVolatileRegs[regsUsed - 6] = zeroReg;
		zeroReg = xRegisterSSE(regsUsed++);
	}
	
	regsUsed -= 6;
	// Backup non-volatile registers if needed
	if (regsUsed > 0)
	{
		xSUB(rsp, 8 + 16 * regsUsed);
		for (int i = 0; i < regsUsed; i++)
			xMOVAPS(ptr128[rsp + 16 * i], nonVolatileRegs[i]);
	}
#else
	colRegs[0] = xmm2;
	colRegs[1] = xmm3;
	colRegs[2] = xmm4;
	colRegs[3] = xmm5;
	rowReg = xmm6;
	tmpReg = xmm7;
	// zeroReg already set;
#endif

	// Value passed determines # of col regs we need to load
	SetMasks(isFill ? blockSize : cycleSize);

	
	if (needXmmZero)
		xXOR.PS(zeroReg, zeroReg);

	while (vNum)
	{
		ShiftDisplacementWindow(dstIndirect, arg1reg);

		if (UnpkNoOfIterations == 0)
			ShiftDisplacementWindow(srcIndirect, arg2reg); //Don't need to do this otherwise as we arent reading the source.

		// Determine if reads/processing can be skipped.
		ProcessMasks();

		if (vCL < cycleSize)
		{
			ModUnpack(upkNum, false);
			xUnpack(upkNum);
			xMovDest();
			ModUnpack(upkNum, true);

			dstIndirect += 16;
			srcIndirect += vift;

			vNum--;
			if (++vCL == blockSize)
				vCL = 0;
		}
		else if (isFill)
		{
			// Filling doesn't need anything fancy, it's pretty much a normal write, just doesnt increment the source.
			xUnpack(upkNum);
			xMovDest();

			dstIndirect += 16;

			vNum--;
			if (++vCL == blockSize)
				vCL = 0;
		}
		else
		{
			dstIndirect += (16 * skipSize);
			vCL = 0;
		}
	}

	if (doMode >= 2)
		writeBackRow();

#ifdef _WIN32
	// Restore non-volatile registers
	if (regsUsed > 0)
	{
		for (int i = 0; i < regsUsed; i++)
			xMOVAPS(nonVolatileRegs[i], ptr128[rsp + 16 * i]);
		xADD(rsp, 8 + 16 * regsUsed);
	}
#endif

	xRET();
}

static u16 dVifComputeLength(uint cl, uint wl, u8 num, bool isFill)
{
	uint length = (num > 0) ? (num * 16) : 4096; // 0 = 256

	if (!isFill)
	{
		const uint skipSize = (cl - wl) * 16;
		const uint blocks   = (num + (wl - 1)) / wl; //Need to round up num's to calculate skip size correctly.
		length += (blocks - 1) * skipSize;
	}

	return std::min(length, 0xFFFFu);
}

_vifT __fi nVifBlock* dVifCompile(nVifBlock& block, bool isFill)
{
	nVifStruct& v = nVif[idx];

	// Check size before the compilation
	if (v.recWritePtr >= v.recEndPtr)
	{
		DevCon.WriteLn("nVif Recompiler Cache Reset! [0x%016" PRIXPTR " > 0x%016" PRIXPTR "]",
			v.recWritePtr, v.recEndPtr);
		dVifReset(idx);
	}

	// Compile the block now
	xSetPtr(v.recWritePtr);

	block.startPtr = (uptr)xGetAlignedCallTarget();
	block.length = dVifComputeLength(block.cl, block.wl, block.num, isFill);
	v.vifBlocks.add(block);

	VifUnpackSSE_Dynarec(v, block).CompileRoutine();

	Perf::vif.RegisterPC(v.recWritePtr, xGetPtr() - v.recWritePtr, block.upkType /* FIXME ideally a key*/);
	v.recWritePtr = xGetPtr();

	return &block;
}

_vifT __fi void dVifUnpack(const u8* data, bool isFill)
{

	nVifStruct&   v       = nVif[idx];
	vifStruct&    vif     = MTVU_VifX;
	VIFregisters& vifRegs = MTVU_VifXRegs;

	const u8  upkType = (vif.cmd & 0x1f) | (vif.usn << 5);
	const int doMask  = isFill ? 1 : (vif.cmd & 0x10);

	nVifBlock block;

	// Performance note: initial code was using u8/u16 field of the struct
	// directly. However reading back the data (as u32) in HashBucket.find
	// leads to various memory stalls. So it is way faster to manually build the data
	// in u32 (aka x86 register).
	//
	// Warning the order of data in hash_key/key0/key1 depends on the nVifBlock struct
	const u32 hash_key = static_cast<u32>(upkType & 0xFF) << 8 | (vifRegs.num & 0xFF);

	u32 key1 = (static_cast<u32>(vifRegs.cycle.wl) << 24) | (static_cast<u32>(vifRegs.cycle.cl) << 16) | (static_cast<u32>(vif.start_aligned & 0xFF) << 8) | (static_cast<u32>(vifRegs.mode) & 0xFF);
	if ((upkType & 0xf) != 9)
		key1 &= 0xFFFF01FF;

	// Zero out the mask parameter if it's unused -- games leave random junk
	// values here which cause false recblock cache misses.
	const u32 key0 = doMask ? vifRegs.mask : 0;

	block.hash_key = hash_key;
	block.key0 = key0;
	block.key1 = key1;

	//DevCon.WriteLn("nVif%d: Recompiled Block!", idx);
	//DevCon.WriteLn(L"[num=% 3d][upkType=0x%02x][scl=%d][cl=%d][wl=%d][mode=%d][m=%d][mask=%s]",
	//	block.num, block.upkType, block.scl, block.cl, block.wl, block.mode,
	//	doMask >> 4, doMask ? wxsFormat( L"0x%08x", block.mask ).c_str() : L"ignored"
	//);

	// Seach in cache before trying to compile the block
	nVifBlock* b = v.vifBlocks.find(block);
	if (!b) [[unlikely]]
		b = dVifCompile<idx>(block, isFill);

	{ // Execute the block
		const VURegs& VU = vuRegs[idx];
		constexpr uint vuMemLimit = idx ? 0x4000 : 0x1000;

		u8* startmem = VU.Mem + (vif.tag.addr & (vuMemLimit - 0x10));
		u8* endmem   = VU.Mem + vuMemLimit;

		if ((startmem + b->length) <= endmem) [[likely]]
		{
			// No wrapping, you can run the fast dynarec
			((nVifrecCall)b->startPtr)((uptr)startmem, (uptr)data);
		}
		else
		{
			VIF_LOG("Running Interpreter Block: nVif%x - VU Mem Ptr Overflow; falling back to interpreter. Start = %x End = %x num = %x, wl = %x, cl = %x",
				v.idx, vif.tag.addr, vif.tag.addr + (block.num * 16), block.num, block.wl, block.cl);
			_nVifUnpack(idx, data, vifRegs.mode, isFill);
		}
	}
}

template void dVifUnpack<0>(const u8* data, bool isFill);
template void dVifUnpack<1>(const u8* data, bool isFill);
