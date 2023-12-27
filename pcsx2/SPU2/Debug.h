// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Config.h"

namespace SPU2
{
#ifdef PCSX2_DEVBUILD
	__fi static bool MsgToConsole()
	{
		return EmuConfig.SPU2.DebugEnabled;
	}

	__fi static bool MsgKeyOnOff() { return EmuConfig.SPU2.MsgKeyOnOff; }
	__fi static bool MsgVoiceOff() { return EmuConfig.SPU2.MsgVoiceOff; }
	__fi static bool MsgDMA() { return EmuConfig.SPU2.MsgDMA; }
	__fi static bool MsgAutoDMA() { return EmuConfig.SPU2.MsgAutoDMA; }
	__fi static bool MsgOverruns() { return EmuConfig.SPU2.MsgOverruns; }
	__fi static bool MsgCache() { return EmuConfig.SPU2.MsgCache; }

	__fi static bool AccessLog() { return EmuConfig.SPU2.AccessLog; }
	__fi static bool DMALog() { return EmuConfig.SPU2.DMALog; }
	__fi static bool WaveLog() { return EmuConfig.SPU2.WaveLog; }

	__fi static bool CoresDump() { return EmuConfig.SPU2.CoresDump; }
	__fi static bool MemDump() { return EmuConfig.SPU2.MemDump; }
	__fi static bool RegDump() { return EmuConfig.SPU2.RegDump; }
	__fi static bool VisualDebug() { return EmuConfig.SPU2.VisualDebugEnabled; }

	extern void OpenFileLog();
	extern void CloseFileLog();
	extern void FileLog(const char* fmt, ...);
	extern void ConLog(const char* fmt, ...);

	extern void DoFullDump();

	extern void WriteRegLog(const char* action, u32 rmem, u16 value);

#else
	__fi static constexpr bool MsgToConsole() { return false; }

	__fi static constexpr bool MsgKeyOnOff() { return false; }
	__fi static constexpr bool MsgVoiceOff() { return false; }
	__fi static constexpr bool MsgDMA() { return false; }
	__fi static constexpr bool MsgAutoDMA() { return false; }
	__fi static constexpr bool MsgOverruns() { return false; }
	__fi static constexpr bool MsgCache() { return false; }

	__fi static constexpr bool AccessLog() { return false; }
	__fi static constexpr bool DMALog() { return false; }
	__fi static constexpr bool WaveLog() { return false; }

	__fi static constexpr bool CoresDump() { return false; }
	__fi static constexpr bool MemDump() { return false; }
	__fi static constexpr bool RegDump() { return false; }
	__fi static constexpr bool VisualDebug() { return false; }

	__fi static void FileLog(const char* fmt, ...) {}
	__fi static void ConLog(const char* fmt, ...) {}
#endif
} // namespace SPU2

#ifdef PCSX2_DEVBUILD

namespace WaveDump
{
	enum CoreSourceType
	{
		// Core's input stream, usually pulled from ADMA streams.
		CoreSrc_Input = 0,

		// Output of the actual 24 input voices which have dry output enabled.
		CoreSrc_DryVoiceMix,

		// Output of the actual 24 input voices that have wet output enabled.
		CoreSrc_WetVoiceMix,

		// Wet mix including inputs and externals, prior to the application of reverb.
		CoreSrc_PreReverb,

		// Wet mix after reverb has turned it into a pile of garbly gook.
		CoreSrc_PostReverb,

		// Final output of the core.  For core 0, it's the feed into Core1.
		// For Core1, it's the feed into SndOut.
		CoreSrc_External,

		CoreSrc_Count
	};

	extern void Open();
	extern void Close();
	extern void WriteCore(uint coreidx, CoreSourceType src, s16 left, s16 right);
	extern void WriteCore(uint coreidx, CoreSourceType src, const StereoOut16& sample);
} // namespace WaveDump

using WaveDump::CoreSrc_DryVoiceMix;
using WaveDump::CoreSrc_External;
using WaveDump::CoreSrc_Input;
using WaveDump::CoreSrc_PostReverb;
using WaveDump::CoreSrc_PreReverb;
using WaveDump::CoreSrc_WetVoiceMix;

#endif
