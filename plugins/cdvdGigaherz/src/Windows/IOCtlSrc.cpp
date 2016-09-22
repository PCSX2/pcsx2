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

#include "../CDVD.h"

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

s32 IOCtlSrc::Reopen()
{
    if (device != INVALID_HANDLE_VALUE) {
        DWORD size;
        DeviceIoControl(device, IOCTL_DVD_END_SESSION, &sessID, sizeof(DVD_SESSION_ID), NULL, 0, &size, NULL);
        CloseHandle(device);
    }

    DWORD share = FILE_SHARE_READ;
    DWORD flags = FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN;
    DWORD size;

    OpenOK = false;
    // SPTI only works if the device is opened with GENERIC_WRITE access.
    m_can_use_spti = true;
    device = CreateFile(fName, GENERIC_READ | GENERIC_WRITE | FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, 0);
    if (device == INVALID_HANDLE_VALUE) {
        device = CreateFile(fName, GENERIC_READ | FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, 0);
        if (device == INVALID_HANDLE_VALUE)
            return -1;
        m_can_use_spti = false;
    }
    // Dual layer DVDs cannot read from layer 1 without this ioctl
    DeviceIoControl(device, FSCTL_ALLOW_EXTENDED_DASD_IO, nullptr, 0, nullptr, 0, &size, nullptr);

    // FIXME: 0 is a valid session id, but the code assumes that it isn't.
    sessID = 0;
    DeviceIoControl(device, IOCTL_DVD_START_SESSION, NULL, 0, &sessID, sizeof(DVD_SESSION_ID), &size, NULL);

    tocCached = false;
    mediaTypeCached = false;
    discSizeCached = false;
    layerBreakCached = false;

    OpenOK = true;
    return 0;
}

IOCtlSrc::IOCtlSrc(const char *fileName)
{
    device = INVALID_HANDLE_VALUE;

    strcpy_s(fName, 256, fileName);

    Reopen();
    SetSpindleSpeed(false);
}

IOCtlSrc::~IOCtlSrc()
{
    if (OpenOK) {
        SetSpindleSpeed(true);
        DWORD size;
        DeviceIoControl(device, IOCTL_DVD_END_SESSION, &sessID, sizeof(DVD_SESSION_ID), NULL, 0, &size, NULL);

        CloseHandle(device);
    }
}

struct mycrap
{
    DWORD shit;
    DVD_LAYER_DESCRIPTOR ld;
    // The IOCTL_DVD_READ_STRUCTURE expects a size of at least 22 bytes when
    // reading the dvd physical layer descriptor
    // 4 bytes header
    // 17 bytes for the layer descriptor
    // 1 byte of the media specific data for no reason whatsoever...
    UCHAR fixup;
};

DVD_READ_STRUCTURE dvdrs;
mycrap dld;
CDROM_READ_TOC_EX tocrq = {0};

