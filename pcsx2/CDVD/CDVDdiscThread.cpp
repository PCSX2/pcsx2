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

#include "PrecompiledHeader.h"
#include "CDVDdiscReader.h"
#include "CDVD/CDVD.h"

#include <atomic>
#include <condition_variable>
#include <limits>
#include <queue>
#include <thread>

const u32 sectors_per_read = 16;

static_assert(sectors_per_read > 1 && !(sectors_per_read & (sectors_per_read - 1)),
			  "sectors_per_read must by a power of 2");

struct SectorInfo
{
	u32 lsn;
	// Sectors are read in blocks, not individually
	u8 data[2352 * sectors_per_read];
};

u32 g_last_sector_block_lsn;

static std::thread s_thread;

static std::mutex s_notify_lock;
static std::condition_variable s_notify_cv;
static std::mutex s_request_lock;
static std::queue<u32> s_request_queue;
static std::mutex s_cache_lock;

static std::atomic<bool> cdvd_is_open;

//bits: 12 would use 1<<12 entries, or 4096*16 sectors ~ 128MB
#define CACHE_SIZE 12

const u32 CacheSize = 1U << CACHE_SIZE;
SectorInfo Cache[CacheSize];

u32 cdvdSectorHash(u32 lsn)
{
	u32 t = 0;

	int i = 32;
	u32 m = CacheSize - 1;

	while (i >= 0)
	{
		t ^= lsn & m;
		lsn >>= CACHE_SIZE;
		i -= CACHE_SIZE;
	}

	return t & m;
}

void cdvdCacheUpdate(u32 lsn, u8* data)
{
	std::lock_guard<std::mutex> guard(s_cache_lock);
	u32 entry = cdvdSectorHash(lsn);

	memcpy(Cache[entry].data, data, 2352 * sectors_per_read);
	Cache[entry].lsn = lsn;
}

bool cdvdCacheCheck(u32 lsn)
{
	std::lock_guard<std::mutex> guard(s_cache_lock);
	u32 entry = cdvdSectorHash(lsn);

	return Cache[entry].lsn == lsn;
}

bool cdvdCacheFetch(u32 lsn, u8* data)
{
	std::lock_guard<std::mutex> guard(s_cache_lock);
	u32 entry = cdvdSectorHash(lsn);

	if (Cache[entry].lsn == lsn)
	{
		memcpy(data, Cache[entry].data, 2352 * sectors_per_read);
		return true;
	}
	//printf("NOT IN CACHE\n");
	return false;
}

void cdvdCacheReset()
{
	std::lock_guard<std::mutex> guard(s_cache_lock);
	for (u32 i = 0; i < CacheSize; i++)
	{
		Cache[i].lsn = std::numeric_limits<u32>::max();
	}
}

