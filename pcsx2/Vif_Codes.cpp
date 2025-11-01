// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "GS.h"
#include "Gif_Unit.h"
#include "MTVU.h"
#include "VUmicro.h"
#include "Vif_Dma.h"
#include "Vif_Dynarec.h"

#define vifOp(vifCodeName) _vifT int vifCodeName(int pass, const u32* data)
#define pass1 if (pass == 0)
#define pass2 if (pass == 1)
#define pass3 if (pass == 2)
#define pass1or2 if (pass == 0 || pass == 1)
#define vif1Only() \
	{ \
		if (!idx) \
			return vifCode_Null<idx>(pass, (u32*)data); \
	}
vifOp(vifCode_Null);

//------------------------------------------------------------------
// Vif0/Vif1 Misc Functions
//------------------------------------------------------------------

__ri void vifExecQueue(int idx)
{
	if (!GetVifX.queued_program || (VU0.VI[REG_VPU_STAT].UL & 1 << (idx * 8)))
		return;

	if (GetVifX.queued_gif_wait)
	{
		if (gifUnit.checkPaths(1, 1, 0))
			return;
	}

	GetVifX.queued_program = false;

	if (!idx)
		vu0ExecMicro(vif0.queued_pc);
	else
		vu1ExecMicro(vif1.queued_pc);

	// Hack for Wakeboarding Unleashed, game runs a VU program in parallel with a VIF unpack list.
	// The start of the VU program clears the VU memory, while VIF populates it from behind, so we need to get the clear out of the way.
	/*if (idx && !INSTANT_VU1)
	{
		VU1.cycle -= 256;
		CpuVU1->ExecuteBlock(0);
	}*/
}

static __fi EE_EventType vif1InternalIrq()
{
	return dmacRegs.ctrl.is_mfifo(true) ? DMAC_MFIFO_VIF : DMAC_VIF1;
}

static __fi void vifFlush(int idx)
{
	vifExecQueue(idx);

	if (!idx)
		vif0FLUSH();
	else
		vif1FLUSH();

	vifExecQueue(idx);
}

static __fi void vuExecMicro(int idx, u32 addr, bool requires_wait)
{
	VIFregisters& vifRegs = vifXRegs;

	vifFlush(idx);
	if (GetVifX.waitforvu)
	{
		CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
		return;
	}

	if (vifRegs.itops > (idx ? 0x3ffu : 0xffu))
	{
		Console.WriteLn("VIF%d ITOP overrun! %x", idx, vifRegs.itops);
		vifRegs.itops &= (idx ? 0x3ffu : 0xffu);
	}

	vifRegs.itop = vifRegs.itops;

	if (idx)
	{
		// in case we're handling a VIF1 execMicro, set the top with the tops value
		vifRegs.top = vifRegs.tops & 0x3ff;

		// is DBF flag set in VIF_STAT?
		if (vifRegs.stat.DBF)
		{
			// it is, so set tops with base, and clear the stat DBF flag
			vifRegs.tops = vifRegs.base;
			vifRegs.stat.DBF = false;
		}
		else
		{
			// it is not, so set tops with base + offset, and set stat DBF flag
			vifRegs.tops = vifRegs.base + vifRegs.ofst;
			vifRegs.stat.DBF = true;
		}
	}

	GetVifX.queued_program = true;
	if (static_cast<s32>(addr) == -1)
		GetVifX.queued_pc = addr;
	else
		GetVifX.queued_pc = addr & (idx ? 0x7ffu : 0x1ffu);
	GetVifX.unpackcalls = 0;

	GetVifX.queued_gif_wait = requires_wait;

	if (!idx || (!THREAD_VU1 && !INSTANT_VU1))
		vifExecQueue(idx);
}

//------------------------------------------------------------------
// Vif0/Vif1 Code Implementations
//------------------------------------------------------------------

vifOp(vifCode_Base)
{
	vif1Only();
	pass1
	{
		vif1Regs.base = vif1Regs.code & 0x3ff;
		vif1.cmd = 0;
		vif1.pass = 0;
	}
	pass3 { VifCodeLog("Base"); }
	return 1;
}