s32 IOCtlSrc::GetSectorCount()
{
    DWORD size;

    LARGE_INTEGER li;

    if (discSizeCached)
        return discSize;

    if (GetFileSizeEx(device, &li)) {
        discSizeCached = true;
        discSize = (s32)(li.QuadPart / 2048);
        return discSize;
    }

    GET_LENGTH_INFORMATION info;
    if (DeviceIoControl(device, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &info, sizeof(info), &size, NULL)) {
        discSizeCached = true;
        discSize = (s32)(info.Length.QuadPart / 2048);
        return discSize;
    }

    memset(&tocrq, 0, sizeof(CDROM_READ_TOC_EX));
    tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
    tocrq.Msf = 1;
    tocrq.SessionTrack = 1;

    CDROM_TOC_FULL_TOC_DATA *ftd = (CDROM_TOC_FULL_TOC_DATA *)sectorbuffer;

    if (DeviceIoControl(device, IOCTL_CDROM_READ_TOC_EX, &tocrq, sizeof(tocrq), ftd, 2048, &size, NULL)) {
        for (int i = 0; i < 101; i++) {
            if (ftd->Descriptors[i].Point == 0xa2) {
                if (ftd->Descriptors[i].SessionNumber == ftd->LastCompleteSession) {
                    int min = ftd->Descriptors[i].Msf[0];
                    int sec = ftd->Descriptors[i].Msf[1];
                    int frm = ftd->Descriptors[i].Msf[2];

                    discSizeCached = true;
                    discSize = (s32)MSF_TO_LBA(min, sec, frm);
                    return discSize;
                }
            }
        }
    }

    dvdrs.BlockByteOffset.QuadPart = 0;
    dvdrs.Format = DvdPhysicalDescriptor;
    dvdrs.SessionId = sessID;
    dvdrs.LayerNumber = 0;
    if (DeviceIoControl(device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs), &dld, sizeof(dld), &size, NULL) != 0) {
        s32 sectors1 = _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector) + 1;
        if (dld.ld.NumberOfLayers == 1) {  // PTP, OTP
            if (dld.ld.TrackPath == 0) {   // PTP
                dvdrs.LayerNumber = 1;
                if (DeviceIoControl(device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs), &dld, sizeof(dld), &size, nullptr) != 0) {
                    sectors1 += _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector) + 1;
                }
            } else {  // OTP
                // sectors = end_sector - (~end_sector_l0 & 0xFFFFFF) + end_sector_l0 - start_sector
                dld.ld.EndLayerZeroSector = _byteswap_ulong(dld.ld.EndLayerZeroSector);
                sectors1 += dld.ld.EndLayerZeroSector - (~dld.ld.EndLayerZeroSector & 0x00FFFFFF) + 1;
            }
        }

        discSizeCached = true;
        discSize = sectors1;
        return discSize;
    }

    return -1;
}

s32 IOCtlSrc::GetLayerBreakAddress()
{
    DWORD size;

    if (GetMediaType() < 0)
        return -1;

    if (layerBreakCached)
        return layerBreak;

    dvdrs.BlockByteOffset.QuadPart = 0;
    dvdrs.Format = DvdPhysicalDescriptor;
    dvdrs.SessionId = sessID;
    dvdrs.LayerNumber = 0;
    if (DeviceIoControl(device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs), &dld, sizeof(dld), &size, nullptr)) {
        if (dld.ld.NumberOfLayers == 0) {  // Single layer
            layerBreak = 0;
        } else if (dld.ld.TrackPath == 0) {  // PTP
            layerBreak = _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector);
        } else {  // OTP
            layerBreak = _byteswap_ulong(dld.ld.EndLayerZeroSector) - _byteswap_ulong(dld.ld.StartingDataSector);
        }

        layerBreakCached = true;
        return layerBreak;
    }

    //if not a cd, and fails, assume single layer
    return 0;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults)
{

    DWORD dontcare;
    USHORT speed = 0;

    if (GetMediaType() < 0)
        speed = 4800;  // CD-ROM to ~32x (PS2 has 24x (3600 KB/s))
    else
        speed = 11080;  // DVD-ROM to  ~8x (PS2 has 4x (5540 KB/s))

    if (!restore_defaults) {
        CDROM_SET_SPEED s;
        s.RequestType = CdromSetSpeed;
        s.RotationControl = CdromDefaultRotation;
        s.ReadSpeed = speed;
        s.WriteSpeed = speed;

        if (DeviceIoControl(device,
                            IOCTL_CDROM_SET_SPEED,  //operation to perform
                            &s, sizeof(s),          //no input buffer
                            NULL, 0,                //output buffer
                            &dontcare,              //#bytes returned
                            (LPOVERLAPPED)NULL))    //synchronous I/O == 0)
        {
            printf(" * CDVD: setSpindleSpeed success (%uKB/s)\n", speed);
        } else {
            printf(" * CDVD: setSpindleSpeed failed! \n");
        }
    } else {
        CDROM_SET_SPEED s;
        s.RequestType = CdromSetSpeed;
        s.RotationControl = CdromDefaultRotation;
        s.ReadSpeed = 0xffff;  // maximum ?
        s.WriteSpeed = 0xffff;

        DeviceIoControl(device,
                        IOCTL_CDROM_SET_SPEED,  //operation to perform
                        &s, sizeof(s),          //no input buffer
                        NULL, 0,                //output buffer
                        &dontcare,              //#bytes returned
                        (LPOVERLAPPED)NULL);    //synchronous I/O == 0)
    }
}

