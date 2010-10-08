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

#include "PrecompiledHeader.h"
#include "Common.h"
#include "GS.h"
#include "Gif.h"
#include "newVif.h"
#include "VUmicro.h"

#define _vifCodeT template< uint idx > static void

#define vif1Only()	pxAssumeMsg( idx==1, "Vifcode command is VIF1-only; please remove the link from the VIFcode0 LUT.")

_vifCodeT vifCode_Null();

// ------------------------------------------------------------------
//  Vif0/Vif1 Misc Functions
// ------------------------------------------------------------------

template<uint idx>
static __fi void vuExecMicro(VIFregisters& regs, u32 addr)
{
	if (idx)
	{
		// in case we're handling a VIF1 execMicro, set the top with the tops value
		regs.top = regs.tops;

		// VU1 Double-buffering operation!  Depending on the DBF flag, we do the following:
		//  * set tops with base, and clear the stat DBF flag
		//  * set tops with base + offset, and set stat DBF flag

		if (regs.stat.DBF) {
			regs.tops = regs.base;
			regs.stat.DBF = 0;
		}
		else {
			regs.tops = regs.base + regs.ofst;
			regs.stat.DBF = 1;
		}

		regs.itop = regs.itops;

		vu1ExecMicro(addr);
	}
	else
	{
		vu0ExecMicro(addr);
	}
}

// ------------------------------------------------------------------
//  Vif0/Vif1 Code Implementations
// ------------------------------------------------------------------
// Pass1 is for simple commands that require only 32 bits to process.
// Pass3 is for logging.


// Writes lower 10 bits of the code to the BASE register.
_vifCodeT vc_Base() {
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("Base @ 0x%04X", regs.code.ADDR);
	vif1Only();

	regs.base = regs.code.ADDR;
}

// Perform direct transfer of GIFtag data to the GIF via PATH2.  The GIF manually checks the
// vifProc[1] code status to determine PATH3 interruption procedures, thus both DIRECT/HL
// codes are handled by the same function here.
//
_vifCodeT vc_Direct()
{
	VifProcessingUnit&	vpu		= vifProc[idx];

	vif1Only();

	// DIRECT: If PATH3 is in transfer and has intermittent transfer mode enabled, the transfer
	// can be interrupted.
	// DIRECTHL:  PATH3 transfer is not interrupted, regardless of PATH3 transfer status.

	if (vif1Regs.stat.VPS == VPS_IDLE)
	{
		VifCodeLog("Direct%s IMM=0x%04X qwc",
			(vif1Regs.code.CMD == VIFcode_DIRECTHL) ? "HL" : "",
			vif1Regs.code.IMMEDIATE
		);

		uint vifImm		= vif1Regs.code.IMMEDIATE;
		vif1Regs.num	= (vifImm ? vifImm : 65536);

		// The VIF has a strict alignment requirement on DIRECT/HL tags.  The tag must be
		// positioned such that the data is 128-bit aligned.
		pxAssume(((uptr)vpu.data & 15) == 0);
		pxAssume((vpu.fragment_size & 3) == 0);

		if (vpu.fragment_size == 0)
		{
			vif1Regs.stat.VPS = VPS_WAITING;
			return;
		}

		// Attempt to acquire the GIF via PATH2 transfer immediately.  If the GIF is busy then
		// we'll have to stall transfer until it becomes available.

		if (!GIF_QueuePath2())
		{
			vif1Regs.stat.VPS = VPS_TRANSFERRING;
			return;
		}
	}

	// Fragment size should never be zero here since the VIFunpacker shouldn't rerun the
	// code with an empty packet (if fragment size is zero on the first run, it's handled above).
	pxAssume( vpu.fragment_size != 0 );

	uint minSize = std::min(vif1Regs.num, vpu.fragment_size/4);
	uint ret = GIF_UploadTag((u128*)vpu.data, minSize);

	vpu.data			+= ret * 4;
	vpu.fragment_size	-= ret * 4;
	vif1Regs.num		-= ret;
	vif1Regs.stat.VPS	= VPS_IDLE;

	if (vif1Regs.num != 0)
	{
		// Partial transfer.  Whether or not we're waiting for the GIF to finish transfer or
		// waiting for the VIF to acquire more data depends on the fragment_size.
		vif1Regs.stat.VPS = vpu.fragment_size ? VPS_TRANSFERRING : VPS_WAITING;
	}
}

