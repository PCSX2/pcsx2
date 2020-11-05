/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "Global.h"

const char* ParamNames[8] = {"VOLL", "VOLR", "PITCH", "ADSR1", "ADSR2", "ENVX", "VOLXL", "VOLXR"};
const char* AddressNames[6] = {"SSAH", "SSAL", "LSAH", "LSAL", "NAXH", "NAXL"};

__forceinline void _RegLog_(const char* action, int level, const char* RName, u32 mem, u32 core, u16 value)
{
	if (level > 1)
		FileLog("[%10d] SPU2 %s mem %08x (core %d, register %s) value %04x\n",
				Cycles, action, mem, core, RName, value);
}

#define RegLog(lev, rname, mem, core, val) _RegLog_(action, lev, rname, mem, core, val)

void SPU2writeLog(const char* action, u32 rmem, u16 value)
{
	if (!IsDevBuild)
		return;

	//u32 vx=0, vc=0;
	u32 core = 0, omem, mem;
	omem = mem = rmem & 0x7FF; //FFFF;
	if (mem & 0x400)
	{
		omem ^= 0x400;
		core = 1;
	}

	if (omem < 0x0180) // Voice Params (VP)
	{
		const u32 voice = (omem & 0x1F0) >> 4;
		const u32 param = (omem & 0xF) >> 1;
		char dest[192];
		sprintf(dest, "Voice %d %s", voice, ParamNames[param]);
		RegLog(2, dest, rmem, core, value);
	}
	else if ((omem >= 0x01C0) && (omem < 0x02E0)) // Voice Addressing Params (VA)
	{
		const u32 voice = ((omem - 0x01C0) / 12);
		const u32 address = ((omem - 0x01C0) % 12) >> 1;

		char dest[192];
		sprintf(dest, "Voice %d %s", voice, AddressNames[address]);
		RegLog(2, dest, rmem, core, value);
	}
	else if ((mem >= 0x0760) && (mem < 0x07b0))
	{
		omem = mem;
		core = 0;
		if (mem >= 0x0788)
		{
			omem -= 0x28;
			core = 1;
		}
		switch (omem)
		{
			case REG_P_EVOLL:
				RegLog(2, "EVOLL", rmem, core, value);
				break;
			case REG_P_EVOLR:
				RegLog(2, "EVOLR", rmem, core, value);
				break;
			case REG_P_AVOLL:
				if (core)
				{
					RegLog(2, "AVOLL", rmem, core, value);
				}
				break;
			case REG_P_AVOLR:
				if (core)
				{
					RegLog(2, "AVOLR", rmem, core, value);
				}
				break;
			case REG_P_BVOLL:
				RegLog(2, "BVOLL", rmem, core, value);
				break;
			case REG_P_BVOLR:
				RegLog(2, "BVOLR", rmem, core, value);
				break;
			case REG_P_MVOLXL:
				RegLog(2, "MVOLXL", rmem, core, value);
				break;
			case REG_P_MVOLXR:
				RegLog(2, "MVOLXR", rmem, core, value);
				break;
			case R_IIR_VOL:
				RegLog(2, "IIR_VOL", rmem, core, value);
				break;
			case R_COMB1_VOL:
				RegLog(2, "COMB1_VOL", rmem, core, value);
				break;
			case R_COMB2_VOL:
				RegLog(2, "COMB2_VOL", rmem, core, value);
				break;
			case R_COMB3_VOL:
				RegLog(2, "COMB3_VOL", rmem, core, value);
				break;
			case R_COMB4_VOL:
				RegLog(2, "COMB4_VOL", rmem, core, value);
				break;
			case R_WALL_VOL:
				RegLog(2, "WALL_VOL", rmem, core, value);
				break;
			case R_APF1_VOL:
				RegLog(2, "APF1_VOL", rmem, core, value);
				break;
			case R_APF2_VOL:
				RegLog(2, "APF2_VOL", rmem, core, value);
				break;
			case R_IN_COEF_L:
				RegLog(2, "IN_COEF_L", rmem, core, value);
				break;
			case R_IN_COEF_R:
				RegLog(2, "IN_COEF_R", rmem, core, value);
				break;
		}
	}
	else if ((mem >= 0x07C0) && (mem < 0x07CE))
	{
		switch (mem)
		{
			case SPDIF_OUT:
				RegLog(2, "SPDIF_OUT", rmem, -1, value);
				break;
			case SPDIF_IRQINFO:
				RegLog(2, "SPDIF_IRQINFO", rmem, -1, value);
				break;
			case 0x7c4:
				if (Spdif.Unknown1 != value)
					ConLog("* SPU2: SPDIF Unknown Register 1 set to %04x\n", value);
				RegLog(2, "SPDIF_UNKNOWN1", rmem, -1, value);
				break;
			case SPDIF_MODE:
				if (Spdif.Mode != value)
					ConLog("* SPU2: SPDIF Mode set to %04x\n", value);
				RegLog(2, "SPDIF_MODE", rmem, -1, value);
				break;
			case SPDIF_MEDIA:
				if (Spdif.Media != value)
					ConLog("* SPU2: SPDIF Media set to %04x\n", value);
				RegLog(2, "SPDIF_MEDIA", rmem, -1, value);
				break;
			case 0x7ca:
				if (Spdif.Unknown2 != value)
					ConLog("* SPU2: SPDIF Unknown Register 2 set to %04x\n", value);
				RegLog(2, "SPDIF_UNKNOWN2", rmem, -1, value);
				break;
			case SPDIF_PROTECT:
				if (Spdif.Protection != value)
					ConLog("* SPU2: SPDIF Copy set to %04x\n", value);
				RegLog(2, "SPDIF_PROTECT", rmem, -1, value);
				break;
		}
		UpdateSpdifMode();
	}
	else
	{
		switch (omem)
		{
			case REG_C_ATTR:
				RegLog(4, "ATTR", rmem, core, value);
				break;
			case REG_S_PMON:
				RegLog(1, "PMON0", rmem, core, value);
				break;
			case (REG_S_PMON + 2):
				RegLog(1, "PMON1", rmem, core, value);
				break;
			case REG_S_NON:
				RegLog(1, "NON0", rmem, core, value);
				break;
			case (REG_S_NON + 2):
				RegLog(1, "NON1", rmem, core, value);
				break;
			case REG_S_VMIXL:
				RegLog(1, "VMIXL0", rmem, core, value);
				break;
			case (REG_S_VMIXL + 2):
				RegLog(1, "VMIXL1", rmem, core, value);
				break;
			case REG_S_VMIXEL:
				RegLog(1, "VMIXEL0", rmem, core, value);
				break;
			case (REG_S_VMIXEL + 2):
				RegLog(1, "VMIXEL1", rmem, core, value);
				break;
			case REG_S_VMIXR:
				RegLog(1, "VMIXR0", rmem, core, value);
				break;
			case (REG_S_VMIXR + 2):
				RegLog(1, "VMIXR1", rmem, core, value);
				break;
			case REG_S_VMIXER:
				RegLog(1, "VMIXER0", rmem, core, value);
				break;
			case (REG_S_VMIXER + 2):
				RegLog(1, "VMIXER1", rmem, core, value);
				break;
			case REG_P_MMIX:
				RegLog(1, "MMIX", rmem, core, value);
				break;
			case REG_A_IRQA:
				RegLog(2, "IRQAH", rmem, core, value);
				break;
			case (REG_A_IRQA + 2):
				RegLog(2, "IRQAL", rmem, core, value);
				break;
			case (REG_S_KON + 2):
				RegLog(1, "KON1", rmem, core, value);
				break;
			case REG_S_KON:
				RegLog(1, "KON0", rmem, core, value);
				break;
			case (REG_S_KOFF + 2):
				RegLog(1, "KOFF1", rmem, core, value);
				break;
			case REG_S_KOFF:
				RegLog(1, "KOFF0", rmem, core, value);
				break;
			case REG_A_TSA:
				RegLog(2, "TSAH", rmem, core, value);
				break;
			case (REG_A_TSA + 2):
				RegLog(2, "TSAL", rmem, core, value);
				break;
			case REG_S_ENDX:
				//ConLog("* SPU2: Core %d ENDX cleared!\n",core);
				RegLog(2, "ENDX0", rmem, core, value);
				break;
			case (REG_S_ENDX + 2):
				//ConLog("* SPU2: Core %d ENDX cleared!\n",core);
				RegLog(2, "ENDX1", rmem, core, value);
				break;
			case REG_P_MVOLL:
				RegLog(1, "MVOLL", rmem, core, value);
				break;
			case REG_P_MVOLR:
				RegLog(1, "MVOLR", rmem, core, value);
				break;
			case REG_S_ADMAS:
				RegLog(3, "ADMAS", rmem, core, value);
				//ConLog("* SPU2: Core %d AutoDMAControl set to %d\n",core,value);
				break;
			case REG_P_STATX:
				RegLog(3, "STATX", rmem, core, value);
				break;
			case REG_A_ESA:
				RegLog(2, "ESAH", rmem, core, value);
				break;
			case (REG_A_ESA + 2):
				RegLog(2, "ESAL", rmem, core, value);
				break;
			case REG_A_EEA:
				RegLog(2, "EEAH", rmem, core, value);
				break;

#define LOG_REVB_REG(n, t)                  \
	case R_##n:                             \
		RegLog(2, t "H", mem, core, value); \
		break;                              \
	case (R_##n + 2):                       \
		RegLog(2, t "L", mem, core, value); \
		break;

				LOG_REVB_REG(APF1_SIZE, "APF1_SIZE")
				LOG_REVB_REG(APF2_SIZE, "APF2_SIZE")
				LOG_REVB_REG(SAME_L_SRC, "SAME_L_SRC")
				LOG_REVB_REG(SAME_R_SRC, "SAME_R_SRC")
				LOG_REVB_REG(DIFF_L_SRC, "DIFF_L_SRC")
				LOG_REVB_REG(DIFF_R_SRC, "DIFF_R_SRC")
				LOG_REVB_REG(SAME_L_DST, "SAME_L_DST")
				LOG_REVB_REG(SAME_R_DST, "SAME_R_DST")
				LOG_REVB_REG(DIFF_L_DST, "DIFF_L_DST")
				LOG_REVB_REG(DIFF_R_DST, "DIFF_R_DST")
				LOG_REVB_REG(COMB1_L_SRC, "COMB1_L_SRC")
				LOG_REVB_REG(COMB1_R_SRC, "COMB1_R_SRC")
				LOG_REVB_REG(COMB2_L_SRC, "COMB2_L_SRC")
				LOG_REVB_REG(COMB2_R_SRC, "COMB2_R_SRC")
				LOG_REVB_REG(COMB3_L_SRC, "COMB3_L_SRC")
				LOG_REVB_REG(COMB3_R_SRC, "COMB3_R_SRC")
				LOG_REVB_REG(COMB4_L_SRC, "COMB4_L_SRC")
				LOG_REVB_REG(COMB4_R_SRC, "COMB4_R_SRC")
				LOG_REVB_REG(APF1_L_DST, "APF1_L_DST")
				LOG_REVB_REG(APF1_R_DST, "APF1_R_DST")
				LOG_REVB_REG(APF2_L_DST, "APF2_L_DST")
				LOG_REVB_REG(APF2_R_DST, "APF2_R_DST")

			default:
				RegLog(2, "UNKNOWN", rmem, core, value);
				spu2Ru16(mem) = value;
		}
	}
}
