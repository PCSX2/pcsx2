/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "GS.h"
#include "GSCapture.h"
#include "GSGL.h"
#include "GSUtil.h"
#include "GSExtra.h"
#include "Renderers/Null/GSRendererNull.h"
#include "Renderers/Null/GSDeviceNull.h"
#include "Renderers/HW/GSRendererHW.h"
#include "Renderers/HW/GSTextureReplacements.h"
#include "GSLzma.h"
#include "MultiISA.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "pcsx2/Config.h"
#include "pcsx2/Counters.h"
#include "pcsx2/Host.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/Frontend/FullscreenUI.h"
#include "pcsx2/Frontend/InputManager.h"
#include "pcsx2/GS.h"

#ifdef ENABLE_OPENGL
#include "Renderers/OpenGL/GSDeviceOGL.h"
#endif

#ifdef __APPLE__
#include "Renderers/Metal/GSMetalCPPAccessible.h"
#endif

#ifdef ENABLE_VULKAN
#include "Renderers/Vulkan/GSDeviceVK.h"
#endif

#ifdef _WIN32

#include "Renderers/DX11/GSDevice11.h"
#include "Renderers/DX12/GSDevice12.h"
#include "GS/Renderers/DX11/D3D.h"


static HRESULT s_hr = E_FAIL;

#endif

#include <fstream>

// do NOT undefine this/put it above includes, as x11 people love to redefine
// things that make obscure compiler bugs, unless you want to run around and
// debug obscure compiler errors --govanify
#undef None

Pcsx2Config::GSOptions GSConfig;

static RenderAPI s_render_api;
static u64 s_next_manual_present_time;

int GSinit()
{
	GSVertexSW::InitStatic();

	GSUtil::Init();

#ifdef _WIN32
	s_hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

	return 0;
}

void GSshutdown()
{
	GSclose();

#ifdef _WIN32
	if (SUCCEEDED(s_hr))
	{
		::CoUninitialize();

		s_hr = E_FAIL;
	}
#endif

	// ensure all screenshots have been saved
	GSJoinSnapshotThreads();
}

void GSclose()
{
	if (g_gs_renderer)
	{
		g_gs_renderer->Destroy();
		g_gs_renderer.reset();
	}
	if (g_gs_device)
	{
		g_gs_device->Destroy();
		g_gs_device.reset();
	}

	if (g_host_display)
		g_host_display->SetGPUTimingEnabled(false);

	Host::ReleaseHostDisplay(true);
}

static RenderAPI GetAPIForRenderer(GSRendererType renderer)
{
#if defined(_WIN32)
	// On Windows, we use DX11 for software, since it's always available.
	constexpr RenderAPI default_api = RenderAPI::D3D11;
#elif defined(__APPLE__)
	// For Macs, default to Metal.
	constexpr RenderAPI default_api = RenderAPI::Metal;
#else
	// For Linux, default to OpenGL (because of hardware compatibility), if we
	// have it, otherwise Vulkan (if we have it).
#if defined(ENABLE_OPENGL)
	constexpr RenderAPI default_api = RenderAPI::OpenGL;
#elif defined(ENABLE_VULKAN)
	constexpr RenderAPI default_api = RenderAPI::Vulkan;
#else
	constexpr RenderAPI default_api = RenderAPI::None;
#endif
#endif

	switch (renderer)
	{
		case GSRendererType::OGL:
			return RenderAPI::OpenGL;

		case GSRendererType::VK:
			return RenderAPI::Vulkan;

#ifdef _WIN32
		case GSRendererType::DX11:
			return RenderAPI::D3D11;

		case GSRendererType::DX12:
			return RenderAPI::D3D12;
#endif

#ifdef __APPLE__
		case GSRendererType::Metal:
			return RenderAPI::Metal;
#endif

		default:
			return default_api;
	}
}

