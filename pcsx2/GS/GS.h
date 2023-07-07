/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class RenderAPI
{
	None,
	D3D11,
	Metal,
	D3D12,
	Vulkan,
	OpenGL
};

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

enum class GSDisplayAlignment
{
	Center,
	LeftOrTop,
	RightOrBottom
};

extern Pcsx2Config::GSOptions GSConfig;

class HostDisplay;

// Returns the ID for the specified function, otherwise -1.
s16 GSLookupGetSkipCountFunctionId(const std::string_view& name);
s16 GSLookupBeforeDrawFunctionId(const std::string_view& name);
s16 GSLookupMoveHandlerFunctionId(const std::string_view& name);

void GSinit();
void GSshutdown();
bool GSopen(const Pcsx2Config::GSOptions& config, GSRendererType renderer, u8* basemem);
bool GSreopen(bool recreate_device, bool recreate_renderer, const Pcsx2Config::GSOptions& old_config);
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
std::string GSGetBaseVideoFilename();
void GSQueueSnapshot(const std::string& path, u32 gsdump_frames = 0);
void GSStopGSDump();
bool GSBeginCapture(std::string filename);
void GSEndCapture();
void GSPresentCurrentFrame();
void GSThrottlePresentation();
void GSSetGameCRC(u32 crc);
void GSSetDisplayAlignment(GSDisplayAlignment alignment);
void GSResizeDisplayWindow(int width, int height, float scale);
void GSUpdateDisplayWindow();
void GSSetVSyncMode(VsyncMode mode);

bool GSWantsExclusiveFullscreen();
bool GSGetHostRefreshRate(float* refresh_rate);
void GSGetAdaptersAndFullscreenModes(
	GSRendererType renderer, std::vector<std::string>* adapters, std::vector<std::string>* fullscreen_modes);
GSVideoMode GSgetDisplayMode();
void GSgetInternalResolution(int* width, int* height);
void GSgetStats(std::string& info);
void GSgetMemoryStats(std::string& info);
void GSgetTitleStats(std::string& info);

/// Converts window position to normalized display coordinates (0..1). A value less than 0 or greater than 1 is
/// returned if the position lies outside the display area.
void GSTranslateWindowToDisplayCoordinates(float window_x, float window_y, float* display_x, float* display_y);

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config);
void GSSwitchRenderer(GSRendererType new_renderer);
bool GSSaveSnapshotToMemory(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
	u32* width, u32* height, std::vector<u32>* pixels);
void GSJoinSnapshotThreads();

namespace Host
{
	/// Called when the GS is creating a render device.
	/// This could also be fullscreen transition.
	std::optional<WindowInfo> AcquireRenderWindow(bool recreate_window);

	/// Called before drawing the OSD and other display elements.
	void BeginPresentFrame();

	/// Called when the GS is finished with a render window.
	void ReleaseRenderWindow();

	/// Returns true if the hosting application is currently fullscreen.
	bool IsFullscreen();

	/// Alters fullscreen state of hosting application.
	void SetFullscreen(bool enabled);

	/// Returns the desired vsync mode, depending on the runtime environment.
	VsyncMode GetEffectiveVSyncMode();

	/// Called when video capture starts or stops. Called on the MTGS thread.
	void OnCaptureStarted(const std::string& filename);
	void OnCaptureStopped();
}
