// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include "common/Pcsx2Defs.h"

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Error;

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

public:
	IOCtlSrc(std::string filename);
	~IOCtlSrc();

	bool Reopen(Error* error);

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

void cdvdStartThread();
void cdvdStopThread();
void cdvdRequestSector(u32 sector, s32 mode);
u8* cdvdGetSector(u32 sector, s32 mode);
s32 cdvdDirectReadSector(u32 sector, s32 mode, u8* buffer);
void cdvdRefreshData();
void cdvdParseTOC();
