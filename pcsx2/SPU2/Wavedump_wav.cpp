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
#ifdef __POSIX__
#include "WavFile.h"
#else
#include "soundtouch/source/SoundStretch/WavFile.h"
#include "common/StringUtil.h"
#endif

#include <mutex>

static WavOutFile* _new_WavOutFile(const char* destfile)
{
	return new WavOutFile(destfile, 48000, 16, 2);
}

namespace WaveDump
{
	static WavOutFile* m_CoreWav[2][CoreSrc_Count];

	static const char* m_tbl_CoreOutputTypeNames[CoreSrc_Count] =
		{
			"Input",
			"DryVoiceMix",
			"WetVoiceMix",
			"PreReverb",
			"PostReverb",
			"External"};

	void Open()
	{
		if (!IsDevBuild)
			return;
		if (!WaveLog())
			return;

		char wavfilename[256];

		for (uint cidx = 0; cidx < 2; cidx++)
		{
			for (int srcidx = 0; srcidx < CoreSrc_Count; srcidx++)
			{
				safe_delete(m_CoreWav[cidx][srcidx]);
#ifdef __POSIX__
				sprintf(wavfilename, "logs/spu2x-Core%ud-%s.wav",
						cidx, m_tbl_CoreOutputTypeNames[srcidx]);
#else
				sprintf(wavfilename, "logs\\spu2x-Core%ud-%s.wav",
						cidx, m_tbl_CoreOutputTypeNames[srcidx]);
#endif

				try
				{
					m_CoreWav[cidx][srcidx] = _new_WavOutFile(wavfilename);
				}
				catch (std::runtime_error& ex)
				{
					printf("SPU2 > %s.\n\tWave Log for this core source disabled.", ex.what());
					m_CoreWav[cidx][srcidx] = nullptr;
				}
			}
		}
	}

	void Close()
	{
		if (!IsDevBuild)
			return;
		for (uint cidx = 0; cidx < 2; cidx++)
		{
			for (int srcidx = 0; srcidx < CoreSrc_Count; srcidx++)
			{
				safe_delete(m_CoreWav[cidx][srcidx]);
			}
		}
	}

	void WriteCore(uint coreidx, CoreSourceType src, const StereoOut16& sample)
	{
		if (!IsDevBuild)
			return;
		if (m_CoreWav[coreidx][src] != nullptr)
			m_CoreWav[coreidx][src]->write((s16*)&sample, 2);
	}

	void WriteCore(uint coreidx, CoreSourceType src, s16 left, s16 right)
	{
		WriteCore(coreidx, src, StereoOut16(left, right));
	}
} // namespace WaveDump

#include "common/Threading.h"

using namespace Threading;

bool WavRecordEnabled = false;

static WavOutFile* m_wavrecord = nullptr;
static std::mutex WavRecordMutex;

bool RecordStart(const std::string* filename)
{
	try
	{
		std::unique_lock lock(WavRecordMutex);
		safe_delete(m_wavrecord);
		if (filename)
#ifdef _WIN32
			m_wavrecord = new WavOutFile(_wfopen(StringUtil::UTF8StringToWideString(*filename).c_str(), L"wb"), 48000, 16, 2);
#else
			m_wavrecord = new WavOutFile(filename->c_str(), 48000, 16, 2);
#endif
		else
			m_wavrecord = new WavOutFile("audio_recording.wav", 48000, 16, 2);
		WavRecordEnabled = true;
		return true;
	}
	catch (std::runtime_error&)
	{
		m_wavrecord = nullptr; // not needed, but what the heck. :)
		if (filename)
			SysMessage("SPU2 couldn't open file for recording: %s.\nWavfile capture disabled.", filename->c_str());
		else
			SysMessage("SPU2 couldn't open file for recording: audio_recording.wav.\nWavfile capture disabled.");
		return false;
	}
}

void RecordStop()
{
	WavRecordEnabled = false;
	std::unique_lock lock(WavRecordMutex);
	safe_delete(m_wavrecord);
}

void RecordWrite(const StereoOut16& sample)
{
	std::unique_lock lock(WavRecordMutex);
	if (m_wavrecord == nullptr)
		return;
	m_wavrecord->write((s16*)&sample, 2);
}
