/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#ifndef __CDVD_H__
#define __CDVD_H__

#define _WIN32_WINNT 0x0600
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#define CDVDdefs
#include <PS2Edefs.h>

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
    IOCtlSrc(const IOCtlSrc &) = delete;
    IOCtlSrc &operator=(const IOCtlSrc &) = delete;

    HANDLE m_device = INVALID_HANDLE_VALUE;
    std::string m_filename;

    s32 m_media_type = 0;
    u32 m_sectors = 0;
    u32 m_layer_break = 0;
    std::vector<toc_entry> m_toc;

    bool ReadDVDInfo();
    bool ReadCDInfo();
    bool Reopen();

public:
    IOCtlSrc(const char *filename);
    ~IOCtlSrc();

    u32 GetSectorCount() const;
    const std::vector<toc_entry> &ReadTOC() const;
    s32 ReadSectors2048(u32 sector, u32 count, char *buffer);
    s32 ReadSectors2352(u32 sector, u32 count, char *buffer);
    u32 GetLayerBreakAddress() const;
    s32 GetMediaType() const;
    void SetSpindleSpeed(bool restore_defaults) const;
    bool DiscReady();
};

extern IOCtlSrc *src;

void configure();

extern char source_drive;

extern HINSTANCE hinst;

void ReadSettings();
void WriteSettings();
void CfgSetSettingsDir(const char *dir);

extern bool cdvd_is_open;
extern bool cdvdKeepAlive_is_open;
extern bool disc_has_changed;
extern bool weAreInNewDiskCB;

extern void (*newDiscCB)();

s32 cdvdStartThread();
void cdvdStopThread();
s32 cdvdRequestSector(u32 sector, s32 mode);
s32 cdvdRequestComplete();
char *cdvdGetSector(s32 sector, s32 mode);
s32 cdvdDirectReadSector(s32 first, s32 mode, char *buffer);
s32 cdvdGetMediaType();
s32 cdvdRefreshData();
void cdvdParseTOC();

#endif /* __CDVD_H__ */
