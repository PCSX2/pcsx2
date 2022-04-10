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

#ifndef __CDVD_DISC_READER_H__
#define __CDVD_DISC_READER_H__


#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

#include <mutex>
#include <array>

struct track
{
	u32 start_lba;
	u8 type;
};

extern u8 strack;
extern u8 etrack;
extern track tracks[100];

extern int curDiskType;
extern int curTrayStatus;

struct toc_entry
{
	u32 lba;
	u8 track;
	u8 adr : 4;
	u8 control : 4;
};

class IOCtlSrc
{
	IOCtlSrc(const IOCtlSrc&) = delete;
	IOCtlSrc& operator=(const IOCtlSrc&) = delete;

	std::string m_filename;

#if defined(_WIN32)
	HANDLE m_device = INVALID_HANDLE_VALUE;
	mutable std::mutex m_lock;
#else
	int m_device = -1;
#endif

	s32 m_media_type = 0;
	u32 m_sectors = 0;
	u32 m_layer_break = 0;
	std::vector<toc_entry> m_toc;

	bool ReadDVDInfo();
	bool ReadCDInfo();
	bool Reopen();

public:
	IOCtlSrc(std::string filename);
	~IOCtlSrc();

	u32 GetSectorCount() const;
	const std::vector<toc_entry>& ReadTOC() const;
	bool ReadSectors2048(u32 sector, u32 count, u8* buffer) const;
	bool ReadSectors2352(u32 sector, u32 count, u8* buffer) const;
	u32 GetLayerBreakAddress() const;
	s32 GetMediaType() const;
	void SetSpindleSpeed(bool restore_defaults) const;
	bool DiscReady();
};

extern std::unique_ptr<IOCtlSrc> src;

std::vector<std::string> GetOpticalDriveList();
void GetValidDrive(std::string& drive);

extern bool disc_has_changed;
extern bool weAreInNewDiskCB;

extern void (*newDiscCB)();

bool cdvdStartThread();
void cdvdStopThread();
void cdvdRequestSector(u32 sector, s32 mode);
u8* cdvdGetSector(u32 sector, s32 mode);
s32 cdvdDirectReadSector(u32 sector, s32 mode, u8* buffer);
s32 cdvdGetMediaType();
s32 cdvdRefreshData();
void cdvdParseTOC();
#endif /* __CDVD_DISC_READER_H__ */