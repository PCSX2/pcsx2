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

#include "IPU_MultiISA.h"

#include "IPU.h"
#include "IPUdma.h"
#include "yuv2rgb.h"

MULTI_ISA_UNSHARED_START

//////////////////////////////////////////////////////
// IPU Commands (exec on worker thread only)

static __fi bool ipuVDEC(u32 val)
{
	static int count = 0;
	if (count++ > 5) {
		if (!FMVstarted) {
			EnableFMV = true;
			FMVstarted = true;
		}
		count = 0;
	}
	eecount_on_last_vdec = cpuRegs.cycle;

	switch (ipu_cmd.pos[0])
	{
		case 0:
			if (!bitstream_init()) return false;

			switch ((val >> 26) & 3)
			{
				case 0://Macroblock Address Increment
					decoder.mpeg1 = ipuRegs.ctrl.MP1;
					ipuRegs.cmd.DATA = get_macroblock_address_increment();
					break;

				case 1://Macroblock Type
					decoder.frame_pred_frame_dct = 1;
					decoder.coding_type = ipuRegs.ctrl.PCT > 0 ? ipuRegs.ctrl.PCT : 1; // Kaiketsu Zorro Mezase doesn't set a Picture type, seems happy with I
					ipuRegs.cmd.DATA = get_macroblock_modes();
					break;

				case 2://Motion Code
					ipuRegs.cmd.DATA = get_motion_delta(0);
					break;

				case 3://DMVector
					ipuRegs.cmd.DATA = get_dmv();
					break;

				jNO_DEFAULT
			}

			// HACK ATTACK!  This code OR's the MPEG decoder's bitstream position into the upper
			// 16 bits of DATA; which really doesn't make sense since (a) we already rewound the bits
			// back into the IPU internal buffer above, and (b) the IPU doesn't have an MPEG internal
			// 32-bit decoder buffer of its own anyway.  Furthermore, setting the upper 16 bits to
			// any value other than zero appears to work fine.  When set to zero, however, FMVs run
			// very choppy (basically only decoding/updating every 30th frame or so). So yeah,
			// someone with knowledge on the subject please feel free to explain this one. :) --air

			// The upper bits are the "length" of the decoded command, where the lower is the address.
			// This is due to differences with IPU and the MPEG standard. See get_macroblock_address_increment().

			ipuRegs.ctrl.ECD = (ipuRegs.cmd.DATA == 0);
			[[fallthrough]];

		case 1:
			if (!getBits32((u8*)&ipuRegs.top, 0))
			{
				ipu_cmd.pos[0] = 1;
				return false;
			}

			ipuRegs.top = BigEndian(ipuRegs.top);

			IPU_LOG("VDEC command data 0x%x(0x%x). Skip 0x%X bits/Table=%d (%s), pct %d",
			        ipuRegs.cmd.DATA, ipuRegs.cmd.DATA >> 16, val & 0x3f, (val >> 26) & 3, (val >> 26) & 1 ?
			        ((val >> 26) & 2 ? "DMV" : "MBT") : (((val >> 26) & 2 ? "MC" : "MBAI")), ipuRegs.ctrl.PCT);

			return true;

		jNO_DEFAULT
	}

	return false;
}

static __ri bool ipuFDEC(u32 val)
{
	if (!getBits32((u8*)&ipuRegs.cmd.DATA, 0)) return false;

	ipuRegs.cmd.DATA = BigEndian(ipuRegs.cmd.DATA);
	ipuRegs.top = ipuRegs.cmd.DATA;

	IPU_LOG("FDEC read: 0x%08x", ipuRegs.top);

	return true;
}

static bool ipuSETIQ(u32 val)
{
	if ((val >> 27) & 1)
	{
		u8 (&niq)[64] = decoder.niq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)niq + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		IPU_LOG("Read non-intra quantization matrix from FIFO.");
		for (uint i = 0; i < 8; i++)
		{
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X",
			        niq[i * 8 + 0], niq[i * 8 + 1], niq[i * 8 + 2], niq[i * 8 + 3],
			        niq[i * 8 + 4], niq[i * 8 + 5], niq[i * 8 + 6], niq[i * 8 + 7]);
		}
	}
	else
	{
		u8 (&iq)[64] = decoder.iq;

		for(;ipu_cmd.pos[0] < 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)iq + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		IPU_LOG("Read intra quantization matrix from FIFO.");
		for (uint i = 0; i < 8; i++)
		{
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X",
			        iq[i * 8 + 0], iq[i * 8 + 1], iq[i * 8 + 2], iq[i *8 + 3],
			        iq[i * 8 + 4], iq[i * 8 + 5], iq[i * 8 + 6], iq[i *8 + 7]);
		}
	}

	return true;
}

