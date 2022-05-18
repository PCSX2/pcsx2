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

/*
 *  Original code from DuckStation by Stenznek
 *  Modified by Weirdbeardgame for PCSX2 emu
 */

#pragma once
#include "CDVDcommon.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace Common
{
	class Error;
}

namespace CueParser
{
	enum : s32
	{
		MIN_TRACK_NUMBER = 1,
		MAX_TRACK_NUMBER = 99,
		MIN_INDEX_NUMBER = 0,
		MAX_INDEX_NUMBER = 99
	};

	enum class TrackFlag : u32
	{
		PreEmphasis = (1 << 0),
		CopyPermitted = (1 << 1),
		FourChannelAudio = (1 << 2),
		SerialCopyManagement = (1 << 3),
	};

	class File
	{
	public:
		File();
		~File();

		const cdvdTrack* GetTrack(u32 n) const;

		bool Parse(std::FILE* fp, Common::Error* error);

		std::vector<cdvdTrack> tempTracks;

	private:
		cdvdTrack* GetMutableTrack(u32 n);
		cdvdTD trackDescriptor;

		void SetError(u32 line_number, Common::Error* error, const char* format, ...);

		static std::string_view GetToken(const char*& line);
		static std::optional<u8*> GetMSF(const std::string_view& token);

		bool ParseLine(const char* line, u32 line_number, Common::Error* error);

		bool HandleFileCommand(const char* line, u32 line_number, Common::Error* error);
		bool HandleTrackCommand(const char* line, u32 line_number, Common::Error* error);
		bool HandleIndexCommand(const char* line, u32 line_number, Common::Error* error);
		bool HandlePregapCommand(const char* line, u32 line_number, Common::Error* error);
		bool HandleFlagCommand(const char* line, u32 line_number, Common::Error* error);

		bool CompleteLastTrack(u32 line_number, Common::Error* error);
		bool SetTrackLengths(u32 line_number, Common::Error* error);

		std::optional<std::string> m_current_file;
		std::optional<cdvdTrack> m_current_track;
	};

} // namespace CueParser
