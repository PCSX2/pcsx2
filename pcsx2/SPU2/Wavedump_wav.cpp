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

#include "SPU2/Global.h"
#include "pcsx2/Config.h"
#include "fmt/format.h"

#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/WAVWriter.h"

#include <memory>
#include <mutex>

#ifdef PCSX2_DEVBUILD

namespace WaveDump
{
	static std::unique_ptr<Common::WAVWriter> m_CoreWav[2][CoreSrc_Count];

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
		if (!SPU2::WaveLog())
			return;

		for (uint cidx = 0; cidx < 2; cidx++)
		{
			for (int srcidx = 0; srcidx < CoreSrc_Count; srcidx++)
			{
				m_CoreWav[cidx][srcidx].reset();
				
				std::string wavfilename(Path::Combine(EmuFolders::Logs, fmt::format("spu2x-Core{}d-{}.wav", cidx, m_tbl_CoreOutputTypeNames[srcidx])));
				m_CoreWav[cidx][srcidx] = std::make_unique<Common::WAVWriter>();
				if (!m_CoreWav[cidx][srcidx]->Open(wavfilename.c_str(), SampleRate, 2))
				{
					Console.Error(fmt::format("Failed to open '{}'. Wave Log for this core source disabled.", wavfilename));
					m_CoreWav[cidx][srcidx].reset();
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
				m_CoreWav[cidx][srcidx].reset();
		}
	}

	void WriteCore(uint coreidx, CoreSourceType src, const StereoOut16& sample)
	{
		if (!IsDevBuild)
			return;
		if (m_CoreWav[coreidx][src] != nullptr)
			m_CoreWav[coreidx][src]->WriteFrames(reinterpret_cast<const s16*>(&sample), 1);
	}

	void WriteCore(uint coreidx, CoreSourceType src, s16 left, s16 right)
	{
		WriteCore(coreidx, src, StereoOut16(left, right));
	}
} // namespace WaveDump

#endif // PCSX2_DEVBUILD