_vifCodeT vc_ITop()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("ITop @ 0x%03x", regs.code.ADDR);

	regs.itops = regs.code.ADDR;
}

_vifCodeT vc_Mark()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("Mark = 0x%04x", regs.code.IMMEDIATE);

	regs.mark     = regs.code.IMMEDIATE;
	regs.stat.MRK = 1;
	//regs.stat.INT = 1;
}


// Returns TRUE if the VU microprogram finished execution, or FALSE if the microprogram
// itself cannot finish execution until some other system event occurs.
static __fi bool flushVU(int idx)
{

	if (idx)
	{
		// Note that the only realistic reason for failure here is if we're operating on VPU1
		// and the VU is blocking due to an XGKICK.  The EE may be able to clear such a condition
		// by completing a PATH3 transfer or clearing the SIGNAL/IMR condition.
		
		vif0Regs.stat.VEW = 1;
		vif1Regs.stat.VEW = !vu1Finish();
		return !vif1Regs.stat.VEW;
	}
	else
	{
		// The VU0 automatically ForceBreaks after running for a long period of time without
		// encountering an E-Bit, STOP, or ForceBreak (currently not implemented).  Thusly, the
		// VU0 cannot deadlock, and games can actually block against a VU0 which is spinning
		// indefinitely, and then expect it *not* to deadlock. ;)

		vif0Regs.stat.VEW = 1;
		vu0Finish();
		vif0Regs.stat.VEW = 0;
		return true;
	}
}

// Returns TRUE if both PATHs 1 and 2 flushed successfully, or FALSE if the flush stalled
// due to some other event obstruction (typically should only happen in purist DMA emulation
// modes -- if UseDmaBurstHask is enabled, this should always return TRUE).
static __fi bool flushPaths12(uint idx)
{
	if (gifRegs.stat.APATH == GIF_APATH_IDLE)
	{
		pxAssume( !gifRegs.stat.OPH );
		return true;
	}
	
	GetVifXregs.stat.VGW = (gifRegs.stat.APATH != GIF_APATH3);
	return !GetVifXregs.stat.VGW;
}

// Flushes only the current microprogram.  Pending PATH transfers are not relevant.
_vifCodeT vc_FlushE()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("FlushE");

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx)) return;
	regs.stat.VPS = VPS_IDLE;
}

// Flushes the running microprogram and waits for VIF-related PATH transfers to finish
// (PATHs 1 and 2 only).  [vif1 only]
_vifCodeT vc_Flush()
{
	VIFregisters&		regs	= GetVifXregs;

	VifCodeLog("Flush");
	vif1Only();

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx) || !flushPaths12(idx)) return;
	regs.stat.VPS = VPS_IDLE;
}

// Flushes the running microprogram and waits for all PATH transfers to finish
// (PATHs 1, 2, and 3).  [vif1 only]
_vifCodeT vc_FlushA()
{
	VIFregisters&		regs	= GetVifXregs;

	VifCodeLog("FlushA");
	vif1Only();

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx) || (gifRegs.stat.APATH != GIF_APATH_IDLE)) return;
	regs.stat.VPS = VPS_IDLE;

	pxAssume( !gifRegs.stat.OPH );

	// Casual note:
	// PATH3 could be busy for more than a few reasons; such as the EE/DMAC being masked or
	// disabled temporarily, or the EE performing direct GIF FIFO writes.

	// [TODO] register a listener for GIFpath.EOP?
	//  (the DMAC will eventually re-invoke VIF regardless, though an EOP listener would
	//   be more direct and perhaps more efficient).
}