bool cdvdReadBlockOfSectors(u32 sector, u8* data)
{
	u32 count = std::min(sectors_per_read, src->GetSectorCount() - sector);
	const s32 media = src->GetMediaType();

	// TODO: Is it really necessary to retry if it fails? I'm not sure the
	// second time is really going to be any better.
	for (int tries = 0; tries < 2; ++tries)
	{
		if (media >= 0)
		{
			if (src->ReadSectors2048(sector, count, data))
				return true;
		}
		else
		{
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

	if (!ready)
	{
		if (!disc_has_changed)
		{
			disc_has_changed = true;
			curDiskType = CDVD_TYPE_NODISC;
			curTrayStatus = CDVD_TRAY_OPEN;
			cdvdCallNewDiscCB();
		}
	}
	else
	{
		if (disc_has_changed)
		{
			curDiskType = CDVD_TYPE_NODISC;
			curTrayStatus = CDVD_TRAY_CLOSE;

			disc_has_changed = false;
			cdvdRefreshData();

			{
				std::lock_guard<std::mutex> request_guard(s_request_lock);
				s_request_queue = decltype(s_request_queue)();
			}

			cdvdCallNewDiscCB();
		}
	}
	return !ready;
}

void cdvdThread()
{
	u8 buffer[2352 * sectors_per_read];
	u32 prefetches_left = 0;

	printf(" * CDVD: IO thread started...\n");
	std::unique_lock<std::mutex> guard(s_notify_lock);

	while (cdvd_is_open)
	{
		if (cdvdUpdateDiscStatus())
		{
			// Need to sleep some to avoid an aggressive spin that sucks the cpu dry.
			s_notify_cv.wait_for(guard, std::chrono::milliseconds(10));
			prefetches_left = 0;
			continue;
		}

		if (prefetches_left == 0)
			s_notify_cv.wait_for(guard, std::chrono::milliseconds(250));

		// check again to make sure we're not done here...
		if (!cdvd_is_open)
			break;

		// Read request
		bool handling_request = false;
		u32 request_lsn;

		{
			std::lock_guard<std::mutex> request_guard(s_request_lock);
			if (!s_request_queue.empty())
			{
				request_lsn = s_request_queue.front();
				s_request_queue.pop();
				handling_request = true;
			}
		}

		if (!handling_request)
		{
			if (prefetches_left == 0)
				continue;

			--prefetches_left;

			u32 next_prefetch_lsn = g_last_sector_block_lsn + sectors_per_read;
			request_lsn = next_prefetch_lsn;
		}

		// Handle request
		if (!cdvdCacheCheck(request_lsn))
		{
			if (cdvdReadBlockOfSectors(request_lsn, buffer))
			{
				cdvdCacheUpdate(request_lsn, buffer);
			}
			else
			{
				// If the read fails, further reads are likely to fail too.
				prefetches_left = 0;
				continue;
			}
		}

		g_last_sector_block_lsn = request_lsn;

		if (!handling_request)
			continue;

		// Prefetch
		u32 next_prefetch_lsn = g_last_sector_block_lsn + sectors_per_read;
		if (next_prefetch_lsn >= src->GetSectorCount())
		{
			prefetches_left = 0;
		}
		else
		{
			const u32 max_prefetches = 16;
			u32 remaining = src->GetSectorCount() - next_prefetch_lsn;
			prefetches_left = std::min((remaining + sectors_per_read - 1) / sectors_per_read, max_prefetches);
		}
	}
	printf(" * CDVD: IO thread finished.\n");
}

bool cdvdStartThread()
{
	if (cdvd_is_open == false)
	{
		cdvd_is_open = true;
		try
		{
			s_thread = std::thread(cdvdThread);
		}
		catch (std::system_error&)
		{
			cdvd_is_open = false;
			return false;
		}
	}

	cdvdCacheReset();

	return true;
}

void cdvdStopThread()
{
	cdvd_is_open = false;
	s_notify_cv.notify_one();
	if (s_thread.joinable())
		s_thread.join();
}

void cdvdRequestSector(u32 sector, s32 mode)
{
	if (sector >= src->GetSectorCount())
		return;

	// Align to cache block
	sector &= ~(sectors_per_read - 1);

	if (cdvdCacheCheck(sector))
		return;

	{
		std::lock_guard<std::mutex> guard(s_request_lock);
		s_request_queue.push(sector);
	}

	s_notify_cv.notify_one();
}

u8* cdvdGetSector(u32 sector, s32 mode)
{
	static u8 buffer[2352 * sectors_per_read];

	// Align to cache block
	u32 sector_block = sector & ~(sectors_per_read - 1);

	if (!cdvdCacheFetch(sector_block, buffer))
		if (cdvdReadBlockOfSectors(sector_block, buffer))
			cdvdCacheUpdate(sector_block, buffer);

	if (src->GetMediaType() >= 0)
	{
		u32 offset = 2048 * (sector - sector_block);
		return buffer + offset;
	}

	u32 offset = 2352 * (sector - sector_block);
	u8* data = buffer + offset;

	switch (mode)
	{
		case CDVD_MODE_2048:
			// Data location depends on CD mode
			return (data[15] & 3) == 2 ? data + 24 : data + 16;
		case CDVD_MODE_2328:
			return data + 24;
		case CDVD_MODE_2340:
			return data + 12;
	}
	return data;
}

s32 cdvdDirectReadSector(u32 sector, s32 mode, u8* buffer)
{
	static u8 data[2352 * sectors_per_read];

	if (src == nullptr)
		return -1;

	try
	{
		if (sector >= src->GetSectorCount())
			return -1;
	}
	catch (...)
	{
		return -1;
	}

	// Align to cache block
	u32 sector_block = sector & ~(sectors_per_read - 1);

	if (!cdvdCacheFetch(sector_block, data))
	{
		if (cdvdReadBlockOfSectors(sector_block, data))
			cdvdCacheUpdate(sector_block, data);
	}

	if (src->GetMediaType() >= 0)
	{
		u32 offset = 2048 * (sector - sector_block);
		memcpy(buffer, data + offset, 2048);
		return 0;
	}

	u32 offset = 2352 * (sector - sector_block);
	u8* bfr = data + offset;

	switch (mode)
	{
		case CDVD_MODE_2048:
			// Data location depends on CD mode
			std::memcpy(buffer, (bfr[15] & 3) == 2 ? bfr + 24 : bfr + 16, 2048);
			return 0;
		case CDVD_MODE_2328:
			memcpy(buffer, bfr + 24, 2328);
			return 0;
		case CDVD_MODE_2340:
			memcpy(buffer, bfr + 12, 2340);
			return 0;
		default:
			memcpy(buffer, bfr, 2352);
			return 0;
	}
}

s32 cdvdGetMediaType()
{
	return src->GetMediaType();
}

s32 cdvdRefreshData()
{
	const char* diskTypeName = "Unknown";

	//read TOC from device
	cdvdParseTOC();

	if ((etrack == 0) || (strack > etrack))
	{
		curDiskType = CDVD_TYPE_NODISC;
	}
	else
	{
		s32 mt = cdvdGetMediaType();

		if (mt < 0)
			curDiskType = CDVD_TYPE_DETCTCD;
		else if (mt == 0)
			curDiskType = CDVD_TYPE_DETCTDVDS;
		else
			curDiskType = CDVD_TYPE_DETCTDVDD;
	}

	curTrayStatus = CDVD_TRAY_CLOSE;

	switch (curDiskType)
	{
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