template <int idx>
__fi int _vifCode_Direct(int pass, const u8* data, bool isDirectHL)
{
	vif1Only();
	pass1
	{
		const int vifImm = static_cast<u16>(vif1Regs.code);
		vif1.tag.size = vifImm ? (vifImm * 4) : (65536 * 4);
		vif1.pass = 1;
		return 1;
	}
	pass2
	{
		const char* name = isDirectHL ? "DirectHL" : "Direct";
		const GIF_TRANSFER_TYPE tranType = isDirectHL ? GIF_TRANS_DIRECTHL : GIF_TRANS_DIRECT;
		const uint size = std::min(vif1.vifpacketsize, vif1.tag.size) * 4; // Get size in bytes
		const uint ret = gifUnit.TransferGSPacketData(tranType, (u8*)data, size);

		vif1.tag.size -= ret / 4; // Convert to u32's
		vif1Regs.stat.VGW = false;

		g_vif1Cycles += (ret / 16) * 2; // Need to add on the same amount of cycles again, as the GS has a 64bit bus bottleneck.

		if (ret & 3)
			DevCon.Warning("Vif %s: Ret wasn't a multiple of 4!", name); // Shouldn't happen
		if (size == 0)
			DevCon.Warning("Vif %s: No Data Transfer?", name); // Can this happen?
		if (size != ret)
		{ // Stall if gif didn't process all the data (path2 queued)
			GUNIT_WARN("Vif %s: Stall! [size=%d][ret=%d]", name, size, ret);
			//gifUnit.PrintInfo();
			vif1.vifstalled.enabled = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
			vif1Regs.stat.VGW = true;
			return 0;
		}
		if (vif1.tag.size == 0)
		{
			vif1.cmd = 0;
			vif1.pass = 0;
			vif1.vifstalled.enabled = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
		}
		return ret / 4;
	}
	return 0;
}

vifOp(vifCode_Direct)
{
	pass3 { VifCodeLog("Direct"); }
	return _vifCode_Direct<idx>(pass, (u8*)data, 0);
}

vifOp(vifCode_DirectHL)
{
	pass3 { VifCodeLog("DirectHL"); }
	return _vifCode_Direct<idx>(pass, (u8*)data, 1);
}

vifOp(vifCode_Flush)
{
	vif1Only();
	//vifStruct& vifX = GetVifX;
	pass1or2
	{
		const bool p1or2 = (gifRegs.stat.APATH != 0 && gifRegs.stat.APATH != 3);
		vif1Regs.stat.VGW = false;
		vifFlush(idx);
		if (gifUnit.checkPaths(1, 1, 0) || p1or2)
		{
			GUNIT_WARN("Vif Flush: Stall!");
			//gifUnit.PrintInfo();
			vif1Regs.stat.VGW = true;
			vif1.vifstalled.enabled = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
		}

		if (vif1.waitforvu || vif1Regs.stat.VGW)
		{
			CPU_SET_DMASTALL(vif1InternalIrq(), true);
			return 0;
		}

		vif1.cmd = 0;
		vif1.pass = 0;
	}
	pass3 { VifCodeLog("Flush"); }
	return 1;
}

vifOp(vifCode_FlushA)
{
	vif1Only();
	//vifStruct& vifX = GetVifX;
	pass1or2
	{
		//Gif_Path& p3      = gifUnit.gifPath[GIF_PATH_3];
		const u32 gifBusy = gifUnit.checkPaths(1, 1, 1) || (gifRegs.stat.APATH != 0);
		//bool      doStall = false;
		vif1Regs.stat.VGW = false;
		vifFlush(idx);

		if (gifBusy)
		{
			GUNIT_WARN("Vif FlushA: Stall!");
			vif1Regs.stat.VGW = true;
			vif1.vifstalled.enabled = VifStallEnable(vif1ch);
			vif1.vifstalled.value = VIF_TIMING_BREAK;
		}

		if (vif1.waitforvu || vif1Regs.stat.VGW)
		{
			CPU_SET_DMASTALL(vif1InternalIrq(), true);
			return 0;
		}

		vif1.cmd = 0;
		vif1.pass = 0;
	}
	pass3 { VifCodeLog("FlushA"); }
	return 1;
}

// ToDo: FixMe
vifOp(vifCode_FlushE)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifFlush(idx);

		if (vifX.waitforvu)
		{
			CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
			return 0;
		}

		vifX.cmd = 0;
		vifX.pass = 0;
	}
	pass3 { VifCodeLog("FlushE"); }
	return 1;
}