s32 IOCtlSrc::GetMediaType()
{
    DWORD size;

    if (mediaTypeCached)
        return mediaType;

    memset(&tocrq, 0, sizeof(CDROM_READ_TOC_EX));
    tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
    tocrq.Msf = 1;
    tocrq.SessionTrack = 1;

    CDROM_TOC_FULL_TOC_DATA *ftd = (CDROM_TOC_FULL_TOC_DATA *)sectorbuffer;

    if (DeviceIoControl(device, IOCTL_CDROM_READ_TOC_EX, &tocrq, sizeof(tocrq), ftd, 2048, &size, NULL)) {
        mediaTypeCached = true;
        mediaType = -1;
        return mediaType;
    }

    dvdrs.BlockByteOffset.QuadPart = 0;
    dvdrs.Format = DvdPhysicalDescriptor;
    dvdrs.SessionId = sessID;
    dvdrs.LayerNumber = 0;
    if (DeviceIoControl(device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs), &dld, sizeof(dld), &size, nullptr)) {
        if (dld.ld.NumberOfLayers == 0) {  // Single layer
            mediaType = 0;
        } else if (dld.ld.TrackPath == 0) {  // PTP
            mediaType = 1;
        } else {  // OTP
            mediaType = 2;
        }

        mediaTypeCached = true;
        return mediaType;
    }

    //if not a cd, and fails, assume single layer
    mediaTypeCached = true;
    mediaType = 0;
    return mediaType;
}

s32 IOCtlSrc::ReadTOC(char *toc, int msize)
{
    DWORD size = 0;

    if (GetMediaType() >= 0)
        return -1;

    if (!tocCached) {
        memset(&tocrq, 0, sizeof(CDROM_READ_TOC_EX));
        tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
        tocrq.Msf = 1;
        tocrq.SessionTrack = 1;

        if (!OpenOK)
            return -1;

        int code = DeviceIoControl(device, IOCTL_CDROM_READ_TOC_EX, &tocrq, sizeof(tocrq), tocCacheData, 2048, &size, NULL);

        if (code == 0)
            return -1;

        tocCached = true;
    }

    memcpy(toc, tocCacheData, min(2048, msize));

    return 0;
}

s32 IOCtlSrc::ReadSectors2048(u32 sector, u32 count, char *buffer)
{
    RAW_READ_INFO rri;

    DWORD size = 0;

    if (!OpenOK)
        return -1;

    rri.DiskOffset.QuadPart = sector * (u64)2048;
    rri.SectorCount = count;

    //fall back to standard reading
    if (SetFilePointer(device, rri.DiskOffset.LowPart, &rri.DiskOffset.HighPart, FILE_BEGIN) == -1) {
        if (GetLastError() != 0)
            return -1;
    }

    if (ReadFile(device, buffer, 2048 * count, &size, NULL) == 0) {
        return -1;
    }

    if (size != (2048 * count)) {
        return -1;
    }

    return 0;
}


