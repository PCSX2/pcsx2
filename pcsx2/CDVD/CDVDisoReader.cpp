// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "IsoFileFormats.h"
#include "CDVD/CDVD.h"
#include "CDVD/IsoReader.h"
#include "DebugTools/SimpSkateTrace.h"
#include "R3000A.h"
#include "R5900.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/StringUtil.h"

#include <cstring>
#include <array>
#include <algorithm>
#include <string>
#include <vector>

static InputIsoFile iso;

static int pmode, cdtype;

static s32 layer1start = -1;
static bool layer1searched = false;

struct IsoMappedFile final
{
	std::string path;
	u32 start_lsn = 0;
	u32 end_lsn = 0;
	u32 size = 0;
};

struct PendingIsoReadRun final
{
	bool valid = false;
	u32 start_lsn = 0;
	u32 sector_count = 0;
	int mode = 0;
	u32 ee_pc = 0;
	u32 iop_pc = 0;
	std::string owner_path;
	u32 owner_offset = 0;
	u32 owner_size = 0;
};

static std::vector<IsoMappedFile> s_iso_file_map;
static PendingIsoReadRun s_pending_iso_read;

static void FlushPendingIsoRead()
{
	if (!s_pending_iso_read.valid)
		return;

	SimpSkateTrace::OnIsoReadRun(
		s_pending_iso_read.start_lsn,
		s_pending_iso_read.sector_count,
		s_pending_iso_read.mode,
		s_pending_iso_read.ee_pc,
		s_pending_iso_read.iop_pc,
		s_pending_iso_read.owner_path,
		s_pending_iso_read.owner_offset,
		s_pending_iso_read.owner_size);
	s_pending_iso_read = {};
}

static const IsoMappedFile* FindOwnerForLsn(const u32 lsn)
{
	if (s_iso_file_map.empty())
		return nullptr;

	const auto it = std::upper_bound(
		s_iso_file_map.begin(),
		s_iso_file_map.end(),
		lsn,
		[](const u32 needle, const IsoMappedFile& file) { return needle < file.start_lsn; });
	if (it == s_iso_file_map.begin())
		return nullptr;

	const IsoMappedFile& candidate = *(it - 1);
	if (lsn >= candidate.start_lsn && lsn < candidate.end_lsn)
		return &candidate;

	return nullptr;
}

static void BuildIsoFileMap()
{
	s_iso_file_map.clear();

	if (!SimpSkateTrace::IsEnabled())
		return;

	IsoReader reader;
	Error open_error;
	if (!reader.Open(&open_error))
	{
		Console.Warning("SimpSkateTrace: failed to build ISO map (%s)", open_error.GetDescription().c_str());
		return;
	}

	std::vector<std::string> pending_dirs;
	pending_dirs.emplace_back("");

	while (!pending_dirs.empty())
	{
		const std::string current_dir = std::move(pending_dirs.back());
		pending_dirs.pop_back();

		Error list_error;
		const std::vector<std::string> entries = reader.GetFilesInDirectory(current_dir, &list_error);
		if (list_error.IsValid())
		{
			Console.Warning("SimpSkateTrace: directory list failed for '%s' (%s)",
				current_dir.c_str(), list_error.GetDescription().c_str());
			continue;
		}

		for (const std::string& entry : entries)
		{
			Error locate_error;
			const auto de = reader.LocateFile(entry, &locate_error);
			if (!de.has_value())
				continue;

			if ((de->flags & IsoReader::ISODirectoryEntryFlag_Directory) != 0)
			{
				pending_dirs.push_back(entry);
				continue;
			}

			const u32 sector_count = (de->length_le + (IsoReader::SECTOR_SIZE - 1)) / IsoReader::SECTOR_SIZE;
			if (sector_count == 0)
				continue;

			IsoMappedFile file;
			file.path = std::string(IsoReader::RemoveVersionIdentifierFromPath(entry));
			file.start_lsn = de->location_le;
			file.end_lsn = de->location_le + sector_count;
			file.size = de->length_le;
			s_iso_file_map.push_back(std::move(file));
		}
	}

	std::sort(
		s_iso_file_map.begin(),
		s_iso_file_map.end(),
		[](const IsoMappedFile& lhs, const IsoMappedFile& rhs) { return lhs.start_lsn < rhs.start_lsn; });

	bool has_assets_blt = false;
	u32 assets_blt_lsn = 0;
	u32 assets_blt_size = 0;
	for (const IsoMappedFile& file : s_iso_file_map)
	{
		const std::string::size_type slash = file.path.find_last_of('/');
		const std::string_view basename = (slash == std::string::npos) ? std::string_view(file.path) :
			std::string_view(file.path).substr(slash + 1);
		if (StringUtil::compareNoCase(basename, "ASSETS.BLT"))
		{
			has_assets_blt = true;
			assets_blt_lsn = file.start_lsn;
			assets_blt_size = file.size;
			break;
		}
	}

	SimpSkateTrace::OnIsoMapBuilt(
		s_iso_file_map.size(),
		has_assets_blt,
		assets_blt_lsn,
		assets_blt_size);
}

