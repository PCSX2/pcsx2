/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

#include "common/WindowInfo.h"
#include "SaveState.h"
#include "pcsx2/Config.h"
#include "pcsx2/GS/config.h"
#include "gsl/span"

#include <map>

// ST_WRITE is defined in libc, avoid this
enum stateType
{
	SAVE_WRITE,
	SAVE_TRANSFER,
	SAVE_VSYNC
};

enum class GSVideoMode : u8
{
	Unknown,
	NTSC,
	PAL,
	VESA,
	SDTV_480P,
	HDTV_720P,
	HDTV_1080I
};

extern Pcsx2Config::GSOptions GSConfig;

class HostDisplay;

int GSinit();
void GSshutdown();
bool GSopen(const Pcsx2Config::GSOptions& config, GSRendererType renderer, u8* basemem);
bool GSreopen(bool recreate_display, const Pcsx2Config::GSOptions& old_config);
void GSreset(bool hardware_reset);
void GSclose();
void GSgifSoftReset(u32 mask);
void GSwriteCSR(u32 csr);
void GSInitAndReadFIFO(u8* mem, u32 size);
void GSReadLocalMemoryUnsync(u8* mem, u32 qwc, u64 BITBLITBUF, u64 TRXPOS, u64 TRXREG);
void GSgifTransfer(const u8* mem, u32 size);
void GSgifTransfer1(u8* mem, u32 addr);
void GSgifTransfer2(u8* mem, u32 size);
void GSgifTransfer3(u8* mem, u32 size);
void GSvsync(u32 field, bool registers_written);
int GSfreeze(FreezeAction mode, freezeData* data);
std::string GSGetBaseSnapshotFilename();
void GSQueueSnapshot(const std::string& path, u32 gsdump_frames = 0);
void GSStopGSDump();
bool GSBeginCapture(std::string filename);
void GSEndCapture();
void GSPresentCurrentFrame();
void GSThrottlePresentation();
void GSsetGameCRC(u32 crc, int options);

GSVideoMode GSgetDisplayMode();
void GSgetInternalResolution(int* width, int* height);
void GSgetStats(std::string& info);
void GSgetTitleStats(std::string& info);

/// Converts window position to normalized display coordinates (0..1). A value less than 0 or greater than 1 is
/// returned if the position lies outside the display area.
void GSTranslateWindowToDisplayCoordinates(float window_x, float window_y, float* display_x, float* display_y);

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config);
void GSSwitchRenderer(GSRendererType new_renderer);
void GSResetAPIState();
void GSRestoreAPIState();
bool GSSaveSnapshotToMemory(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
	u32* width, u32* height, std::vector<u32>* pixels);
void GSJoinSnapshotThreads();

struct GSError
{
};
struct GSRecoverableError : GSError
{
};
