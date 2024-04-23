// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/spu2.h"
#include "SPU2/defs.h"
#include "SPU2/Debug.h"
#include "SPU2/Dma.h"
#include "Host/AudioStream.h"
#include "Host.h"
#include "GS/GSCapture.h"
#include "MTGS.h"
#include "R3000A.h"
#include "VMManager.h"

#include "common/Error.h"

const StereoOut32 StereoOut32::Empty(0, 0);

namespace SPU2
{
	static void CreateOutputStream();
	static void UpdateSampleRate();
	static float GetNominalRate();
	static void InternalReset(bool psxmode);
} // namespace SPU2

u32 lClocks = 0;

static bool s_audio_capture_active = false;
static bool s_psxmode = false;

static std::unique_ptr<AudioStream> s_output_stream;
static std::array<s16, AudioStream::CHUNK_SIZE * 2> s_current_chunk;
static u32 s_current_chunk_pos;

u32 SPU2::GetConsoleSampleRate()
{
	return s_psxmode ? PSX_SAMPLE_RATE : SAMPLE_RATE;
}

// --------------------------------------------------------------------------------------
//  DMA 4/7 Callbacks from Core Emulator
// --------------------------------------------------------------------------------------


void SPU2readDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	SPU2::FileLog("[%10d] SPU2 readDMA4Mem size %x\n", Cycles, size << 1);
	Cores[0].DoDMAread(pMem, size);
}

void SPU2writeDMA4Mem(u16* pMem, u32 size) // size now in 16bit units
{
	TimeUpdate(psxRegs.cycle);

	SPU2::FileLog("[%10d] SPU2 writeDMA4Mem size %x at address %x\n", Cycles, size << 1, Cores[0].TSA);

	Cores[0].DoDMAwrite(pMem, size);
}

void SPU2interruptDMA4()
{
	SPU2::FileLog("[%10d] SPU2 interruptDMA4\n", Cycles);
	if (Cores[0].DmaMode)
		Cores[0].Regs.STATX |= 0x80;
	Cores[0].Regs.STATX &= ~0x400;
	Cores[0].TSA = Cores[0].ActiveTSA;
}

void SPU2interruptDMA7()
{
	SPU2::FileLog("[%10d] SPU2 interruptDMA7\n", Cycles);
	if (Cores[1].DmaMode)
		Cores[1].Regs.STATX |= 0x80;
	Cores[1].Regs.STATX &= ~0x400;
	Cores[1].TSA = Cores[1].ActiveTSA;
}

void SPU2readDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	SPU2::FileLog("[%10d] SPU2 readDMA7Mem size %x\n", Cycles, size << 1);
	Cores[1].DoDMAread(pMem, size);
}

void SPU2writeDMA7Mem(u16* pMem, u32 size)
{
	TimeUpdate(psxRegs.cycle);

	SPU2::FileLog("[%10d] SPU2 writeDMA7Mem size %x at address %x\n", Cycles, size << 1, Cores[1].TSA);

	Cores[1].DoDMAwrite(pMem, size);
}

void SPU2::CreateOutputStream()
{
	// Persist volume through stream recreates.
	const u32 volume = s_output_stream ? s_output_stream->GetOutputVolume() : GetResetVolume();
	const u32 sample_rate = GetConsoleSampleRate();
	s_output_stream.reset();

	Error error;
	s_output_stream = AudioStream::CreateStream(EmuConfig.SPU2.Backend, sample_rate, EmuConfig.SPU2.StreamParameters,
		EmuConfig.SPU2.DriverName.c_str(), EmuConfig.SPU2.DeviceName.c_str(), EmuConfig.SPU2.IsTimeStretchEnabled(), &error);
	if (!s_output_stream)
	{
		Host::ReportErrorAsync("Error",
			fmt::format("Failed to create or configure audio stream, falling back to null output. The error was:\n{}",
				error.GetDescription()));

		s_output_stream = AudioStream::CreateNullStream(sample_rate, EmuConfig.SPU2.StreamParameters.buffer_ms);
	}

	s_output_stream->SetOutputVolume(volume);
	s_output_stream->SetNominalRate(GetNominalRate());
	s_output_stream->SetPaused(VMManager::GetState() == VMState::Paused);
}

void SPU2::UpdateSampleRate()
{
	if (s_output_stream && s_output_stream->GetSampleRate() == GetConsoleSampleRate())
		return;

	CreateOutputStream();

	// Can't be capturing when the sample rate changes.
	if (IsAudioCaptureActive())
	{
		MTGS::RunOnGSThread(&GSEndCapture);
		MTGS::WaitGS(false, false, false);
	}
}

u32 SPU2::GetOutputVolume()
{
	return s_output_stream->GetOutputVolume();
}

void SPU2::SetOutputVolume(u32 volume)
{
	s_output_stream->SetOutputVolume(volume);
}

u32 SPU2::GetResetVolume()
{
	return EmuConfig.SPU2.OutputMuted ? 0 :
										((VMManager::GetTargetSpeed() != 1.0f) ?
												EmuConfig.SPU2.FastForwardVolume :
												EmuConfig.SPU2.OutputVolume);
}