_vifCodeT vc_MPG()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	if (regs.stat.VPS == VPS_IDLE)
	{
		VifCodeLog("MPG  imm=0x%04X  num=0x%03X", regs.code.IMMEDIATE, regs.code.NUM);

		regs.num			= regs.code.NUM ? regs.code.NUM : 256;
		vpu.vu_target_addr	= regs.code.IMMEDIATE;
		
		// The VIF has a strict alignment requirement on MPG tags.  The tag must be
		// positioned such that the data is 64-bit aligned.
		pxAssume(((uptr)vpu.data & 7) == 0);
		pxAssume((vpu.fragment_size & 1) == 0);

		if (vpu.fragment_size == 0)
		{
			regs.stat.VPS = VPS_WAITING;
			return;
		}
	}

	// Fragment size should never be zero here (zero is checked for above)
	pxAssume(vpu.fragment_size != 0);

	uint minSize64 = min(regs.num, vpu.fragment_size/2);		// size, in 64 bit units.

	u64* dest = vuRegs[idx].GetProgMem(vpu.vu_target_addr);
	if (memcmp_mmx(dest, vpu.data, minSize64*8)) {
		// (VUs expect size to be 32-bit scale and addresses to be in bytes -_-)
		if (!vpu.idx)	CpuVU0->Clear(vpu.vu_target_addr*8, minSize64*2);
		else			CpuVU1->Clear(vpu.vu_target_addr*8, minSize64*2);

		memcpy_fast(dest, vpu.data, minSize64*8);
	}

	vpu.data			+= minSize64 * 2;
	vpu.fragment_size	-= minSize64 * 2;
	regs.num			-= minSize64;
	regs.stat.VPS		= VPS_IDLE;

	if (regs.num != 0)
	{
		// Partial transfer.  Whether or not we're waiting for the GIF to finish transfer or
		// waiting for the VIF to acquire more data depends on the fragment_size.
		regs.stat.VPS = vpu.fragment_size ? VPS_TRANSFERRING : VPS_WAITING;
		vpu.vu_target_addr += minSize64;
	}
}

// Finishes execution of the current microprogram and starts execution of a new microprogram.
// Immediate field of the VIFcode contains the execution address of the new program.
_vifCodeT vc_MSCAL()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("MSCAL imm=0x%04X", regs.code.IMMEDIATE);

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx)) return;
	regs.stat.VPS = VPS_IDLE;

	vuExecMicro<idx>(regs, regs.code.IMMEDIATE * 8);
}

_vifCodeT vc_MSCALF()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("MSCALF imm=0x%04X", regs.code.IMMEDIATE);

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx) || !flushPaths12(idx)) return;
	regs.stat.VPS = VPS_IDLE;

	vuExecMicro<idx>(regs, regs.code.IMMEDIATE * 8);
}

_vifCodeT vc_MSCNT()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("MSCNT");

	regs.stat.VPS = VPS_WAITING;
	if (!flushVU(idx)) return;
	regs.stat.VPS = VPS_IDLE;

	vuExecMicro<idx>(regs, -1);
}

_vifCodeT vc_MskPath3()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("MskPath3 %s", regs.code.MASKPATH3 ? "enable" : "disable");
	vif1Only();

	if (vpu.maskpath3 && !regs.code.MASKPATH3)
		GIF_ArbitratePaths();

	vpu.maskpath3 = regs.code.MASKPATH3;
	
	// If set to FALSE, The GIF will deny PATH3 arbitration accordingly the next time
	// the GIF DMA (PATH3) is invoked.
}

_vifCodeT vc_Nop()
{
	VifCodeLog("Nop");
}

// ToDo: Review Flags
_vifCodeT vc_Null()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	VifCodeLog("Null [cmd=0x%02X]", regs.code.CMD);

	// if ME1 is unmasked (0), then force the vif to stall
	// [Ps2Confirm] Its unknown if VIF errors should cause an interrupt or not.  Current assumption
	//  is that an interrupt is generated.

	if (!regs.err.ME1)
	{
		VIF_LOG("VIF STALL due to unmasked ME1 (invalid code).");
		pxFailDev( pxsFmt("VIF STALL due to unmasked ME1 (invalid code). [cmd=0x%02x]", regs.code.CMD) );
		regs.stat.ER1 = 1;
		hwIntcIrq(idx ? INTC_VIF1 : INTC_VIF0);
	}
}

