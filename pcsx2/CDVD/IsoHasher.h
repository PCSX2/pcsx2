/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
#include "common/ProgressCallback.h"

#include <string>
#include <vector>

class Error;

class IsoHasher
{
public:
	struct Track
	{
		u32 number;
		u32 type;
		u32 start_lsn;
		u32 sectors;
		u64 size;
		std::string hash;
	};

public:
	IsoHasher();
	~IsoHasher();

	static std::string_view GetTrackTypeString(u32 type);

	u32 GetTrackCount() const { return static_cast<u32>(m_tracks.size()); }
	const Track& GetTrack(u32 n) const { return m_tracks.at(n); }
	const std::vector<Track>& GetTracks() const { return m_tracks; }
	bool IsCD() const { return m_is_cd; }

	bool Open(std::string iso_path, Error* error = nullptr);
	void Close();

	void ComputeHashes(ProgressCallback* callback = ProgressCallback::NullProgressCallback);

private:
	bool ComputeTrackHash(Track& track, ProgressCallback* callback);

	std::vector<Track> m_tracks;
	bool m_is_open = false;
	bool m_is_cd = false;
};
