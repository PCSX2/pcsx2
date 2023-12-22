// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/Global.h"
#include "Config.h"

#include "common/Console.h"
#include "common/FileSystem.h"

#include <cstdarg>

#ifdef PCSX2_DEVBUILD

static FILE* spu2Log = nullptr;

void SPU2::OpenFileLog()
{
	if (spu2Log)
		return;

	spu2Log = EmuFolders::OpenLogFile("SPU2Log.txt", "w");
	setvbuf(spu2Log, nullptr, _IONBF, 0);
}

void SPU2::CloseFileLog()
{
	if (!spu2Log)
		return;

	std::fclose(spu2Log);
	spu2Log = nullptr;
}

void SPU2::FileLog(const char* fmt, ...)
{
	if (!spu2Log)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	std::vfprintf(spu2Log, fmt, ap);
	std::fflush(spu2Log);
	va_end(ap);
}

//Note to developer on the usage of ConLog:
//  while ConLog doesn't print anything if messages to console are disabled at the GUI,
//    it's still better to outright not call it on tight loop scenarios, by testing MsgToConsole() (which is inline and very quick).
//    Else, there's some (small) overhead in calling and returning from ConLog.
void SPU2::ConLog(const char* fmt, ...)
{
	if (!SPU2::MsgToConsole())
		return;

	std::va_list ap;
	va_start(ap, fmt);
	Console.FormatV(fmt, ap);
	va_end(ap);

	if (spu2Log)
	{
		va_start(ap, fmt);
		std::vfprintf(spu2Log, fmt, ap);
		std::fflush(spu2Log);
		va_end(ap);
	}
}

void V_VolumeSlide::DebugDump(FILE* dump, const char* title, const char* nameLR)
{
	fprintf(dump, "%s Volume for %s Channel:\t%x\n"
				  "  - Value:     %x\n",
			title, nameLR, Reg_VOL, Value);
}

void V_VolumeSlideLR::DebugDump(FILE* dump, const char* title)
{
	Left.DebugDump(dump, title, "Left");
	Right.DebugDump(dump, title, "Right");
}

void V_VolumeLR::DebugDump(FILE* dump, const char* title)
{
	fprintf(dump, "Volume for %s (%s Channel):\t%x\n", title, "Left", Left);
	fprintf(dump, "Volume for %s (%s Channel):\t%x\n", title, "Right", Right);
}

