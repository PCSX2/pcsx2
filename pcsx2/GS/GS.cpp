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
#ifndef PCSX2_CORE
// NOTE: The include order matters - GS.h includes windows.h
#include "GS/Window/GSwxDialog.h"
#endif
#include "GS.h"
#include "GSGL.h"
#include "GSUtil.h"
#include "GSExtra.h"
#include "Renderers/SW/GSRendererSW.h"
#include "Renderers/Null/GSRendererNull.h"
#include "Renderers/Null/GSDeviceNull.h"
#include "Renderers/HW/GSRendererHW.h"
#include "Renderers/HW/GSTextureReplacements.h"
#include "GSLzma.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "pcsx2/Config.h"
#include "pcsx2/Counters.h"
#include "pcsx2/Host.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/GS.h"
#ifdef PCSX2_CORE
#include "pcsx2/HostSettings.h"
#include "pcsx2/Frontend/InputManager.h"
#endif

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

static HostDisplay::RenderAPI s_render_api;

int GSinit()
{
	if (!GSUtil::CheckSSE())
	{
		return -1;
	}

	// Vector instructions must be avoided when initialising GS since PCSX2
	// can crash if the CPU does not support the instruction set.
	// Initialise it here instead - it's not ideal since we have to strip the
	// const type qualifier from all the affected variables.
	GSinitConfig();



	GSUtil::Init();

	if (g_const == nullptr)
		return -1;
	else
		g_const->Init();

#ifdef _WIN32
	s_hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

	return 0;
}

void GSinitConfig()
{
	static bool config_inited = false;
	if (config_inited)
		return;

	config_inited = true;
	theApp.SetConfigDir();
	theApp.Init();
}

void GSshutdown()
{
#ifndef PCSX2_CORE
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

	Host::ReleaseHostDisplay();
#endif

#ifdef _WIN32
	if (SUCCEEDED(s_hr))
	{
		::CoUninitialize();

		s_hr = E_FAIL;
	}
#endif
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

	if (HostDisplay* display = Host::GetHostDisplay(); display)
		display->SetGPUTimingEnabled(false);

	Host::ReleaseHostDisplay();
}

static HostDisplay::RenderAPI GetAPIForRenderer(GSRendererType renderer)
{
#if defined(_WIN32)
	// On Windows, we use DX11 for software, since it's always available.
	constexpr HostDisplay::RenderAPI default_api = HostDisplay::RenderAPI::D3D11;
#elif defined(__APPLE__)
	// For Macs, default to Metal.
	constexpr HostDisplay::RenderAPI default_api = HostDisplay::RenderAPI::Metal;
#else
	// For Linux, default to OpenGL (because of hardware compatibility), if we
	// have it, otherwise Vulkan (if we have it).
#if defined(ENABLE_OPENGL)
	constexpr HostDisplay::RenderAPI default_api = HostDisplay::RenderAPI::OpenGL;
#elif defined(ENABLE_VULKAN)
	constexpr HostDisplay::RenderAPI default_api = HostDisplay::RenderAPI::Vulkan;
#else
	constexpr HostDisplay::RenderAPI default_api = HostDisplay::RenderAPI::None;
#endif
#endif

	switch (renderer)
	{
		case GSRendererType::OGL:
			return HostDisplay::RenderAPI::OpenGL;

		case GSRendererType::VK:
			return HostDisplay::RenderAPI::Vulkan;

#ifdef _WIN32
		case GSRendererType::DX11:
			return HostDisplay::RenderAPI::D3D11;

		case GSRendererType::DX12:
			return HostDisplay::RenderAPI::D3D12;
#endif

#ifdef __APPLE__
		case GSRendererType::Metal:
			return HostDisplay::RenderAPI::Metal;
#endif

		default:
			return default_api;
	}
}