_vifCodeT vc_Offset()
{
	VifCodeLog("Offset @ 0x%04X (base/tops=0x%04X)", vif1Regs.code.ADDR, vif1Regs.base);
	vif1Only();

	vif1Regs.stat.DBF	= 0;
	vif1Regs.ofst.ADDR	= vif1Regs.code.ADDR;
	vif1Regs.tops		= vif1Regs.base;
}

// Loads the following 128 bits of data into the C0->C3 registers (32 bits each).  PCSX2 also
// stores the values internally at g_vifmask.Col0-3 for SIMD convenience.
_vifCodeT vc_STCol()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	if (regs.stat.VPS == VPS_IDLE)
	{
		VifCodeLog("STCol");
		vpu.running_idx = 0;
	}

	do
	{
		vpu.MaskCol._u32[vpu.running_idx] = *vpu.data;

		++vpu.data;
		++vpu.running_idx;
		--vpu.fragment_size;

		if (vpu.fragment_size == 0)
		{
			regs.stat.VPS = VPS_WAITING;
			return;
		}
	} while(vpu.running_idx < 4);

	regs.stat.VPS = VPS_IDLE;
}

_vifCodeT vc_STRow()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	if (regs.stat.VPS == VPS_IDLE)
	{
		VifCodeLog("STRow");
		vpu.running_idx = 0;
	}

	do
	{
		vpu.MaskRow._u32[vpu.running_idx] = *vpu.data;

		++vpu.data;
		++vpu.running_idx;
		--vpu.fragment_size;

		if (vpu.fragment_size == 0)
		{
			regs.stat.VPS = VPS_WAITING;
			return;
		}
	} while(vpu.running_idx < 4);

	regs.stat.VPS = VPS_IDLE;
}

// Loads the vifRegs.CYCLE register with CL/WL values accordingly.
_vifCodeT vc_STCycl()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	u8 cl = regs.code.IMMEDIATE;
	u8 wl = regs.code.IMMEDIATE >> 8;

	VifCodeLog("STCycl cl=0x%02X  wl=0x%02X", cl, wl);

	regs.cycle.cl = cl;
	regs.cycle.wl = wl;
}

_vifCodeT vc_STMask()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	if (regs.stat.VPS == VPS_IDLE)
	{
		VifCodeLog("STMask");
		if (vpu.fragment_size == 0)
		{
			regs.stat.VPS = VPS_WAITING;
			return;
		}
	}

	pxAssume(vpu.fragment_size != 0);

	regs.mask = *vpu.data;
	++vpu.data;
	--vpu.fragment_size;
	
	regs.stat.VPS = VPS_IDLE;
}

_vifCodeT vc_STMod()
{
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;
	VifCodeLog("STMod");
	regs.mode = regs.code.MODE;
}

template< uint idx >
static uint calc_addr(bool flg)
{
	VIFregisters&		regs	= GetVifXregs;

	uint retval = regs.code.ADDR;
	if (idx && flg) retval += regs.tops.ADDR;
	return retval;
}

