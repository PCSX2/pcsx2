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

// newVif!
// authors: cottonvibes(@gmail.com)
//			Jake.Stine (@gmail.com)

#include "PrecompiledHeader.h"
#include "Common.h"
#include "newVif.h"
#include "Vif_Unpack.h"

__aligned16 nVifStruct	nVif[2];
__aligned16 nVifCall	nVifUpk[(2*2*16)  *4];		// ([USN][Masking][Unpack Type]) [curCycle]
__aligned16 u32			nVifMask[3][4][4] = {0};	// [MaskNumber][CycleNumber][Vector]

// ----------------------------------------------------------------------------


static __fi u8* getVUptr(int idx, int offset) {
	return (u8*)(vuRegs[idx].Mem + ( offset & (idx ? 0x3ff0 : 0xff0) ));
}

void VifUnpackSetMasks(VifProcessingUnit& vpu, const VIFregisters& v) {
	for (int i = 0; i < 16; i++) {
		int m = (v.mask >> (i*2)) & 3;
		switch (m) {
			case 0: // Data
				nVifMask[0][i/4][i%4] = 0xffffffff;
				nVifMask[1][i/4][i%4] = 0;
				nVifMask[2][i/4][i%4] = 0;
				break;
			case 1: // MaskRow
				nVifMask[0][i/4][i%4] = 0;
				nVifMask[1][i/4][i%4] = 0;
				nVifMask[2][i/4][i%4] = vpu.MaskRow._u32[i%4];
				break;
			case 2: // MaskCol
				nVifMask[0][i/4][i%4] = 0;
				nVifMask[1][i/4][i%4] = 0;
				nVifMask[2][i/4][i%4] = vpu.MaskCol._u32[i/4];
				break;
			case 3: // Write Protect
				nVifMask[0][i/4][i%4] = 0;
				nVifMask[1][i/4][i%4] = 0xffffffff;
				nVifMask[2][i/4][i%4] = 0;
				break;
		}
	}
}

// ----------------------------------------------------------------------------
//  Unpacking Optimization notes:
// ----------------------------------------------------------------------------
// Some games send a LOT of single-cycle packets (God of War, SotC, TriAce games, etc),
// so we always need to be weary of keeping loop setup code optimized.  It's not always
// a "win" to move code outside the loop, like normally in most other loop scenarios.
//

// size - size of the packet fragment incoming from DMAC.
template< uint idx, bool doMode, bool isFill >
__ri void __fastcall _nVifUnpackLoop(const uint vSize, const u8* src)
{
	VifProcessingUnit& vpu = vifProc[idx];
	VIFregisters& regs = GetVifXregs;

	// skipSize used for skipping writes only
	const int skipSize  = (regs.cycle.cl - regs.cycle.wl) * 16;

	//DevCon.WriteLn("[%d][%d][%d][num=%d][upk=%d][cl=%d][bl=%d][skip=%d]", isFill, doMask, doMode, regs.num, upkNum, vpu.cl, blockSize, skipSize);

	const int				upkNum	= regs.code.CMD & 0x1f;
	const bool				usn		= regs.code.USN;
	const UNPACKFUNCTYPE	ft		= VIFfuncTable[idx][doMode ? regs.mode : 0][ (usn*2*16) + upkNum ];

	// used for SSE unpacks only:
	const nVifCall*			fnbase	= &nVifUpk[ ((usn*2*16) + upkNum) * (4*1) ];

	pxAssume (vpu.cl == 0);
	pxAssume (regs.cycle.wl > 0);

	do {
		u8* dest = getVUptr(idx, vpu.vu_target_addr);
		vpu.vu_target_addr += 16;

		if (doMode) {
			ft(dest, src);
		}
		else {
			//DevCon.WriteLn("SSE Unpack!");
			uint cl3 = aMin(vpu.cl,3);
			fnbase[cl3]((u32*)dest, (u32*)src);
		}

		++vpu.cl;

		if (isFill) {
			//DevCon.WriteLn("isFill!");
			if (vpu.cl < regs.cycle.cl)			src += vSize;
			else if (vpu.cl == regs.cycle.wl)	vpu.cl = 0;
		}
		else
		{
			src += vSize;

			if (vpu.cl >= regs.cycle.wl) {
				vpu.vu_target_addr += skipSize;
				vpu.cl = 0;
			}
		}
	} while (--regs.num);
}

static const __aligned16 Fnptr_VifUnpackLoop VifUnpackLoopTable[2][2][2] = {
	{{ _nVifUnpackLoop<0,0,0>, _nVifUnpackLoop<0,0,1> },
	{  _nVifUnpackLoop<0,1,0>, _nVifUnpackLoop<0,1,1> },},
	{{ _nVifUnpackLoop<1,0,0>, _nVifUnpackLoop<1,0,1> },
	{  _nVifUnpackLoop<1,1,0>, _nVifUnpackLoop<1,1,1> },},
};
