/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "IPU.h"
#include "IPU_MultiISA.h"
#include "IPUdma.h"

#include <limits.h>
#include "Config.h"

// the BP doesn't advance and returns -1 if there is no data to be read
alignas(16) tIPU_cmd ipu_cmd;
alignas(16) tIPU_BP g_BP;
alignas(16) decoder_t decoder;

static void (*IPUWorker)();

// Color conversion stuff, the memory layout is a total hack
// convert_data_buffer is a pointer to the internal rgb struct (the first param in convert_init_t)
//char convert_data_buffer[sizeof(convert_rgb_t)];
//char convert_data_buffer[0x1C];							// unused?
//u8 PCT[] = {'r', 'I', 'P', 'B', 'D', '-', '-', '-'};		// unused?

// Quantization matrix
rgb16_t g_ipu_vqclut[16]; //clut conversion table
u16 g_ipu_thresh[2]; //thresholds for color conversions
int coded_block_pattern = 0;

alignas(16) u8 g_ipu_indx4[16*16/2];

alignas(16) const int non_linear_quantizer_scale[32] =
{
	0,  1,  2,  3,  4,  5,	6,	7,
	8, 10, 12, 14, 16, 18,  20,  22,
	24, 28, 32, 36, 40, 44,  48,  52,
	56, 64, 72, 80, 88, 96, 104, 112
};

uint eecount_on_last_vdec = 0;
bool FMVstarted = false;
bool EnableFMV = false;

// Also defined in IPU_MultiISA.cpp, but IPU.cpp is not unshared.
// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
__ri static u8 getBits32(u8* address, bool advance)
{
	if (!g_BP.FillBuffer(32))
		return 0;

	const u8* readpos = &g_BP.internal_qwc->_u8[g_BP.BP / 8];

	if (uint shift = (g_BP.BP & 7))
	{
		u32 mask = (0xff >> shift);
		mask = mask | (mask << 8) | (mask << 16) | (mask << 24);

		*(u32*)address = ((~mask & *(u32*)(readpos + 1)) >> (8 - shift)) | (((mask) & *(u32*)readpos) << shift);
	}
	else
	{
		// Bit position-aligned -- no masking/shifting necessary
		*(u32*)address = *(u32*)readpos;
	}

	if (advance)
		g_BP.Advance(32);

	return 1;
}

void tIPU_cmd::clear()
{
	std::memset(this, 0, sizeof(*this));
	current = 0xffffffff;
}

__fi void IPUProcessInterrupt()
{
	if (ipuRegs.ctrl.BUSY && !CommandExecuteQueued)
		IPUWorker();

	if (ipuRegs.ctrl.BUSY)
	{
		CPU_INT(IPU_PROCESS, ProcessedData ? ProcessedData : 64);
	}
	else
		ProcessedData = 0;
}

/////////////////////////////////////////////////////////
// Register accesses (run on EE thread)

void ipuReset()
{
	IPUWorker = MULTI_ISA_SELECT(IPUWorker);
	std::memset(&ipuRegs, 0, sizeof(ipuRegs));
	std::memset(&g_BP, 0, sizeof(g_BP));
	std::memset(&decoder, 0, sizeof(decoder));

	decoder.picture_structure = FRAME_PICTURE;      //default: progressive...my guess:P

	ipu_fifo.init();
	ipu_cmd.clear();
	ipuDmaReset();
}

void ReportIPU()
{
	//Console.WriteLn(g_nDMATransfer.desc());
	Console.WriteLn(ipu_fifo.in.desc());
	Console.WriteLn(ipu_fifo.out.desc());
	Console.WriteLn(g_BP.desc());
	Console.WriteLn("vqclut = 0x%x.", g_ipu_vqclut);
	Console.WriteLn("thresh = 0x%x.", g_ipu_thresh);
	Console.WriteLn("coded_block_pattern = 0x%x.", coded_block_pattern);
	Console.WriteLn("g_decoder = 0x%x.", &decoder);
	Console.WriteLn(ipu_cmd.desc());
	Console.Newline();
}

bool SaveStateBase::ipuFreeze()
{
	// Get a report of the status of the ipu variables when saving and loading savestates.
	//ReportIPU();
	if (!FreezeTag("IPU"))
		return false;

	Freeze(ipu_fifo);

	Freeze(g_BP);
	Freeze(g_ipu_vqclut);
	Freeze(g_ipu_thresh);
	Freeze(coded_block_pattern);
	Freeze(decoder);
	Freeze(ipu_cmd);

	return IsOkay();
}