void SPU2::DoFullDump()
{
	FILE* dump;

	if (SPU2::MemDump())
	{
		dump = EmuFolders::OpenLogFile("SPU2mem.dat", "wb");
		if (dump)
		{
			fwrite(_spu2mem, 0x200000, 1, dump);
			fclose(dump);
		}
	}
	if (SPU2::RegDump())
	{
		dump = EmuFolders::OpenLogFile("SPU2regs.dat", "wb");
		if (dump)
		{
			fwrite(spu2regs, 0x2000, 1, dump);
			fclose(dump);
		}
	}

	if (!SPU2::CoresDump())
		return;
	dump = EmuFolders::OpenLogFile("SPU2Cores.txt", "wt");
	if (dump)
	{
		for (u8 c = 0; c < 2; c++)
		{
			fprintf(dump, "#### CORE %d DUMP.\n", c);

			Cores[c].MasterVol.DebugDump(dump, "Master");

			Cores[c].ExtVol.DebugDump(dump, "External Data Input");
			Cores[c].InpVol.DebugDump(dump, "Voice Data Input [dry]");
			Cores[c].FxVol.DebugDump(dump, "Effects/Reverb [wet]");

			fprintf(dump, "Interrupt Address:          %x\n", Cores[c].IRQA);
			fprintf(dump, "DMA Transfer Start Address: %x\n", Cores[c].TSA);
			fprintf(dump, "External Input to Direct Output (Left):    %s\n", Cores[c].DryGate.ExtL ? "Yes" : "No");
			fprintf(dump, "External Input to Direct Output (Right):   %s\n", Cores[c].DryGate.ExtR ? "Yes" : "No");
			fprintf(dump, "External Input to Effects (Left):          %s\n", Cores[c].WetGate.ExtL ? "Yes" : "No");
			fprintf(dump, "External Input to Effects (Right):         %s\n", Cores[c].WetGate.ExtR ? "Yes" : "No");
			fprintf(dump, "Sound Data Input to Direct Output (Left):  %s\n", Cores[c].DryGate.SndL ? "Yes" : "No");
			fprintf(dump, "Sound Data Input to Direct Output (Right): %s\n", Cores[c].DryGate.SndR ? "Yes" : "No");
			fprintf(dump, "Sound Data Input to Effects (Left):        %s\n", Cores[c].WetGate.SndL ? "Yes" : "No");
			fprintf(dump, "Sound Data Input to Effects (Right):       %s\n", Cores[c].WetGate.SndR ? "Yes" : "No");
			fprintf(dump, "Voice Data Input to Direct Output (Left):  %s\n", Cores[c].DryGate.InpL ? "Yes" : "No");
			fprintf(dump, "Voice Data Input to Direct Output (Right): %s\n", Cores[c].DryGate.InpR ? "Yes" : "No");
			fprintf(dump, "Voice Data Input to Effects (Left):        %s\n", Cores[c].WetGate.InpL ? "Yes" : "No");
			fprintf(dump, "Voice Data Input to Effects (Right):       %s\n", Cores[c].WetGate.InpR ? "Yes" : "No");
			fprintf(dump, "IRQ Enabled:     %s\n", Cores[c].IRQEnable ? "Yes" : "No");
			fprintf(dump, "Effects Enabled: %s\n", Cores[c].FxEnable ? "Yes" : "No");
			fprintf(dump, "Mute Enabled:    %s\n", Cores[c].Mute ? "Yes" : "No");
			fprintf(dump, "Noise Clock:     %d\n", Cores[c].NoiseClk);
			fprintf(dump, "DMA Bits:        %d\n", Cores[c].DMABits);
			fprintf(dump, "Effects Start:   %x\n", Cores[c].EffectsStartA);
			fprintf(dump, "Effects End:     %x\n", Cores[c].EffectsEndA);
			fprintf(dump, "Registers:\n");
			fprintf(dump, "  - PMON:   %x\n", Cores[c].Regs.PMON);
			fprintf(dump, "  - NON:    %x\n", Cores[c].Regs.NON);
			fprintf(dump, "  - VMIXL:  %x\n", Cores[c].Regs.VMIXL);
			fprintf(dump, "  - VMIXR:  %x\n", Cores[c].Regs.VMIXR);
			fprintf(dump, "  - VMIXEL: %x\n", Cores[c].Regs.VMIXEL);
			fprintf(dump, "  - VMIXER: %x\n", Cores[c].Regs.VMIXER);
			fprintf(dump, "  - MMIX:   %x\n", Cores[c].Regs.VMIXEL);
			fprintf(dump, "  - ENDX:   %x\n", Cores[c].Regs.VMIXER);
			fprintf(dump, "  - STATX:  %x\n", Cores[c].Regs.VMIXEL);
			fprintf(dump, "  - ATTR:   %x\n", Cores[c].Regs.VMIXER);
			for (u8 v = 0; v < 24; v++)
			{
				fprintf(dump, "Voice %d:\n", v);
				Cores[c].Voices[v].Volume.DebugDump(dump, "");

				fprintf(dump, "  - ADSR Envelope: %x & %x\n"
							  "     - Ash: %x\n"
							  "     - Ast: %x\n"
							  "     - Am: %x\n"
							  "     - Dsh: %x\n"
							  "     - Sl: %x\n"
							  "     - Ssh: %x\n"
							  "     - Sst: %x\n"
							  "     - Sm: %x\n"
							  "     - Rsh: %x\n"
							  "     - Rm: %x\n"
							  "     - Phase: %x\n"
							  "     - Value: %x\n",
						Cores[c].Voices[v].ADSR.regADSR1,
						Cores[c].Voices[v].ADSR.regADSR2,
						Cores[c].Voices[v].ADSR.AttackShift,
						Cores[c].Voices[v].ADSR.AttackStep,
						Cores[c].Voices[v].ADSR.AttackMode,
						Cores[c].Voices[v].ADSR.DecayShift,
						Cores[c].Voices[v].ADSR.SustainLevel,
						Cores[c].Voices[v].ADSR.SustainShift,
						Cores[c].Voices[v].ADSR.SustainStep,
						Cores[c].Voices[v].ADSR.SustainMode,
						Cores[c].Voices[v].ADSR.ReleaseShift,
						Cores[c].Voices[v].ADSR.ReleaseMode,
						Cores[c].Voices[v].ADSR.Phase,
						Cores[c].Voices[v].ADSR.Value);

				fprintf(dump, "  - Pitch:     %x\n", Cores[c].Voices[v].Pitch);
				fprintf(dump, "  - Modulated: %s\n", Cores[c].Voices[v].Modulated ? "Yes" : "No");
				fprintf(dump, "  - Source:    %s\n", Cores[c].Voices[v].Noise ? "Noise" : "Wave");
				fprintf(dump, "  - Direct Output for Left Channel:   %s\n", Cores[c].VoiceGates[v].DryL ? "Yes" : "No");
				fprintf(dump, "  - Direct Output for Right Channel:  %s\n", Cores[c].VoiceGates[v].DryR ? "Yes" : "No");
				fprintf(dump, "  - Effects Output for Left Channel:  %s\n", Cores[c].VoiceGates[v].WetL ? "Yes" : "No");
				fprintf(dump, "  - Effects Output for Right Channel: %s\n", Cores[c].VoiceGates[v].WetR ? "Yes" : "No");
				fprintf(dump, "  - Loop Start Address:  %x\n", Cores[c].Voices[v].LoopStartA);
				fprintf(dump, "  - Sound Start Address: %x\n", Cores[c].Voices[v].StartA);
				fprintf(dump, "  - Next Data Address:   %x\n", Cores[c].Voices[v].NextA);
				fprintf(dump, "  - Play Start Cycle:    %d\n", Cores[c].Voices[v].PlayCycle);
				fprintf(dump, "  - Play Status:         %s\n", (Cores[c].Voices[v].ADSR.Phase > 0) ? "Playing" : "Not Playing");
				fprintf(dump, "  - Block Sample:        %d\n", Cores[c].Voices[v].SCurrent);
			}
			fprintf(dump, "#### END OF DUMP.\n\n");
		}
		fclose(dump);
	}

	dump = EmuFolders::OpenLogFile("SPU2effects.txt", "wt");
	if (dump)
	{
		for (u8 c = 0; c < 2; c++)
		{
			fprintf(dump, "#### CORE %d EFFECTS PROCESSOR DUMP.\n", c);

			fprintf(dump, "  - IN_COEF_L:   %x\n", Cores[c].Revb.IN_COEF_R);
			fprintf(dump, "  - IN_COEF_R:   %x\n", Cores[c].Revb.IN_COEF_L);

			fprintf(dump, "  - APF1_VOL:    %x\n", Cores[c].Revb.APF1_VOL);
			fprintf(dump, "  - APF2_VOL:    %x\n", Cores[c].Revb.APF2_VOL);
			fprintf(dump, "  - APF1_SIZE:   %x\n", Cores[c].Revb.APF1_SIZE);
			fprintf(dump, "  - APF2_SIZE:   %x\n", Cores[c].Revb.APF2_SIZE);

			fprintf(dump, "  - IIR_VOL:     %x\n", Cores[c].Revb.IIR_VOL);
			fprintf(dump, "  - WALL_VOL:    %x\n", Cores[c].Revb.WALL_VOL);
			fprintf(dump, "  - SAME_L_SRC:  %x\n", Cores[c].Revb.SAME_L_SRC);
			fprintf(dump, "  - SAME_R_SRC:  %x\n", Cores[c].Revb.SAME_R_SRC);
			fprintf(dump, "  - DIFF_L_SRC:  %x\n", Cores[c].Revb.DIFF_L_SRC);
			fprintf(dump, "  - DIFF_R_SRC:  %x\n", Cores[c].Revb.DIFF_R_SRC);
			fprintf(dump, "  - SAME_L_DST:  %x\n", Cores[c].Revb.SAME_L_DST);
			fprintf(dump, "  - SAME_R_DST:  %x\n", Cores[c].Revb.SAME_R_DST);
			fprintf(dump, "  - DIFF_L_DST:  %x\n", Cores[c].Revb.DIFF_L_DST);
			fprintf(dump, "  - DIFF_R_DST:  %x\n", Cores[c].Revb.DIFF_R_DST);

			fprintf(dump, "  - COMB1_VOL:   %x\n", Cores[c].Revb.COMB1_VOL);
			fprintf(dump, "  - COMB2_VOL:   %x\n", Cores[c].Revb.COMB2_VOL);
			fprintf(dump, "  - COMB3_VOL:   %x\n", Cores[c].Revb.COMB3_VOL);
			fprintf(dump, "  - COMB4_VOL:   %x\n", Cores[c].Revb.COMB4_VOL);
			fprintf(dump, "  - COMB1_L_SRC: %x\n", Cores[c].Revb.COMB1_L_SRC);
			fprintf(dump, "  - COMB1_R_SRC: %x\n", Cores[c].Revb.COMB1_R_SRC);
			fprintf(dump, "  - COMB2_L_SRC: %x\n", Cores[c].Revb.COMB2_L_SRC);
			fprintf(dump, "  - COMB2_R_SRC: %x\n", Cores[c].Revb.COMB2_R_SRC);
			fprintf(dump, "  - COMB3_L_SRC: %x\n", Cores[c].Revb.COMB3_L_SRC);
			fprintf(dump, "  - COMB3_R_SRC: %x\n", Cores[c].Revb.COMB3_R_SRC);
			fprintf(dump, "  - COMB4_L_SRC: %x\n", Cores[c].Revb.COMB4_L_SRC);
			fprintf(dump, "  - COMB4_R_SRC: %x\n", Cores[c].Revb.COMB4_R_SRC);

			fprintf(dump, "  - APF1_L_DST:  %x\n", Cores[c].Revb.APF1_L_DST);
			fprintf(dump, "  - APF1_R_DST:  %x\n", Cores[c].Revb.APF1_R_DST);
			fprintf(dump, "  - APF2_L_DST:  %x\n", Cores[c].Revb.APF2_L_DST);
			fprintf(dump, "  - APF2_R_DST:  %x\n", Cores[c].Revb.APF2_R_DST);
			fprintf(dump, "#### END OF DUMP.\n\n");
		}
		fclose(dump);
	}
}

