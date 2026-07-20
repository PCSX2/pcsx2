// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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

struct GSAdapterInfo
{
	std::string name;
	std::vector<std::string> fullscreen_modes;
	u32 max_texture_size;
	u32 max_upscale_multiplier;
};

class SmallStringBase;

// Returns the ID for the specified function, otherwise -1.
s16 GSLookupGetSkipCountFunctionId(const std::string_view name);
s16 GSLookupBeforeDrawFunctionId(const std::string_view name);
s16 GSLookupMoveHandlerFunctionId(const std::string_view name);

bool GSopen(const Pcsx2Config::GSOptions& config, GSRendererType renderer, u8* basemem,
	GSVSyncMode vsync_mode, bool allow_present_throttle);
bool GSreopen(bool recreate_device, bool recreate_renderer, GSRendererType new_renderer,
	std::optional<const Pcsx2Config::GSOptions*> old_config);
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
// Manual frameskip (Android low-end devices): present 1 of every (frames+1)
// VSyncs, skipping presentation of the rest. 0 disables. See GSRenderer::VSync.
void GSSetManualFrameSkip(u32 frames);
u32 GSGetManualFrameSkip();
// Max presented-FPS cap (Android). Caps the DISPLAY frame rate without touching
// emulation speed — dropped on the GS thread in GSRenderer::VSync. 0 disables.
void GSSetMaxPresentFps(u32 fps, u64 present_interval);
u32 GSGetMaxPresentFps();
u64 GSGetMaxPresentInterval();
// While true (set when the limiter enters Turbo / fast-forward), the present cap
// above is bypassed so the speed-up is actually visible. The cap resumes — with
// a clean re-prime, no catch-up burst — as soon as fast-forward ends.
void GSSetPresentCapSuspended(bool suspended);
bool GSGetPresentCapSuspended();
int GSfreeze(FreezeAction mode, freezeData* data);
std::string GSGetBaseSnapshotFilename();
std::string GSGetBaseVideoFilename();
void GSQueueSnapshot(const std::string& path, u32 gsdump_frames = 0);
void GSStopGSDump();
bool GSBeginCapture(std::string filename);
void GSEndCapture();
void GSPresentCurrentFrame();
void GSThrottlePresentation();
void GSGameChanged();
void GSSetDisplayAlignment(GSDisplayAlignment alignment);
void GSSetPortraitRenderTopAlign(bool enabled);
bool GSHasDisplayWindow();
void GSResizeDisplayWindow(u32 width, u32 height, float scale);
void GSUpdateDisplayWindow();
void GSSetVSyncMode(GSVSyncMode mode, bool allow_present_throttle);

GSRendererType GSGetCurrentRenderer();
bool GSIsHardwareRenderer();
std::string GetDefaultAdapter();
bool GSWantsExclusiveFullscreen();
std::optional<float> GSGetHostRefreshRate();
std::vector<GSAdapterInfo> GSGetAdapterInfo(GSRendererType renderer);
u32 GSGetMaxUpscaleMultiplier(u32 max_texture_size);
GSVideoMode GSgetDisplayMode();
void GSgetInternalResolution(int* width, int* height);
void GSgetStats(SmallStringBase& info);
void GSgetMemoryStats(SmallStringBase& info);
void GSgetTitleStats(std::string& info);

/// Converts window position to normalized display coordinates (0..1). A value less than 0 or greater than 1 is
/// returned if the position lies outside the display area.
void GSTranslateWindowToDisplayCoordinates(float window_x, float window_y, float* display_x, float* display_y);

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config);
void GSSetSoftwareRendering(bool software_renderer, GSInterlaceMode new_interlace);
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

	/// Called when video capture starts or stops. Called on the MTGS thread.
	void OnCaptureStarted(const std::string& filename);
	void OnCaptureStopped();
}

extern Pcsx2Config::GSOptions GSConfig;