static void TrackIsoRead(const u32 lsn, const int mode)
{
	if (!SimpSkateTrace::IsEnabled())
		return;

	const IsoMappedFile* owner = FindOwnerForLsn(lsn);
	const std::string owner_path = owner ? owner->path : std::string();
	const u32 owner_offset = owner ? ((lsn - owner->start_lsn) * IsoReader::SECTOR_SIZE) : 0;
	const u32 owner_size = owner ? owner->size : 0;
	const u32 ee_pc = cpuRegs.pc;
	const u32 iop_pc = psxRegs.pc;

	if (s_pending_iso_read.valid &&
		s_pending_iso_read.mode == mode &&
		s_pending_iso_read.ee_pc == ee_pc &&
		s_pending_iso_read.iop_pc == iop_pc &&
		s_pending_iso_read.owner_path == owner_path &&
		s_pending_iso_read.owner_size == owner_size &&
		lsn == (s_pending_iso_read.start_lsn + s_pending_iso_read.sector_count))
	{
		s_pending_iso_read.sector_count++;
		return;
	}

	FlushPendingIsoRead();
	s_pending_iso_read.valid = true;
	s_pending_iso_read.start_lsn = lsn;
	s_pending_iso_read.sector_count = 1;
	s_pending_iso_read.mode = mode;
	s_pending_iso_read.ee_pc = ee_pc;
	s_pending_iso_read.iop_pc = iop_pc;
	s_pending_iso_read.owner_path = owner_path;
	s_pending_iso_read.owner_offset = owner_offset;
	s_pending_iso_read.owner_size = owner_size;
}

static void ISOclose()
{
	FlushPendingIsoRead();
	s_iso_file_map.clear();

	iso.Close();
}

static bool ISOopen(std::string filename, Error* error)
{
	ISOclose(); // just in case
	SimpSkateTrace::OnIsoOpen(filename);

	if (filename.empty())
	{
		Error::SetString(error, "No filename specified.");
		return false;
	}

	if (!iso.Open(std::move(filename), error))
		return false;

	switch (iso.GetType())
	{
		case ISOTYPE_DVD:
			cdtype = CDVD_TYPE_PS2DVD;
			break;
		case ISOTYPE_AUDIO:
			cdtype = CDVD_TYPE_CDDA;
			break;
		default:
			cdtype = CDVD_TYPE_PS2CD;
			break;
	}

	layer1start = -1;
	layer1searched = false;
	BuildIsoFileMap();

	return true;
}

static bool ISOprecache(ProgressCallback* progress, Error* error)
{
	return iso.Precache(progress, error);
}

static s32 ISOreadSubQ(u32 lsn, cdvdSubQ* subq)
{
	// fake it
	u8 min, sec, frm;
	subq->ctrl = 4;
	subq->adr = 1;
	subq->trackNum = itob(1);
	subq->trackIndex = itob(1);

	lba_to_msf(lsn, &min, &sec, &frm);
	subq->trackM = itob(min);
	subq->trackS = itob(sec);
	subq->trackF = itob(frm);

	subq->pad = 0;

	lba_to_msf(lsn + (2 * 75), &min, &sec, &frm);
	subq->discM = itob(min);
	subq->discS = itob(sec);
	subq->discF = itob(frm);

	return 0;
}

static s32 ISOgetTN(cdvdTN* Buffer)
{
	Buffer->strack = 1;
	Buffer->etrack = 1;

	return 0;
}

static s32 ISOgetTD(u8 Track, cdvdTD* Buffer)
{
	if (Track == 0)
	{
		Buffer->lsn = iso.GetBlockCount();
	}
	else
	{
		Buffer->type = CDVD_MODE1_TRACK;
		Buffer->lsn = 0;
	}

	return 0;
}

static bool testForPrimaryVolumeDescriptor(const std::array<u8, CD_FRAMESIZE_RAW>& buffer)
{
	const std::array<u8, 6> identifier = {1, 'C', 'D', '0', '0', '1'};

	return std::equal(identifier.begin(), identifier.end(), buffer.begin() + iso.GetBlockOffset());
}