vifOp(vifCode_ITop)
{
	pass1
	{
		vifXRegs.itops = vifXRegs.code & 0x3ff;
		GetVifX.cmd = 0;
		GetVifX.pass = 0;
	}
	pass3 { VifCodeLog("ITop"); }
	return 1;
}

vifOp(vifCode_Mark)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifXRegs.mark = static_cast<u16>(vifXRegs.code);
		vifXRegs.stat.MRK = true;
		vifX.cmd = 0;
		vifX.pass = 0;
	}
	pass3 { VifCodeLog("Mark"); }
	return 1;
}

static __fi void _vifCode_MPG(int idx, u32 addr, const u32* data, int size)
{
	VURegs& VUx = idx ? VU1 : VU0;
	vifStruct& vifX = GetVifX;
	const u16 vuMemSize = idx ? 0x4000 : 0x1000;
	pxAssert(VUx.Micro);

	vifExecQueue(idx);

	if (idx && THREAD_VU1)
	{
		if ((addr + size * 4) > vuMemSize)
		{
			vu1Thread.WriteMicroMem(addr, (u8*)data, vuMemSize - addr);
			size -= (vuMemSize - addr) / 4;
			data += (vuMemSize - addr) / 4;
			vu1Thread.WriteMicroMem(0, (u8*)data, size * 4);
			vifX.tag.addr = size * 4;
		}
		else
		{
			vu1Thread.WriteMicroMem(addr, (u8*)data, size * 4);
			vifX.tag.addr += size * 4;
		}
		return;
	}

	// Don't forget the Unsigned designator for these checks
	if ((addr + size * 4) > vuMemSize)
	{
		//DevCon.Warning("Handling split MPG");
		if (!idx)
			CpuVU0->Clear(addr, vuMemSize - addr);
		else
			CpuVU1->Clear(addr, vuMemSize - addr);

		memcpy(VUx.Micro + addr, data, vuMemSize - addr);
		size -= (vuMemSize - addr) / 4;
		data += (vuMemSize - addr) / 4;
		memcpy(VUx.Micro, data, size * 4);

		vifX.tag.addr = size * 4;
	}
	else
	{
		//The compare is pretty much a waste of time, likelyhood is that the program isnt there, thats why its copying it.
		//Faster without.
		//if (memcmp(VUx.Micro + addr, data, size*4)) {
		// Clear VU memory before writing!
		if (!idx)
			CpuVU0->Clear(addr, size * 4);
		else
			CpuVU1->Clear(addr, size * 4);
		memcpy(VUx.Micro + addr, data, size * 4); //from tests, memcpy is 1fps faster on Grandia 3 than memcpy

		vifX.tag.addr += size * 4;
	}
}

vifOp(vifCode_MPG)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		const int vifNum = static_cast<u8>(vifXRegs.code >> 16);
		vifX.tag.addr = static_cast<u16>(vifXRegs.code << 3) & (idx ? 0x3fff : 0xfff);
		vifX.tag.size = vifNum ? (vifNum * 2) : 512;
		vifFlush(idx);

		if (vifX.waitforvu)
		{
			CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
			return 0;
		}
		else
		{
			vifX.pass = 1;
			return 1;
		}
	}
	pass2
	{
		if (vifX.vifpacketsize < vifX.tag.size)
		{ // Partial Transfer
			if ((vifX.tag.addr + vifX.vifpacketsize * 4) > (idx ? 0x4000 : 0x1000))
			{
				//DevCon.Warning("Vif%d MPG Split Overflow", idx);
			}
			_vifCode_MPG(idx, vifX.tag.addr, data, vifX.vifpacketsize);
			vifX.tag.size -= vifX.vifpacketsize; //We can do this first as its passed as a pointer
			return vifX.vifpacketsize;
		}
		else
		{ // Full Transfer
			if ((vifX.tag.addr + vifX.tag.size * 4) > (idx ? 0x4000 : 0x1000))
			{
				//DevCon.Warning("Vif%d MPG Split Overflow full %x", idx, vifX.tag.addr + vifX.tag.size*4);
			}
			_vifCode_MPG(idx, vifX.tag.addr, data, vifX.tag.size);
			const int ret = vifX.tag.size;
			vifX.tag.size = 0;
			vifX.cmd = 0;
			vifX.pass = 0;
			return ret;
		}
	}
	pass3 { VifCodeLog("MPG"); }
	return 0;
}