static bool ipuSETVQ(u32 val)
{
	for(;ipu_cmd.pos[0] < 4; ipu_cmd.pos[0]++)
	{
		if (!getBits64(((u8*)g_ipu_vqclut) + 8 * ipu_cmd.pos[0], 1)) return false;
	}

	IPU_LOG("SETVQ command.   Read VQCLUT table from FIFO.\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
	        "%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d",
	        g_ipu_vqclut[ 0].r, g_ipu_vqclut[ 0].g, g_ipu_vqclut[ 0].b,
	        g_ipu_vqclut[ 1].r, g_ipu_vqclut[ 1].g, g_ipu_vqclut[ 1].b,
	        g_ipu_vqclut[ 2].r, g_ipu_vqclut[ 2].g, g_ipu_vqclut[ 2].b,
	        g_ipu_vqclut[ 3].r, g_ipu_vqclut[ 3].g, g_ipu_vqclut[ 3].b,
	        g_ipu_vqclut[ 4].r, g_ipu_vqclut[ 4].g, g_ipu_vqclut[ 4].b,
	        g_ipu_vqclut[ 5].r, g_ipu_vqclut[ 5].g, g_ipu_vqclut[ 5].b,
	        g_ipu_vqclut[ 6].r, g_ipu_vqclut[ 6].g, g_ipu_vqclut[ 6].b,
	        g_ipu_vqclut[ 7].r, g_ipu_vqclut[ 7].g, g_ipu_vqclut[ 7].b,
	        g_ipu_vqclut[ 8].r, g_ipu_vqclut[ 8].g, g_ipu_vqclut[ 8].b,
	        g_ipu_vqclut[ 9].r, g_ipu_vqclut[ 9].g, g_ipu_vqclut[ 9].b,
	        g_ipu_vqclut[10].r, g_ipu_vqclut[10].g, g_ipu_vqclut[10].b,
	        g_ipu_vqclut[11].r, g_ipu_vqclut[11].g, g_ipu_vqclut[11].b,
	        g_ipu_vqclut[12].r, g_ipu_vqclut[12].g, g_ipu_vqclut[12].b,
	        g_ipu_vqclut[13].r, g_ipu_vqclut[13].g, g_ipu_vqclut[13].b,
	        g_ipu_vqclut[14].r, g_ipu_vqclut[14].g, g_ipu_vqclut[14].b,
	        g_ipu_vqclut[15].r, g_ipu_vqclut[15].g, g_ipu_vqclut[15].b);

	return true;
}

// IPU Transfers are split into 8Qwords so we need to send ALL the data
static __ri bool ipuCSC(tIPU_CMD_CSC csc)
{
	csc.log_from_YCbCr();

	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < 48; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.mb8 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_csc(decoder.mb8, decoder.rgb32, 0);
		if (csc.OFM) ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);

		if (csc.OFM)
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32) return false;
		}
		else
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb32) + 4 * ipu_cmd.pos[1], 64 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 64) return false;
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return true;
}

static __ri bool ipuPACK(tIPU_CMD_CSC csc)
{
	csc.log_from_RGB32();

	for (;ipu_cmd.index < (int)csc.MBC; ipu_cmd.index++)
	{
		for(;ipu_cmd.pos[0] < (int)sizeof(macroblock_rgb32) / 8; ipu_cmd.pos[0]++)
		{
			if (!getBits64((u8*)&decoder.rgb32 + 8 * ipu_cmd.pos[0], 1)) return false;
		}

		ipu_dither(decoder.rgb32, decoder.rgb16, csc.DTE);

		if (!csc.OFM) ipu_vq(decoder.rgb16, g_ipu_indx4);

		if (csc.OFM)
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*) & decoder.rgb16) + 4 * ipu_cmd.pos[1], 32 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 32) return false;
		}
		else
		{
			ipu_cmd.pos[1] += ipu_fifo.out.write(((u32*)g_ipu_indx4) + 4 * ipu_cmd.pos[1], 8 - ipu_cmd.pos[1]);
			if (ipu_cmd.pos[1] < 8) return false;
		}

		ipu_cmd.pos[0] = 0;
		ipu_cmd.pos[1] = 0;
	}

	return true;
}

