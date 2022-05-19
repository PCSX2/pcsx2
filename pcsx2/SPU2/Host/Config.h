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

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <string>

extern bool DebugEnabled;

extern bool _MsgToConsole;
extern bool _MsgKeyOnOff;
extern bool _MsgVoiceOff;
extern bool _MsgDMA;
extern bool _MsgAutoDMA;
extern bool _MsgOverruns;
extern bool _MsgCache;

extern bool _AccessLog;
extern bool _DMALog;
extern bool _WaveLog;

extern bool _CoresDump;
extern bool _MemDump;
extern bool _RegDump;
extern bool _visual_debug_enabled;

/*static __forceinline bool MsgToConsole() { return _MsgToConsole & DebugEnabled; }

static __forceinline bool MsgKeyOnOff() { return _MsgKeyOnOff & MsgToConsole(); }
static __forceinline bool MsgVoiceOff() { return _MsgVoiceOff & MsgToConsole(); }
static __forceinline bool MsgDMA() { return _MsgDMA & MsgToConsole(); }
static __forceinline bool MsgAutoDMA() { return _MsgAutoDMA & MsgDMA(); }
static __forceinline bool MsgOverruns() { return _MsgOverruns & MsgToConsole(); }
static __forceinline bool MsgCache() { return _MsgCache & MsgToConsole(); }

static __forceinline bool AccessLog() { return _AccessLog & DebugEnabled; }
static __forceinline bool DMALog() { return _DMALog & DebugEnabled; }
static __forceinline bool WaveLog() { return _WaveLog & DebugEnabled; }

static __forceinline bool CoresDump() { return _CoresDump & DebugEnabled; }
static __forceinline bool MemDump() { return _MemDump & DebugEnabled; }
static __forceinline bool RegDump() { return _RegDump & DebugEnabled; }*/


//extern wchar_t AccessLogFileName[255];
//extern wchar_t WaveLogFileName[255];
//extern wchar_t DMA4LogFileName[255];
//extern wchar_t DMA7LogFileName[255];
//extern wchar_t CoresDumpFileName[255];
//extern wchar_t MemDumpFileName[255];
//extern wchar_t RegDumpFileName[255];

extern int Interpolation;
extern float FinalVolume;

extern int AutoDMAPlayRate[2];

extern u32 OutputModule;
extern int SndOutLatencyMS;

extern int SynchMode;

#ifdef PCSX2_DEVBUILD
const int LATENCY_MAX = 3000;
#else
const int LATENCY_MAX = 750;
#endif

const int LATENCY_MIN = 3;
const int LATENCY_MIN_TIMESTRETCH = 15;

namespace SoundtouchCfg
{
	extern const int SequenceLen_Min;
	extern const int SequenceLen_Max;

	extern const int SeekWindow_Min;
	extern const int SeekWindow_Max;

	extern const int Overlap_Min;
	extern const int Overlap_Max;

	extern int SequenceLenMS;
	extern int SeekWindowMS;
	extern int OverlapMS;

	void ReadSettings();
}; // namespace SoundtouchCfg

void ReadSettings();

#endif // CONFIG_H_INCLUDED