vifOp(vifCode_MSCAL)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifFlush(idx);

		if (vifX.waitforvu)
		{
			CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
			return 0;
		}

		vuExecMicro(idx, static_cast<u16>(vifXRegs.code), false);
		vifX.cmd = 0;
		vifX.pass = 0;

		if (GetVifX.vifpacketsize > 1)
		{
			//Warship Gunner 2 has a rather big dislike for the delays
			if (((data[1] >> 24) & 0x60) == 0x60) // Immediate following Unpack
			{
				//Snowblind games only use MSCAL, so other MS kicks force the program directly.
				vifExecQueue(idx);
			}
		}
	}
	pass3 { VifCodeLog("MSCAL"); }
	return 1;
}

vifOp(vifCode_MSCALF)
{
	vifStruct& vifX = GetVifX;
	pass1or2
	{
		vifXRegs.stat.VGW = false;
		vifFlush(idx);
		if ([[maybe_unused]] const u32 a = gifUnit.checkPaths(1, 1, 0))
		{
			GUNIT_WARN("Vif MSCALF: Stall! [%d,%d]", !!(a & 1), !!(a & 2));
			vif1Regs.stat.VGW = true;
			vifX.vifstalled.enabled = VifStallEnable(vifXch);
			vifX.vifstalled.value = VIF_TIMING_BREAK;
		}

		if (vifX.waitforvu || vif1Regs.stat.VGW)
		{
			CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
			return 0;
		}

		vuExecMicro(idx, static_cast<u16>(vifXRegs.code), true);
		vifX.cmd = 0;
		vifX.pass = 0;
		vifExecQueue(idx);
	}
	pass3 { VifCodeLog("MSCALF"); }
	return 1;
}

vifOp(vifCode_MSCNT)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifFlush(idx);

		if (vifX.waitforvu)
		{
			CPU_SET_DMASTALL(idx ? vif1InternalIrq() : DMAC_VIF0, true);
			return 0;
		}

		vuExecMicro(idx, -1, false);
		vifX.cmd = 0;
		vifX.pass = 0;
		if (GetVifX.vifpacketsize > 1)
		{
			if (((data[1] >> 24) & 0x60) == 0x60) // Immediate following Unpack
			{
				vifExecQueue(idx);
			}
		}
	}
	pass3 { VifCodeLog("MSCNT"); }
	return 1;
}

// ToDo: FixMe
vifOp(vifCode_MskPath3)
{
	vif1Only();
	pass1
	{
		vif1Regs.mskpath3 = (vif1Regs.code >> 15) & 0x1;
		gifRegs.stat.M3P = (vif1Regs.code >> 15) & 0x1;
		GUNIT_LOG("Vif1 - MskPath3 [p3 = %s]", vif1Regs.mskpath3 ? "masked" : "enabled");
		if (!vif1Regs.mskpath3)
		{
			GUNIT_WARN("VIF MSKPATH3 off Path3 triggering!");
			gifInterrupt();
		}

		vif1.cmd = 0;
		vif1.pass = 0;
	}
	pass3 { VifCodeLog("MskPath3"); }
	return 1;
}

vifOp(vifCode_Nop)
{
	pass1
	{
		GetVifX.cmd = 0;
		GetVifX.pass = 0;
		vifExecQueue(idx);

		if (GetVifX.vifpacketsize > 1)
		{
			if (((data[1] >> 24) & 0x7f) == 0x6 && (data[1] & 0x1)) //is mskpath3 next
			{
				GetVifX.vifstalled.enabled = VifStallEnable(vifXch);
				GetVifX.vifstalled.value = VIF_TIMING_BREAK;
			}
		}
	}
	pass3 { VifCodeLog("Nop"); }
	return 1;
}

// ToDo: Review Flags
vifOp(vifCode_Null)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		// if ME1, then force the vif to interrupt
		if (!(vifXRegs.err.ME1))
		{ // Ignore vifcode and tag mismatch error
			Console.WriteLn("Vif%d: Unknown VifCmd! [%x]", idx, vifX.cmd);
			vifXRegs.stat.ER1 = true;
			vifX.vifstalled.enabled = VifStallEnable(vifXch);
			vifX.vifstalled.value = VIF_IRQ_STALL;
			//vifX.irq++;
		}
		vifX.cmd = 0;
		vifX.pass = 0;

		//If the top bit was set to interrupt, we don't want it to take commands from a bad code
		if (vifXRegs.code & 0x80000000)
			vifX.irq = 0;
	}
	pass2 { Console.Error("Vif%d bad vifcode! [CMD = %x]", idx, vifX.cmd); }
	pass3 { VifCodeLog("Null"); }
	return 1;
}

