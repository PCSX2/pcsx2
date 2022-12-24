/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "spu2.h"
#include "Dma.h"
#include "Host/Dialogs.h"
#include "R3000A.h"

using namespace Threading;

static int ConsoleSampleRate = 48000;
int SampleRate = 48000;

static double DeviceSampleRateMultiplier = 1.0;

u32 lClocks = 0;

// --------------------------------------------------------------------------------------
//  DMA 4/7 Callbacks from Core Emulator
// --------------------------------------------------------------------------------------


void SPU2setSettingsDir(const char* dir)
{
	CfgSetSettingsDir(dir);
}

void SPU2setLogDir(const char* dir)
{
	CfgSetLogDir(dir);
}

void SPU2readDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	FileLog("[%10d] SPU2 readDMA4Mem size %x\n", Cycles, size << 1);
	Cores[0].DoDMAread(pMem, size);
}

void SPU2writeDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	FileLog("[%10d] SPU2 writeDMA4Mem size %x at address %x\n", Cycles, size << 1, Cores[0].TSA);

	Cores[0].DoDMAwrite(pMem, size);
}

void SPU2interruptDMA4()
{
	FileLog("[%10d] SPU2 interruptDMA4\n", Cycles);
	if (Cores[0].DmaMode)
		Cores[0].Regs.STATX |= 0x80;
	Cores[0].Regs.STATX &= ~0x400;
	Cores[0].TSA = Cores[0].ActiveTSA;
}

void SPU2interruptDMA7()
{
	FileLog("[%10d] SPU2 interruptDMA7\n", Cycles);
	if (Cores[1].DmaMode)
		Cores[1].Regs.STATX |= 0x80;
	Cores[1].Regs.STATX &= ~0x400;
	Cores[1].TSA = Cores[1].ActiveTSA;
}

void SPU2readDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	FileLog("[%10d] SPU2 readDMA7Mem size %x\n", Cycles, size << 1);
	Cores[1].DoDMAread(pMem, size);
}

void SPU2writeDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	FileLog("[%10d] SPU2 writeDMA7Mem size %x at address %x\n", Cycles, size << 1, Cores[1].TSA);

	Cores[1].DoDMAwrite(pMem, size);
}

static void SPU2InitSndBuffer()
{
	Console.WriteLn("Initializing SndBuffer at sample rate of %u...", SampleRate);
	if (SndBuffer::Init())
		return;

	if (SampleRate != ConsoleSampleRate)
	{
		// TODO: Resample on our side...
		const int original_sample_rate = SampleRate;
		Console.Error("Failed to init SPU2 at adjusted sample rate %u, trying console rate.", SampleRate);
		SampleRate = ConsoleSampleRate;
		if (SndBuffer::Init())
			return;

		SampleRate = original_sample_rate;
	}

	// just use nullout
	OutputModule = FindOutputModuleById(NullOut->GetIdent());
	if (!SndBuffer::Init())
		pxFailRel("Failed to initialize nullout.");
}

static void SPU2UpdateSampleRate()
{
	const int new_sample_rate = static_cast<int>(std::round(static_cast<double>(ConsoleSampleRate) * DeviceSampleRateMultiplier));
	if (SampleRate == new_sample_rate)
		return;

	SndBuffer::Cleanup();
	SampleRate = new_sample_rate;
	SPU2InitSndBuffer();
}

static void SPU2InternalReset(PS2Modes isRunningPSXMode)
{
	ConsoleSampleRate = (isRunningPSXMode == PS2Modes::PSX) ? 44100 : 48000;

	if (isRunningPSXMode == PS2Modes::PS2)
	{
		memset(spu2regs, 0, 0x010000);
		memset(_spu2mem, 0, 0x200000);
		memset(_spu2mem + 0x2800, 7, 0x10); // from BIOS reversal. Locks the voices so they don't run free.
		memset(_spu2mem + 0xe870, 7, 0x10); // Loop which gets left over by the BIOS, Megaman X7 relies on it being there.

		Spdif.Info = 0; // Reset IRQ Status if it got set in a previously run game

		Cores[0].Init(0);
		Cores[1].Init(1);
	}
}

