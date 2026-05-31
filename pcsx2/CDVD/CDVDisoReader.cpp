// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "IsoFileFormats.h"
#include "CDVD/CDVD.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"

#include <cstring>
#include <array>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

static InputIsoFile iso;

static int pmode, cdtype;

static s32 layer1start = -1;
static bool layer1searched = false;

/* ── .cue multi-track support ── */
static bool cue_parsed = false;
static bool cue_tried = false;
static u8 cue_strack = 1, cue_etrack = 1;
static std::string iso_filename;
struct CueTrack { u32 lba; u8 type; };
static std::vector<CueTrack> cue_tracks;

/* Parse MSF string "MM:SS:FF" → LBA */
static s32 msf_to_lba(const std::string& msf) {
	int m = 0, s = 0, f = 0;
	char sep;
	std::istringstream ss(msf);
	ss >> m >> sep >> s >> sep >> f;
	return (m * 60 + s) * 75 + f;
}

static void ParseCueSheet(const std::string& cue_path) {
	std::ifstream f(cue_path);
	if (!f.is_open()) return;
	
	cue_tracks.clear();
	cue_parsed = false;
	cue_strack = 1;
	
	int current_track = 0;
	u8 current_type = 0;
	u32 current_lba = 0;
	bool have_index1 = false;
	
	std::string line;
	while (std::getline(f, line)) {
		/* Trim */
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
			line.pop_back();
		if (line.empty()) continue;
		
		std::istringstream ss(line);
		std::string cmd;
		ss >> cmd;
		
		if (cmd == "TRACK") {
			/* Flush previous track if any */
			if (current_track > 0 && have_index1) {
				CueTrack t;
				t.lba = current_lba;
				t.type = current_type;
				cue_tracks.push_back(t);
			}
			int tnum; std::string ttype;
			ss >> tnum >> ttype;
			current_track = tnum;
			have_index1 = false;
			if (ttype == "AUDIO")
				current_type = CDVD_AUDIO_TRACK;
			else if (ttype == "MODE2/2352")
				current_type = CDVD_MODE2_TRACK;
			else
				current_type = CDVD_MODE1_TRACK;
		}
		else if (cmd == "INDEX") {
			int idx; std::string pos;
			ss >> idx >> pos;
			if (idx == 1 || (idx == 0 && !have_index1)) {
				current_lba = msf_to_lba(pos);
				have_index1 = true;
			}
		}
	}
	f.close();
	
	/* Flush last track */
	if (current_track > 0 && have_index1) {
		CueTrack t;
		t.lba = current_lba;
		t.type = current_type;
		cue_tracks.push_back(t);
	}
	
	if (!cue_tracks.empty()) {
		cue_parsed = true;
		cue_etrack = (u8)cue_tracks.size();
		Console.WriteLn("CDVD: Parsed %d tracks from .cue", cue_tracks.size());
	}
}

/* Lazy .cue detection: try companion .cue for the current ISO file */
static void TryCueSheet() {
	if (cue_tried) return;
	cue_tried = true;
	
	if (iso_filename.empty()) return;
	
	/* Try .cue */
	std::string cue_path = iso_filename;
	size_t dot = cue_path.find_last_of('.');
	if (dot != std::string::npos) {
		cue_path = cue_path.substr(0, dot) + ".cue";
		ParseCueSheet(cue_path);
	}
	if (!cue_parsed) {
		/* Try .CUE */
		cue_path = iso_filename;
		dot = cue_path.find_last_of('.');
		if (dot != std::string::npos) {
			cue_path = cue_path.substr(0, dot) + ".CUE";
			ParseCueSheet(cue_path);
		}
	}
	
	if (cue_parsed) {
		for (auto& t : cue_tracks) {
			if (t.type == CDVD_AUDIO_TRACK) {
				if (cdtype == CDVD_TYPE_PS2CD) cdtype = CDVD_TYPE_PS2CDDA;
				else if (cdtype == CDVD_TYPE_PSCD) cdtype = CDVD_TYPE_PSCDDA;
				break;
			}
		}
	}
}