vifOp(vifCode_Offset)
{
	vif1Only();
	pass1
	{
		vif1Regs.stat.DBF = false;
		vif1Regs.ofst = vif1Regs.code & 0x3ff;
		vif1Regs.tops = vif1Regs.base;
		vif1.cmd = 0;
		vif1.pass = 0;
	}
	pass3 { VifCodeLog("Offset"); }
	return 1;
}

template <int idx>
static __fi int _vifCode_STColRow(const u32* data, u32* pmem2)
{
	vifStruct& vifX = GetVifX;

	const int ret = std::min(4 - vifX.tag.addr, vifX.vifpacketsize);
	pxAssume(vifX.tag.addr < 4);
	pxAssume(ret > 0);

	switch (ret)
	{
		case 4:
			pmem2[3] = data[3];
			[[fallthrough]];
		case 3:
			pmem2[2] = data[2];
			[[fallthrough]];
		case 2:
			pmem2[1] = data[1];
			[[fallthrough]];
		case 1:
			pmem2[0] = data[0];
			break;
			jNO_DEFAULT
	}

	vifX.tag.addr += ret;
	vifX.tag.size -= ret;
	if (!vifX.tag.size)
	{
		vifX.pass = 0;
		vifX.cmd = 0;
	}



	return ret;
}

vifOp(vifCode_STCol)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifX.tag.addr = 0;
		vifX.tag.size = 4;
		vifX.pass = 1;
		return 1;
	}
	pass2
	{
		const u32 ret = _vifCode_STColRow<idx>(data, &vifX.MaskCol._u32[vifX.tag.addr]);
		if (idx && vifX.tag.size == 0)
			vu1Thread.WriteCol(vifX);
		return ret;
	}
	pass3 { VifCodeLog("STCol"); }
	return 0;
}

vifOp(vifCode_STRow)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifX.tag.addr = 0;
		vifX.tag.size = 4;
		vifX.pass = 1;
		return 1;
	}
	pass2
	{
		const u32 ret = _vifCode_STColRow<idx>(data, &vifX.MaskRow._u32[vifX.tag.addr]);
		if (idx && vifX.tag.size == 0)
			vu1Thread.WriteRow(vifX);
		return ret;
	}
	pass3 { VifCodeLog("STRow"); }
	return 1;
}

vifOp(vifCode_STCycl)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifXRegs.cycle.cl = static_cast<u8>(vifXRegs.code);
		vifXRegs.cycle.wl = static_cast<u8>(vifXRegs.code >> 8);
		vifX.cmd = 0;
		vifX.pass = 0;
	}
	pass3 { VifCodeLog("STCycl"); }
	return 1;
}

vifOp(vifCode_STMask)
{
	vifStruct& vifX = GetVifX;
	pass1
	{
		vifX.tag.size = 1;
		vifX.pass = 1;
		return 1;
	}
	pass2
	{
		vifXRegs.mask = data[0];
		vifX.tag.size = 0;
		vifX.cmd = 0;
		vifX.pass = 0;
	}
	pass3 { VifCodeLog("STMask"); }
	return 1;
}

vifOp(vifCode_STMod)
{
	pass1
	{
		vifXRegs.mode = vifXRegs.code & 0x3;
		GetVifX.cmd = 0;
		GetVifX.pass = 0;
	}
	pass3 { VifCodeLog("STMod"); }
	return 1;
}

template <uint idx>
static uint calc_addr(bool flg)
{
	VIFregisters& vifRegs = vifXRegs;

	uint retval = vifRegs.code;
	if (idx && flg)
		retval += vifRegs.tops;
	return retval & (idx ? 0x3ff : 0xff);
}

