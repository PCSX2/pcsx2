// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/Debug.h"
#include "SPU2/spu2.h"
#include "pcsx2/Config.h"
#include "fmt/format.h"

#include "common/Console.h"
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
		if (!SPU2::WaveLog())
			return;

		for (uint cidx = 0; cidx < 2; cidx++)
		{
			for (int srcidx = 0; srcidx < CoreSrc_Count; srcidx++)
			{
				m_CoreWav[cidx][srcidx].reset();

				std::string wavfilename(Path::Combine(EmuFolders::Logs, fmt::format("spu2x-Core{}d-{}.wav", cidx, m_tbl_CoreOutputTypeNames[srcidx])));
				m_CoreWav[cidx][srcidx] = std::make_unique<Common::WAVWriter>();
				if (!m_CoreWav[cidx][srcidx]->Open(wavfilename.c_str(), SPU2::GetConsoleSampleRate(), 2))
				{
					Console.Error(fmt::format("Failed to open '{}'. Wave Log for this core source disabled.", wavfilename));
					m_CoreWav[cidx][srcidx].reset();
				}
			}
		}
	}

	void Close()
	{
		for (uint cidx = 0; cidx < 2; cidx++)
		{
			for (int srcidx = 0; srcidx < CoreSrc_Count; srcidx++)
				m_CoreWav[cidx][srcidx].reset();
		}
	}

	void WriteCore(uint coreidx, CoreSourceType src, const StereoOut32& sample)
	{
		if (!m_CoreWav[coreidx][src])
			return;

		const s16 frame[] = {static_cast<s16>(clamp_mix(sample.Left)), static_cast<s16>(clamp_mix(sample.Right))};
		m_CoreWav[coreidx][src]->WriteFrames(frame, 1);
	}

	void WriteCore(uint coreidx, CoreSourceType src, s16 left, s16 right)
	{
		if (!m_CoreWav[coreidx][src])
			return;

		const s16 frame[] = {left, right};
		m_CoreWav[coreidx][src]->WriteFrames(frame, 1);
	}
} // namespace WaveDump

#endif // PCSX2_DEVBUILD