static bool DoGSOpen(GSRendererType renderer, u8* basemem)
{
	HostDisplay* display = Host::GetHostDisplay();
	pxAssert(display);

	s_render_api = Host::GetHostDisplay()->GetRenderAPI();

	switch (display->GetRenderAPI())
	{
#ifdef _WIN32
		case HostDisplay::RenderAPI::D3D11:
			g_gs_device = std::make_unique<GSDevice11>();
			break;
		case HostDisplay::RenderAPI::D3D12:
			g_gs_device = std::make_unique<GSDevice12>();
			break;
#endif
#ifdef __APPLE__
		case HostDisplay::RenderAPI::Metal:
			g_gs_device = std::unique_ptr<GSDevice>(MakeGSDeviceMTL());
			break;
#endif
#ifdef ENABLE_OPENGL
		case HostDisplay::RenderAPI::OpenGL:
		case HostDisplay::RenderAPI::OpenGLES:
			g_gs_device = std::make_unique<GSDeviceOGL>();
			break;
#endif

#ifdef ENABLE_VULKAN
		case HostDisplay::RenderAPI::Vulkan:
			g_gs_device = std::make_unique<GSDeviceVK>();
			break;
#endif

		default:
			Console.Error("Unknown render API %u", static_cast<unsigned>(display->GetRenderAPI()));
			return false;
	}

	try
	{
		if (!g_gs_device->Create(display))
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
			const int threads = theApp.GetConfigI("extrathreads");
			g_gs_renderer = std::make_unique<GSRendererSW>(threads);
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

	g_gs_renderer->SetRegsMem(basemem);

	display->SetVSync(EmuConfig.GetEffectiveVsyncMode());
	display->SetGPUTimingEnabled(GSConfig.OsdShowGPU);
	return true;
}

bool GSreopen(bool recreate_display)
{
	Console.WriteLn("Reopening GS with %s display", recreate_display ? "new" : "existing");

	g_gs_renderer->Flush();

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
		Host::ReleaseHostDisplay();
		if (!Host::AcquireHostDisplay(GetAPIForRenderer(GSConfig.Renderer)))
		{
			pxFailRel("(GSreopen) Failed to reacquire host display");
			return false;
		}
	}

	if (!DoGSOpen(GSConfig.Renderer, basemem))
	{
		pxFailRel("(GSreopen) Failed to recreate GS");
		return false;
	}

	if (g_gs_renderer->Defrost(&fd) != 0)
	{
		pxFailRel("(GSreopen) Failed to defrost");
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

	if (!Host::AcquireHostDisplay(GetAPIForRenderer(renderer)))
	{
		Console.Error("Failed to acquire host display");
		return false;
	}

	if (!DoGSOpen(renderer, basemem))
	{
		Host::ReleaseHostDisplay();
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

#ifndef PCSX2_CORE

void GSkeyEvent(const HostKeyEvent& e)
{
	try
	{
		if (g_gs_renderer)
			g_gs_renderer->KeyEvent(e);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSconfigure()
{
	try
	{
		if (!GSUtil::CheckSSE())
			return;

		theApp.SetConfigDir();
		theApp.Init();

		if (RunwxDialog())
		{
			theApp.ReloadConfig();
			// Force a reload of the gs state
			//theApp.SetCurrentRendererType(GSRendererType::Undefined);
		}
	}
	catch (GSRecoverableError)
	{
	}
}

int GStest()
{
	if (!GSUtil::CheckSSE())
		return -1;

	return 0;
}

static void pt(const char* str)
{
	struct tm* current;
	time_t now;

	time(&now);
	current = localtime(&now);

	printf("%02i:%02i:%02i%s", current->tm_hour, current->tm_min, current->tm_sec, str);
}

bool GSsetupRecording(std::string& filename)
{
	if (g_gs_renderer == NULL)
	{
		printf("GS: no s_gs for recording\n");
		return false;
	}
#if defined(__unix__) || defined(__APPLE__)
	if (!theApp.GetConfigB("capture_enabled"))
	{
		printf("GS: Recording is disabled\n");
		return false;
	}
#endif
	printf("GS: Recording start command\n");
	if (g_gs_renderer->BeginCapture(filename))
	{
		pt(" - Capture started\n");
		return true;
	}
	else
	{
		pt(" - Capture cancelled\n");
		return false;
	}
}

void GSendRecording()
{
	printf("GS: Recording end command\n");
	g_gs_renderer->EndCapture();
	pt(" - Capture ended\n");
}
#endif

void GSsetGameCRC(u32 crc, int options)
{
	g_gs_renderer->SetGameCRC(crc, options);
}

void GSsetFrameSkip(int frameskip)
{
	g_gs_renderer->SetFrameSkip(frameskip);
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
				(int)std::ceil(GSRendererHW::GetInstance()->GetTextureCache()->GetHashCacheMemoryUsage() / 1048576.0f),
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
	const char* api_name = HostDisplay::RenderAPIToString(s_render_api);
	const char* hw_sw_name = (GSConfig.Renderer == GSRendererType::Null) ? " Null" : (GSConfig.UseHardwareRenderer() ? " HW" : " SW");
	const char* deinterlace_mode = theApp.m_gs_deinterlace[static_cast<int>(GSConfig.InterlaceMode)].name.c_str();

#ifndef PCSX2_CORE
	int iwidth, iheight;
	GSgetInternalResolution(&iwidth, &iheight);

	info = StringUtil::StdStringFromFormat("%s%s | %s | %dx%d", api_name, hw_sw_name, deinterlace_mode,  iwidth, iheight);
#else
	const char* interlace_mode = ReportInterlaceMode();
	const char* video_mode = ReportVideoMode();
	info = StringUtil::StdStringFromFormat("%s%s | %s | %s | %s", api_name, hw_sw_name, video_mode, interlace_mode, deinterlace_mode);
#endif
}

void GSUpdateConfig(const Pcsx2Config::GSOptions& new_config)
{
	Pcsx2Config::GSOptions old_config(std::move(GSConfig));
	GSConfig = new_config;
	GSConfig.Renderer = (GSConfig.Renderer == GSRendererType::Auto) ? GSUtil::GetPreferredRenderer() : GSConfig.Renderer;
	if (!g_gs_renderer)
		return;

	HostDisplay* display = Host::GetHostDisplay();

	// Handle OSD scale changes by pushing a window resize through.
	if (new_config.OsdScale != old_config.OsdScale)
	{
		g_gs_device->ResetAPIState();
		Host::ResizeHostDisplay(display->GetWindowWidth(), display->GetWindowHeight(), display->GetWindowScale());
		g_gs_device->RestoreAPIState();
	}

	// Options which need a full teardown/recreate.
	if (!GSConfig.RestartOptionsAreEqual(old_config))
	{
		HostDisplay::RenderAPI existing_api = Host::GetHostDisplay()->GetRenderAPI();
		if (existing_api == HostDisplay::RenderAPI::OpenGLES)
			existing_api = HostDisplay::RenderAPI::OpenGL;

		const bool do_full_restart = (
			existing_api != GetAPIForRenderer(GSConfig.Renderer) ||
			GSConfig.Adapter != old_config.Adapter ||
			GSConfig.UseDebugDevice != old_config.UseDebugDevice ||
			GSConfig.UseBlitSwapChain != old_config.UseBlitSwapChain ||
			GSConfig.DisableShaderCache != old_config.DisableShaderCache ||
			GSConfig.ThreadedPresentation != old_config.ThreadedPresentation
		);
		GSreopen(do_full_restart);
		return;
	}

	// Options which aren't using the global struct yet, so we need to recreate all GS objects.
	if (
		GSConfig.DumpGSData != old_config.DumpGSData ||
		GSConfig.SaveRT != old_config.SaveRT ||
		GSConfig.SaveFrame != old_config.SaveFrame ||
		GSConfig.SaveTexture != old_config.SaveTexture ||
		GSConfig.SaveDepth != old_config.SaveDepth ||

		GSConfig.UpscaleMultiplier != old_config.UpscaleMultiplier ||
		GSConfig.CRCHack != old_config.CRCHack ||
		GSConfig.SWExtraThreads != old_config.SWExtraThreads ||
		GSConfig.SWExtraThreadsHeight != old_config.SWExtraThreadsHeight ||

		GSConfig.SaveN != old_config.SaveN ||
		GSConfig.SaveL != old_config.SaveL ||

		GSConfig.ShaderFX_Conf != old_config.ShaderFX_Conf ||
		GSConfig.ShaderFX_GLSL != old_config.ShaderFX_GLSL)
	{
		GSreopen(false);
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
		GSConfig.ConservativeFramebuffer != old_config.ConservativeFramebuffer ||
		GSConfig.TexturePreloading != old_config.TexturePreloading ||
		GSConfig.UserHacks_TriFilter != old_config.UserHacks_TriFilter ||
		GSConfig.GPUPaletteConversion != old_config.GPUPaletteConversion ||
		GSConfig.PreloadFrameWithGSData != old_config.PreloadFrameWithGSData ||
		GSConfig.WrapGSMem != old_config.WrapGSMem ||
		GSConfig.UserHacks_CPUFBConversion != old_config.UserHacks_CPUFBConversion ||
		GSConfig.UserHacks_DisableDepthSupport != old_config.UserHacks_DisableDepthSupport ||
		GSConfig.UserHacks_DisablePartialInvalidation != old_config.UserHacks_DisablePartialInvalidation ||
		GSConfig.UserHacks_TextureInsideRt != old_config.UserHacks_TextureInsideRt)
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
		if (HostDisplay* display = Host::GetHostDisplay(); display)
			display->SetGPUTimingEnabled(GSConfig.OsdShowGPU);
	}
}

void GSSwitchRenderer(GSRendererType new_renderer)
{
	if (new_renderer == GSRendererType::Auto)
		new_renderer = GSUtil::GetPreferredRenderer();

	if (!g_gs_renderer || GSConfig.Renderer == new_renderer)
		return;

	HostDisplay::RenderAPI existing_api = Host::GetHostDisplay()->GetRenderAPI();
	if (existing_api == HostDisplay::RenderAPI::OpenGLES)
		existing_api = HostDisplay::RenderAPI::OpenGL;

	const bool is_software_switch = (new_renderer == GSRendererType::SW || GSConfig.Renderer == GSRendererType::SW);
	GSConfig.Renderer = new_renderer;
	GSreopen(!is_software_switch && existing_api != GetAPIForRenderer(new_renderer));
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

bool GSSaveSnapshotToMemory(u32 width, u32 height, std::vector<u32>* pixels)
{
	if (!g_gs_renderer)
		return false;

	return g_gs_renderer->SaveSnapshotToMemory(width, height, pixels);
}

std::string format(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(nullptr, 0, fmt, args) + 1;
	va_end(args);

	assert(size > 0);
	std::vector<char> buffer(std::max(1, size));

	va_start(args, fmt);
	vsnprintf(buffer.data(), size, fmt, args);
	va_end(args);

	return {buffer.data()};
}

// Helper path to dump texture
#ifdef _WIN32
const std::string root_sw("c:\\temp1\\_");
const std::string root_hw("c:\\temp2\\_");
#else
#ifdef _M_AMD64
const std::string root_sw("/tmp/GS_SW_dump64/");
const std::string root_hw("/tmp/GS_HW_dump64/");
#else
const std::string root_sw("/tmp/GS_SW_dump32/");
const std::string root_hw("/tmp/GS_HW_dump32/");
#endif
#endif

#ifdef _WIN32

void* vmalloc(size_t size, bool code)
{
	void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}

void vmfree(void* ptr, size_t size)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

static HANDLE s_fh = NULL;
static u8* s_Next[8];

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_fh == NULL);

	if (repeat >= std::size(s_Next))
	{
		fprintf(stderr, "Memory mapping overflow (%zu >= %u)\n", repeat, static_cast<unsigned>(std::size(s_Next)));
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	s_fh = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
	DWORD errorID = ::GetLastError();
	if (s_fh == NULL)
	{
		fprintf(stderr, "Failed to reserve memory. WIN API ERROR:%u\n", errorID);
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	int mmap_segment_failed = 0;
	void* fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
	for (size_t i = 1; i < repeat; i++)
	{
		void* base = (u8*)fifo + size * i;
		s_Next[i] = (u8*)MapViewOfFileEx(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size, base);
		errorID = ::GetLastError();
		if (s_Next[i] != base)
		{
			mmap_segment_failed++;
			if (mmap_segment_failed > 4)
			{
				fprintf(stderr, "Memory mapping failed after %d attempts, aborting. WIN API ERROR:%u\n", mmap_segment_failed, errorID);
				fifo_free(fifo, size, repeat);
				return vmalloc(size * repeat, false); // Fallback to default vmalloc
			}
			do
			{
				UnmapViewOfFile(s_Next[i]);
				s_Next[i] = 0;
			} while (--i > 0);

			fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
		}
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_fh != NULL);

	if (s_fh == NULL)
	{
		if (ptr != NULL)
			vmfree(ptr, size);
		return;
	}

	UnmapViewOfFile(ptr);

	for (size_t i = 1; i < std::size(s_Next); i++)
	{
		if (s_Next[i] != 0)
		{
			UnmapViewOfFile(s_Next[i]);
			s_Next[i] = 0;
		}
	}

	CloseHandle(s_fh);
	s_fh = NULL;
}

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void* vmalloc(size_t size, bool code)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if (code)
	{
		prot |= PROT_EXEC;
#if defined(_M_AMD64) && !defined(__APPLE__)
		// macOS doesn't allow any mappings in the first 4GB of address space
		flags |= MAP_32BIT;
#endif
	}

	void* ptr = mmap(NULL, size, prot, flags, -1, 0);
	if (ptr == MAP_FAILED)
		throw std::bad_alloc();
	return ptr;
}

void vmfree(void* ptr, size_t size)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	munmap(ptr, size);
}

static int s_shm_fd = -1;

void* fifo_alloc(size_t size, size_t repeat)
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

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_shm_fd >= 0);

	if (s_shm_fd < 0)
		return;

	munmap(ptr, size * repeat);

	close(s_shm_fd);
	s_shm_fd = -1;
}

#endif

size_t GSApp::GetIniString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName)
{
#ifdef PCSX2_CORE
	std::string ret(Host::GetStringSettingValue("EmuCore/GS", lpKeyName, lpDefault));
	return StringUtil::Strlcpy(lpReturnedString, ret, nSize);
#else
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value = m_configuration_map[key];
	if (value.empty())
	{
		// save the value for futur call
		m_configuration_map[key] = std::string(lpDefault);
		strcpy(lpReturnedString, lpDefault);
	}
	else
		strcpy(lpReturnedString, value.c_str());

	return 0;
#endif
}

