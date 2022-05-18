/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// newVif Dynarec - Dynamically Recompiles Vif 'unpack' Packets
// authors: cottonvibes(@gmail.com)
//			Jake.Stine (@gmail.com)

#include "PrecompiledHeader.h"
#include "newVif_UnpackSSE.h"
#include "MTVU.h"
#include "common/Perf.h"
#include "fmt/core.h"

static void recReset(int idx)
{
	nVif[idx].vifBlocks.reset();

	nVif[idx].recReserve->Reset();

	nVif[idx].recWritePtr = nVif[idx].recReserve->GetPtr();
}

void dVifReserve(int idx)
{
	if (!nVif[idx].recReserve)
		nVif[idx].recReserve = new RecompiledCodeReserve(fmt::format("VIF{} Unpack Recompiler Cache", idx), _8mb);

	auto offset = idx ? HostMemoryMap::VIF1recOffset : HostMemoryMap::VIF0recOffset;
	nVif[idx].recReserve->Reserve(GetVmMemory().MainMemory(), offset, 8 * _1mb);
}

void dVifReset(int idx)
{
	pxAssertDev(nVif[idx].recReserve, "Dynamic VIF recompiler reserve must be created prior to VIF use or reset!");

	recReset(idx);
}

void dVifClose(int idx)
{
	if (nVif[idx].recReserve)
		nVif[idx].recReserve->Reset();
}

void dVifRelease(int idx)
{
	dVifClose(idx);
	safe_delete(nVif[idx].recReserve);
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
	u32 m0 = vB.mask; //The actual mask example 0x03020100
	u32 m3 = ((m0 & 0xaaaaaaaa) >> 1) & ~m0; //all the upper bits, so our example 0x01010000 & 0xFCFDFEFF = 0x00010000 just the cols (shifted right for maskmerge)
	u32 m2 = (m0 & 0x55555555) & (~m0 >> 1); // 0x1000100 & 0xFE7EFF7F = 0x00000100 Just the row

	if ((m2 && doMask) || doMode)
	{
		xMOVAPS(xmmRow, ptr128[&vif.MaskRow]);
		MSKPATH3_LOG("Moving row");
	}
	if (m3 && doMask)
	{
		MSKPATH3_LOG("Merging Cols");
		xMOVAPS(xmmCol0, ptr128[&vif.MaskCol]);
		if ((cS >= 2) && (m3 & 0x0000ff00)) xPSHUF.D(xmmCol1, xmmCol0, _v1);
		if ((cS >= 3) && (m3 & 0x00ff0000)) xPSHUF.D(xmmCol2, xmmCol0, _v2);
		if ((cS >= 4) && (m3 & 0xff000000)) xPSHUF.D(xmmCol3, xmmCol0, _v3);
		if ((cS >= 1) && (m3 & 0x000000ff)) xPSHUF.D(xmmCol0, xmmCol0, _v0);
	}
	//if (doMask||doMode) loadRowCol((nVifStruct&)v);
}

void VifUnpackSSE_Dynarec::doMaskWrite(const xRegisterSSE& regX) const
{
	pxAssertDev(regX.Id <= 1, "Reg Overflow! XMM2 thru XMM6 are reserved for masking.");

	int cc = std::min(vCL, 3);
	u32 m0 = (vB.mask >> (cc * 8)) & 0xff; //The actual mask example 0xE4 (protect, col, row, clear)
	u32 m3 = ((m0 & 0xaa) >> 1) & ~m0; //all the upper bits (cols shifted right) cancelling out any write protects 0x10
	u32 m2 = (m0 & 0x55) & (~m0 >> 1); // all the lower bits (rows)cancelling out any write protects 0x04
	u32 m4 = (m0 & ~((m3 << 1) | m2)) & 0x55; //  = 0xC0 & 0x55 = 0x40 (for merge mask)

	makeMergeMask(m2);
	makeMergeMask(m3);
	makeMergeMask(m4);

	if (doMask && m2) // Merge MaskRow
	{
		mergeVectors(regX, xmmRow, xmmTemp, m2);
	}
	if (doMask && m3) // Merge MaskCol
	{
		mergeVectors(regX, xRegisterSSE(xmmCol0.Id + cc), xmmTemp, m3);
	}
	if (doMask && m4) // Merge Write Protect
	{
		xMOVAPS(xmmTemp, ptr[dstIndirect]);
		mergeVectors(regX, xmmTemp, xmmTemp, m4);
	}
	if (doMode)
	{
		u32 m5 = ~(m2 | m3 | m4) & 0xf;

		if (!doMask)
			m5 = 0xf;

		if (m5 < 0xf)
		{
			xPXOR(xmmTemp, xmmTemp);
			if (doMode == 3)
			{
				mergeVectors(xmmRow, regX, xmmTemp, m5);
			}
			else
			{
				mergeVectors(xmmTemp, xmmRow, xmmTemp, m5);
				xPADD.D(regX, xmmTemp);
				if (doMode == 2)
					mergeVectors(xmmRow, regX, xmmTemp, m5);
			}
		}
		else
		{
			if (doMode == 3)
			{
				xMOVAPS(xmmRow, regX);
			}
			else
			{
				xPADD.D(regX, xmmRow);
				if (doMode == 2)
					xMOVAPS(xmmRow, regX);
			}
		}
	}
	xMOVAPS(ptr32[dstIndirect], regX);
}

