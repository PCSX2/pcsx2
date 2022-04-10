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
#include "CDVD/CDVDdiscReader.h"
#include "CDVD/CDVD.h"

#ifdef __linux__
#include <linux/cdrom.h>
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <climits>
#include <cstring>

IOCtlSrc::IOCtlSrc(decltype(m_filename) filename)
	: m_filename(filename)
{
	if (!Reopen())
		throw std::runtime_error(" * CDVD: Error opening source.\n");
}

IOCtlSrc::~IOCtlSrc()
{
	if (m_device != -1)
	{
		SetSpindleSpeed(true);
		close(m_device);
	}
}

bool IOCtlSrc::Reopen()
{
	if (m_device != -1)
		close(m_device);

	// O_NONBLOCK allows a valid file descriptor to be returned even if the
	// drive is empty. Probably does other things too.
	m_device = open(m_filename.c_str(), O_RDONLY | O_NONBLOCK);
	if (m_device == -1)
		return false;

	// DVD detection MUST be first on Linux - The TOC ioctls work for both
	// CDs and DVDs.
	if (ReadDVDInfo() || ReadCDInfo())
		SetSpindleSpeed(false);

	return true;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults) const
{
	// TODO: CD seems easy enough (CDROM_SELECT_SPEED ioctl), but I'm not sure
	// about DVD.
}

u32 IOCtlSrc::GetSectorCount() const
{
	return m_sectors;
}

u32 IOCtlSrc::GetLayerBreakAddress() const
{
	return m_layer_break;
}

s32 IOCtlSrc::GetMediaType() const
{
	return m_media_type;
}

const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const
{
	return m_toc;
}

bool IOCtlSrc::ReadSectors2048(u32 sector, u32 count, u8* buffer) const
{
	const ssize_t bytes_to_read = 2048 * count;
	ssize_t bytes_read = pread(m_device, buffer, bytes_to_read, sector * 2048ULL);
	if (bytes_read == bytes_to_read)
		return true;

	if (bytes_read == -1)
		fprintf(stderr, " * CDVD read sectors %u-%u failed: %s\n",
				sector, sector + count - 1, strerror(errno));
	else
		fprintf(stderr, " * CDVD read sectors %u-%u: %zd bytes read, %zd bytes expected\n",
				sector, sector + count - 1, bytes_read, bytes_to_read);
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
#ifdef __linux__
	union
	{
		cdrom_msf msf;
		char buffer[CD_FRAMESIZE_RAW];
	} data;

	for (u32 n = 0; n < count; ++n)
	{
		u32 lba = sector + n;
		lba_to_msf(lba, &data.msf.cdmsf_min0, &data.msf.cdmsf_sec0, &data.msf.cdmsf_frame0);
		if (ioctl(m_device, CDROMREADRAW, &data) == -1)
		{
			fprintf(stderr, " * CDVD CDROMREADRAW sector %u failed: %s\n",
					lba, strerror(errno));
			return false;
		}
		memcpy(buffer, data.buffer, CD_FRAMESIZE_RAW);
		buffer += CD_FRAMESIZE_RAW;
	}

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::ReadDVDInfo()
{
#ifdef __linux__
	dvd_struct dvdrs;
	dvdrs.type = DVD_STRUCT_PHYSICAL;
	dvdrs.physical.layer_num = 0;

	int ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
	if (ret == -1)
		return false;

	u32 start_sector = dvdrs.physical.layer[0].start_sector;
	u32 end_sector = dvdrs.physical.layer[0].end_sector;

	if (dvdrs.physical.layer[0].nlayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (dvdrs.physical.layer[0].track_path == 0)
	{
		// Dual layer, Parallel Track Path
		dvdrs.physical.layer_num = 1;
		ret = ioctl(m_device, DVD_READ_STRUCT, &dvdrs);
		if (ret == -1)
			return false;
		u32 layer1_start_sector = dvdrs.physical.layer[1].start_sector;
		u32 layer1_end_sector = dvdrs.physical.layer[1].end_sector;

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = dvdrs.physical.layer[0].end_sector_l0;
		m_media_type = 2;
		m_layer_break = end_sector_layer0 - start_sector;
		m_sectors = end_sector_layer0 - start_sector + 1 + end_sector - (~end_sector_layer0 & 0xFFFFFFU) + 1;
	}

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::ReadCDInfo()
{
#ifdef __linux__
	cdrom_tochdr header;

	if (ioctl(m_device, CDROMREADTOCHDR, &header) == -1)
		return false;

	cdrom_tocentry entry{};
	entry.cdte_format = CDROM_LBA;

	m_toc.clear();
	for (u8 n = header.cdth_trk0; n <= header.cdth_trk1; ++n)
	{
		entry.cdte_track = n;
		if (ioctl(m_device, CDROMREADTOCENTRY, &entry) != -1)
			m_toc.push_back({static_cast<u32>(entry.cdte_addr.lba), entry.cdte_track,
							 entry.cdte_adr, entry.cdte_ctrl});
	}

	// TODO: Do I need a fallback if this doesn't work?
	entry.cdte_track = 0xAA;
	if (ioctl(m_device, CDROMREADTOCENTRY, &entry) == -1)
		return false;

	m_sectors = entry.cdte_addr.lba;
	m_media_type = -1;

	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::DiscReady()
{
#ifdef __linux__
	if (m_device == -1)
		return false;

	// CDSL_CURRENT must be used - 0 will cause the drive tray to close.
	if (ioctl(m_device, CDROM_DRIVE_STATUS, CDSL_CURRENT) == CDS_DISC_OK)
	{
		if (!m_sectors)
			Reopen();
	}
	else
	{
		m_sectors = 0;
		m_layer_break = 0;
		m_media_type = 0;
	}

	return !!m_sectors;
#else
	return false;
#endif
}