/* Extract .bin filename from a .cue sheet's FILE line.
   Returns empty string if not found. */
static std::string ParseCueForBinFile(const std::string& cue_path) {
	std::ifstream f(cue_path);
	if (!f.is_open()) return "";
	
	std::string line, bin_name;
	while (std::getline(f, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
			line.pop_back();
		if (line.empty()) continue;
		
		std::istringstream ss(line);
		std::string cmd;
		ss >> cmd;
		if (cmd == "FILE") {
			/* FILE "filename" BINARY */
			std::string quoted;
			ss >> quoted;
			/* Remove surrounding quotes */
			if (quoted.size() >= 2 && quoted.front() == '"' && quoted.back() == '"')
				bin_name = quoted.substr(1, quoted.size() - 2);
			else
				bin_name = quoted;
			break;
		}
	}
	f.close();
	return bin_name;
}

static void ISOclose()
{
	iso.Close();
	cue_parsed = false;
	cue_tried = false;
	cue_tracks.clear();
	iso_filename.clear();
}

static bool ISOopen(std::string filename, Error* error)
{
	ISOclose(); // just in case

	if (filename.empty())
	{
		Error::SetString(error, "No filename specified.");
		return false;
	}

	/* Save filename before move for .cue detection */
	std::string saved_name = filename;

	/* If user selected a .cue file directly, redirect to the .bin inside it */
	std::string ext;
	size_t dot = saved_name.find_last_of('.');
	if (dot != std::string::npos) {
		ext = saved_name.substr(dot);
		for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
	}
	if (ext == ".cue") {
		std::string bin_in_cue = ParseCueForBinFile(saved_name);
		if (!bin_in_cue.empty()) {
			std::string cue_dir;
			size_t slash = saved_name.find_last_of("/\\");
			if (slash != std::string::npos)
				cue_dir = saved_name.substr(0, slash + 1);
			std::string bin_path = cue_dir + bin_in_cue;
			Console.WriteLn("CDVD: .cue redirect: %s → %s", saved_name.c_str(), bin_path.c_str());
			filename = bin_path;
			saved_name = bin_path;
		}
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

	/* Save filename for lazy .cue detection */
	iso_filename = saved_name;
	cue_tried = false;
	cue_parsed = false;
	cue_tracks.clear();
	TryCueSheet();

	layer1start = -1;
	layer1searched = false;

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

	if (cue_parsed && cue_tracks.size() > 1) {
		/* Find which track this LSN belongs to */
		int track = 1;
		u32 track_start = 0;
		for (int i = (int)cue_tracks.size() - 1; i >= 0; i--) {
			if (lsn >= cue_tracks[i].lba) {
				track = i + 1;
				track_start = cue_tracks[i].lba;
				break;
			}
		}
		u32 rel_lsn = lsn - track_start;
		subq->ctrl = (cue_tracks[track-1].type == CDVD_AUDIO_TRACK) ? 0 : 4;
		subq->adr = 1;
		subq->trackNum = itob(track);
		subq->trackIndex = itob(1);

		lba_to_msf(rel_lsn, &min, &sec, &frm);
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
	TryCueSheet(); /* Lazy .cue detection on first TOC query */

	if (cue_parsed) {
		Buffer->strack = cue_strack;
		Buffer->etrack = cue_etrack;
		return 0;
	}
	Buffer->strack = 1;
	Buffer->etrack = 1;

	return 0;
}

static s32 ISOgetTD(u8 Track, cdvdTD* Buffer)
{
	TryCueSheet(); /* Lazy .cue detection */

	if (cue_parsed) {
		if (Track == 0) {
			Buffer->lsn = iso.GetBlockCount();
			return 0;
		}
		int idx = (int)Track - 1;
		if (idx < 0 || idx >= (int)cue_tracks.size())
			return -1;
		Buffer->type = cue_tracks[idx].type;
		Buffer->lsn = cue_tracks[idx].lba;
		return 0;
	}

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
