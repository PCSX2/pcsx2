// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "SaveState.h"
#include "IopCounters.h"
#include <mutex>

struct Pcsx2Config;

namespace SPU2
{
/// Open/close, call at VM startup/shutdown.
bool Open();
void Close();

/// Reset, rebooting VM or going into PSX mode.
void Reset(bool psxmode);

/// Identifies any configuration changes and applies them.
void CheckForConfigChanges(const Pcsx2Config& old_config);

/// Returns the current output volume, irrespective of the configuration.
s32 GetOutputVolume();

/// Directly updates the output volume without going through the configuration.
void SetOutputVolume(s32 volume);

/// Pauses/resumes the output stream.
void SetOutputPaused(bool paused);

/// Clears output buffers in no-sync mode, prevents long delays after fast forwarding.
void OnTargetSpeedChanged();

/// Adjusts the premultiplier on the output sample rate. Used for syncing to host refresh rate.
void SetDeviceSampleRateMultiplier(double multiplier);

/// Returns true if we're currently running in PSX mode.
bool IsRunningPSXMode();

/// Returns the current sample rate the SPU2 is operating at.
s32 GetConsoleSampleRate();

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