float SPU2::GetNominalRate()
{
	// Adjust nominal rate when syncing to host.
	return VMManager::IsTargetSpeedAdjustedToHost() ? VMManager::GetTargetSpeed() : 1.0f;
}

void SPU2::SetOutputPaused(bool paused)
{
	s_output_stream->SetPaused(paused);
}

void SPU2::SetAudioCaptureActive(bool active)
{
	s_audio_capture_active = active;
}

bool SPU2::IsAudioCaptureActive()
{
	return s_audio_capture_active;
}

void SPU2::InternalReset(bool psxmode)
{
	s_current_chunk_pos = 0;
	s_psxmode = psxmode;
	if (!s_psxmode)
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

void SPU2::Reset(bool psxmode)
{
	InternalReset(psxmode);
	UpdateSampleRate();
}

void SPU2::OnTargetSpeedChanged()
{
	if (!s_output_stream)
		return;

	if (!s_output_stream->IsStretchEnabled())
	{
		s_output_stream->EmptyBuffer();
		s_current_chunk_pos = 0;
	}

	s_output_stream->SetNominalRate(GetNominalRate());

	if (EmuConfig.SPU2.OutputVolume != EmuConfig.SPU2.FastForwardVolume && !EmuConfig.SPU2.OutputMuted)
		s_output_stream->SetOutputVolume(GetResetVolume());
}

bool SPU2::Open()
{
#ifdef PCSX2_DEVBUILD
	if (SPU2::AccessLog())
		SPU2::OpenFileLog();
#endif

#ifdef PCSX2_DEVBUILD
	DMALogOpen();

	FileLog("[%10d] SPU2 Open\n", Cycles);
#endif

	lClocks = psxRegs.cycle;

	InternalReset(false);

	CreateOutputStream();
#ifdef PCSX2_DEVBUILD
	WaveDump::Open();
#endif

	return true;
}

void SPU2::Close()
{
	FileLog("[%10d] SPU2 Close\n", Cycles);

	s_output_stream.reset();

#ifdef PCSX2_DEVBUILD
	WaveDump::Close();
	DMALogClose();

	DoFullDump();
	CloseFileLog();
#endif
}

bool SPU2::IsRunningPSXMode()
{
	return s_psxmode;
}

void SPU2::CheckForConfigChanges(const Pcsx2Config& old_config)
{
	const Pcsx2Config::SPU2Options& opts = EmuConfig.SPU2;
	const Pcsx2Config::SPU2Options& oldopts = old_config.SPU2;

	// No need to reinit for volume change.
	if ((opts.OutputVolume != oldopts.OutputVolume && VMManager::GetTargetSpeed() == 1.0f) ||
		(opts.FastForwardVolume != oldopts.FastForwardVolume && VMManager::GetTargetSpeed() != 1.0f) ||
		opts.OutputMuted != oldopts.OutputMuted)
	{
		SetOutputVolume(GetResetVolume());
	}

	// Things which require re-initialzing the output.
	if (opts.Backend != oldopts.Backend ||
		opts.StreamParameters != oldopts.StreamParameters ||
		opts.DriverName != oldopts.DriverName ||
		opts.DeviceName != oldopts.DeviceName)
	{
		CreateOutputStream();
	}
	else if (opts.IsTimeStretchEnabled() != oldopts.IsTimeStretchEnabled())
	{
		s_output_stream->SetStretchEnabled(opts.IsTimeStretchEnabled());
	}

#ifdef PCSX2_DEVBUILD
	// AccessLog controls file output.
	if (opts.AccessLog != oldopts.AccessLog)
	{
		if (AccessLog())
			OpenFileLog();
		else
			CloseFileLog();
	}
#endif
}

void SPU2async()
{
	TimeUpdate(psxRegs.cycle);
}

u16 SPU2read(u32 rmem)
{
	u16 ret = 0xDEAD;
	u32 core = 0;
	const u32 mem = rmem & 0xFFFF;
	u32 omem = mem;

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
			if (SPU2::MsgToConsole())
				SPU2::ConLog("* SPU2: Read from reg>=0x800: %x value %x\n", mem, ret);
		}
		else
		{
			ret = *(regtable[(mem >> 1)]);
#ifdef PCSX2_DEVBUILD
			//FileLog("[%10d] SPU2 read mem %x (core %d, register %x): %x\n",Cycles, mem, core, (omem & 0x7ff), ret);
			SPU2::WriteRegLog("read", rmem, ret);
#endif
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
#ifdef PCSX2_DEVBUILD
		SPU2::WriteRegLog("write", rmem, value);
#endif
		SPU2_FastWrite(rmem, value);
	}
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

__forceinline void spu2Output(StereoOut32 out)
{
	// Final clamp, take care not to exceed 16 bits from here on
	s_current_chunk[s_current_chunk_pos++] = static_cast<s16>(clamp_mix(out.Left));
	s_current_chunk[s_current_chunk_pos++] = static_cast<s16>(clamp_mix(out.Right));
	if (s_current_chunk_pos == s_current_chunk.size())
	{
		s_current_chunk_pos = 0;

		s_output_stream->WriteChunk(s_current_chunk.data());

		if (SPU2::IsAudioCaptureActive()) [[unlikely]]
			GSCapture::DeliverAudioPacket(s_current_chunk.data());
	}
}
