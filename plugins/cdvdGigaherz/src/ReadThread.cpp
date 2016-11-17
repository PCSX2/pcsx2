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
#include <atomic>
#include <condition_variable>
#include <limits>
#include <queue>
#include <thread>

struct CacheRequest
{
    u32 lsn;
    s32 mode;
};

struct SectorInfo
{
    u32 lsn;
    s32 mode;
    u8 data[2352 * 16]; // we will read in blocks of 16 sectors
};

const s32 prefetch_max_blocks = 16;
s32 prefetch_mode = 0;
s32 prefetch_last_lba = 0;
s32 prefetch_last_mode = 0;
s32 prefetch_left = 0;

static std::thread s_thread;

static std::mutex s_notify_lock;
static std::condition_variable s_notify_cv;
static std::mutex s_request_lock;
static std::queue<CacheRequest> s_request_queue;
static std::mutex s_cache_lock;

static std::atomic<bool> cdvd_is_open;

//bits: 12 would use 1<<12 entries, or 4096*16 sectors ~ 128MB
#define CACHE_SIZE 12

const u32 CacheSize = 1U << CACHE_SIZE;
SectorInfo Cache[CacheSize];

u32 cdvdSectorHash(u32 lsn, s32 mode)
{
    u32 t = 0;

    int i = 32;
    u32 m = CacheSize - 1;

    while (i >= 0) {
        t ^= lsn & m;
        lsn >>= CACHE_SIZE;
        i -= CACHE_SIZE;
    }

    return (t ^ mode) & m;
}

void cdvdCacheUpdate(u32 lsn, s32 mode, u8 *data)
{
    std::lock_guard<std::mutex> guard(s_cache_lock);
    u32 entry = cdvdSectorHash(lsn, mode);

    memcpy(Cache[entry].data, data, 2352 * 16);
    Cache[entry].lsn = lsn;
    Cache[entry].mode = mode;
}

bool cdvdCacheCheck(u32 lsn, s32 mode)
{
    std::lock_guard<std::mutex> guard(s_cache_lock);
    u32 entry = cdvdSectorHash(lsn, mode);

    return Cache[entry].lsn == lsn && Cache[entry].mode == mode;
}

bool cdvdCacheFetch(u32 lsn, s32 mode, u8 *data)
{
    std::lock_guard<std::mutex> guard(s_cache_lock);
    u32 entry = cdvdSectorHash(lsn, mode);

    if ((Cache[entry].lsn == lsn) &&
        (Cache[entry].mode == mode)) {
        memcpy(data, Cache[entry].data, 2352 * 16);
        return true;
    }
    //printf("NOT IN CACHE\n");
    return false;
}

void cdvdCacheReset()
{
    std::lock_guard<std::mutex> guard(s_cache_lock);
    for (u32 i = 0; i < CacheSize; i++) {
        Cache[i].lsn = std::numeric_limits<u32>::max();
        Cache[i].mode = -1;
    }
}

bool cdvdReadBlockOfSectors(u32 sector, s32 mode, u8 *data)
{
    u32 count = std::min(16U, src->GetSectorCount() - sector);

    // TODO: Is it really necessary to retry 3 times if it fails?
    for (int tries = 0; tries < 4; ++tries) {
        if (mode == CDVD_MODE_2048) {
            if (src->ReadSectors2048(sector, count, data))
                return true;
        } else {
            if (src->ReadSectors2352(sector, count, data))
                return true;
        }
    }
    return false;
}

void cdvdCallNewDiscCB()
{
    weAreInNewDiskCB = true;
    newDiscCB();
    weAreInNewDiskCB = false;
}

bool cdvdUpdateDiscStatus()
{
    bool ready = src->DiscReady();

    if (!ready) {
        if (!disc_has_changed) {
            disc_has_changed = true;
            curDiskType = CDVD_TYPE_NODISC;
            curTrayStatus = CDVD_TRAY_OPEN;
            cdvdCallNewDiscCB();
        }
    } else {
        if (disc_has_changed) {
            curDiskType = CDVD_TYPE_NODISC;
            curTrayStatus = CDVD_TRAY_CLOSE;

            disc_has_changed = false;
            cdvdRefreshData();

            {
                std::lock_guard<std::mutex> request_guard(s_request_lock);
                s_request_queue = std::queue<CacheRequest>();
            }

            cdvdCallNewDiscCB();
        }
    }
    return !ready;
}