template< bool UseRecs, uint idx, uint mask, uint vn, uint vl >
static void vc_Unpack() {
	VifProcessingUnit&	vpu		= vifProc[idx];
	VIFregisters&		regs	= GetVifXregs;

	const bool isFill = (regs.cycle.cl < regs.cycle.wl);

	uint vSize = ((32 >> vl) * (vn+1)) / 8;		// size of data (in bytes) used for each write cycle
	u8* vpd = (u8*)vpu.data;

	if (regs.stat.VPS == VPS_IDLE)
	{
		static const char* const	vntbl[] = { "S", "V2", "V3", "V4" };
		static const uint			vltbl[] = { 32,	  16,   8,    5   };

		VifCodeLog("Unpack %s_%u (%s) @ 0x%04X%s (cl=%u  wl=%u  num=0x%02X)",
			vntbl[vn], vltbl[vl], mask ? "masked" : "unmasked", calc_addr<idx>(regs.code.FLG), 
			regs.cycle.cl, regs.cycle.wl, regs.code.NUM
		);

		regs.num			= regs.code.NUM ? regs.code.NUM : 256;
		vpu.vu_target_addr	= regs.code.ADDR;
		vpu.cl				= 0;
		vpu.running_idx		= 0;		// needed for incomplete vector write cycles (due to fragmented packets)

		if (idx && regs.code.FLG)
		{
			vpu.vu_target_addr += regs.tops.ADDR;		// TOPS register is VU1 only.
		}

		if (isFill)
		{
			// since regs.num is based on the amount of data *written* and not the amount of data
			// read, we have to do some interesting math to calculate the length of the incoming
			// packet of data when its a Filling Write.

			int limit = regs.num % regs.cycle.wl;
			if (limit > regs.cycle.cl) limit = regs.cycle.cl;

			int n = regs.cycle.cl * (regs.num / regs.cycle.wl) + limit;
			vpu.packet_size = ((n * vSize) + 3) >> 2;
		}
		else
		{
			vpu.packet_size = ((regs.num * vSize) + 3) >> 2;
		}
	}

	if (vpu.packet_size > vpu.fragment_size)
	{
		// This unpack is split across the end of the fragment.  The unpackers expect only complete
		// packets only (for sake of logic sanity), so we need to copy and queue this data into
		// a buffer until the full packet is received later.

		if (vpu.fragment_size)
		{
			vpd = (u8*)vpu.buffer;
			memcpy_fast( vpd + vpu.running_idx, vpd, vpu.fragment_size * 4 );

			vpu.data			+= vpu.fragment_size;
			vpu.running_idx		+= vpu.fragment_size;
			vpu.packet_size		-= vpu.fragment_size;

			// We need to provide accurate accounting of the NUM register, in case games decided
			// to read back from it mid-transfer.  Since so few games actually use partial transfers
			// of VIF unpacks, this code should not be any bottleneck.

			while (vpu.fragment_size >= vSize) {
				pxAssume( regs.num != 0 );
				--regs.num;
				++vpu.cl;

				if (isFill) {
					if (vpu.cl < regs.cycle.cl)			vpu.fragment_size -= vSize;
					else if (vpu.cl == regs.cycle.wl)	vpu.cl = 0;
				}
				else
				{
					vpu.fragment_size -= vSize;
					if (vpu.cl >= regs.cycle.wl) vpu.cl = 0;
				}
			}

			vpu.fragment_size	= 0;
		}

		regs.stat.VPS		= VPS_WAITING;
		return;
	}

	// Complete transfer...

	if (vpu.running_idx)
	{
		// Previous partial transfer, so we'll need to concatenate them together in the
		// buffer for processing.  We grab NUM from the original VIFcode input, since the current
		// NUM is "simulated" (above), and the unpacker expects the original value.

		vpd			= (u8*)vpu.buffer;
		memcpy_fast( vpd + vpu.running_idx, vpd, vpu.packet_size );
		regs.num	= regs.code.NUM ? regs.code.NUM : 256;
	}

	vpu.cl = 0;

	if (UseRecs)
	{
		dVifUnpack<idx,mask,(vn<<2) | vl>(vpd, isFill, vSize);
	}
	else
	{
		if ((regs.mode!=0) && mask) VifUnpackSetMasks(vpu, regs);

		const bool doMode = !!regs.mode;
		VifUnpackLoopTable[idx][doMode][isFill](vSize, vpd);
	}

	regs.stat.VPS		= VPS_IDLE;
	regs.num			= 0;
	vpu.fragment_size	-= vpu.packet_size;
	vpu.data			+= vpu.packet_size;
}

// [TODO] This can be replaced with a GUI option that modifies the following tables accordingly
// based on 2 sets of source tables (one each true and false).
static const bool UseVpuRecompilers = true;

//------------------------------------------------------------------
// Vif0/Vif1 Code Tables
//------------------------------------------------------------------