// FIXME: Probably doesn't work if the sectors to be read are from two tracks
// of different types.
s32 IOCtlSrc::ReadSectors2352(u32 sector, u32 count, char *buffer)
{
    RAW_READ_INFO rri;

    DWORD size = 0;

    if (!OpenOK)
        return -1;

    if (m_can_use_spti) {
        struct sptdinfo
        {
            SCSI_PASS_THROUGH_DIRECT info;
            char sense_buffer[20];
        } sptd = {};

        // READ CD command
        sptd.info.Cdb[0] = 0xBE;
        // Don't care about sector type.
        sptd.info.Cdb[1] = 0;
        sptd.info.Cdb[2] = (sector >> 24) & 0xFF;
        sptd.info.Cdb[3] = (sector >> 16) & 0xFF;
        sptd.info.Cdb[4] = (sector >> 8) & 0xFF;
        sptd.info.Cdb[5] = sector & 0xFF;
        sptd.info.Cdb[6] = (count >> 16) & 0xFF;
        sptd.info.Cdb[7] = (count >> 8) & 0xFF;
        sptd.info.Cdb[8] = count & 0xFF;
        // Sync + all headers + user data + EDC/ECC. Excludes C2 + subchannel
        sptd.info.Cdb[9] = 0xF8;
        sptd.info.Cdb[10] = 0;
        sptd.info.Cdb[11] = 0;

        sptd.info.CdbLength = 12;
        sptd.info.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
        sptd.info.DataIn = SCSI_IOCTL_DATA_IN;
        sptd.info.DataTransferLength = 2352 * count;
        sptd.info.DataBuffer = buffer;
        sptd.info.SenseInfoLength = sizeof(sptd.sense_buffer);
        sptd.info.SenseInfoOffset = offsetof(sptdinfo, sense_buffer);
        sptd.info.TimeOutValue = 5;

        if (DeviceIoControl(device, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sptd,
                            sizeof(sptd), &sptd, sizeof(sptd), &size, nullptr)) {
            if (sptd.info.DataTransferLength == 2352 * count)
                return 0;
        }
        printf(" * CDVD: SPTI failed reading sectors %u-%u\n", sector, sector + count - 1);
    }

    rri.DiskOffset.QuadPart = sector * (u64)2048;
    rri.SectorCount = count;

    rri.TrackMode = (TRACK_MODE_TYPE)last_read_mode;
    if (DeviceIoControl(device, IOCTL_CDROM_RAW_READ, &rri, sizeof(rri), buffer, 2352 * count, &size, NULL) == 0) {
        rri.TrackMode = XAForm2;
        printf(" * CDVD: CD-ROM read mode change\n");
        printf(" * CDVD: Trying XAForm2\n");
        if (DeviceIoControl(device, IOCTL_CDROM_RAW_READ, &rri, sizeof(rri), buffer, 2352 * count, &size, NULL) == 0) {
            rri.TrackMode = YellowMode2;
            printf(" * CDVD: Trying YellowMode2\n");
            if (DeviceIoControl(device, IOCTL_CDROM_RAW_READ, &rri, sizeof(rri), buffer, 2352 * count, &size, NULL) == 0) {
                rri.TrackMode = CDDA;
                printf(" * CDVD: Trying CDDA\n");
                if (DeviceIoControl(device, IOCTL_CDROM_RAW_READ, &rri, sizeof(rri), buffer, 2352 * count, &size, NULL) == 0) {
                    printf(" * CDVD: Failed to read this CD-ROM with error code: %u\n", GetLastError());
                    return -1;
                }
            }
        }
    }

    last_read_mode = rri.TrackMode;

    if (size != (2352 * count)) {
        return -1;
    }

    return 0;
}

s32 IOCtlSrc::DiscChanged()
{
    DWORD size = 0;

    if (!OpenOK)
        return -1;

    int ret = DeviceIoControl(device, IOCTL_STORAGE_CHECK_VERIFY, NULL, 0, NULL, 0, &size, NULL);

    if (ret == 0) {
        tocCached = false;
        mediaTypeCached = false;
        discSizeCached = false;
        layerBreakCached = false;

        if (sessID != 0) {
            DeviceIoControl(device, IOCTL_DVD_END_SESSION, &sessID, sizeof(DVD_SESSION_ID), NULL, 0, &size, NULL);
            sessID = 0;
        }
        return 1;
    }

    if (sessID == 0) {
        DeviceIoControl(device, IOCTL_DVD_START_SESSION, NULL, 0, &sessID, sizeof(DVD_SESSION_ID), &size, NULL);
    }

    return 0;
}

s32 IOCtlSrc::IsOK()
{
    return OpenOK;
}
