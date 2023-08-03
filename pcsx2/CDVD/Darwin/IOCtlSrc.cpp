/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#ifdef __APPLE__
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IODVDMediaBSDClient.h>
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

IOCtlSrc::IOCtlSrc(std::string filename)
	: m_filename(std::move(filename))
{
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
#ifdef __APPLE__
	u16 speed = restore_defaults ? 0xFFFF : m_media_type >= 0 ? 5540 :
																3600;
	int ioctl_code = m_media_type >= 0 ? DKIOCDVDSETSPEED : DKIOCCDSETSPEED;
	if (ioctl(m_device, ioctl_code, &speed) == -1)
	{
		DevCon.Warning("CDVD: Failed to set spindle speed: %s", strerror(errno));
	}
	else if (!restore_defaults)
	{
		DevCon.WriteLn("CDVD: Spindle speed set to %d", speed);
	}
#else
	// FIXME: FreeBSD equivalent for DKIOCDVDSETSPEED DKIOCCDSETSPEED.
	DevCon.Warning("CDVD: Setting spindle speed not supported!");
#endif
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
		DevCon.Warning("CDVD: read sectors %u-%u failed: %s",
			sector, sector + count - 1, strerror(errno));
	else
		DevCon.Warning("CDVD: read sectors %u-%u: %zd bytes read, %zd bytes expected",
			sector, sector + count - 1, bytes_read, bytes_to_read);
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
#ifdef __APPLE__
	dk_cd_read_t desc;
	memset(&desc, 0, sizeof(dk_cd_read_t));
	desc.sectorArea = kCDSectorAreaSync | kCDSectorAreaHeader | kCDSectorAreaSubHeader | kCDSectorAreaUser | kCDSectorAreaAuxiliary;
	desc.sectorType = kCDSectorTypeUnknown;
	for (u32 i = 0; i < count; ++i)
	{
		desc.offset = (sector + i) * 2352ULL;
		desc.buffer = buffer + i * 2352;
		desc.bufferLength = 2352;
		if (ioctl(m_device, DKIOCCDREAD, &desc) == -1)
		{
			DevCon.Warning("CDVD: DKIOCCDREAD sector %u failed: %s",
				sector + i, strerror(errno));
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::ReadDVDInfo()
{
#ifdef __APPLE__
	dk_dvd_read_structure_t dvdrs;
	memset(&dvdrs, 0, sizeof(dk_dvd_read_structure_t));
	dvdrs.format = kDVDStructureFormatPhysicalFormatInfo;
	dvdrs.layer = 0;

	DVDPhysicalFormatInfo layer0;
	dvdrs.buffer = &layer0;
	dvdrs.bufferLength = sizeof(DVDPhysicalFormatInfo);

	int ret = ioctl(m_device, DKIOCDVDREADSTRUCTURE, &dvdrs);
	if (ret == -1)
	{
		return false;
	}

	u32 start_sector = *(u32*)layer0.startingPhysicalSectorNumberOfDataArea;
	u32 end_sector = *(u32*)layer0.endPhysicalSectorNumberOfDataArea;
	if (layer0.numberOfLayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (layer0.trackPath == 0)
	{
		// Dual layer, Parallel Track Path
		DVDPhysicalFormatInfo layer1;
		dvdrs.layer = 1;
		dvdrs.buffer = &layer1;
		dvdrs.bufferLength = sizeof(DVDPhysicalFormatInfo);
		ret = ioctl(m_device, DKIOCDVDREADSTRUCTURE, &dvdrs);
		if (ret == -1)
			return false;
		u32 layer1_start_sector = *(u32*)layer1.startingPhysicalSectorNumberOfDataArea;
		u32 layer1_end_sector = *(u32*)layer1.endPhysicalSectorNumberOfDataArea;

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = *(u32*)layer0.endSectorNumberInLayerZero;
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
#ifdef __APPLE__
	u8* buffer = (u8*)malloc(2048);
	dk_cd_read_toc_t cdrt;
	memset(&cdrt, 0, sizeof(dk_cd_read_toc_t));
	cdrt.format = kCDTOCFormatTOC;
	cdrt.formatAsTime = 1;
	cdrt.address.track = 0;
	cdrt.buffer = buffer;
	cdrt.bufferLength = 2048;
	memset(buffer, 0, 2048);

	if (ioctl(m_device, DKIOCCDREADTOC, &cdrt) == -1)
	{
		DevCon.Warning("CDVD: DKIOCCDREADTOC failed: %s\n", strerror(errno));
		return false;
	}

	CDTOC* toc = (CDTOC*)buffer;

	u32 desc_count = CDTOCGetDescriptorCount(toc);

	for (u32 i = 0; i < desc_count; ++i)
	{
		CDTOCDescriptor desc = toc->descriptors[i];
		if (desc.point < 0xa0 && desc.adr == 1)
		{
			u32 lba = CDConvertMSFToLBA(desc.p);
			m_toc.push_back({lba, desc.point, desc.adr, desc.control});
		}
		else if (desc.point == 0xa2) // lead out, use to get total sector count
		{
			m_sectors = CDConvertMSFToLBA(desc.p);
		}
	}

	m_media_type = -1;

	free(buffer);
	return true;
#else
	return false;
#endif
}

bool IOCtlSrc::DiscReady()
{
#ifdef __APPLE__
	if (m_device == -1)
		return false;

	if (!m_sectors)
	{
		Reopen();
	}

	return !!m_sectors;
#else
	return false;
#endif
}
