// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Stubs for functionality not available/needed on Android.

#include "PrecompiledHeader.h"

#include "Input/InputManager.h"
#include "CDVD/CDVDdiscReader.h"

// g_host_hotkeys - normally defined in pcsx2-qt
BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()

// Host::SetMouseLock - no mouse lock on Android
void Host::SetMouseLock(bool state)
{
}

// HTTPDownloader::Create is now provided by common/HTTPDownloaderAndroid.cpp,
// which bridges to java.net.HttpURLConnection via JNI. The stub that
// returned null lived here previously — RA login + cover downloads
// silently failed and the cleanup path crashed when the unique_ptr was
// dereferenced. See HTTPDownloaderAndroid.{h,cpp}.

// Optical drive / disc reader stubs - no physical disc on Android
std::vector<std::string> GetOpticalDriveList()
{
	return {};
}

void GetValidDrive(std::string& drive)
{
}

// IOCtlSrc stubs
IOCtlSrc::IOCtlSrc(std::string filename)
{
}

IOCtlSrc::~IOCtlSrc()
{
}

bool IOCtlSrc::Reopen(Error* error)
{
	return false;
}

bool IOCtlSrc::DiscReady()
{
	return false;
}

u32 IOCtlSrc::GetSectorCount() const
{
	return 0;
}

s32 IOCtlSrc::GetMediaType() const
{
	return 0;
}

const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const
{
	static const std::vector<toc_entry> empty;
	return empty;
}

bool IOCtlSrc::ReadSectors2048(u32 sector, u32 count, u8* buffer) const
{
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32 sector, u32 count, u8* buffer) const
{
	return false;
}

bool IOCtlSrc::ReadTrackSubQ(cdvdSubQ* subq) const
{
	return false;
}

u32 IOCtlSrc::GetLayerBreakAddress() const
{
	return 0;
}
