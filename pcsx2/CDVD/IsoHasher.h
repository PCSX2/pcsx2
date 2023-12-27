// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