void cdvdThread()
{
    u8 buffer[2352 * 16];

    printf(" * CDVD: IO thread started...\n");
    std::unique_lock<std::mutex> guard(s_notify_lock);

    while (cdvd_is_open) {
        if (cdvdUpdateDiscStatus()) {
            // Need to sleep some to avoid an aggressive spin that sucks the cpu dry.
            s_notify_cv.wait_for(guard, std::chrono::milliseconds(10));
            continue;
        }

        s_notify_cv.wait_for(guard, std::chrono::milliseconds(prefetch_left ? 1 : 250));

        // check again to make sure we're not done here...
        if (!cdvd_is_open)
            break;

        bool handling_request = false;
        CacheRequest request;

        {
            std::lock_guard<std::mutex> request_guard(s_request_lock);
            if (!s_request_queue.empty()) {
                request = s_request_queue.front();
                s_request_queue.pop();
                handling_request = true;
            } else {
                request.lsn = prefetch_last_lba;
                request.mode = prefetch_last_mode;
            }
        }

        if (handling_request || prefetch_left) {
            if (!cdvdCacheCheck(request.lsn, request.mode))
                if (cdvdReadBlockOfSectors(request.lsn, request.mode, buffer))
                    cdvdCacheUpdate(request.lsn, request.mode, buffer);

            if (handling_request) {
                prefetch_last_lba = request.lsn;
                prefetch_last_mode = request.mode;

                prefetch_left = prefetch_max_blocks;
            } else {
                prefetch_last_lba += 16;
                prefetch_left--;
            }
        }
    }
    printf(" * CDVD: IO thread finished.\n");
}

bool cdvdStartThread()
{
    cdvd_is_open = true;
    try {
        s_thread = std::thread(cdvdThread);
    } catch (std::system_error &) {
        cdvd_is_open = false;
        return false;
    }

    cdvdCacheReset();

    return true;
}

void cdvdStopThread()
{
    cdvd_is_open = false;
    s_notify_cv.notify_one();
    s_thread.join();
}

s32 cdvdRequestSector(u32 sector, s32 mode)
{
    if (sector >= src->GetSectorCount())
        return -1;

    sector &= ~15; //align to 16-sector block

    if (cdvdCacheCheck(sector, mode))
        return 0;

    {
        std::lock_guard<std::mutex> guard(s_request_lock);
        s_request_queue.push({sector, mode});
    }

    s_notify_cv.notify_one();

    return 0;
}

u8 *cdvdGetSector(u32 sector, s32 mode)
{
    static u8 buffer[2352 * 16];
    u32 sector_block = sector & ~15;

    if (!cdvdCacheFetch(sector_block, mode, buffer))
        if (cdvdReadBlockOfSectors(sector_block, mode, buffer))
            cdvdCacheUpdate(sector_block, mode, buffer);

    if (mode == CDVD_MODE_2048) {
        u32 offset = 2048 * (sector - sector_block);
        return buffer + offset;
    }

    u32 offset = 2352 * (sector - sector_block);
    u8 *data = buffer + offset;

    switch (mode) {
        case CDVD_MODE_2328:
            return data + 24;
        case CDVD_MODE_2340:
            return data + 12;
    }
    return data;
}

s32 cdvdDirectReadSector(u32 first, s32 mode, u8 *buffer)
{
    static u8 data[16 * 2352];

    if (first >= src->GetSectorCount())
        return -1;

    u32 sector = first & (~15); //align to 16-sector block

    if (!cdvdCacheFetch(sector, mode, data)) {
        if (cdvdReadBlockOfSectors(sector, mode, data))
            cdvdCacheUpdate(sector, mode, data);
    }

    if (mode == CDVD_MODE_2048) {
        u32 offset = 2048 * (first - sector);
        memcpy(buffer, data + offset, 2048);
        return 0;
    }

    u32 offset = 2352 * (first - sector);
    u8 *bfr = data + offset;

    switch (mode) {
        case CDVD_MODE_2328:
            memcpy(buffer, bfr + 24, 2328);
            return 0;
        case CDVD_MODE_2340:
            memcpy(buffer, bfr + 12, 2340);
            return 0;
        default:
            memcpy(buffer, bfr + 12, 2352);
            return 0;
    }
}

s32 cdvdGetMediaType()
{
    return src->GetMediaType();
}

s32 cdvdRefreshData()
{
    const char *diskTypeName = "Unknown";

    //read TOC from device
    cdvdParseTOC();

    if ((etrack == 0) || (strack > etrack)) {
        curDiskType = CDVD_TYPE_NODISC;
    } else {
        s32 mt = cdvdGetMediaType();

        if (mt < 0)
            curDiskType = CDVD_TYPE_DETCTCD;
        else if (mt == 0)
            curDiskType = CDVD_TYPE_DETCTDVDS;
        else
            curDiskType = CDVD_TYPE_DETCTDVDD;
    }

    curTrayStatus = CDVD_TRAY_CLOSE;

    switch (curDiskType) {
        case CDVD_TYPE_DETCTDVDD:
            diskTypeName = "Double-Layer DVD";
            break;
        case CDVD_TYPE_DETCTDVDS:
            diskTypeName = "Single-Layer DVD";
            break;
        case CDVD_TYPE_DETCTCD:
            diskTypeName = "CD-ROM";
            break;
        case CDVD_TYPE_NODISC:
            diskTypeName = "No Disc";
            break;
    }

    printf(" * CDVD: Disk Type: %s\n", diskTypeName);

    cdvdCacheReset();

    return 0;
}