static void FindLayer1Start()
{
	if (layer1searched)
		return;

	layer1searched = true;

	std::array<u8, CD_FRAMESIZE_RAW> buffer;

	// The ISO9660 primary volume descriptor for layer 0 is located at sector 16
	iso.ReadSync(buffer.data(), 16);
	if (!testForPrimaryVolumeDescriptor(buffer))
	{
		Console.Error("isoFile: Invalid layer0 Primary Volume Descriptor");
		return;
	}

	// The volume space size (sector count) is located at bytes 80-87 - 80-83
	// is the little endian size, 84-87 is the big endian size.
	const int offset = iso.GetBlockOffset();
	uint blockresult = buffer[offset + 80] + (buffer[offset + 81] << 8) + (buffer[offset + 82] << 16) + (buffer[offset + 83] << 24);

	// If the ISO sector count is larger than the volume size, then we should
	// have a dual layer DVD. Layer 1 is on a different volume.
	if (blockresult < iso.GetBlockCount())
	{
		// The layer 1 start LSN contains the primary volume descriptor for layer 1.
		// The check might be a bit unnecessary though.
		if (iso.ReadSync(buffer.data(), blockresult) == -1)
			return;

		if (!testForPrimaryVolumeDescriptor(buffer))
		{
			Console.Error("isoFile: Invalid layer1 Primary Volume Descriptor");
			return;
		}
		layer1start = blockresult;
		Console.WriteLn(Color_Blue, "isoFile: second layer found at sector 0x%08x", layer1start);
	}
}

// Should return 0 if no error occurred, or -1 if layer detection FAILED.
static s32 ISOgetDualInfo(s32* dualType, u32* _layer1start)
{
	FindLayer1Start();

	if (layer1start < 0)
	{
		*dualType = 0;
		*_layer1start = iso.GetBlockCount();
	}
	else
	{
		*dualType = 1;
		*_layer1start = layer1start;
	}
	return 0;
}

static s32 ISOgetDiskType()
{
	return cdtype;
}

static s32 ISOgetTOC(void* toc)
{
	u8 type = ISOgetDiskType();
	u8* tocBuff = (u8*)toc;

	//CDVD_LOG("CDVDgetTOC\n");

	if (type == CDVD_TYPE_DVDV || type == CDVD_TYPE_PS2DVD)
	{
		// get dvd structure format
		// scsi command 0x43
		memset(tocBuff, 0, 2048);

		FindLayer1Start();

		if (layer1start < 0)
		{
			// Single Layer - Values are fixed.
			tocBuff[0] = 0x04;
			tocBuff[1] = 0x02;
			tocBuff[2] = 0xF2;
			tocBuff[3] = 0x00;
			tocBuff[4] = 0x86;
			tocBuff[5] = 0x72;

			// These values are fixed on all discs, except position 14 which is the OTP/PTP flags which are 0 in single layer.
			tocBuff[12] = 0x01;
			tocBuff[13] = 0x02;
			tocBuff[14] = 0x01;
			tocBuff[15] = 0x00;

			// Values are fixed.
			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			cdvdTD trackInfo;
			// Get the max LSN for the track
			if (ISOgetTD(0, &trackInfo) == -1)
				trackInfo.lsn = 0;

			// Max LSN in the TOC is calculated as the blocks + 0x30000, then - 1.
			// same as layer 1 start.
			const s32 maxlsn = trackInfo.lsn + (0x30000 - 1);
			tocBuff[20] = maxlsn >> 24;
			tocBuff[21] = (maxlsn >> 16) & 0xff;
			tocBuff[22] = (maxlsn >> 8) & 0xff;
			tocBuff[23] = (maxlsn >> 0) & 0xff;
			return 0;
		}
		else
		{
			// Dual sided - Values are fixed.
			tocBuff[0] = 0x24;
			tocBuff[1] = 0x02;
			tocBuff[2] = 0xF2;
			tocBuff[3] = 0x00;
			tocBuff[4] = 0x41;
			tocBuff[5] = 0x95;

			// These values are fixed on all discs, except position 14 which is the OTP/PTP flags.
			tocBuff[12] = 0x01;
			tocBuff[13] = 0x02;
			tocBuff[14] = 0x21; // PTP
			tocBuff[15] = 0x10;

			// Values are fixed.
			tocBuff[16] = 0x00;
			tocBuff[17] = 0x03;
			tocBuff[18] = 0x00;
			tocBuff[19] = 0x00;

			const s32 l1s = layer1start + 0x30000 - 1;
			tocBuff[20] = (l1s >> 24);
			tocBuff[21] = (l1s >> 16) & 0xff;
			tocBuff[22] = (l1s >> 8) & 0xff;
			tocBuff[23] = (l1s >> 0) & 0xff;
		}
	}
	else if ((type == CDVD_TYPE_CDDA) || (type == CDVD_TYPE_PS2CDDA) ||
			 (type == CDVD_TYPE_PS2CD) || (type == CDVD_TYPE_PSCDDA) || (type == CDVD_TYPE_PSCD))
	{
		// cd toc
		// (could be replaced by 1 command that reads the full toc)
		u8 min, sec, frm;
		s32 i, err;
		cdvdTN diskInfo;
		cdvdTD trackInfo;
		memset(tocBuff, 0, 1024);
		if (ISOgetTN(&diskInfo) == -1)
		{
			diskInfo.etrack = 0;
			diskInfo.strack = 1;
		}
		if (ISOgetTD(0, &trackInfo) == -1)
			trackInfo.lsn = 0;

		tocBuff[0] = 0x41;
		tocBuff[1] = 0x00;

		//Number of FirstTrack
		tocBuff[2] = 0xA0;
		tocBuff[7] = itob(diskInfo.strack);

		//Number of LastTrack
		tocBuff[12] = 0xA1;
		tocBuff[17] = itob(diskInfo.etrack);

		//DiskLength
		lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
		tocBuff[22] = 0xA2;
		tocBuff[27] = itob(min);
		tocBuff[28] = itob(sec);
		tocBuff[29] = itob(frm);

		// TODO: When cue support is added, this will need to account for pregap.
		for (i = diskInfo.strack; i <= diskInfo.etrack; i++)
		{
			err = ISOgetTD(i, &trackInfo);
			lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
			tocBuff[i * 10 + 30] = trackInfo.type;
			tocBuff[i * 10 + 32] = err == -1 ? 0 : itob(i); //number
			tocBuff[i * 10 + 37] = itob(min);
			tocBuff[i * 10 + 38] = itob(sec);
			tocBuff[i * 10 + 39] = itob(frm);
		}
	}
	else
		return -1;

	return 0;
}