bool GSApp::WriteIniString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName)
{
#ifndef PCSX2_CORE
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value(pString);
	m_configuration_map[key] = value;

	// Save config to a file
	FILE* f = FileSystem::OpenCFile(lpFileName, "w");

	if (f == NULL)
		return false; // FIXME print a nice message

		// Maintain compatibility with GSDumpGUI/old Windows ini.
#ifdef _WIN32
	fprintf(f, "[Settings]\n");
#endif

	for (const auto& entry : m_configuration_map)
	{
		// Do not save the inifile key which is not an option
		if (entry.first.compare("inifile") == 0)
			continue;

		// Only keep option that have a default value (allow to purge old option of the GS.ini)
		if (!entry.second.empty() && m_default_configuration.find(entry.first) != m_default_configuration.end())
			fprintf(f, "%s = %s\n", entry.first.c_str(), entry.second.c_str());
	}
	fclose(f);
#endif

	return false;
}

#ifndef PCSX2_CORE
int GSApp::GetIniInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string value = m_configuration_map[std::string(lpKeyName)];
	if (value.empty())
	{
		// save the value for futur call
		SetConfig(lpKeyName, nDefault);
		return nDefault;
	}
	else
		return atoi(value.c_str());
}
#endif

GSApp theApp;

GSApp::GSApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSApp::Init()
{
	static bool is_initialised = false;
	if (is_initialised)
		return;
	is_initialised = true;

	m_section = "Settings";

	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::Auto), "Automatic", ""));
