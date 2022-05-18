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

#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

extern FILE* spu2Log;

extern void FileLog(const char* fmt, ...);
extern void ConLog(const char* fmt, ...);

extern void DoFullDump();

extern FILE* OpenBinaryLog(const char* logfile);
extern FILE* OpenLog(const char* logfile);
extern FILE* OpenDump(const char* logfile);

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

#endif // DEBUG_H_INCLUDED //