static const char* ParamNames[8] = {"VOLL", "VOLR", "PITCH", "ADSR1", "ADSR2", "ENVX", "VOLXL", "VOLXR"};
static const char* AddressNames[6] = {"SSAH", "SSAL", "LSAH", "LSAL", "NAXH", "NAXL"};

__forceinline static void _RegLog_(const char* action, int level, const char* RName, u32 mem, u32 core, u16 value)
{
	if (level > 1)
	{
		SPU2::FileLog("[%10d] SPU2 %s mem %08x (core %d, register %s) value %04x\n",
				Cycles, action, mem, core, RName, value);
	}
}

#define RegLog(lev, rname, mem, core, val) _RegLog_(action, lev, rname, mem, core, val)

void SPU2::WriteRegLog(const char* action, u32 rmem, u16 value)
{
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
		snprintf(dest, std::size(dest), "Voice %d %s", voice, ParamNames[param]);
		RegLog(2, dest, rmem, core, value);
	}
	else if ((omem >= 0x01C0) && (omem < 0x02E0)) // Voice Addressing Params (VA)
	{
		const u32 voice = ((omem - 0x01C0) / 12);
		const u32 address = ((omem - 0x01C0) % 12) >> 1;

		char dest[192];
		snprintf(dest, std::size(dest), "Voice %d %s", voice, AddressNames[address]);
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
				if (Spdif.Unknown1 != value && SPU2::MsgToConsole())
					SPU2::ConLog("* SPU2: SPDIF Unknown Register 1 set to %04x\n", value);
				RegLog(2, "SPDIF_UNKNOWN1", rmem, -1, value);
				break;
			case SPDIF_MODE:
				if (Spdif.Mode != value && SPU2::MsgToConsole())
					SPU2::ConLog("* SPU2: SPDIF Mode set to %04x\n", value);
				RegLog(2, "SPDIF_MODE", rmem, -1, value);
				break;
			case SPDIF_MEDIA:
				if (Spdif.Media != value && SPU2::MsgToConsole())
					SPU2::ConLog("* SPU2: SPDIF Media set to %04x\n", value);
				RegLog(2, "SPDIF_MEDIA", rmem, -1, value);
				break;
			case 0x7ca:
				if (Spdif.Unknown2 != value && SPU2::MsgToConsole())
					SPU2::ConLog("* SPU2: SPDIF Unknown Register 2 set to %04x\n", value);
				RegLog(2, "SPDIF_UNKNOWN2", rmem, -1, value);
				break;
			case SPDIF_PROTECT:
				if (Spdif.Protection != value && SPU2::MsgToConsole())
					SPU2::ConLog("* SPU2: SPDIF Copy set to %04x\n", value);
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

#undef RegLog

#endif // PCSX2_DEVBUILD