void SPU2reset(PS2Modes isRunningPSXMode)
{
	SPU2InternalReset(isRunningPSXMode);
	SPU2UpdateSampleRate();
}

void SPU2SetDeviceSampleRateMultiplier(double multiplier)
{
	if (DeviceSampleRateMultiplier == multiplier)
		return;

	DeviceSampleRateMultiplier = multiplier;
	SPU2UpdateSampleRate();
}

bool SPU2init()
{
	pxAssert(regtable[0x400] == nullptr);
	spu2regs = (s16*)malloc(0x010000);
	_spu2mem = (s16*)malloc(0x200000);

	// adpcm decoder cache:
	//  the cache data size is determined by taking the number of adpcm blocks
	//  (2MB / 16) and multiplying it by the decoded block size (28 samples).
	//  Thus: pcm_cache_data = 7,340,032 bytes (ouch!)
	//  Expanded: 16 bytes expands to 56 bytes [3.5:1 ratio]
	//    Resulting in 2MB * 3.5.

	pcm_cache_data = (PcmCacheEntry*)calloc(pcm_BlockCount, sizeof(PcmCacheEntry));

	if (!spu2regs || !_spu2mem || !pcm_cache_data)
	{
		Console.Error("SPU2: Error allocating Memory");
		return false;
	}

	// Patch up a copy of regtable that directly maps "nullptrs" to SPU2 memory.

	memcpy(regtable, regtable_original, sizeof(regtable));

	for (uint mem = 0; mem < 0x800; mem++)
	{
		u16* ptr = regtable[mem >> 1];
		if (!ptr)
		{
			regtable[mem >> 1] = &(spu2Ru16(mem));
		}
	}

	InitADSR();
	return true;
}


bool SPU2open(PS2Modes isRunningPSXMode)
{
	ReadSettings();

#ifdef SPU2_LOG
	if (AccessLog())
	{
		spu2Log = OpenLog(AccessLogFileName.c_str());
		setvbuf(spu2Log, nullptr, _IONBF, 0);
		FileLog("SPU2init\n");
	}
#endif

	DMALogOpen();

	FileLog("[%10d] SPU2 Open\n", Cycles);

	lClocks = psxRegs.cycle;

	SPU2InternalReset(isRunningPSXMode);

	try
	{
		SampleRate = static_cast<int>(std::round(static_cast<double>(ConsoleSampleRate) * DeviceSampleRateMultiplier));
		SPU2InitSndBuffer();
		WaveDump::Open();
	}
	catch (std::exception& ex)
	{
		Console.Error("SPU2 Error: Could not initialize device, or something.\nReason: %s", ex.what());
		SPU2close();
		return false;
	}

	return true;
}

void SPU2close()
{
	FileLog("[%10d] SPU2 Close\n", Cycles);

	SndBuffer::Cleanup();
}

void SPU2shutdown()
{
	ConLog("* SPU2: Shutting down.\n");

	SPU2close();

	DoFullDump();
#ifdef STREAM_DUMP
	fclose(il0);
	fclose(il1);
#endif
#ifdef EFFECTS_DUMP
	fclose(el0);
	fclose(el1);
#endif
	WaveDump::Close();

	DMALogClose();

	safe_free(spu2regs);
	safe_free(_spu2mem);
	safe_free(pcm_cache_data);


#ifdef SPU2_LOG
	if (!AccessLog())
		return;
	FileLog("[%10d] SPU2shutdown\n", Cycles);
	if (spu2Log)
		fclose(spu2Log);
#endif
}

bool SPU2IsRunningPSXMode()
{
	return (ConsoleSampleRate == 44100);
}

void SPU2SetOutputPaused(bool paused)
{
	SndBuffer::SetPaused(paused);
}

#ifdef DEBUG_KEYS
static u32 lastTicks;
static bool lState[6];
#endif

