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

#include "PrecompiledHeader.h"

#include "CDVD/CDVDcommon.h"
#include "CDVD/IsoHasher.h"
#include "Host.h"

#include "common/Error.h"
#include "common/MD5Digest.h"
#include "common/StringUtil.h"

#include "fmt/core.h"

#include <algorithm>

IsoHasher::IsoHasher() = default;

IsoHasher::~IsoHasher()
{
	Close();
}

std::string_view IsoHasher::GetTrackTypeString(u32 type)
{
	switch (type)
	{
		case CDVD_AUDIO_TRACK:
			return TRANSLATE_SV("CDVD", "Audio");
		case CDVD_MODE1_TRACK:
			return TRANSLATE_SV("CDVD", "Mode 1");
		case CDVD_MODE2_TRACK:
			return TRANSLATE_SV("CDVD", "Mode 2");
		default:
			return TRANSLATE_SV("CDVD", "Unknown");
	}
}

bool IsoHasher::Open(std::string iso_path, Error* error)
{
	Close();

	CDVDsys_SetFile(CDVD_SourceType::Iso, std::move(iso_path));
	CDVDsys_ChangeSource(CDVD_SourceType::Iso);

	m_is_open = DoCDVDopen();
	if (!m_is_open)
	{
		Error::SetString(error, "Failed to open CDVD.");
		return false;
	}

	const s32 type = DoCDVDdetectDiskType();
	switch (type)
	{
		case CDVD_TYPE_PSCD:
		case CDVD_TYPE_PSCDDA:
		case CDVD_TYPE_PS2CD:
		case CDVD_TYPE_PS2CDDA:
			m_is_cd = true;
			break;

		case CDVD_TYPE_PS2DVD:
			m_is_cd = false;
			break;

		default:
			Error::SetString(error, fmt::format("Unknown CDVD disk type {}", type));
			return false;
	}

	cdvdTN tn;
	if (CDVD->getTN(&tn) < 0)
	{
		Error::SetString(error, "Failed to get track count.");
		return false;
	}

	for (u8 track = tn.strack; track <= tn.etrack; track++)
	{
		cdvdTD td, next_td;
		if (CDVD->getTD(track, &td) < 0 || CDVD->getTD((track == tn.etrack) ? 0 : (track + 1), &next_td) < 0)
		{
			Error::SetString(error, fmt::format("Failed to get track range for {}", static_cast<unsigned>(track)));
			return false;
		}

		// sanity check..
		if (next_td.lsn < td.lsn)
		{
			Error::SetString(error,
				fmt::format("Invalid track range for {} ({},{})", static_cast<unsigned>(track), td.lsn, next_td.lsn));
			return false;
		}

		Track strack;
		strack.number = track;
		strack.type = td.type;
		strack.start_lsn = td.lsn;
		strack.sectors = next_td.lsn - td.lsn;
		strack.size = static_cast<u64>(strack.sectors) * (m_is_cd ? 2352 : 2048);
		m_tracks.push_back(std::move(strack));
	}

	return true;
}

void IsoHasher::Close()
{
	if (!m_is_open)
		return;

	DoCDVDclose();
	m_tracks.clear();
	m_is_cd = false;
	m_is_open = false;
}

void IsoHasher::ComputeHashes(ProgressCallback* callback)
{
	callback->SetProgressRange(GetTrackCount());
	callback->SetProgressValue(0);
	callback->SetCancellable(true);

	for (u32 index = 0; index < GetTrackCount(); index++)
	{
		Track& track = m_tracks[index];
		if (!track.hash.empty())
		{
			callback->SetProgressValue(index + 1);
			continue;
		}

		callback->PushState();
		const bool result = ComputeTrackHash(track, callback);
		callback->PopState();

		if (!result)
			break;

		callback->SetProgressValue(index + 1);
		callback->IncrementProgressValue();
	}

	callback->SetProgressValue(GetTrackCount());
}

bool IsoHasher::ComputeTrackHash(Track& track, ProgressCallback* callback)
{
	// use 2048 byte reads for DVDs, otherwise 2352 raw.
	const int read_mode = m_is_cd ? CDVD_MODE_2352 : CDVD_MODE_2048;
	const u32 sector_size = m_is_cd ? 2352 : 2048;
	std::vector<u8> sector_buffer(sector_size);

	const u32 update_interval = std::max<u32>(track.sectors / 100u, 1u);
	callback->SetFormattedStatusText("Computing hash for track %u...", track.number);
	callback->SetProgressRange(track.sectors);

	MD5Digest md5;
	for (u32 i = 0; i < track.sectors; i++)
	{
		if (callback->IsCancelled())
			return false;

		const u32 lsn = track.start_lsn + i;
		if (DoCDVDreadSector(sector_buffer.data(), lsn, read_mode) != 0)
		{
			callback->DisplayFormattedModalError("Read error at LSN %u", lsn);
			return false;
		}

		md5.Update(sector_buffer.data(), sector_size);

		if ((i % update_interval) == 0)
			callback->SetProgressValue(i);
	}

	u8 digest[16];
	md5.Final(digest);
	track.hash =
		fmt::format("{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
			digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8],
			digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);

	callback->SetProgressValue(track.sectors);
	return true;
}