static s32 ISOreadSector(u8* tempbuffer, u32 lsn, int mode)
{
	static u8 cdbuffer[CD_FRAMESIZE_RAW] = {0};

	int _lsn = lsn;

	if (_lsn < 0)
		lsn = iso.GetBlockCount() + _lsn;
	if (lsn >= iso.GetBlockCount())
		return -1;

	if (mode == CDVD_MODE_2352)
	{
		iso.ReadSync(tempbuffer, lsn);
		return 0;
	}

	iso.ReadSync(cdbuffer, lsn);


	u8* pbuffer = cdbuffer;
	int psize = 0;

	switch (mode)
	{
			//case CDVD_MODE_2352:
			// Unreachable due to shortcut above.
			//	pxAssume(false);
			//	break;

		case CDVD_MODE_2340:
			pbuffer += 12;
			psize = 2340;
			break;
		case CDVD_MODE_2328:
			pbuffer += 24;
			psize = 2328;
			break;
		case CDVD_MODE_2048:
			pbuffer += 24;
			psize = 2048;
			break;

			jNO_DEFAULT
	}

	memcpy(tempbuffer, pbuffer, psize);

	return 0;
}

static s32 ISOreadTrack(u32 lsn, int mode)
{
	int _lsn = lsn;

	if (_lsn < 0)
		lsn = iso.GetBlockCount() + _lsn;

	TrackIsoRead(lsn, mode);
	iso.BeginRead2(lsn);

	pmode = mode;

	return 0;
}

static s32 ISOgetBuffer(u8* buffer)
{
	return iso.FinishRead3(buffer, pmode);
}

static s32 ISOgetTrayStatus()
{
	return CDVD_TRAY_CLOSE;
}

static s32 ISOctrlTrayOpen()
{
	return 0;
}
static s32 ISOctrlTrayClose()
{
	return 0;
}

static void ISOnewDiskCB(void (* /* callback */)())
{
}

const CDVD_API CDVDapi_Iso =
	{
		ISOclose,

		ISOopen,
		ISOprecache,
		ISOreadTrack,
		ISOgetBuffer,
		ISOreadSubQ,
		ISOgetTN,
		ISOgetTD,
		ISOgetTOC,
		ISOgetDiskType,
		ISOgetTrayStatus,
		ISOctrlTrayOpen,
		ISOctrlTrayClose,
		ISOnewDiskCB,

		ISOreadSector,
		ISOgetDualInfo,
};