void tIPU_CMD_IDEC::log() const
{
	IPU_LOG("IDEC command.");

	if (FB) IPU_LOG(" Skip %d	bits.", FB);
	IPU_LOG(" Quantizer step code=0x%X.", QSC);

	if (DTD == 0)
		IPU_LOG(" Does not decode DT.");
	else
		IPU_LOG(" Decodes DT.");

	if (SGN == 0)
		IPU_LOG(" No bias.");
	else
		IPU_LOG(" Bias=128.");

	if (DTE == 1) IPU_LOG(" Dither Enabled.");
	if (OFM == 0)
		IPU_LOG(" Output format is RGB32.");
	else
		IPU_LOG(" Output format is RGB16.");

	IPU_LOG("");
}

void tIPU_CMD_BDEC::log(int s_bdec) const
{
	IPU_LOG("BDEC(macroblock decode) command %x, num: 0x%x", cpuRegs.pc, s_bdec);
	if (FB) IPU_LOG(" Skip 0x%X bits.", FB);

	if (MBI)
		IPU_LOG(" Intra MB.");
	else
		IPU_LOG(" Non-intra MB.");

	if (DCR)
		IPU_LOG(" Resets DC prediction value.");
	else
		IPU_LOG(" Doesn't reset DC prediction value.");

	if (DT)
		IPU_LOG(" Use field DCT.");
	else
		IPU_LOG(" Use frame DCT.");

	IPU_LOG(" Quantizer step=0x%X", QSC);
}

void tIPU_CMD_CSC::log_from_YCbCr() const
{
	IPU_LOG("CSC(Colorspace conversion from YCbCr) command (%d).", MBC);
	if (OFM)
		IPU_LOG("Output format is RGB16. ");
	else
		IPU_LOG("Output format is RGB32. ");

	if (DTE) IPU_LOG("Dithering enabled.");
}

void tIPU_CMD_CSC::log_from_RGB32() const
{
	IPU_LOG("PACK (Colorspace conversion from RGB32) command.");

	if (OFM)
		IPU_LOG("Output format is RGB16. ");
	else
		IPU_LOG("Output format is INDX4. ");

	if (DTE) IPU_LOG("Dithering enabled.");

	IPU_LOG("Number of macroblocks to be converted: %d", MBC);
}


__fi u32 ipuRead32(u32 mem)
{
	// Note: It's assumed that mem's input value is always in the 0x10002000 page
	// of memory (if not, it's probably bad code).

	pxAssert((mem & ~0xff) == 0x10002000);
	mem &= 0xff;	// ipu repeats every 0x100

	switch (mem)
	{
		ipucase(IPU_CMD) : // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}
			return ipuRegs.cmd.DATA;
		}

		ipucase(IPU_CTRL): // IPU_CTRL
		{
			ipuRegs.ctrl.IFC = g_BP.IFC;
			ipuRegs.ctrl.CBP = coded_block_pattern;

			if (!ipuRegs.ctrl.BUSY)
				IPU_LOG("read32: IPU_CTRL=0x%08X", ipuRegs.ctrl._u32);

			return ipuRegs.ctrl._u32;
		}

		ipucase(IPU_BP): // IPU_BP
		{
			pxAssume(g_BP.FP <= 2);

			ipuRegs.ipubp = g_BP.BP & 0x7f;
			ipuRegs.ipubp |= g_BP.IFC << 8;
			ipuRegs.ipubp |= g_BP.FP << 16;

			IPU_LOG("read32: IPU_BP=0x%08X", ipuRegs.ipubp);
			return ipuRegs.ipubp;
		}

		default:
			IPU_LOG("read32: Addr=0x%08X Value = 0x%08X", mem, psHu32(IPU_CMD + mem));
	}

	return psHu32(IPU_CMD + mem);
}