static bool DoGSOpen(GSRendererType renderer, u8* basemem)
{
	s_render_api = g_host_display->GetRenderAPI();

	switch (g_host_display->GetRenderAPI())
	{
#ifdef _WIN32
		case RenderAPI::D3D11:
			g_gs_device = std::make_unique<GSDevice11>();
			break;
		case RenderAPI::D3D12:
			g_gs_device = std::make_unique<GSDevice12>();
			break;
#endif
#ifdef __APPLE__
		case RenderAPI::Metal:
			g_gs_device = std::unique_ptr<GSDevice>(MakeGSDeviceMTL());
			break;
#endif
#ifdef ENABLE_OPENGL
		case RenderAPI::OpenGL:
		case RenderAPI::OpenGLES:
			g_gs_device = std::make_unique<GSDeviceOGL>();
			break;
#endif

#ifdef ENABLE_VULKAN
		case RenderAPI::Vulkan:
			g_gs_device = std::make_unique<GSDeviceVK>();
			break;
#endif

		default:
			Console.Error("Unknown render API %u", static_cast<unsigned>(g_host_display->GetRenderAPI()));
			return false;
	}

	try
	{
		if (!g_gs_device->Create())
		{
			g_gs_device->Destroy();
			g_gs_device.reset();
			return false;
		}

		if (renderer == GSRendererType::Null)
		{
			g_gs_renderer = std::make_unique<GSRendererNull>();
		}
		else if (renderer != GSRendererType::SW)
		{
			g_gs_renderer = std::make_unique<GSRendererHW>();
		}
		else
		{
			g_gs_renderer = std::unique_ptr<GSRenderer>(MULTI_ISA_SELECT(makeGSRendererSW)(GSConfig.SWExtraThreads));
		}
	}
	catch (std::exception& ex)
	{
		Host::ReportFormattedErrorAsync("GS", "GS error: Exception caught in GSopen: %s", ex.what());
		g_gs_renderer.reset();
		g_gs_device->Destroy();
		g_gs_device.reset();
		return false;
	}

	GSConfig.OsdShowGPU = EmuConfig.GS.OsdShowGPU && g_host_display->SetGPUTimingEnabled(true);

	g_gs_renderer->SetRegsMem(basemem);
	g_perfmon.Reset();
	return true;
}

bool GSreopen(bool recreate_display, const Pcsx2Config::GSOptions& old_config)
{
	Console.WriteLn("Reopening GS with %s display", recreate_display ? "new" : "existing");

	g_gs_renderer->Flush(GSState::GSFlushReason::GSREOPEN);

	freezeData fd = {};
	if (g_gs_renderer->Freeze(&fd, true) != 0)
	{
		Console.Error("(GSreopen) Failed to get GS freeze size");
		return false;
	}

	std::unique_ptr<u8[]> fd_data = std::make_unique<u8[]>(fd.size);
	fd.data = fd_data.get();
	if (g_gs_renderer->Freeze(&fd, false) != 0)
	{
		Console.Error("(GSreopen) Failed to freeze GS");
		return false;
	}

	if (recreate_display)
	{
		g_gs_device->ResetAPIState();
		if (Host::BeginPresentFrame(true))
			Host::EndPresentFrame();
	}

	u8* basemem = g_gs_renderer->GetRegsMem();
	const u32 gamecrc = g_gs_renderer->GetGameCRC();
	const int gamecrc_options = g_gs_renderer->GetGameCRCOptions();
	g_gs_renderer->Destroy();
	g_gs_renderer.reset();
	g_gs_device->Destroy();
	g_gs_device.reset();

	if (recreate_display)
	{
		Host::ReleaseHostDisplay(false);
		if (!Host::AcquireHostDisplay(GetAPIForRenderer(GSConfig.Renderer), false))
		{
			Console.Error("(GSreopen) Failed to reacquire host display");

			// try to get the old one back
			if (!Host::AcquireHostDisplay(GetAPIForRenderer(old_config.Renderer), false))
			{
				pxFailRel("Failed to recreate old config host display");
				return false;
			}

			Host::AddKeyedOSDMessage("GSReopenFailed", fmt::format("Failed to open {} display, switching back to {}.",
														   HostDisplay::RenderAPIToString(GetAPIForRenderer(GSConfig.Renderer)),
														   HostDisplay::RenderAPIToString(GetAPIForRenderer(old_config.Renderer)), Host::OSD_CRITICAL_ERROR_DURATION));
			GSConfig = old_config;
		}
	}

	if (!DoGSOpen(GSConfig.Renderer, basemem))
	{
		Console.Error("(GSreopen) Failed to recreate GS");

		// try the old config
		if (recreate_display && GSConfig.Renderer != old_config.Renderer)
		{
			Host::ReleaseHostDisplay(false);
			if (!Host::AcquireHostDisplay(GetAPIForRenderer(old_config.Renderer), false))
			{
				pxFailRel("Failed to recreate old config host display (part 2)");
				return false;
			}
		}

		Host::AddKeyedOSDMessage("GSReopenFailed","Failed to reopen, restoring old configuration.", Host::OSD_CRITICAL_ERROR_DURATION);
		GSConfig = old_config;
		if (!DoGSOpen(GSConfig.Renderer, basemem))
		{
			pxFailRel("Failed to reopen GS on old config");
			return false;
		}
	}

	if (g_gs_renderer->Defrost(&fd) != 0)
	{
		Console.Error("(GSreopen) Failed to defrost");
		return false;
	}

	g_gs_renderer->SetGameCRC(gamecrc, gamecrc_options);
	return true;
}