#ifdef _WIN32
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::DX11), "Direct3D 11", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::DX12), "Direct3D 12", ""));
#endif
#ifdef __APPLE__
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::Metal), "Metal", ""));
#endif
#ifdef ENABLE_OPENGL
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::OGL), "OpenGL", ""));
#endif
#ifdef ENABLE_VULKAN
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::VK), "Vulkan", ""));
#endif
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::SW), "Software", ""));

	// The null renderer goes last, it has use for benchmarking purposes in a release build
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::Null), "Null", ""));

	m_gs_deinterlace.push_back(GSSetting(0, "None", ""));
	m_gs_deinterlace.push_back(GSSetting(1, "Weave tff", "saw-tooth"));
	m_gs_deinterlace.push_back(GSSetting(2, "Weave bff", "saw-tooth"));
	m_gs_deinterlace.push_back(GSSetting(3, "Bob tff", "use blend if shaking"));
	m_gs_deinterlace.push_back(GSSetting(4, "Bob bff", "use blend if shaking"));
	m_gs_deinterlace.push_back(GSSetting(5, "Blend tff", "slight blur, 1/2 fps"));
	m_gs_deinterlace.push_back(GSSetting(6, "Blend bff", "slight blur, 1/2 fps"));
	m_gs_deinterlace.push_back(GSSetting(7, "Automatic", "Default"));

	m_gs_upscale_multiplier.push_back(GSSetting(1, "Native", "PS2"));
	m_gs_upscale_multiplier.push_back(GSSetting(2, "2x Native", "~720p"));
	m_gs_upscale_multiplier.push_back(GSSetting(3, "3x Native", "~1080p"));
	m_gs_upscale_multiplier.push_back(GSSetting(4, "4x Native", "~1440p 2K"));
	m_gs_upscale_multiplier.push_back(GSSetting(5, "5x Native", "~1620p"));
	m_gs_upscale_multiplier.push_back(GSSetting(6, "6x Native", "~2160p 4K"));
	m_gs_upscale_multiplier.push_back(GSSetting(7, "7x Native", "~2520p"));
	m_gs_upscale_multiplier.push_back(GSSetting(8, "8x Native", "~2880p"));

	m_gs_max_anisotropy.push_back(GSSetting(0, "Off", "Default"));
	m_gs_max_anisotropy.push_back(GSSetting(2, "2x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(4, "4x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(8, "8x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(16, "16x", ""));

	m_gs_dithering.push_back(GSSetting(0, "Off", ""));
	m_gs_dithering.push_back(GSSetting(2, "Unscaled", "Default"));
	m_gs_dithering.push_back(GSSetting(1, "Scaled", ""));

	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Nearest), "Nearest", ""));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Forced_But_Sprite), "Bilinear", "Forced excluding sprite"));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Forced), "Bilinear", "Forced"));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::PS2), "Bilinear", "PS2"));

	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::Automatic), "Automatic", "Default"));
	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::Off), "None", ""));
	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::PS2), "Trilinear", ""));
	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::Forced), "Trilinear", "Ultra/Slow"));

	m_gs_texture_preloading.push_back(GSSetting(static_cast<u32>(TexturePreloadingLevel::Off), "None", "Default"));
	m_gs_texture_preloading.push_back(GSSetting(static_cast<u32>(TexturePreloadingLevel::Partial), "Partial", ""));
	m_gs_texture_preloading.push_back(GSSetting(static_cast<u32>(TexturePreloadingLevel::Full), "Full", "Hash Cache"));

	m_gs_generic_list.push_back(GSSetting(-1, "Automatic", "Default"));
	m_gs_generic_list.push_back(GSSetting(0, "Force-Disabled", ""));
	m_gs_generic_list.push_back(GSSetting(1, "Force-Enabled", ""));

	m_gs_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_hack.push_back(GSSetting(1, "Half", ""));
	m_gs_hack.push_back(GSSetting(2, "Full", ""));

	m_gs_offset_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_offset_hack.push_back(GSSetting(1, "Normal", "Vertex"));
	m_gs_offset_hack.push_back(GSSetting(2, "Special", "Texture"));
	m_gs_offset_hack.push_back(GSSetting(3, "Special", "Texture - aggressive"));

	m_gs_hw_mipmapping = {
		GSSetting(HWMipmapLevel::Automatic, "Automatic", "Default"),
		GSSetting(HWMipmapLevel::Off, "Off", ""),
		GSSetting(HWMipmapLevel::Basic, "Basic", "Fast"),
		GSSetting(HWMipmapLevel::Full, "Full", "Slow"),
	};

	m_gs_crc_level = {
		GSSetting(CRCHackLevel::Automatic, "Automatic", "Default"),
		GSSetting(CRCHackLevel::Off, "None", "Debug"),
		GSSetting(CRCHackLevel::Minimum, "Minimum", "Debug"),
#ifdef _DEBUG
		GSSetting(CRCHackLevel::Partial, "Partial", "OpenGL"),
		GSSetting(CRCHackLevel::Full, "Full", "Direct3D"),
#endif
		GSSetting(CRCHackLevel::Aggressive, "Aggressive", ""),
	};

	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::Minimum), "Minimum", ""));
	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::Basic), "Basic", "Recommended"));
	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::Medium), "Medium", ""));
	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::High), "High", ""));
	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::Full), "Full", "Slow"));
	m_gs_acc_blend_level.push_back(GSSetting(static_cast<u32>(AccBlendLevel::Maximum), "Maximum", "Very Slow"));

	m_gs_tv_shaders.push_back(GSSetting(0, "None", ""));
	m_gs_tv_shaders.push_back(GSSetting(1, "Scanline filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(2, "Diagonal filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(3, "Triangular filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(4, "Wave filter", ""));

	m_gs_dump_compression.push_back(GSSetting(static_cast<u32>(GSDumpCompressionMethod::Uncompressed), "Uncompressed", ""));
	m_gs_dump_compression.push_back(GSSetting(static_cast<u32>(GSDumpCompressionMethod::LZMA), "LZMA (xz)", ""));
	m_gs_dump_compression.push_back(GSSetting(static_cast<u32>(GSDumpCompressionMethod::Zstandard), "Zstandard (zst)", ""));

	// clang-format off
	// Avoid to clutter the ini file with useless options
#if defined(ENABLE_VULKAN) || defined(_WIN32)
	m_default_configuration["Adapter"]                                    = "";
#endif
#ifdef _WIN32
	// Per OS option.
	m_default_configuration["CaptureFileName"]                            = "";
	m_default_configuration["CaptureVideoCodecDisplayName"]               = "";
#else
	m_default_configuration["linux_replay"]                               = "1";
#endif
	m_default_configuration["aa1"]                                        = "1";
	m_default_configuration["accurate_date"]                              = "1";
	m_default_configuration["accurate_blending_unit"]                     = "1";
	m_default_configuration["AspectRatio"]                                = "1";
	m_default_configuration["autoflush_sw"]                               = "1";
	m_default_configuration["capture_enabled"]                            = "0";
	m_default_configuration["capture_out_dir"]                            = "/tmp/GS_Capture";
	m_default_configuration["capture_threads"]                            = "4";
	m_default_configuration["CaptureHeight"]                              = "480";
	m_default_configuration["CaptureWidth"]                               = "640";
	m_default_configuration["crc_hack_level"]                             = std::to_string(static_cast<s8>(CRCHackLevel::Automatic));
	m_default_configuration["CrcHacksExclusions"]                         = "";
	m_default_configuration["disable_hw_gl_draw"]                         = "0";
	m_default_configuration["disable_shader_cache"]                       = "0";
	m_default_configuration["DisableDualSourceBlend"]                     = "0";
	m_default_configuration["DisableFramebufferFetch"]                    = "0";
	m_default_configuration["dithering_ps2"]                              = "2";
	m_default_configuration["dump"]                                       = "0";
	m_default_configuration["DumpReplaceableTextures"]                    = "0";
	m_default_configuration["DumpReplaceableMipmaps"]                     = "0";
	m_default_configuration["DumpTexturesWithFMVActive"]                  = "0";
	m_default_configuration["DumpDirectTextures"]                         = "1";
	m_default_configuration["DumpPaletteTextures"]                        = "1";
	m_default_configuration["extrathreads"]                               = "2";
	m_default_configuration["extrathreads_height"]                        = "4";
	m_default_configuration["filter"]                                     = std::to_string(static_cast<s8>(BiFiltering::PS2));
	m_default_configuration["FMVSoftwareRendererSwitch"]                  = "0";
	m_default_configuration["fxaa"]                                       = "0";
	m_default_configuration["GSDumpCompression"]                          = "0";
	m_default_configuration["HWDisableReadbacks"]                         = "0";
	m_default_configuration["disable_interlace_offset"]                   = "0";
	m_default_configuration["pcrtc_offsets"]                              = "0";
	m_default_configuration["IntegerScaling"]                             = "0";
	m_default_configuration["deinterlace"]                                = "7";
	m_default_configuration["conservative_framebuffer"]                   = "1";
	m_default_configuration["linear_present"]                             = "1";
	m_default_configuration["LoadTextureReplacements"]                    = "0";
	m_default_configuration["LoadTextureReplacementsAsync"]               = "1";
	m_default_configuration["MaxAnisotropy"]                              = "0";
	m_default_configuration["mipmap"]                                     = "1";
	m_default_configuration["mipmap_hw"]                                  = std::to_string(static_cast<int>(HWMipmapLevel::Automatic));
	m_default_configuration["OsdShowMessages"]                            = "1";
	m_default_configuration["OsdShowSpeed"]                               = "0";
	m_default_configuration["OsdShowFPS"]                                 = "0";
	m_default_configuration["OsdShowCPU"]                                 = "0";
	m_default_configuration["OsdShowGPU"]                                 = "0";
	m_default_configuration["OsdShowResolution"]                          = "0";
	m_default_configuration["OsdShowGSStats"]                             = "0";
	m_default_configuration["OsdShowIndicators"]                          = "1";
	m_default_configuration["OsdScale"]                                   = "100";
	m_default_configuration["override_GL_ARB_copy_image"]                 = "-1";
	m_default_configuration["override_GL_ARB_clear_texture"]              = "-1";
	m_default_configuration["override_GL_ARB_clip_control"]               = "-1";
	m_default_configuration["override_GL_ARB_direct_state_access"]        = "-1";
	m_default_configuration["override_GL_ARB_draw_buffers_blend"]         = "-1";
	m_default_configuration["override_GL_ARB_gpu_shader5"]                = "-1";
	m_default_configuration["override_GL_ARB_shader_image_load_store"]    = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture"]             = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture2"]            = "-1";
	m_default_configuration["override_GL_ARB_texture_barrier"]            = "-1";
	m_default_configuration["OverrideTextureBarriers"]                    = "-1";
	m_default_configuration["OverrideGeometryShaders"]                    = "-1";
	m_default_configuration["paltex"]                                     = "0";
	m_default_configuration["png_compression_level"]                      = std::to_string(Z_BEST_SPEED);
	m_default_configuration["PointListPalette"]                           = "0";
	m_default_configuration["PrecacheTextureReplacements"]                = "0";
	m_default_configuration["preload_frame_with_gs_data"]                 = "0";
	m_default_configuration["Renderer"]                                   = std::to_string(static_cast<int>(GSRendererType::Auto));
	m_default_configuration["save"]                                       = "0";
	m_default_configuration["savef"]                                      = "0";
	m_default_configuration["savel"]                                      = "5000";
	m_default_configuration["saven"]                                      = "0";
	m_default_configuration["savet"]                                      = "0";
	m_default_configuration["savez"]                                      = "0";
	m_default_configuration["ShadeBoost"]                                 = "0";
	m_default_configuration["ShadeBoost_Brightness"]                      = "50";
	m_default_configuration["ShadeBoost_Contrast"]                        = "50";
	m_default_configuration["ShadeBoost_Saturation"]                      = "50";
	m_default_configuration["shaderfx"]                                   = "0";
	m_default_configuration["shaderfx_conf"]                              = "shaders/GS_FX_Settings.ini";
	m_default_configuration["shaderfx_glsl"]                              = "shaders/GS.fx";
	m_default_configuration["skip_duplicate_frames"]                      = "0";
	m_default_configuration["texture_preloading"]                         = "0";
	m_default_configuration["ThreadedPresentation"]                       = "0";
	m_default_configuration["TVShader"]                                   = "0";
	m_default_configuration["upscale_multiplier"]                         = "1";
	m_default_configuration["UseBlitSwapChain"]                           = "0";
	m_default_configuration["UseDebugDevice"]                             = "0";
	m_default_configuration["UserHacks"]                                  = "0";
	m_default_configuration["UserHacks_align_sprite_X"]                   = "0";
	m_default_configuration["UserHacks_AutoFlush"]                        = "0";
	m_default_configuration["UserHacks_DisableDepthSupport"]              = "0";
	m_default_configuration["UserHacks_Disable_Safe_Features"]            = "0";
	m_default_configuration["UserHacks_DisablePartialInvalidation"]       = "0";
	m_default_configuration["UserHacks_CPU_FB_Conversion"]                = "0";
	m_default_configuration["UserHacks_Half_Bottom_Override"]             = "-1";
	m_default_configuration["UserHacks_HalfPixelOffset"]                  = "0";
	m_default_configuration["UserHacks_merge_pp_sprite"]                  = "0";
	m_default_configuration["UserHacks_round_sprite_offset"]              = "0";
	m_default_configuration["UserHacks_SkipDraw_Start"]                   = "0";
	m_default_configuration["UserHacks_SkipDraw_End"]                     = "0";
	m_default_configuration["UserHacks_TCOffsetX"]                        = "0";
	m_default_configuration["UserHacks_TCOffsetY"]                        = "0";
	m_default_configuration["UserHacks_TextureInsideRt"]                  = "0";
	m_default_configuration["UserHacks_TriFilter"]                        = std::to_string(static_cast<s8>(TriFiltering::Automatic));
	m_default_configuration["UserHacks_WildHack"]                         = "0";
	m_default_configuration["wrap_gs_mem"]                                = "0";
	m_default_configuration["vsync"]                                      = "0";
	// clang-format on
}

#ifndef PCSX2_CORE
void GSApp::ReloadConfig()
{
	if (m_configuration_map.empty())
		return;

	auto file = m_configuration_map.find("inifile");
	if (file == m_configuration_map.end())
		return;

	// A map was built so reload it
	std::string filename = file->second;
	m_configuration_map.clear();
	BuildConfigurationMap(filename.c_str());
}

void GSApp::BuildConfigurationMap(const char* lpFileName)
{
	// Check if the map was already built
	std::string inifile_value(lpFileName);
	if (inifile_value.compare(m_configuration_map["inifile"]) == 0)
		return;
	m_configuration_map["inifile"] = inifile_value;

	// Load config from file
#ifdef _WIN32
	std::ifstream file(StringUtil::UTF8StringToWideString(lpFileName));
#else
	std::ifstream file(lpFileName);
#endif
	if (!file.is_open())
		return;

	std::string line;
	while (std::getline(file, line))
	{
		const auto separator = line.find('=');
		if (separator == std::string::npos)
			continue;

		std::string key = line.substr(0, separator);
		// Trim trailing whitespace
		key.erase(key.find_last_not_of(" \r\t") + 1);

		if (key.empty())
			continue;

		// Only keep options that have a default value so older, no longer used
		// ini options can be purged.
		if (m_default_configuration.find(key) == m_default_configuration.end())
			continue;

		std::string value = line.substr(separator + 1);
		// Trim leading whitespace
		value.erase(0, value.find_first_not_of(" \r\t"));

		m_configuration_map[key] = value;
	}
}
#endif

void GSApp::SetConfigDir()
{
	// we need to initialize the ini folder later at runtime than at theApp init, as
	// core settings aren't populated yet, thus we do populate it if needed either when
	// opening GS settings or init -- govanify
	m_ini = Path::Combine(EmuFolders::Settings, "GS.ini");
}

std::string GSApp::GetConfigS(const char* entry)
{
	char buff[4096] = {0};
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end())
	{
		GetIniString(m_section.c_str(), entry, def->second.c_str(), buff, std::size(buff), m_ini.c_str());
	}
	else
	{
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		GetIniString(m_section.c_str(), entry, "", buff, std::size(buff), m_ini.c_str());
	}

	return {buff};
}