__fi u64 ipuRead64(u32 mem)
{
	// Note: It's assumed that mem's input value is always in the 0x10002000 page
	// of memory (if not, it's probably bad code).

	pxAssert((mem & ~0xff) == 0x10002000);
	mem &= 0xff;	// ipu repeats every 0x100

	switch (mem)
	{
		ipucase(IPU_CMD): // IPU_CMD
		{
			if (ipu_cmd.CMD != SCE_IPU_FDEC && ipu_cmd.CMD != SCE_IPU_VDEC)
			{
				if (getBits32((u8*)&ipuRegs.cmd.DATA, 0))
					ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
			}

			if (ipuRegs.cmd.DATA & 0xffffff)
				IPU_LOG("read64: IPU_CMD=BUSY=%x, DATA=%08X", ipuRegs.cmd.BUSY ? 1 : 0, ipuRegs.cmd.DATA);
			return ipuRegs.cmd._u64;
		}

		ipucase(IPU_CTRL):
			IPU_LOG("reading 64bit IPU ctrl");
			break;

		ipucase(IPU_BP):
			IPU_LOG("reading 64bit IPU top");
			break;

		ipucase(IPU_TOP): // IPU_TOP
			IPU_LOG("read64: IPU_TOP=%x,  bp = %d", ipuRegs.top, g_BP.BP);
			break;

		default:
			IPU_LOG("read64: Unknown=%x", mem);
			break;
	}
	return psHu64(IPU_CMD + mem);
}

void ipuSoftReset()
{
	ipu_fifo.clear();
	std::memset(&g_BP, 0, sizeof(g_BP));

	coded_block_pattern = 0;
	g_ipu_thresh[0] = 0;
	g_ipu_thresh[1] = 0;

	ipuRegs.ctrl.reset();
	ipuRegs.top = 0;
	ipu_cmd.clear();
	ipuRegs.cmd.BUSY = 0;
	ipuRegs.cmd.DATA = 0; // required for Enthusia - Professional Racing after fix, or will freeze at start of next video.

	hwIntcIrq(INTC_IPU); // required for FightBox
}

__fi bool ipuWrite32(u32 mem, u32 value)
{
	// Note: It's assumed that mem's input value is always in the 0x10002000 page
	// of memory (if not, it's probably bad code).

	pxAssert((mem & ~0xfff) == 0x10002000);
	mem &= 0xfff;

	switch (mem)
	{
		ipucase(IPU_CMD): // IPU_CMD
			IPU_LOG("write32: IPU_CMD=0x%08X", value);
			IPUCMD_WRITE(value);
		return false;

		ipucase(IPU_CTRL): // IPU_CTRL
			// CTRL = the first 16 bits of ctrl [0x8000ffff], + value for the next 16 bits,
			// minus the reserved bits. (18-19; 27-29) [0x47f30000]
			ipuRegs.ctrl.write(value);
			if (ipuRegs.ctrl.IDP == 3)
			{
				Console.WriteLn("IPU Invalid Intra DC Precision, switching to 9 bits");
				ipuRegs.ctrl.IDP = 1;
			}

			if (ipuRegs.ctrl.RST) ipuSoftReset(); // RESET

			IPU_LOG("write32: IPU_CTRL=0x%08X", value);
		return false;
	}
	return true;
}