#define InsertUnpackSet(useRec, idx, doMask) \
	vc_Unpack<useRec,idx,doMask,0,0>,		vc_Unpack<useRec,idx,doMask,0,1>,		vc_Unpack<useRec,idx,doMask,0,2>,		vc_Null<0>, \
	vc_Unpack<useRec,idx,doMask,1,0>,		vc_Unpack<useRec,idx,doMask,1,1>,		vc_Unpack<useRec,idx,doMask,1,2>,		vc_Null<0>, \
	vc_Unpack<useRec,idx,doMask,2,0>,		vc_Unpack<useRec,idx,doMask,2,1>,		vc_Unpack<useRec,idx,doMask,2,2>,		vc_Null<0>, \
	vc_Unpack<useRec,idx,doMask,3,0>,		vc_Unpack<useRec,idx,doMask,3,1>,		vc_Unpack<useRec,idx,doMask,3,2>,		vc_Unpack<useRec,idx,doMask,3,3>

__aligned16 FnType_VifCmdHandler* const vifCmdHandler[2][128] =
{
	{
		vc_Nop<0>     , vc_STCycl<0>, vc_Offset<0>	, vc_Base<0>   , vc_ITop<0>   , vc_STMod<0>  , vc_MskPath3<0>, vc_Mark<0>,   /*0x00*/
		vc_Null<0>    , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x08*/
		vc_FlushE<0>  , vc_Flush<0>	, vc_Null<0>	, vc_FlushA<0> , vc_MSCAL<0>  , vc_MSCALF<0> , vc_Null<0>	 , vc_MSCNT<0>,  /*0x10*/
		vc_Null<0>    , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x18*/
		vc_STMask<0>  , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>	 , vc_Null<0>,   /*0x20*/
		vc_Null<0>    , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>	 , vc_Null<0>,   /*0x28*/
		vc_STRow<0>   , vc_STCol<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>	 , vc_Null<0>,   /*0x30*/
		vc_Null<0>    , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x38*/
		vc_Null<0>    , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x40*/
		vc_Null<0>    , vc_Null<0>	, vc_MPG<0>		, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x48*/
		vc_Direct<0>  , vc_Direct<0>, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x50*/
		vc_Null<0>	  , vc_Null<0>	, vc_Null<0>	, vc_Null<0>   , vc_Null<0>   , vc_Null<0>   , vc_Null<0>    , vc_Null<0>,   /*0x58*/

		InsertUnpackSet(UseVpuRecompilers, 0, 0),		// unmasked
		InsertUnpackSet(UseVpuRecompilers, 0, 1),		// masked
	},
	{
		vc_Nop<1>     , vc_STCycl<1>  , vc_Offset<1>	, vc_Base<1>   , vc_ITop<1>   , vc_STMod<1>  , vc_MskPath3<1>, vc_Mark<1>,   /*0x00*/
		vc_Null<1>    , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x08*/
		vc_FlushE<1>  , vc_Flush<1>   , vc_Null<1>	, vc_FlushA<1> , vc_MSCAL<1>  , vc_MSCALF<1> , vc_Null<1>	 , vc_MSCNT<1>,  /*0x10*/
		vc_Null<1>    , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x18*/
		vc_STMask<1>  , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>	 , vc_Null<1>,   /*0x20*/
		vc_Null<1>    , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>	 , vc_Null<1>,   /*0x28*/
		vc_STRow<1>   , vc_STCol<1>	  , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>	 , vc_Null<1>,   /*0x30*/
		vc_Null<1>    , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x38*/
		vc_Null<1>    , vc_Null<1>    , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x40*/
		vc_Null<1>    , vc_Null<1>    , vc_MPG<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x48*/
		vc_Direct<1>  , vc_Direct<1>, vc_Null<1>	, vc_Null<1>   , vc_Null<1>	, vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x50*/
		vc_Null<1>	  , vc_Null<1>	  , vc_Null<1>	, vc_Null<1>   , vc_Null<1>   , vc_Null<1>   , vc_Null<1>    , vc_Null<1>,   /*0x58*/

		InsertUnpackSet(UseVpuRecompilers, 1, 0),		// unmasked
		InsertUnpackSet(UseVpuRecompilers, 1, 1),		// masked
	}
};