void GSApp::SetConfig(const char* entry, const char* value)
{
	WriteIniString(m_section.c_str(), entry, value, m_ini.c_str());
}

int GSApp::GetConfigI(const char* entry)
{
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end())
	{
#ifndef PCSX2_CORE
		return GetIniInt(m_section.c_str(), entry, std::stoi(def->second), m_ini.c_str());
#else
		return Host::GetIntSettingValue("EmuCore/GS", entry, std::stoi(def->second));
#endif
	}
	else
	{
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
#ifndef PCSX2_CORE
		return GetIniInt(m_section.c_str(), entry, 0, m_ini.c_str());
#else
		return Host::GetIntSettingValue("EmuCore/GS", entry, 0);
#endif
	}
}

bool GSApp::GetConfigB(const char* entry)
{
#ifndef PCSX2_CORE
	return !!GetConfigI(entry);
#else
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end())
	{
		return Host::GetBoolSettingValue("EmuCore/GS", entry, StringUtil::FromChars<bool>(def->second).value_or(false));
	}
	else
	{
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		return Host::GetBoolSettingValue("EmuCore/GS", entry, false);
	}
#endif
}

void GSApp::SetConfig(const char* entry, int value)
{
	char buff[32] = {0};

	sprintf(buff, "%d", value);

	SetConfig(entry, buff);
}

