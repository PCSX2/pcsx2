// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "common/Pcsx2Defs.h"
#include <cstdio>

namespace Common
{
	class WAVWriter
	{
	public:
		WAVWriter();
		~WAVWriter();

		__fi u32 GetSampleRate() const { return m_sample_rate; }
		__fi u32 GetNumChannels() const { return m_num_channels; }
		__fi u32 GetNumFrames() const { return m_num_frames; }
		__fi bool IsOpen() const { return (m_file != nullptr); }

		bool Open(const char* filename, u32 sample_rate, u32 num_channels);
		void Close();

		void WriteFrames(const s16* samples, u32 num_frames);

	private:
		using SampleType = s16;

		bool WriteHeader();

		std::FILE* m_file = nullptr;
		u32 m_sample_rate = 0;
		u32 m_num_channels = 0;
		u32 m_num_frames = 0;
	};
} // namespace Common