// --------------------------------------------------------------------------------------
//  CORE Functions (referenced from MPEG library)
// --------------------------------------------------------------------------------------

__fi void ipu_csc(macroblock_8& mb8, macroblock_rgb32& rgb32, int sgn)
{
	int i;
	u8* p = (u8*)&rgb32;

	yuv2rgb();

	if (g_ipu_thresh[0] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < g_ipu_thresh[0]) && (p[1] < g_ipu_thresh[0]) && (p[2] < g_ipu_thresh[0]))
				*(u32*)p = 0;
			else if ((p[0] < g_ipu_thresh[1]) && (p[1] < g_ipu_thresh[1]) && (p[2] < g_ipu_thresh[1]))
				p[3] = 0x40;
		}
	}
	else if (g_ipu_thresh[1] > 0)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			if ((p[0] < g_ipu_thresh[1]) && (p[1] < g_ipu_thresh[1]) && (p[2] < g_ipu_thresh[1]))
				p[3] = 0x40;
		}
	}
	if (sgn)
	{
		for (i = 0; i < 16*16; i++, p += 4)
		{
			*(u32*)p ^= 0x808080;
		}
	}
}

__fi void ipu_vq(macroblock_rgb16& rgb16, u8* indx4)
{
	const auto closest_index = [&](int i, int j) {
		u8 index = 0;
		int min_distance = std::numeric_limits<int>::max();
		for (u8 k = 0; k < 16; ++k)
		{
			const int dr = rgb16.c[i][j].r - g_ipu_vqclut[k].r;
			const int dg = rgb16.c[i][j].g - g_ipu_vqclut[k].g;
			const int db = rgb16.c[i][j].b - g_ipu_vqclut[k].b;
			const int distance = dr * dr + dg * dg + db * db;

			// XXX: If two distances are the same which index is used?
			if (min_distance > distance)
			{
				index = k;
				min_distance = distance;
			}
		}

		return index;
	};

	for (int i = 0; i < 16; ++i)
		for (int j = 0; j < 8; ++j)
			indx4[i * 8 + j] = closest_index(i, 2 * j + 1) << 4 | closest_index(i, 2 * j);
}

__noinline void IPUWorker()
{
	pxAssert(ipuRegs.ctrl.BUSY);

	switch (ipu_cmd.CMD)
	{
		// These are unreachable (BUSY will always be 0 for them)
		//case SCE_IPU_BCLR:
		//case SCE_IPU_SETTH:
			//break;

		case SCE_IPU_IDEC:
			if (!mpeg2sliceIDEC()) return;

			//ipuRegs.ctrl.OFC = 0;
			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_BDEC:
			if (!mpeg2_slice()) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;

			//if (ipuRegs.ctrl.SCD || ipuRegs.ctrl.ECD) hwIntcIrq(INTC_IPU);
			break;

		case SCE_IPU_VDEC:
			if (!ipuVDEC(ipu_cmd.current)) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_FDEC:
			if (!ipuFDEC(ipu_cmd.current)) return;

			ipuRegs.topbusy = 0;
			ipuRegs.cmd.BUSY = 0;
			break;

		case SCE_IPU_SETIQ:
			if (!ipuSETIQ(ipu_cmd.current)) return;
			break;

		case SCE_IPU_SETVQ:
			if (!ipuSETVQ(ipu_cmd.current)) return;
			break;

		case SCE_IPU_CSC:
			if (!ipuCSC(ipu_cmd.current)) return;
			break;

		case SCE_IPU_PACK:
			if (!ipuPACK(ipu_cmd.current)) return;
			break;

		jNO_DEFAULT
	}

	// success
	IPU_LOG("IPU Command finished");
	ipuRegs.ctrl.BUSY = 0;
	//ipu_cmd.current = 0xffffffff;
	hwIntcIrq(INTC_IPU);
}

MULTI_ISA_UNSHARED_END