void VifUnpackSSE_Dynarec::writeBackRow() const
{
	const int idx = v.idx;
	xMOVAPS(ptr128[&(MTVU_VifX.MaskRow)], xmmRow);

	DevCon.WriteLn("nVif: writing back row reg! [doMode = %d]", doMode);
	// ToDo: Do we need to write back to vifregs.rX too!? :/
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
			pxFailRel(fmt::format("Vpu/Vif - Invalid Unpack! [%d]", upknum).c_str());
			break;
	}
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
	MSKPATH3_LOG("Compiling new block, unpack number %x, mode %x, masking %x, vNum %x", upkNum, doMode, doMask, vNum);

	pxAssume(vCL == 0);

	// Value passed determines # of col regs we need to load
	SetMasks(isFill ? blockSize : cycleSize);

	while (vNum)
	{


		ShiftDisplacementWindow(dstIndirect, arg1reg);

		if (UnpkNoOfIterations == 0)
			ShiftDisplacementWindow(srcIndirect, arg2reg); //Don't need to do this otherwise as we arent reading the source.


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
			//Filling doesn't need anything fancy, it's pretty much a normal write, just doesnt increment the source.
			//DevCon.WriteLn("filling mode!");
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
	xRET();
}

static u16 dVifComputeLength(uint cl, uint wl, u8 num, bool isFill)
{
	uint length = (num > 0) ? (num * 16) : 4096; // 0 = 256

	if (!isFill)
	{
		uint skipSize = (cl - wl) * 16;
		uint blocks   = (num + (wl - 1)) / wl; //Need to round up num's to calculate skip size correctly.
		length += (blocks - 1) * skipSize;
	}

	return std::min(length, 0xFFFFu);
}

_vifT __fi nVifBlock* dVifCompile(nVifBlock& block, bool isFill)
{
	nVifStruct& v = nVif[idx];

	// Check size before the compilation
	if (v.recWritePtr > (v.recReserve->GetPtrEnd() - _256kb))
	{
		DevCon.WriteLn("nVif Recompiler Cache Reset! [0x%016" PRIXPTR " > 0x%016" PRIXPTR "]",
			v.recWritePtr, v.recReserve->GetPtrEnd());
		recReset(idx);
	}

	// Compile the block now
	xSetPtr(v.recWritePtr);

	block.startPtr = (uptr)xGetAlignedCallTarget();
	block.length = dVifComputeLength(block.cl, block.wl, block.num, isFill);
	v.vifBlocks.add(block);

	VifUnpackSSE_Dynarec(v, block).CompileRoutine();

	Perf::vif.map((uptr)v.recWritePtr, xGetPtr() - v.recWritePtr, block.upkType /* FIXME ideally a key*/);
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
	u32 hash_key = (u32)(upkType & 0xFF) << 8 | (vifRegs.num & 0xFF);

	u32 key1 = ((u32)vifRegs.cycle.wl << 24) | ((u32)vifRegs.cycle.cl << 16) | ((u32)(vif.start_aligned & 0xFF) << 8) | ((u32)vifRegs.mode & 0xFF);
	if ((upkType & 0xf) != 9)
		key1 &= 0xFFFF01FF;

	// Zero out the mask parameter if it's unused -- games leave random junk
	// values here which cause false recblock cache misses.
	u32 key0 = doMask ? vifRegs.mask : 0;

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
	if (unlikely(b == nullptr))
	{
		b = dVifCompile<idx>(block, isFill);
	}

	{ // Execute the block
		const VURegs& VU = vuRegs[idx];
		const uint vuMemLimit = idx ? 0x4000 : 0x1000;

		u8* startmem = VU.Mem + (vif.tag.addr & (vuMemLimit - 0x10));
		u8* endmem   = VU.Mem + vuMemLimit;

		if (likely((startmem + b->length) <= endmem))
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