#ifdef PCSX2_CORE

static void HotkeyAdjustUpscaleMultiplier(s32 delta)
{
	const u32 new_multiplier = static_cast<u32>(std::clamp(static_cast<s32>(EmuConfig.GS.UpscaleMultiplier) + delta, 1, 8));
	Host::AddKeyedFormattedOSDMessage("UpscaleMultiplierChanged", 10.0f, "Upscale multiplier set to %ux.", new_multiplier);
	EmuConfig.GS.UpscaleMultiplier = new_multiplier;

	// this is pretty slow. we only really need to flush the TC and recompile shaders.
	// TODO(Stenzek): Make it faster at some point in the future.
	GetMTGS().ApplySettings();
}

static void HotkeyAdjustZoom(double delta)
{
	const double new_zoom = std::clamp(EmuConfig.GS.Zoom + delta, 1.0, 200.0);
	Host::AddKeyedFormattedOSDMessage("ZoomChanged", 10.0f, "Zoom set to %.1f%%.", new_zoom);
	EmuConfig.GS.Zoom = new_zoom;

	// no need to go through the full settings update for this
	GetMTGS().RunOnGSThread([new_zoom]() { GSConfig.Zoom = new_zoom; });
}

BEGIN_HOTKEY_LIST(g_gs_hotkeys)
	{"Screenshot", "Graphics", "Save Screenshot", [](bool pressed) {
		if (!pressed)
		{
			GetMTGS().RunOnGSThread([]() {
				GSQueueSnapshot(std::string(), 0);
			});
		}
	}},
	{"GSDumpSingleFrame", "Graphics", "Save Single Frame GS Dump", [](bool pressed) {
		if (!pressed)
		{
			GetMTGS().RunOnGSThread([]() {
				GSQueueSnapshot(std::string(), 1);
			});
		}
	}},
	{"GSDumpMultiFrame", "Graphics", "Save Multi Frame GS Dump", [](bool pressed) {
		GetMTGS().RunOnGSThread([pressed]() {
			if (pressed)
				GSQueueSnapshot(std::string(), std::numeric_limits<u32>::max());
			else
				GSStopGSDump();
		});
	}},
	{"ToggleSoftwareRendering", "Graphics", "Toggle Software Rendering", [](bool pressed) {
		if (!pressed)
			GetMTGS().ToggleSoftwareRendering();
	}},
	{"IncreaseUpscaleMultiplier", "Graphics", "Increase Upscale Multiplier", [](bool pressed) {
		 if (!pressed)
			 HotkeyAdjustUpscaleMultiplier(1);
	 }},
	{"DecreaseUpscaleMultiplier", "Graphics", "Decrease Upscale Multiplier", [](bool pressed) {
		 if (!pressed)
			 HotkeyAdjustUpscaleMultiplier(-1);
	 }},
	{"CycleAspectRatio", "Graphics", "Cycle Aspect Ratio", [](bool pressed) {
		 if (pressed)
			 return;

		 // technically this races, but the worst that'll happen is one frame uses the old AR.
		 EmuConfig.CurrentAspectRatio = static_cast<AspectRatioType>((static_cast<int>(EmuConfig.CurrentAspectRatio) + 1) % static_cast<int>(AspectRatioType::MaxCount));
		 Host::AddKeyedFormattedOSDMessage("CycleAspectRatio", 10.0f, "Aspect ratio set to '%s'.", Pcsx2Config::GSOptions::AspectRatioNames[static_cast<int>(EmuConfig.CurrentAspectRatio)]);
	 }},
	{"CycleMipmapMode", "Graphics", "Cycle Hardware Mipmapping", [](bool pressed) {
		 if (pressed)
			 return;

		 static constexpr s32 CYCLE_COUNT = 4;
		 static constexpr std::array<const char*, CYCLE_COUNT> option_names = {{"Automatic", "Off", "Basic (Generated)", "Full (PS2)"}};

		 const HWMipmapLevel new_level = static_cast<HWMipmapLevel>(((static_cast<s32>(EmuConfig.GS.HWMipmap) + 2) % CYCLE_COUNT) - 1);
		 Host::AddKeyedFormattedOSDMessage("CycleMipmapMode", 10.0f, "Hardware mipmapping set to '%s'.", option_names[static_cast<s32>(new_level) + 1]);
		 EmuConfig.GS.HWMipmap = new_level;

		 GetMTGS().RunOnGSThread([new_level]() {
			 GSConfig.HWMipmap = new_level;
			 g_gs_renderer->PurgeTextureCache();
			 g_gs_renderer->PurgePool();
		 });
	 }},
	{"CycleInterlaceMode", "Graphics", "Cycle Deinterlace Mode", [](bool pressed) {
		 if (pressed)
			 return;

		 static constexpr std::array<const char*, static_cast<int>(GSInterlaceMode::Count)> option_names = {{
			 "Off",
			 "Weave (Top Field First)",
			 "Weave (Bottom Field First)",
			 "Bob (Top Field First)",
			 "Bob (Bottom Field First)",
			 "Blend (Top Field First)",
			 "Blend (Bottom Field First)",
			 "Automatic",
		 }};

		 const GSInterlaceMode new_mode = static_cast<GSInterlaceMode>((static_cast<s32>(EmuConfig.GS.InterlaceMode) + 1) % static_cast<s32>(GSInterlaceMode::Count));
		 Host::AddKeyedFormattedOSDMessage("CycleInterlaceMode", 10.0f, "Deinterlace mode set to '%s'.", option_names[static_cast<s32>(new_mode)]);
		 EmuConfig.GS.InterlaceMode = new_mode;

		 GetMTGS().RunOnGSThread([new_mode]() { GSConfig.InterlaceMode = new_mode; });
	 }},
	{"ZoomIn", "Graphics", "Zoom In", [](bool pressed) {
		 if (!pressed)
			 HotkeyAdjustZoom(1.0);
	 }},
	{"ZoomOut", "Graphics", "Zoom Out", [](bool pressed) {
		 if (!pressed)
			 HotkeyAdjustZoom(-1.0);
	 }},
	{"ToggleTextureDumping", "Graphics", "Toggle Texture Dumping", [](bool pressed) {
		 if (!pressed)
		 {
			 EmuConfig.GS.DumpReplaceableTextures = !EmuConfig.GS.DumpReplaceableTextures;
			 Host::AddKeyedOSDMessage("ToggleTextureReplacements", EmuConfig.GS.DumpReplaceableTextures ? "Texture dumping is now enabled." : "Texture dumping is now disabled.", 10.0f);
			 GetMTGS().ApplySettings();
		 }
	 }},
	{"ToggleTextureReplacements", "Graphics", "Toggle Texture Replacements", [](bool pressed) {
		 if (!pressed)
		 {
			 EmuConfig.GS.LoadTextureReplacements = !EmuConfig.GS.LoadTextureReplacements;
			 Host::AddKeyedOSDMessage("ToggleTextureReplacements", EmuConfig.GS.LoadTextureReplacements ? "Texture replacements are now enabled." : "Texture replacements are now disabled.", 10.0f);
			 GetMTGS().ApplySettings();
		 }
	 }},
	{"ReloadTextureReplacements", "Graphics", "Reload Texture Replacements", [](bool pressed) {
		 if (!pressed)
		 {
			 if (!EmuConfig.GS.LoadTextureReplacements)
			 {
				 Host::AddKeyedOSDMessage("ReloadTextureReplacements", "Texture replacements are not enabled.", 10.0f);
			 }
			 else
			 {
				 Host::AddKeyedOSDMessage("ReloadTextureReplacements", "Reloading texture replacements...", 10.0f);
				 GetMTGS().RunOnGSThread([]() {
					 GSTextureReplacements::ReloadReplacementMap();
				 });
			 }
		 }
	 }},
END_HOTKEY_LIST()

#endif
