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

#include "CDVD.h"
#include <cstdio>
#include <atomic>
#include <condition_variable>
#include <thread>
#include "svnrev.h"

Settings g_settings;
static std::string s_config_file{"inis/cdvdGigaherz.ini"};

void (*newDiscCB)();

static std::mutex s_keepalive_lock;
static std::condition_variable s_keepalive_cv;
static std::thread s_keepalive_thread;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// State Information                                                         //

u8 strack;
u8 etrack;
track tracks[100];

int curDiskType;
int curTrayStatus;

int csector;
int cmode;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Plugin Interface                                                          //

#ifndef PCSX2_DEBUG
static std::string s_libname("cdvdGigaherz " + std::to_string(SVN_REV));
#else
static std::string s_libname("cdvdGigaherz Debug " + std::to_string(SVN_REV));
#endif

const unsigned char version = PS2E_CDVD_VERSION;
const unsigned char revision = 0;
const unsigned char build = 11;

EXPORT const char *CALLBACK PS2EgetLibName()
{
    return s_libname.c_str();
}

EXPORT u32 CALLBACK PS2EgetLibType()
{
    return PS2E_LT_CDVD;
}

EXPORT u32 CALLBACK PS2EgetLibVersion2(u32 type)
{
    return (version << 16) | (revision << 8) | build;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Utility Functions                                                         //

inline u8 dec_to_bcd(u8 dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

inline void lsn_to_msf(u8 *minute, u8 *second, u8 *frame, u32 lsn)
{
    *frame = dec_to_bcd(lsn % 75);
    lsn /= 75;
    *second = dec_to_bcd(lsn % 60);
    lsn /= 60;
    *minute = dec_to_bcd(lsn % 100);
}

void ReadSettings()
{
    g_settings.Load(s_config_file);
}

void WriteSettings()
{
    g_settings.Save(s_config_file);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CDVD processing functions                                                 //

std::atomic<bool> s_keepalive_is_open;
bool disc_has_changed = false;
bool weAreInNewDiskCB = false;

std::unique_ptr<IOCtlSrc> src;

extern CacheRequest g_last_sector_block;

///////////////////////////////////////////////////////////////////////////////
// keepAliveThread throws a read event regularly to prevent drive spin down  //

void keepAliveThread()
{
    u8 throwaway[2352];

    printf(" * CDVD: KeepAlive thread started...\n");
    std::unique_lock<std::mutex> guard(s_keepalive_lock);

    while (!s_keepalive_cv.wait_for(guard, std::chrono::seconds(30),
                                    []() { return !s_keepalive_is_open; })) {

        //printf(" * keepAliveThread: polling drive.\n");
        if (src->GetMediaType() >= 0)
            src->ReadSectors2048(g_last_sector_block.lsn, 1, throwaway);
        else
            src->ReadSectors2352(g_last_sector_block.lsn, 1, throwaway);
    }

    printf(" * CDVD: KeepAlive thread finished.\n");
}

bool StartKeepAliveThread()
{
    s_keepalive_is_open = true;
    try {
        s_keepalive_thread = std::thread(keepAliveThread);
    } catch (std::system_error &) {
        s_keepalive_is_open = false;
    }

    return s_keepalive_is_open;
}

void StopKeepAliveThread()
{
    if (!s_keepalive_thread.joinable())
        return;

    {
        std::lock_guard<std::mutex> guard(s_keepalive_lock);
        s_keepalive_is_open = false;
    }
    s_keepalive_cv.notify_one();
    s_keepalive_thread.join();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// CDVD Pluin Interface                                                      //

EXPORT void CALLBACK CDVDsetSettingsDir(const char *dir)
{
    s_config_file = std::string(dir ? dir : "inis") + "/cdvdGigaherz.ini";
}

EXPORT s32 CALLBACK CDVDinit()
{
    return 0;
}

EXPORT s32 CALLBACK CDVDopen(const char *pTitleFilename)
{
    ReadSettings();

    auto drive = GetValidDrive();
    if (drive.empty())
        return -1;

    // open device file
    try {
        src = std::unique_ptr<IOCtlSrc>(new IOCtlSrc(drive));
    } catch (std::runtime_error &ex) {
        fputs(ex.what(), stdout);
        return -1;
    }

    //setup threading manager
    if (!cdvdStartThread()) {
        src.reset();
        return -1;
    }
    StartKeepAliveThread();

    return cdvdRefreshData();
}

EXPORT void CALLBACK CDVDclose()
{
    StopKeepAliveThread();
    cdvdStopThread();
    //close device
    src.reset();
}

EXPORT void CALLBACK CDVDshutdown()
{
    //nothing to do here
}

EXPORT s32 CALLBACK CDVDgetDualInfo(s32 *dualType, u32 *_layer1start)
{
    switch (src->GetMediaType()) {
        case 1:
            *dualType = 1;
            *_layer1start = src->GetLayerBreakAddress() + 1;
            return 0;
        case 2:
            *dualType = 2;
            *_layer1start = src->GetLayerBreakAddress() + 1;
            return 0;
        case 0:
            *dualType = 0;
            *_layer1start = 0;
            return 0;
    }
    return -1;
}

int lastReadInNewDiskCB = 0;
u8 directReadSectorBuffer[2448];

EXPORT s32 CALLBACK CDVDreadSector(u8 *buffer, u32 lsn, int mode)
{
    return cdvdDirectReadSector(lsn, mode, buffer);
}

EXPORT s32 CALLBACK CDVDreadTrack(u32 lsn, int mode)
{
    csector = lsn;
    cmode = mode;

    if (weAreInNewDiskCB) {
        int ret = cdvdDirectReadSector(lsn, mode, directReadSectorBuffer);
        if (ret == 0)
            lastReadInNewDiskCB = 1;
        return ret;
    }

    return lsn < src->GetSectorCount() ? cdvdRequestSector(lsn, mode) : -1;
}

// return can be NULL (for async modes)
EXPORT u8 *CALLBACK CDVDgetBuffer()
{
    if (lastReadInNewDiskCB) {
        lastReadInNewDiskCB = 0;
        return directReadSectorBuffer;
    }

    return cdvdGetSector(csector, cmode);
}

// return can be NULL (for async modes)
EXPORT int CALLBACK CDVDgetBuffer2(u8 *dest)
{
    int csize = 2352;
    switch (cmode) {
        case CDVD_MODE_2048:
            csize = 2048;
            break;
        case CDVD_MODE_2328:
            csize = 2328;
            break;
        case CDVD_MODE_2340:
            csize = 2340;
            break;
    }

    if (lastReadInNewDiskCB) {
        lastReadInNewDiskCB = 0;

        memcpy(dest, directReadSectorBuffer, csize);
        return 0;
    }

    memcpy(dest, cdvdGetSector(csector, cmode), csize);

    return 0;
}

EXPORT s32 CALLBACK CDVDreadSubQ(u32 lsn, cdvdSubQ *subq)
{
    // the formatted subq command returns:  control/adr, track, index, trk min, trk sec, trk frm, 0x00, abs min, abs sec, abs frm

    if (lsn >= src->GetSectorCount())
        return -1;

    memset(subq, 0, sizeof(cdvdSubQ));

    lsn_to_msf(&subq->discM, &subq->discS, &subq->discF, lsn + 150);

    u8 i = strack;
    while (i < etrack && lsn >= tracks[i + 1].start_lba)
        ++i;

    lsn -= tracks[i].start_lba;

    lsn_to_msf(&subq->trackM, &subq->trackS, &subq->trackF, lsn);

    subq->mode = 1;
    subq->ctrl = tracks[i].type;
    subq->trackNum = i;
    subq->trackIndex = 1;

    return 0;
}

EXPORT s32 CALLBACK CDVDgetTN(cdvdTN *Buffer)
{
    Buffer->strack = strack;
    Buffer->etrack = etrack;
    return 0;
}

EXPORT s32 CALLBACK CDVDgetTD(u8 Track, cdvdTD *Buffer)
{
    if (Track == 0) {
        Buffer->lsn = src->GetSectorCount();
        Buffer->type = 0;
        return 0;
    }

    if (Track < strack)
        return -1;
    if (Track > etrack)
        return -1;

    Buffer->lsn = tracks[Track].start_lba;
    Buffer->type = tracks[Track].type;
    return 0;
}

EXPORT s32 CALLBACK CDVDgetTOC(void *toc)
{
    u8 *tocBuff = static_cast<u8 *>(toc);
    if (curDiskType == CDVD_TYPE_NODISC)
        return -1;

    if (curDiskType == CDVD_TYPE_DETCTDVDS || curDiskType == CDVD_TYPE_DETCTDVDD) {
        memset(tocBuff, 0, 2048);

        s32 mt = src->GetMediaType();

        if (mt < 0)
            return -1;

        if (mt == 0) { //single layer
            // fake it
            tocBuff[0] = 0x04;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[3] = 0x00;
            tocBuff[4] = 0x86;
            tocBuff[5] = 0x72;

            tocBuff[16] = 0x00; // first sector for layer 0
            tocBuff[17] = 0x03;
            tocBuff[18] = 0x00;
            tocBuff[19] = 0x00;
        } else if (mt == 1) { //PTP
            u32 layer1start = src->GetLayerBreakAddress() + 0x30000;

            // dual sided
            tocBuff[0] = 0x24;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[3] = 0x00;
            tocBuff[4] = 0x41;
            tocBuff[5] = 0x95;

            tocBuff[14] = 0x61; // PTP

            tocBuff[16] = 0x00;
            tocBuff[17] = 0x03;
            tocBuff[18] = 0x00;
            tocBuff[19] = 0x00;

            tocBuff[20] = (layer1start >> 24);
            tocBuff[21] = (layer1start >> 16) & 0xff;
            tocBuff[22] = (layer1start >> 8) & 0xff;
            tocBuff[23] = (layer1start >> 0) & 0xff;
        } else { //OTP
            u32 layer1start = src->GetLayerBreakAddress() + 0x30000;

            // dual sided
            tocBuff[0] = 0x24;
            tocBuff[1] = 0x02;
            tocBuff[2] = 0xF2;
            tocBuff[3] = 0x00;
            tocBuff[4] = 0x41;
            tocBuff[5] = 0x95;

            tocBuff[14] = 0x71; // OTP

            tocBuff[16] = 0x00;
            tocBuff[17] = 0x03;
            tocBuff[18] = 0x00;
            tocBuff[19] = 0x00;

            tocBuff[24] = (layer1start >> 24);
            tocBuff[25] = (layer1start >> 16) & 0xff;
            tocBuff[26] = (layer1start >> 8) & 0xff;
            tocBuff[27] = (layer1start >> 0) & 0xff;
        }
    } else if (curDiskType == CDVD_TYPE_DETCTCD) {
        // cd toc
        // (could be replaced by 1 command that reads the full toc)
        u8 min, sec, frm, i;
        s32 err;
        cdvdTN diskInfo;
        cdvdTD trackInfo;
        memset(tocBuff, 0, 1024);
        if (CDVDgetTN(&diskInfo) == -1) {
            diskInfo.etrack = 0;
            diskInfo.strack = 1;
        }
        if (CDVDgetTD(0, &trackInfo) == -1)
            trackInfo.lsn = 0;

        tocBuff[0] = 0x41;
        tocBuff[1] = 0x00;

        //Number of FirstTrack
        tocBuff[2] = 0xA0;
        tocBuff[7] = dec_to_bcd(diskInfo.strack);

        //Number of LastTrack
        tocBuff[12] = 0xA1;
        tocBuff[17] = dec_to_bcd(diskInfo.etrack);

        //DiskLength
        lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
        tocBuff[22] = 0xA2;
        tocBuff[27] = dec_to_bcd(min);
        tocBuff[28] = dec_to_bcd(sec);
        tocBuff[29] = dec_to_bcd(frm);

        fprintf(stderr, "Track 0: %u mins %u secs %u frames\n", min, sec, frm);

        for (i = diskInfo.strack; i <= diskInfo.etrack; i++) {
            err = CDVDgetTD(i, &trackInfo);
            lba_to_msf(trackInfo.lsn, &min, &sec, &frm);
            tocBuff[i * 10 + 30] = trackInfo.type;
            tocBuff[i * 10 + 32] = err == -1 ? 0 : dec_to_bcd(i); //number
            tocBuff[i * 10 + 37] = dec_to_bcd(min);
            tocBuff[i * 10 + 38] = dec_to_bcd(sec);
            tocBuff[i * 10 + 39] = dec_to_bcd(frm);
            fprintf(stderr, "Track %u: %u mins %u secs %u frames\n", i, min, sec, frm);
        }
    } else
        return -1;

    return 0;
}

EXPORT s32 CALLBACK CDVDgetDiskType()
{
    return curDiskType;
}

EXPORT s32 CALLBACK CDVDgetTrayStatus()
{
    return curTrayStatus;
}

EXPORT s32 CALLBACK CDVDctrlTrayOpen()
{
    curTrayStatus = CDVD_TRAY_OPEN;
    return 0;
}

EXPORT s32 CALLBACK CDVDctrlTrayClose()
{
    curTrayStatus = CDVD_TRAY_CLOSE;
    return 0;
}

EXPORT void CALLBACK CDVDnewDiskCB(void (*callback)())
{
    newDiscCB = callback;
}

EXPORT void CALLBACK CDVDconfigure()
{
    configure();
}

EXPORT s32 CALLBACK CDVDtest()
{
    return 0;
}