// returns FALSE when the writeback is handled, TRUE if the caller should do the
// writeback itself.
__fi bool ipuWrite64(u32 mem, u64 value)
{
	// Note: It's assumed that mem's input value is always in the 0x10002000 page
	// of memory (if not, it's probably bad code).

	pxAssert((mem & ~0xfff) == 0x10002000);
	mem &= 0xfff;

	switch (mem)
	{
		ipucase(IPU_CMD):
			IPU_LOG("write64: IPU_CMD=0x%08X", value);
			IPUCMD_WRITE((u32)value);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////
// IPU Commands (exec on worker thread only)

static void ipuBCLR(u32 val)
{
	ipu_fifo.in.clear();
	std::memset(&g_BP, 0, sizeof(g_BP));
	g_BP.BP = val & 0x7F;

	ipuRegs.cmd.BUSY = 0;
	IPU_LOG("Clear IPU input FIFO. Set Bit offset=0x%X", g_BP.BP);
}

static __ri void ipuIDEC(tIPU_CMD_IDEC idec)
{
	idec.log();

	//from IPU_CTRL
	ipuRegs.ctrl.PCT = I_TYPE; //Intra DECoding;)

	decoder.coding_type			= ipuRegs.ctrl.PCT;
	decoder.mpeg1				= ipuRegs.ctrl.MP1;
	decoder.q_scale_type		= ipuRegs.ctrl.QST;
	decoder.intra_vlc_format	= ipuRegs.ctrl.IVF;
	decoder.scantype			= ipuRegs.ctrl.AS;
	decoder.intra_dc_precision	= ipuRegs.ctrl.IDP;

//from IDEC value
	decoder.quantizer_scale		= idec.QSC;
	decoder.frame_pred_frame_dct= !idec.DTD;
	decoder.sgn = idec.SGN;
	decoder.dte = idec.DTE;
	decoder.ofm = idec.OFM;

	//other stuff
	decoder.dcr = 1; // resets DC prediction value
}

static int s_bdec = 0;

static __ri void ipuBDEC(tIPU_CMD_BDEC bdec)
{
	bdec.log(s_bdec);
	if (IsDebugBuild) s_bdec++;

	decoder.coding_type			= I_TYPE;
	decoder.mpeg1				= ipuRegs.ctrl.MP1;
	decoder.q_scale_type		= ipuRegs.ctrl.QST;
	decoder.intra_vlc_format	= ipuRegs.ctrl.IVF;
	decoder.scantype			= ipuRegs.ctrl.AS;
	decoder.intra_dc_precision	= ipuRegs.ctrl.IDP;

	//from BDEC value
	decoder.quantizer_scale		= decoder.q_scale_type ? non_linear_quantizer_scale [bdec.QSC] : bdec.QSC << 1;
	decoder.macroblock_modes	= bdec.DT ? DCT_TYPE_INTERLACED : 0;
	decoder.dcr					= bdec.DCR;
	decoder.macroblock_modes	|= bdec.MBI ? MACROBLOCK_INTRA : MACROBLOCK_PATTERN;

	std::memset(&decoder.mb8, 0, sizeof(decoder.mb8));
	std::memset(&decoder.mb16, 0, sizeof(decoder.mb16));
}

static void ipuSETTH(u32 val)
{
	g_ipu_thresh[0] = (val & 0x1ff);
	g_ipu_thresh[1] = ((val >> 16) & 0x1ff);
	IPU_LOG("SETTH (Set threshold value)command %x.", val&0x1ff01ff);
}

// --------------------------------------------------------------------------------------
//  IPU Worker / Dispatcher
// --------------------------------------------------------------------------------------

// When a command is written, we set some various busy flags and clear some other junk.
// The actual decoding will be handled by IPUworker.
__fi void IPUCMD_WRITE(u32 val)
{
	// don't process anything if currently busy
	//if (ipuRegs.ctrl.BUSY) Console.WriteLn("IPU BUSY!"); // wait for thread
	ProcessedData = 0;
	ipuRegs.ctrl.ECD = 0;
	ipuRegs.ctrl.SCD = 0;
	ipu_cmd.clear();
	ipu_cmd.current = val;

	switch (ipu_cmd.CMD)
	{
		// BCLR and SETTH  require no data so they always execute inline:

		case SCE_IPU_BCLR:
			ipuBCLR(val);
			hwIntcIrq(INTC_IPU); //DMAC_TO_IPU
			ipuRegs.ctrl.BUSY = 0;
			return;

		case SCE_IPU_SETTH:
			ipuSETTH(val);
			hwIntcIrq(INTC_IPU);
			ipuRegs.ctrl.BUSY = 0;
			return;

		case SCE_IPU_IDEC:
			g_BP.Advance(val & 0x3F);
			ipuIDEC(val);
			ipuRegs.SetTopBusy();
			break;

		case SCE_IPU_BDEC:
			g_BP.Advance(val & 0x3F);
			ipuBDEC(val);
			ipuRegs.SetTopBusy();
			break;

		case SCE_IPU_VDEC:
			g_BP.Advance(val & 0x3F);
			ipuRegs.SetDataBusy();
			break;

		case SCE_IPU_FDEC:
			IPU_LOG("FDEC command. Skip 0x%X bits, FIFO 0x%X qwords, BP 0x%X, CHCR 0x%x",
			        val & 0x3f, g_BP.IFC, g_BP.BP, ipu1ch.chcr._u32);
			g_BP.Advance(val & 0x3F);
			ipuRegs.SetDataBusy();
			break;

		case SCE_IPU_SETIQ:
			IPU_LOG("SETIQ command.");
			g_BP.Advance(val & 0x3F);
			break;

		case SCE_IPU_SETVQ:
			break;

		case SCE_IPU_CSC:
			break;

		case SCE_IPU_PACK:
			break;

		jNO_DEFAULT;
	}

	ipuRegs.ctrl.BUSY = 1;

	// Have a short delay immitating the time it takes to run IDEC/BDEC, other commands are near instant.
	// Mana Khemia/Metal Saga start IDEC then change IPU0 expecting there to be a delay before IDEC sends data.
	if (!CommandExecuteQueued && (ipu_cmd.CMD == SCE_IPU_IDEC || ipu_cmd.CMD == SCE_IPU_BDEC))
	{
		CommandExecuteQueued = true;
		CPU_INT(IPU_PROCESS, 64);
	}
	else
		IPUWorker();
}
