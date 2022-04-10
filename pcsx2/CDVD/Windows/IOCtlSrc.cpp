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

#include <winioctl.h>
#include <ntddcdvd.h>
#include <ntddcdrm.h>
// "typedef ignored" warning will disappear once we move to the Windows 10 SDK.
#pragma warning(push)
#pragma warning(disable : 4091)
#include <ntddscsi.h>
#pragma warning(pop)

#include <cstddef>
#include <cstdlib>
#include <stdexcept>

IOCtlSrc::IOCtlSrc(std::string filename)
	: m_filename(std::move(filename))
{
	if (!Reopen())
		throw std::runtime_error(" * CDVD: Error opening source.\n");
}

IOCtlSrc::~IOCtlSrc()
{
	if (m_device != INVALID_HANDLE_VALUE)
	{
		SetSpindleSpeed(true);
		CloseHandle(m_device);
	}
}

// If a new disc is inserted, ReadFile will fail unless the device is closed
// and reopened.
bool IOCtlSrc::Reopen()
{
	if (m_device != INVALID_HANDLE_VALUE)
		CloseHandle(m_device);

	// SPTI only works if the device is opened with GENERIC_WRITE access.
	m_device = CreateFileA(m_filename.c_str(), GENERIC_READ | GENERIC_WRITE,
						  FILE_SHARE_READ, nullptr, OPEN_EXISTING,
						  FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (m_device == INVALID_HANDLE_VALUE)
		return false;

	DWORD unused;
	// Required to read from layer 1 of Dual layer DVDs
	DeviceIoControl(m_device, FSCTL_ALLOW_EXTENDED_DASD_IO, nullptr, 0, nullptr,
					0, &unused, nullptr);

	if (ReadDVDInfo() || ReadCDInfo())
		SetSpindleSpeed(false);

	return true;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults) const
{
	// IOCTL_CDROM_SET_SPEED issues a SET CD SPEED command. So 0xFFFF should be
	// equivalent to "optimal performance".
	// 1x DVD-ROM and CD-ROM speeds are respectively 1385 KB/s and 150KB/s.
	// The PS2 can do 4x DVD-ROM and 24x CD-ROM speeds (5540KB/s and 3600KB/s).
	// TODO: What speed? Performance seems smoother with a lower speed (less
	// time required to get up to speed).
	const USHORT speed = restore_defaults ? 0xFFFF : GetMediaType() >= 0 ? 5540 : 3600;
	CDROM_SET_SPEED s{CdromSetSpeed, speed, speed, CdromDefaultRotation};

	DWORD unused;
	if (DeviceIoControl(m_device, IOCTL_CDROM_SET_SPEED, &s, sizeof(s),
						nullptr, 0, &unused, nullptr))
	{
		if (!restore_defaults)
			printf(" * CDVD: setSpindleSpeed success (%uKB/s)\n", speed);
	}
	else
	{
		printf(" * CDVD: setSpindleSpeed failed!\n");
	}
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
	std::lock_guard<std::mutex> guard(m_lock);
	LARGE_INTEGER offset;
	offset.QuadPart = sector * 2048ULL;

	if (!SetFilePointerEx(m_device, offset, nullptr, FILE_BEGIN))
	{
		fprintf(stderr, " * CDVD SetFilePointerEx failed: sector %u: error %u\n",
				sector, GetLastError());
		return false;
	}

	const DWORD bytes_to_read = 2048 * count;
	DWORD bytes_read;
	if (ReadFile(m_device, buffer, bytes_to_read, &bytes_read, nullptr))
	{
		if (bytes_read == bytes_to_read)
			return true;
		fprintf(stderr, " * CDVD ReadFile: sectors %u-%u: %u bytes read, %u bytes expected\n",
				sector, sector + count - 1, bytes_read, bytes_to_read);
	}
	else
	{
		fprintf(stderr, " * CDVD ReadFile failed: sectors %u-%u: error %u\n",
				sector, sector + count - 1, GetLastError());
	}

	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
	struct sptdinfo
	{
		SCSI_PASS_THROUGH_DIRECT info;
		char sense_buffer[20];
	} sptd{};

	// READ CD command
	sptd.info.Cdb[0] = 0xBE;
	// Don't care about sector type.
	sptd.info.Cdb[1] = 0;
	// Number of sectors to read
	sptd.info.Cdb[6] = 0;
	sptd.info.Cdb[7] = 0;
	sptd.info.Cdb[8] = 1;
	// Sync + all headers + user data + EDC/ECC. Excludes C2 + subchannel
	sptd.info.Cdb[9] = 0xF8;
	sptd.info.Cdb[10] = 0;
	sptd.info.Cdb[11] = 0;

	sptd.info.CdbLength = 12;
	sptd.info.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sptd.info.DataIn = SCSI_IOCTL_DATA_IN;
	sptd.info.SenseInfoOffset = offsetof(sptdinfo, sense_buffer);
	sptd.info.TimeOutValue = 5;

	// Read sectors one by one to avoid reading data from 2 tracks of different
	// types in the same read (which will fail).
	for (u32 n = 0; n < count; ++n)
	{
		u32 current_sector = sector + n;
		sptd.info.Cdb[2] = (current_sector >> 24) & 0xFF;
		sptd.info.Cdb[3] = (current_sector >> 16) & 0xFF;
		sptd.info.Cdb[4] = (current_sector >> 8) & 0xFF;
		sptd.info.Cdb[5] = current_sector & 0xFF;
		sptd.info.DataTransferLength = 2352;
		sptd.info.DataBuffer = buffer + 2352 * n;
		sptd.info.SenseInfoLength = sizeof(sptd.sense_buffer);

		DWORD unused;
		if (DeviceIoControl(m_device, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sptd,
							sizeof(sptd), &sptd, sizeof(sptd), &unused, nullptr))
		{
			if (sptd.info.DataTransferLength == 2352)
				continue;
		}
		printf(" * CDVD: SPTI failed reading sector %u; SENSE %u -", current_sector, sptd.info.SenseInfoLength);
		for (const auto& c : sptd.sense_buffer)
			printf(" %02X", c);
		putchar('\n');
		return false;
	}

	return true;
}

bool IOCtlSrc::ReadDVDInfo()
{
	DWORD unused;
	// 4 bytes header + 18 bytes layer descriptor - Technically you only need
	// to read 17 bytes of the layer descriptor since bytes 17-2047 is for
	// media specific information. However, Windows requires you to read at
	// least 18 bytes of the layer descriptor or else the ioctl will fail. The
	// media specific information seems to be empty, so there's no point reading
	// any more than that.
	
	// UPDATE 15 Jan 2021
	// Okay so some drives seem to have descriptors BIGGER than 22 bytes!
	// This causes the read to fail with INVALID_PARAMETER.
	// So lets just give it 32 bytes to play with, it seems happy enough with that.
	// Refraction
	std::array<u8, 32> buffer;
	DVD_READ_STRUCTURE dvdrs{{0}, DvdPhysicalDescriptor, 0, 0};

	if (!DeviceIoControl(m_device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs),
						 buffer.data(), buffer.size(), &unused, nullptr))
	{
		if ((GetLastError() == ERROR_INVALID_FUNCTION) || (GetLastError() == ERROR_NOT_SUPPORTED))
		{
			Console.Warning("IOCTL_DVD_READ_STRUCTURE not supported");
		}
		else if(GetLastError() != ERROR_UNRECOGNIZED_MEDIA) // ERROR_UNRECOGNIZED_MEDIA means probably a CD or no disc
		{
			Console.Warning("IOCTL Unknown Error %d", GetLastError());
		}
		return false;
	}

	auto& layer = *reinterpret_cast<DVD_LAYER_DESCRIPTOR*>(
		reinterpret_cast<DVD_DESCRIPTOR_HEADER*>(buffer.data())->Data);

	u32 start_sector = _byteswap_ulong(layer.StartingDataSector);
	u32 end_sector = _byteswap_ulong(layer.EndDataSector);

	if (layer.NumberOfLayers == 0)
	{
		// Single layer
		m_media_type = 0;
		m_layer_break = 0;
		m_sectors = end_sector - start_sector + 1;
	}
	else if (layer.TrackPath == 0)
	{
		// Dual layer, Parallel Track Path
		dvdrs.LayerNumber = 1;
		if (!DeviceIoControl(m_device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs),
							 buffer.data(), buffer.size(), &unused, nullptr))
			return false;
		u32 layer1_start_sector = _byteswap_ulong(layer.StartingDataSector);
		u32 layer1_end_sector = _byteswap_ulong(layer.EndDataSector);

		m_media_type = 1;
		m_layer_break = end_sector - start_sector;
		m_sectors = end_sector - start_sector + 1 + layer1_end_sector - layer1_start_sector + 1;
	}
	else
	{
		// Dual layer, Opposite Track Path
		u32 end_sector_layer0 = _byteswap_ulong(layer.EndLayerZeroSector);
		m_media_type = 2;
		m_layer_break = end_sector_layer0 - start_sector;
		m_sectors = end_sector_layer0 - start_sector + 1 + end_sector - (~end_sector_layer0 & 0xFFFFFFU) + 1;
	}

	return true;
}