vifOp(vifCode_Unpack)
{
	pass1
	{
		vifUnpackSetup<idx>(data);

		return 1;
	}
	pass2
	{
		return nVifUnpack<idx>((u8*)data);
	}
	pass3
	{
		vifStruct& vifX = GetVifX;
		VIFregisters& vifRegs = vifXRegs;
		const uint vl = vifX.cmd & 0x03;
		const uint vn = (vifX.cmd >> 2) & 0x3;
		const bool flg = (vifRegs.code >> 15) & 1;
		static const char* const vntbl[] = {"S", "V2", "V3", "V4"};
		static const uint vltbl[] = {32, 16, 8, 5};

		VifCodeLog("Unpack %s_%u (%s) @ 0x%04X%s (cl=%u  wl=%u  num=0x%02X)",
			vntbl[vn], vltbl[vl], (vifX.cmd & 0x10) ? "masked" : "unmasked",
			calc_addr<idx>(flg), flg ? "(FLG)" : "",
			vifRegs.cycle.cl, vifRegs.cycle.wl, (vifXRegs.code >> 16) & 0xff);
	}
	return 0;
}

//------------------------------------------------------------------
// Vif0/Vif1 Code Tables
//------------------------------------------------------------------

alignas(16) FnType_VifCmdHandler* const vifCmdHandler[2][128] =
{
	{
		vifCode_Nop<0>     , vifCode_STCycl<0>  , vifCode_Offset<0>	, vifCode_Base<0>   , vifCode_ITop<0>   , vifCode_STMod<0>  , vifCode_MskPath3<0>, vifCode_Mark<0>,   /*0x00*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x08*/
		vifCode_FlushE<0>  , vifCode_Flush<0>   , vifCode_Null<0>	, vifCode_FlushA<0> , vifCode_MSCAL<0>  , vifCode_MSCALF<0> , vifCode_Null<0>	 , vifCode_MSCNT<0>,  /*0x10*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x18*/
		vifCode_STMask<0>  , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x20*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x28*/
		vifCode_STRow<0>   , vifCode_STCol<0>	, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>	 , vifCode_Null<0>,   /*0x30*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x38*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x40*/
		vifCode_Null<0>    , vifCode_Null<0>    , vifCode_MPG<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x48*/
		vifCode_Direct<0>  , vifCode_DirectHL<0>, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x50*/
		vifCode_Null<0>	   , vifCode_Null<0>	, vifCode_Null<0>	, vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>   , vifCode_Null<0>    , vifCode_Null<0>,   /*0x58*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Null<0>,   /*0x60*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Unpack<0>, /*0x68*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Null<0>,   /*0x70*/
		vifCode_Unpack<0>  , vifCode_Unpack<0>  , vifCode_Unpack<0>	, vifCode_Null<0>   , vifCode_Unpack<0> , vifCode_Unpack<0> , vifCode_Unpack<0>  , vifCode_Unpack<0>  /*0x78*/
	},
	{
		vifCode_Nop<1>     , vifCode_STCycl<1>  , vifCode_Offset<1>	, vifCode_Base<1>   , vifCode_ITop<1>   , vifCode_STMod<1>  , vifCode_MskPath3<1>, vifCode_Mark<1>,   /*0x00*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x08*/
		vifCode_FlushE<1>  , vifCode_Flush<1>   , vifCode_Null<1>	, vifCode_FlushA<1> , vifCode_MSCAL<1>  , vifCode_MSCALF<1> , vifCode_Null<1>	 , vifCode_MSCNT<1>,  /*0x10*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x18*/
		vifCode_STMask<1>  , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x20*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x28*/
		vifCode_STRow<1>   , vifCode_STCol<1>	, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>	 , vifCode_Null<1>,   /*0x30*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x38*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x40*/
		vifCode_Null<1>    , vifCode_Null<1>    , vifCode_MPG<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x48*/
		vifCode_Direct<1>  , vifCode_DirectHL<1>, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x50*/
		vifCode_Null<1>	   , vifCode_Null<1>	, vifCode_Null<1>	, vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>   , vifCode_Null<1>    , vifCode_Null<1>,   /*0x58*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Null<1>,   /*0x60*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Unpack<1>, /*0x68*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Null<1>,   /*0x70*/
		vifCode_Unpack<1>  , vifCode_Unpack<1>  , vifCode_Unpack<1>	, vifCode_Null<1>   , vifCode_Unpack<1> , vifCode_Unpack<1> , vifCode_Unpack<1>  , vifCode_Unpack<1>  /*0x78*/
	}
};