bool GSopen(const Pcsx2Config::GSOptions& config, GSRendererType renderer, u8* basemem)
{
	if (renderer == GSRendererType::Auto)
		renderer = GSUtil::GetPreferredRenderer();

	GSConfig = config;
	GSConfig.Renderer = renderer;

	if (!Host::AcquireHostDisplay(GetAPIForRenderer(renderer), true))
	{
		Console.Error("Failed to acquire host display");
		return false;
	}

	if (!DoGSOpen(renderer, basemem))
	{
		Host::ReleaseHostDisplay(true);
		return false;
	}

	return true;
}

void GSreset(bool hardware_reset)
{
	try
	{
		g_gs_renderer->Reset(hardware_reset);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifSoftReset(u32 mask)
{
	try
	{
		g_gs_renderer->SoftReset(mask);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSwriteCSR(u32 csr)
{
	try
	{
		g_gs_renderer->WriteCSR(csr);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSInitAndReadFIFO(u8* mem, u32 size)
{
	GL_PERF("Init and read FIFO %u qwc", size);
	try
	{
		g_gs_renderer->InitReadFIFO(mem, size);
		g_gs_renderer->ReadFIFO(mem, size);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

void GSReadLocalMemoryUnsync(u8* mem, u32 qwc, u64 BITBLITBUF, u64 TRXPOS, u64 TRXREG)
{
	g_gs_renderer->ReadLocalMemoryUnsync(mem, qwc, GIFRegBITBLTBUF{BITBLITBUF}, GIFRegTRXPOS{TRXPOS}, GIFRegTRXREG{TRXREG});
}

void GSgifTransfer(const u8* mem, u32 size)
{
	try
	{
		g_gs_renderer->Transfer<3>(mem, size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer1(u8* mem, u32 addr)
{
	try
	{
		g_gs_renderer->Transfer<0>(const_cast<u8*>(mem) + addr, (0x4000 - addr) / 16);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer2(u8* mem, u32 size)
{
	try
	{
		g_gs_renderer->Transfer<1>(const_cast<u8*>(mem), size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer3(u8* mem, u32 size)
{
	try
	{
		g_gs_renderer->Transfer<2>(const_cast<u8*>(mem), size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSvsync(u32 field, bool registers_written)
{
	try
	{
		g_gs_renderer->VSync(field, registers_written);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

int GSfreeze(FreezeAction mode, freezeData* data)
{
	try
	{
		if (mode == FreezeAction::Save)
		{
			return g_gs_renderer->Freeze(data, false);
		}
		else if (mode == FreezeAction::Size)
		{
			return g_gs_renderer->Freeze(data, true);
		}
		else if (mode == FreezeAction::Load)
		{
			// Since Defrost doesn't do a hardware reset (since it would be clearing
			// local memory just before it's overwritten), we have to manually wipe
			// out the current textures.
			g_gs_device->ClearCurrent();
			return g_gs_renderer->Defrost(data);
		}
	}
	catch (GSRecoverableError)
	{
	}

	return 0;
}

void GSQueueSnapshot(const std::string& path, u32 gsdump_frames)
{
	if (g_gs_renderer)
		g_gs_renderer->QueueSnapshot(path, gsdump_frames);
}

void GSStopGSDump()
{
	if (g_gs_renderer)
		g_gs_renderer->StopGSDump();
}

bool GSBeginCapture(std::string filename)
{
	if (g_gs_renderer)
		return g_gs_renderer->BeginCapture(std::move(filename));
	else
		return false;
}

void GSEndCapture()
{
	if (g_gs_renderer)
		g_gs_renderer->EndCapture();
}		

void GSPresentCurrentFrame()
{
	g_gs_renderer->PresentCurrentFrame();
}

void GSThrottlePresentation()
{
	if (g_host_display->GetVsyncMode() != VsyncMode::Off)
	{
		// Let vsync take care of throttling.
		return;
	}

	// Manually throttle presentation when vsync isn't enabled, so we don't try to render the
	// fullscreen UI at thousands of FPS and make the gpu go brrrrrrrr.
	const float surface_refresh_rate = g_host_display->GetWindowInfo().surface_refresh_rate;
	const float throttle_rate = (surface_refresh_rate > 0.0f) ? surface_refresh_rate : 60.0f;

	const u64 sleep_period = static_cast<u64>(static_cast<double>(GetTickFrequency()) / static_cast<double>(throttle_rate));
	const u64 current_ts = GetCPUTicks();

	// Allow it to fall behind/run ahead up to 2*period. Sleep isn't that precise, plus we need to
	// allow time for the actual rendering.
	const u64 max_variance = sleep_period * 2;
	if (static_cast<u64>(std::abs(static_cast<s64>(current_ts - s_next_manual_present_time))) > max_variance)
		s_next_manual_present_time = current_ts + sleep_period;
	else
		s_next_manual_present_time += sleep_period;

	Threading::SleepUntil(s_next_manual_present_time);
}

void GSsetGameCRC(u32 crc, int options)
{
	g_gs_renderer->SetGameCRC(crc, options);
}

GSVideoMode GSgetDisplayMode()
{
	GSRenderer* gs = g_gs_renderer.get();

	return gs->GetVideoMode();
}

void GSgetInternalResolution(int* width, int* height)
{
	GSRenderer* gs = g_gs_renderer.get();
	if (!gs)
	{
		*width = 0;
		*height = 0;
		return;
	}

	const GSVector2i res(gs->GetInternalResolution());
	*width = res.x;
	*height = res.y;
}

void GSgetStats(std::string& info)
{
	GSPerfMon& pm = g_perfmon;

	const char* api_name = HostDisplay::RenderAPIToString(s_render_api);

	if (GSConfig.Renderer == GSRendererType::SW)
	{
		const double fps = GetVerticalFrequency();
		const double fillrate = pm.Get(GSPerfMon::Fillrate);
		info = StringUtil::StdStringFromFormat("%s SW | %d S | %d P | %d D | %.2f U | %.2f D | %.2f mpps",
			api_name,
			(int)pm.Get(GSPerfMon::SyncPoint),
			(int)pm.Get(GSPerfMon::Prim),
			(int)pm.Get(GSPerfMon::Draw),
			pm.Get(GSPerfMon::Swizzle) / 1024,
			pm.Get(GSPerfMon::Unswizzle) / 1024,
			fps * fillrate / (1024 * 1024));
	}
	else if (GSConfig.Renderer == GSRendererType::Null)
	{
		info = StringUtil::StdStringFromFormat("%s Null", api_name);
	}
	else
	{
		if (GSConfig.TexturePreloading == TexturePreloadingLevel::Full)
		{
			info = StringUtil::StdStringFromFormat("%s HW | HC: %d MB | %d P | %d D | %d DC | %d B | %d RB | %d TC | %d TU",
				api_name,
				(int)std::ceil(GSRendererHW::GetInstance()->GetTextureCache()->GetTotalHashCacheMemoryUsage() / 1048576.0f),
				(int)pm.Get(GSPerfMon::Prim),
				(int)pm.Get(GSPerfMon::Draw),
				(int)std::ceil(pm.Get(GSPerfMon::DrawCalls)),
				(int)std::ceil(pm.Get(GSPerfMon::Barriers)),
				(int)std::ceil(pm.Get(GSPerfMon::Readbacks)),
				(int)std::ceil(pm.Get(GSPerfMon::TextureCopies)),
				(int)std::ceil(pm.Get(GSPerfMon::TextureUploads)));
		}
		else
		{
			info = StringUtil::StdStringFromFormat("%s HW | %d P | %d D | %d DC | %d B | %d RB | %d TC | %d TU",
				api_name,
				(int)pm.Get(GSPerfMon::Prim),
				(int)pm.Get(GSPerfMon::Draw),
				(int)std::ceil(pm.Get(GSPerfMon::DrawCalls)),
				(int)std::ceil(pm.Get(GSPerfMon::Barriers)),
				(int)std::ceil(pm.Get(GSPerfMon::Readbacks)),
				(int)std::ceil(pm.Get(GSPerfMon::TextureCopies)),
				(int)std::ceil(pm.Get(GSPerfMon::TextureUploads)));
		}
	}
}

void GSgetTitleStats(std::string& info)
{
	static constexpr const char* deinterlace_modes[] = {
		"Automatic", "None", "Weave tff", "Weave bff", "Bob tff", "Bob bff", "Blend tff", "Blend bff", "Adaptive tff", "Adaptive bff"};

	const char* api_name = HostDisplay::RenderAPIToString(s_render_api);
	const char* hw_sw_name = (GSConfig.Renderer == GSRendererType::Null) ? " Null" : (GSConfig.UseHardwareRenderer() ? " HW" : " SW");
	const char* deinterlace_mode = deinterlace_modes[static_cast<int>(GSConfig.InterlaceMode)];

	const char* interlace_mode = ReportInterlaceMode();
	const char* video_mode = ReportVideoMode();
	info = StringUtil::StdStringFromFormat("%s%s | %s | %s | %s", api_name, hw_sw_name, video_mode, interlace_mode, deinterlace_mode);
}

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config)
{
	Pcsx2Config::GSOptions old_config(std::move(GSConfig));
	GSConfig = new_config;
	GSConfig.Renderer = (GSConfig.Renderer == GSRendererType::Auto) ? GSUtil::GetPreferredRenderer() : GSConfig.Renderer;
	if (!g_gs_renderer)
		return;


	// Handle OSD scale changes by pushing a window resize through.
	if (new_config.OsdScale != old_config.OsdScale)
	{
		g_gs_device->ResetAPIState();
		Host::ResizeHostDisplay(g_host_display->GetWindowWidth(), g_host_display->GetWindowHeight(), g_host_display->GetWindowScale());
		g_gs_device->RestoreAPIState();
	}

	// Options which need a full teardown/recreate.
	if (!GSConfig.RestartOptionsAreEqual(old_config))
	{
		RenderAPI existing_api = g_host_display->GetRenderAPI();
		if (existing_api == RenderAPI::OpenGLES)
			existing_api = RenderAPI::OpenGL;

		const bool do_full_restart = (
			existing_api != GetAPIForRenderer(GSConfig.Renderer) ||
			GSConfig.Adapter != old_config.Adapter ||
			GSConfig.UseDebugDevice != old_config.UseDebugDevice ||
			GSConfig.UseBlitSwapChain != old_config.UseBlitSwapChain ||
			GSConfig.DisableShaderCache != old_config.DisableShaderCache ||
			GSConfig.ThreadedPresentation != old_config.ThreadedPresentation
		);
		if (!GSreopen(do_full_restart, old_config))
			pxFailRel("Failed to do full GS reopen");
		return;
	}

	// Options which aren't using the global struct yet, so we need to recreate all GS objects.
	if (
		GSConfig.UpscaleMultiplier != old_config.UpscaleMultiplier ||
		GSConfig.CRCHack != old_config.CRCHack ||
		GSConfig.SWExtraThreads != old_config.SWExtraThreads ||
		GSConfig.SWExtraThreadsHeight != old_config.SWExtraThreadsHeight)
	{
		if (!GSreopen(false, old_config))
			pxFailRel("Failed to do quick GS reopen");

		return;
	}

	// This is where we would do finer-grained checks in the future.
	// For example, flushing the texture cache when mipmap settings change.

	if (GSConfig.CRCHack != old_config.CRCHack ||
		GSConfig.PointListPalette != old_config.PointListPalette)
	{
		// for automatic mipmaps, we need to reload the crc
		g_gs_renderer->SetGameCRC(g_gs_renderer->GetGameCRC(), g_gs_renderer->GetGameCRCOptions());
	}

	// renderer-specific options (e.g. auto flush, TC offset)
	g_gs_renderer->UpdateSettings(old_config);

	// reload texture cache when trilinear filtering or TC options change
	if (
		(GSConfig.UseHardwareRenderer() && GSConfig.HWMipmap != old_config.HWMipmap) ||
		GSConfig.TexturePreloading != old_config.TexturePreloading ||
		GSConfig.TriFilter != old_config.TriFilter ||
		GSConfig.GPUPaletteConversion != old_config.GPUPaletteConversion ||
		GSConfig.PreloadFrameWithGSData != old_config.PreloadFrameWithGSData ||
		GSConfig.WrapGSMem != old_config.WrapGSMem ||
		GSConfig.UserHacks_CPUFBConversion != old_config.UserHacks_CPUFBConversion ||
		GSConfig.UserHacks_DisableDepthSupport != old_config.UserHacks_DisableDepthSupport ||
		GSConfig.UserHacks_DisablePartialInvalidation != old_config.UserHacks_DisablePartialInvalidation ||
		GSConfig.UserHacks_TextureInsideRt != old_config.UserHacks_TextureInsideRt ||
		GSConfig.UserHacks_CPUSpriteRenderBW != old_config.UserHacks_CPUSpriteRenderBW ||
		GSConfig.UserHacks_CPUCLUTRender != old_config.UserHacks_CPUCLUTRender)
	{
		g_gs_renderer->PurgeTextureCache();
		g_gs_renderer->PurgePool();
	}

	// clear out the sampler cache when AF options change, since the anisotropy gets baked into them
	if (GSConfig.MaxAnisotropy != old_config.MaxAnisotropy)
		g_gs_device->ClearSamplerCache();

	// texture dumping/replacement options
	GSTextureReplacements::UpdateConfig(old_config);

	// clear the hash texture cache since we might have replacements now
	// also clear it when dumping changes, since we want to dump everything being used
	if (GSConfig.LoadTextureReplacements != old_config.LoadTextureReplacements ||
		GSConfig.DumpReplaceableTextures != old_config.DumpReplaceableTextures)
	{
		g_gs_renderer->PurgeTextureCache();
	}

	if (GSConfig.OsdShowGPU != old_config.OsdShowGPU)
	{
		if (!g_host_display->SetGPUTimingEnabled(GSConfig.OsdShowGPU))
			GSConfig.OsdShowGPU = false;
	}
}

void GSSwitchRenderer(GSRendererType new_renderer)
{
	if (new_renderer == GSRendererType::Auto)
		new_renderer = GSUtil::GetPreferredRenderer();

	if (!g_gs_renderer || GSConfig.Renderer == new_renderer)
		return;

	RenderAPI existing_api = g_host_display->GetRenderAPI();
	if (existing_api == RenderAPI::OpenGLES)
		existing_api = RenderAPI::OpenGL;

	const bool is_software_switch = (new_renderer == GSRendererType::SW || GSConfig.Renderer == GSRendererType::SW);
	const bool recreate_display = (!is_software_switch && existing_api != GetAPIForRenderer(new_renderer));
	const Pcsx2Config::GSOptions old_config(GSConfig);
	GSConfig.Renderer = new_renderer;
	if (!GSreopen(recreate_display, old_config))
		pxFailRel("Failed to reopen GS for renderer switch.");
}

void GSResetAPIState()
{
	if (!g_gs_device)
		return;

	g_gs_device->ResetAPIState();
}

void GSRestoreAPIState()
{
	if (!g_gs_device)
		return;

	g_gs_device->RestoreAPIState();
}

bool GSSaveSnapshotToMemory(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
	u32* width, u32* height, std::vector<u32>* pixels)
{
	if (!g_gs_renderer)
		return false;

	return g_gs_renderer->SaveSnapshotToMemory(window_width, window_height, apply_aspect, crop_borders,
		width, height, pixels);
}

#ifdef _WIN32

static HANDLE s_fh = NULL;

void* GSAllocateWrappedMemory(size_t size, size_t repeat)
{
	pxAssertRel(!s_fh, "Has no file mapping");

	s_fh = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
	if (s_fh == NULL)
	{
		Console.Error("Failed to create file mapping of size %zu. WIN API ERROR:%u", size, GetLastError());
		return nullptr;
	}

	// Reserve the whole area with repeats.
	u8* base = static_cast<u8*>(VirtualAlloc2(
		GetCurrentProcess(), nullptr, repeat * size,
		MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS,
		nullptr, 0));
	if (base)
	{
		bool okay = true;
		for (size_t i = 0; i < repeat; i++)
		{
			// Everything except the last needs the placeholders split to map over them. Then map the same file over the region.
			u8* addr = base + i * size;
			if ((i != (repeat - 1) && !VirtualFreeEx(GetCurrentProcess(), addr, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) ||
				!MapViewOfFile3(s_fh, GetCurrentProcess(), addr, 0, size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0))
			{
				Console.Error("Failed to map repeat %zu of size %zu.", i, size);
				okay = false;

				for (size_t j = 0; j < i; j++)
					UnmapViewOfFile2(GetCurrentProcess(), addr, MEM_PRESERVE_PLACEHOLDER);
			}
		}

		if (okay)
		{
			DbgCon.WriteLn("fifo_alloc(): Mapped %zu repeats of %zu bytes at %p.", repeat, size, base);
			return base;
		}

		VirtualFreeEx(GetCurrentProcess(), base, 0, MEM_RELEASE);
	}

	Console.Error("Failed to reserve VA space of size %zu. WIN API ERROR:%u", size, GetLastError());
	CloseHandle(s_fh);
	s_fh = NULL;
	return nullptr;
}

void GSFreeWrappedMemory(void* ptr, size_t size, size_t repeat)
{
	pxAssertRel(s_fh, "Has a file mapping");

	for (size_t i = 0; i < repeat; i++)
	{
		u8* addr = (u8*)ptr + i * size;
		UnmapViewOfFile2(GetCurrentProcess(), addr, MEM_PRESERVE_PLACEHOLDER);
	}

	VirtualFreeEx(GetCurrentProcess(), ptr, 0, MEM_RELEASE);
	s_fh = NULL;
}

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static int s_shm_fd = -1;

void* GSAllocateWrappedMemory(size_t size, size_t repeat)
{
	ASSERT(s_shm_fd == -1);

	const char* file_name = "/GS.mem";
	s_shm_fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (s_shm_fd != -1)
	{
		shm_unlink(file_name); // file is deleted but descriptor is still open
	}
	else
	{
		fprintf(stderr, "Failed to open %s due to %s\n", file_name, strerror(errno));
		return nullptr;
	}

	if (ftruncate(s_shm_fd, repeat * size) < 0)
		fprintf(stderr, "Failed to reserve memory due to %s\n", strerror(errno));

	void* fifo = mmap(nullptr, size * repeat, PROT_READ | PROT_WRITE, MAP_SHARED, s_shm_fd, 0);

	for (size_t i = 1; i < repeat; i++)
	{
		void* base = (u8*)fifo + size * i;
		u8* next = (u8*)mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, s_shm_fd, 0);
		if (next != base)
			fprintf(stderr, "Fail to mmap contiguous segment\n");
	}

	return fifo;
}

void GSFreeWrappedMemory(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_shm_fd >= 0);

	if (s_shm_fd < 0)
		return;

	munmap(ptr, size * repeat);

	close(s_shm_fd);
	s_shm_fd = -1;
}

#endif

static void HotkeyAdjustUpscaleMultiplier(s32 delta)
{
	const u32 new_multiplier = static_cast<u32>(std::clamp(static_cast<s32>(EmuConfig.GS.UpscaleMultiplier) + delta, 1, 8));
	Host::AddKeyedFormattedOSDMessage("UpscaleMultiplierChanged", Host::OSD_QUICK_DURATION, "Upscale multiplier set to %ux.", new_multiplier);
	EmuConfig.GS.UpscaleMultiplier = new_multiplier;

	// this is pretty slow. we only really need to flush the TC and recompile shaders.
	// TODO(Stenzek): Make it faster at some point in the future.
	GetMTGS().ApplySettings();
}

BEGIN_HOTKEY_LIST(g_gs_hotkeys)
	{"Screenshot", "Graphics", "Save Screenshot", [](s32 pressed) {
		if (!pressed)
		{
			GetMTGS().RunOnGSThread([]() {
				GSQueueSnapshot(std::string(), 0);
			});
		}
	}},
	{"ToggleVideoCapture", "Graphics", "Toggle Video Capture", [](s32 pressed) {
		 if (!pressed)
		 {
			 GetMTGS().RunOnGSThread([]() {
				 if (GSCapture::IsCapturing())
				 {
					 g_gs_renderer->EndCapture();
					 return;
				 }

				 std::string filename(fmt::format("{}.{}", GSGetBaseSnapshotFilename(), GSConfig.VideoCaptureContainer));
				 g_gs_renderer->BeginCapture(std::move(filename));
			 });
		 }
	 }},
	{"GSDumpSingleFrame", "Graphics", "Save Single Frame GS Dump", [](s32 pressed) {
		if (!pressed)
		{
			GetMTGS().RunOnGSThread([]() {
				GSQueueSnapshot(std::string(), 1);
			});
		}
	}},
	{"GSDumpMultiFrame", "Graphics", "Save Multi Frame GS Dump", [](s32 pressed) {
		GetMTGS().RunOnGSThread([pressed]() {
			if (pressed > 0)
				GSQueueSnapshot(std::string(), std::numeric_limits<u32>::max());
			else
				GSStopGSDump();
		});
	}},
	{"ToggleSoftwareRendering", "Graphics", "Toggle Software Rendering", [](s32 pressed) {
		if (!pressed)
			GetMTGS().ToggleSoftwareRendering();
	}},
	{"IncreaseUpscaleMultiplier", "Graphics", "Increase Upscale Multiplier", [](s32 pressed) {
		 if (!pressed)
			 HotkeyAdjustUpscaleMultiplier(1);
	 }},
	{"DecreaseUpscaleMultiplier", "Graphics", "Decrease Upscale Multiplier", [](s32 pressed) {
		 if (!pressed)
			 HotkeyAdjustUpscaleMultiplier(-1);
	 }},
	{"CycleAspectRatio", "Graphics", "Cycle Aspect Ratio", [](s32 pressed) {
		 if (pressed)
			 return;

		 // technically this races, but the worst that'll happen is one frame uses the old AR.
		 EmuConfig.CurrentAspectRatio = static_cast<AspectRatioType>((static_cast<int>(EmuConfig.CurrentAspectRatio) + 1) % static_cast<int>(AspectRatioType::MaxCount));
		 Host::AddKeyedFormattedOSDMessage("CycleAspectRatio", Host::OSD_QUICK_DURATION, "Aspect ratio set to '%s'.", Pcsx2Config::GSOptions::AspectRatioNames[static_cast<int>(EmuConfig.CurrentAspectRatio)]);
	 }},
	{"CycleMipmapMode", "Graphics", "Cycle Hardware Mipmapping", [](s32 pressed) {
		 if (pressed)
			 return;

		 static constexpr s32 CYCLE_COUNT = 4;
		 static constexpr std::array<const char*, CYCLE_COUNT> option_names = {{"Automatic", "Off", "Basic (Generated)", "Full (PS2)"}};

		 const HWMipmapLevel new_level = static_cast<HWMipmapLevel>(((static_cast<s32>(EmuConfig.GS.HWMipmap) + 2) % CYCLE_COUNT) - 1);
		 Host::AddKeyedFormattedOSDMessage("CycleMipmapMode", Host::OSD_QUICK_DURATION, "Hardware mipmapping set to '%s'.", option_names[static_cast<s32>(new_level) + 1]);
		 EmuConfig.GS.HWMipmap = new_level;

		 GetMTGS().RunOnGSThread([new_level]() {
			 GSConfig.HWMipmap = new_level;
			 g_gs_renderer->PurgeTextureCache();
			 g_gs_renderer->PurgePool();
		 });
	 }},
	{"CycleInterlaceMode", "Graphics", "Cycle Deinterlace Mode", [](s32 pressed) {
		 if (pressed)
			 return;

		 static constexpr std::array<const char*, static_cast<int>(GSInterlaceMode::Count)> option_names = {{
			 "Automatic",
			 "Off",
			 "Weave (Top Field First)",
			 "Weave (Bottom Field First)",
			 "Bob (Top Field First)",
			 "Bob (Bottom Field First)",
			 "Blend (Top Field First)",
			 "Blend (Bottom Field First)",
			 "Adaptive (Top Field First)",
			 "Adaptive (Bottom Field First)",
		 }};

		 const GSInterlaceMode new_mode = static_cast<GSInterlaceMode>((static_cast<s32>(EmuConfig.GS.InterlaceMode) + 1) % static_cast<s32>(GSInterlaceMode::Count));
		 Host::AddKeyedFormattedOSDMessage("CycleInterlaceMode", Host::OSD_QUICK_DURATION, "Deinterlace mode set to '%s'.", option_names[static_cast<s32>(new_mode)]);
		 EmuConfig.GS.InterlaceMode = new_mode;

		 GetMTGS().RunOnGSThread([new_mode]() { GSConfig.InterlaceMode = new_mode; });
	 }},
	{"ToggleTextureDumping", "Graphics", "Toggle Texture Dumping", [](s32 pressed) {
		 if (!pressed)
		 {
			 EmuConfig.GS.DumpReplaceableTextures = !EmuConfig.GS.DumpReplaceableTextures;
			 Host::AddKeyedOSDMessage("ToggleTextureReplacements",
				 EmuConfig.GS.DumpReplaceableTextures ? "Texture dumping is now enabled." : "Texture dumping is now disabled.",
				 Host::OSD_INFO_DURATION);
			 GetMTGS().ApplySettings();
		 }
	 }},
	{"ToggleTextureReplacements", "Graphics", "Toggle Texture Replacements", [](s32 pressed) {
		 if (!pressed)
		 {
			 EmuConfig.GS.LoadTextureReplacements = !EmuConfig.GS.LoadTextureReplacements;
			 Host::AddKeyedOSDMessage("ToggleTextureReplacements",
				 EmuConfig.GS.LoadTextureReplacements ? "Texture replacements are now enabled." : "Texture replacements are now disabled.",
				 Host::OSD_INFO_DURATION);
			 GetMTGS().ApplySettings();
		 }
	 }},
	{"ReloadTextureReplacements", "Graphics", "Reload Texture Replacements", [](s32 pressed) {
		 if (!pressed)
		 {
			 if (!EmuConfig.GS.LoadTextureReplacements)
			 {
				 Host::AddKeyedOSDMessage("ReloadTextureReplacements", "Texture replacements are not enabled.", Host::OSD_INFO_DURATION);
			 }
			 else
			 {
				 Host::AddKeyedOSDMessage("ReloadTextureReplacements", "Reloading texture replacements...", Host::OSD_INFO_DURATION);
				 GetMTGS().RunOnGSThread([]() {
					 GSTextureReplacements::ReloadReplacementMap();
				 });
			 }
		 }
	 }},
END_HOTKEY_LIST()