bool IOCtlSrc::ReadCDInfo()
{
	DWORD unused;
	CDROM_READ_TOC_EX toc_ex{};
	toc_ex.Format = CDROM_READ_TOC_EX_FORMAT_TOC;
	toc_ex.Msf = 0;
	toc_ex.SessionTrack = 1;

	CDROM_TOC toc;
	if (!DeviceIoControl(m_device, IOCTL_CDROM_READ_TOC_EX, &toc_ex,
						 sizeof(toc_ex), &toc, sizeof(toc), &unused, nullptr))
		return false;

	m_toc.clear();
	size_t track_count = ((toc.Length[0] << 8) + toc.Length[1] - 2) / sizeof(TRACK_DATA);
	for (size_t n = 0; n < track_count; ++n)
	{
		TRACK_DATA& track = toc.TrackData[n];
		// Exclude the lead-out track descriptor.
		if (track.TrackNumber == 0xAA)
			continue;
		u32 lba = (track.Address[1] << 16) + (track.Address[2] << 8) + track.Address[3];
		m_toc.push_back({lba, track.TrackNumber, track.Adr, track.Control});
	}

	GET_LENGTH_INFORMATION info;
	if (!DeviceIoControl(m_device, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &info,
						 sizeof(info), &unused, nullptr))
		return false;

	m_sectors = static_cast<u32>(info.Length.QuadPart / 2048);
	m_media_type = -1;

	return true;
}

bool IOCtlSrc::DiscReady()
{
	if (m_device == INVALID_HANDLE_VALUE)
		return false;

	DWORD unused;
	if (DeviceIoControl(m_device, IOCTL_STORAGE_CHECK_VERIFY, nullptr, 0,
						nullptr, 0, &unused, nullptr))
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
}
