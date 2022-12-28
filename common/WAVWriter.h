/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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