void SPU2async(u32 cycles)
{
	TimeUpdate(psxRegs.cycle);

#ifdef DEBUG_KEYS
	u32 curTicks = GetTickCount();
	if ((curTicks - lastTicks) >= 50)
	{
		int oldI = Interpolation;
		bool cState[6];
		for (int i = 0; i < 6; i++)
		{
			cState[i] = !!(GetAsyncKeyState(VK_NUMPAD0 + i) & 0x8000);

			if ((cState[i] && !lState[i]) && i != 5)
				Interpolation = i;

			lState[i] = cState[i];
		}

		if (Interpolation != oldI)
		{
			printf("Interpolation set to %d", Interpolation);
			switch (Interpolation)
			{
				case 0:
					printf(" - Nearest.\n");
					break;
				case 1:
					printf(" - Linear.\n");
					break;
				case 2:
					printf(" - Cubic.\n");
					break;
				case 3:
					printf(" - Hermite.\n");
					break;
				case 4:
					printf(" - Catmull-Rom.\n");
					break;
				case 5:
					printf(" - Gaussian.\n");
					break;
				default:
					printf(" (unknown).\n");
					break;
			}
		}

		lastTicks = curTicks;
	}
#endif
}

u16 SPU2read(u32 rmem)
{
	u16 ret = 0xDEAD;
	u32 core = 0, mem = rmem & 0xFFFF, omem = mem;

	if (mem & 0x400)
	{
		omem ^= 0x400;
		core = 1;
	}

	if (omem == 0x1f9001AC)
	{
		Cores[core].ActiveTSA = Cores[core].TSA;
		for (int i = 0; i < 2; i++)
		{
			if (Cores[i].IRQEnable && (Cores[i].IRQA == Cores[core].ActiveTSA))
			{
				SetIrqCall(i);
			}
		}
		ret = Cores[core].DmaRead();
	}
	else
	{
		TimeUpdate(psxRegs.cycle);

		if (rmem >> 16 == 0x1f80)
		{
			ret = Cores[0].ReadRegPS1(rmem);
		}
		else if (mem >= 0x800)
		{
			ret = spu2Ru16(mem);
			ConLog("* SPU2: Read from reg>=0x800: %x value %x\n", mem, ret);
		}
		else
		{
			ret = *(regtable[(mem >> 1)]);
			//FileLog("[%10d] SPU2 read mem %x (core %d, register %x): %x\n",Cycles, mem, core, (omem & 0x7ff), ret);
			SPU2writeLog("read", rmem, ret);
		}
	}

	return ret;
}

void SPU2write(u32 rmem, u16 value)
{
	// Note: Reverb/Effects are very sensitive to having precise update timings.
	// If the SPU2 isn't in in sync with the IOP, samples can end up playing at rather
	// incorrect pitches and loop lengths.

	TimeUpdate(psxRegs.cycle);

	if (rmem >> 16 == 0x1f80)
		Cores[0].WriteRegPS1(rmem, value);
	else
	{
		SPU2writeLog("write", rmem, value);
		SPU2_FastWrite(rmem, value);
	}
}

// returns a non zero value if successful
bool SPU2setupRecording(const std::string* filename)
{
	return RecordStart(filename);
}

void SPU2endRecording()
{
	if (WavRecordEnabled)
		RecordStop();
}

s32 SPU2freeze(FreezeAction mode, freezeData* data)
{
	pxAssume(data != nullptr);
	if (!data)
	{
		printf("SPU2 savestate null pointer!\n");
		return -1;
	}

	if (mode == FreezeAction::Size)
	{
		data->size = SPU2Savestate::SizeIt();
		return 0;
	}

	pxAssume(mode == FreezeAction::Load || mode == FreezeAction::Save);

	if (data->data == nullptr)
	{
		printf("SPU2 savestate null pointer!\n");
		return -1;
	}

	auto& spud = (SPU2Savestate::DataBlock&)*(data->data);

	switch (mode)
	{
		case FreezeAction::Load:
			return SPU2Savestate::ThawIt(spud);
		case FreezeAction::Save:
			return SPU2Savestate::FreezeIt(spud);

			jNO_DEFAULT;
	}

	// technically unreachable, but kills a warning:
	return 0;
}
