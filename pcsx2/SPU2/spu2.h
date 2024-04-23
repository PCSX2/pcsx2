// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "SaveState.h"
#include "IopCounters.h"

#include <memory>

struct Pcsx2Config;

class AudioStream;

namespace SPU2
{
/// PS2/Native Sample Rate.
static constexpr u32 SAMPLE_RATE = 48000;

/// PSX Mode Sample Rate.
static constexpr u32 PSX_SAMPLE_RATE = 44100;

/// Open/close, call at VM startup/shutdown.
bool Open();
void Close();

/// Reset, rebooting VM or going into PSX mode.
void Reset(bool psxmode);

/// Identifies any configuration changes and applies them.
void CheckForConfigChanges(const Pcsx2Config& old_config);

/// Returns the current output volume, irrespective of the configuration.
u32 GetOutputVolume();

/// Directly updates the output volume without going through the configuration.
void SetOutputVolume(u32 volume);

/// Returns the volume that we would reset the output to on startup.
u32 GetResetVolume();

/// Pauses/resumes the output stream.
void SetOutputPaused(bool paused);

/// Clears output buffers in no-sync mode, prevents long delays after fast forwarding.
void OnTargetSpeedChanged();

/// Returns true if we're currently running in PSX mode.
bool IsRunningPSXMode();

/// Returns the current sample rate the SPU2 is operating at.
u32 GetConsoleSampleRate();

/// Tells SPU2 to forward audio packets to GSCapture.
void SetAudioCaptureActive(bool active);
bool IsAudioCaptureActive();
} // namespace SPU2

void SPU2write(u32 mem, u16 value);
u16 SPU2read(u32 mem);

void SPU2async();
s32 SPU2freeze(FreezeAction mode, freezeData* data);

void SPU2readDMA4Mem(u16* pMem, u32 size);
void SPU2writeDMA4Mem(u16* pMem, u32 size);
void SPU2interruptDMA4();
void SPU2interruptDMA7();
void SPU2readDMA7Mem(u16* pMem, u32 size);
void SPU2writeDMA7Mem(u16* pMem, u32 size);

extern u32 lClocks;

extern void TimeUpdate(u32 cClocks);
extern void SPU2_FastWrite(u32 rmem, u16 value);

//#define PCM24_S1_INTERLEAVE
