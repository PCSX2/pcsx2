// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiManager.h"
#include "GS/Renderers/Common/GSRenderer.h"
#include "GS/GSCapture.h"
#include "GS/GSDump.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "GSDumpReplayer.h"
#include "Host.h"
#include "PerformanceMetrics.h"
#include "pcsx2/Config.h"
#include "VMManager.h"

#include "common/FileSystem.h"
#include "common/Image.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/core.h"
#include "IconsFontAwesome5.h"

#include <algorithm>
#include <array>
#include <deque>
#include <thread>
#include <mutex>

static void DumpGSPrivRegs(const GSPrivRegSet& r, const std::string& filename);

static constexpr std::array<PresentShader, 8> s_tv_shader_indices = {
	PresentShader::COPY, PresentShader::SCANLINE,
	PresentShader::DIAGONAL_FILTER, PresentShader::TRIANGULAR_FILTER,
	PresentShader::COMPLEX_FILTER, PresentShader::LOTTES_FILTER,
	PresentShader::SUPERSAMPLE_4xRGSS, PresentShader::SUPERSAMPLE_AUTO};

static std::deque<std::thread> s_screenshot_threads;
static std::mutex s_screenshot_threads_mutex;

std::unique_ptr<GSRenderer> g_gs_renderer;

// Since we read this on the EE thread, we can't put it in the renderer, because
// we might be switching while the other thread reads it.
static GSVector4 s_last_draw_rect;

// Last time we reset the renderer due to a GPU crash, if any.
static Common::Timer::Value s_last_gpu_reset_time;

// Screen alignment
static GSDisplayAlignment s_display_alignment = GSDisplayAlignment::Center;

GSRenderer::GSRenderer()
	: m_shader_time_start(Common::Timer::GetCurrentValue())
{
	s_last_draw_rect = GSVector4::zero();
}

GSRenderer::~GSRenderer() = default;

void GSRenderer::Reset(bool hardware_reset)
{
	// Clear the current display texture.
	if (hardware_reset)
		g_gs_device->ClearCurrent();

	GSState::Reset(hardware_reset);
}

void GSRenderer::Destroy()
{
	GSCapture::EndCapture();
}

void GSRenderer::PurgePool()
{
	g_gs_device->PurgePool();
}

void GSRenderer::UpdateRenderFixes()
{
}

bool GSRenderer::Merge(int field)
{
	GSVector2i fs(0, 0);
	GSTexture* tex[3] = { nullptr, nullptr, nullptr };
	float tex_scale[3] = { 0.0f, 0.0f, 0.0f };
	int y_offset[3] = { 0, 0, 0 };
	const bool feedback_merge = m_regs->EXTWRITE.WRITE == 1;

	PCRTCDisplays.SetVideoMode(GetVideoMode());
	PCRTCDisplays.EnableDisplays(m_regs->PMODE, m_regs->SMODE2, isReallyInterlaced());
	PCRTCDisplays.CheckSameSource();

	if (!PCRTCDisplays.PCRTCDisplays[0].enabled && !PCRTCDisplays.PCRTCDisplays[1].enabled)
		return false;

	// Need to do this here, if the user has Anti-Blur enabled, these offsets can get wiped out/changed.
	const bool game_deinterlacing = (m_regs->DISP[0].DISPFB.DBY != PCRTCDisplays.PCRTCDisplays[0].prevFramebufferReg.DBY) !=
									(m_regs->DISP[1].DISPFB.DBY != PCRTCDisplays.PCRTCDisplays[1].prevFramebufferReg.DBY);

	PCRTCDisplays.SetRects(0, m_regs->DISP[0].DISPLAY, m_regs->DISP[0].DISPFB);
	PCRTCDisplays.SetRects(1, m_regs->DISP[1].DISPLAY, m_regs->DISP[1].DISPFB);
	PCRTCDisplays.CalculateDisplayOffset(m_scanmask_used);
	PCRTCDisplays.CalculateFramebufferOffset(m_scanmask_used);

	// Only need to check the right/bottom on software renderer, hardware always gets the full texture then cuts a bit out later.
	if (PCRTCDisplays.FrameRectMatch() && !PCRTCDisplays.FrameWrap() && !feedback_merge)
	{
		tex[0] = GetOutput(-1, tex_scale[0], y_offset[0]);
		tex[1] = tex[0]; // saves one texture fetch
		y_offset[1] = y_offset[0];
		tex_scale[1] = tex_scale[0];
	}
	else
	{
		if (PCRTCDisplays.PCRTCDisplays[0].enabled)
			tex[0] = GetOutput(0, tex_scale[0], y_offset[0]);
		if (PCRTCDisplays.PCRTCDisplays[1].enabled)
			tex[1] = GetOutput(1, tex_scale[1], y_offset[1]);
		if (feedback_merge)
			tex[2] = GetFeedbackOutput(tex_scale[2]);
	}

	if (!tex[0] && !tex[1])
		return false;

	s_n++;

	GSVector4 src_gs_read[2];
	GSVector4 dst[3];

	// Use offset for bob deinterlacing always, extra offset added later for FFMD mode.
	const bool scanmask_frame = m_scanmask_used && abs(PCRTCDisplays.PCRTCDisplays[0].displayRect.y - PCRTCDisplays.PCRTCDisplays[1].displayRect.y) != 1;
	int field2 = 0;
	int mode = 3; // If the game is manually deinterlacing then we need to bob (if we want to get away with no deinterlacing).
	bool is_bob = GSConfig.InterlaceMode == GSInterlaceMode::BobTFF || GSConfig.InterlaceMode == GSInterlaceMode::BobBFF;

	// FFMD (half frames) requires blend deinterlacing, so automatically use that. Same when SCANMSK is used but not blended in the merge circuit (Alpine Racer 3).
	if (GSConfig.InterlaceMode != GSInterlaceMode::Automatic || (!m_regs->SMODE2.FFMD && !scanmask_frame))
	{
		// If the game is offsetting each frame itself and we're using full height buffers, we can offset this with Bob.
		if (game_deinterlacing && !scanmask_frame && GSConfig.InterlaceMode == GSInterlaceMode::Automatic)
		{
			mode = 1; // Bob.
			is_bob = true;
		}
		else
		{
			field2 = ((static_cast<int>(GSConfig.InterlaceMode) - 2) & 1);
			mode = ((static_cast<int>(GSConfig.InterlaceMode) - 2) >> 1);
		}
	}

	for (int i = 0; i < 2; i++)
	{
		 const GSPCRTCRegs::PCRTCDisplay& curCircuit = PCRTCDisplays.PCRTCDisplays[i];

		if (!curCircuit.enabled || !tex[i])
			continue;

		const GSVector4 scale = GSVector4(tex_scale[i]);

		// dst is the final destination rect with offset on the screen.
		dst[i] = scale * GSVector4(curCircuit.displayRect);

		// src_gs_read is the size which we're really reading from GS memory.
		src_gs_read[i] = ((GSVector4(curCircuit.framebufferRect) + GSVector4(0, y_offset[i], 0, y_offset[i])) * scale) / GSVector4(tex[i]->GetSize()).xyxy();
		
		float interlace_offset = 0.0f;
		if (isReallyInterlaced() && m_regs->SMODE2.FFMD && !is_bob && !GSConfig.DisableInterlaceOffset && GSConfig.InterlaceMode != GSInterlaceMode::Off)
		{
			interlace_offset = (scale.y) * static_cast<float>(field ^ field2);
		}
		// Scanmask frame offsets. It's gross, I'm sorry but it sucks.
		if (m_scanmask_used)
		{
			int displayIntOffset = PCRTCDisplays.PCRTCDisplays[i].displayRect.y - PCRTCDisplays.PCRTCDisplays[1 - i].displayRect.y;
			
			if (displayIntOffset > 0)
			{
				displayIntOffset &= 1;
				dst[i].y -= displayIntOffset * scale.y;
				dst[i].w -= displayIntOffset * scale.y;
				interlace_offset += displayIntOffset;
			}
		}

		dst[i] += GSVector4(0.0f, interlace_offset, 0.0f, interlace_offset);
	}

	if (feedback_merge && tex[2])
	{
		const GSVector4 scale = GSVector4(tex_scale[2]);
		GSVector4i feedback_rect;

		feedback_rect.left = m_regs->EXTBUF.WDX;
		feedback_rect.right = feedback_rect.left + ((m_regs->EXTDATA.WW + 1) / ((m_regs->EXTDATA.SMPH - m_regs->DISP[m_regs->EXTBUF.FBIN].DISPLAY.MAGH) + 1));
		feedback_rect.top = m_regs->EXTBUF.WDY;
		feedback_rect.bottom = ((m_regs->EXTDATA.WH + 1) * (2 - m_regs->EXTBUF.WFFMD)) / ((m_regs->EXTDATA.SMPV - m_regs->DISP[m_regs->EXTBUF.FBIN].DISPLAY.MAGV) + 1);

		dst[2] = GSVector4(scale * GSVector4(feedback_rect.rsize()));
	}

	const GSVector2i resolution = PCRTCDisplays.GetResolution();
	fs = GSVector2i(static_cast<int>(static_cast<float>(resolution.x) * GetUpscaleMultiplier()),
		static_cast<int>(static_cast<float>(resolution.y) * GetUpscaleMultiplier()));

	m_real_size = GSVector2i(fs.x, fs.y);

	if ((tex[0] == tex[1]) && (src_gs_read[0] == src_gs_read[1]).alltrue() && (dst[0] == dst[1]).alltrue() &&
		(PCRTCDisplays.PCRTCDisplays[0].displayRect == PCRTCDisplays.PCRTCDisplays[1].displayRect).alltrue() &&
		(PCRTCDisplays.PCRTCDisplays[0].framebufferRect == PCRTCDisplays.PCRTCDisplays[1].framebufferRect).alltrue() &&
		!feedback_merge && !m_regs->PMODE.SLBG)
	{
		// the two outputs are identical, skip drawing one of them (the one that is alpha blended)
		tex[0] = nullptr;
	}

	const u32 c = (m_regs->BGCOLOR.U32[0] & 0x00FFFFFFu) | (m_regs->PMODE.ALP << 24);
	g_gs_device->Merge(tex, src_gs_read, dst, fs, m_regs->PMODE, m_regs->EXTBUF, c);

	if (isReallyInterlaced() && GSConfig.InterlaceMode != GSInterlaceMode::Off)
	{
		const float offset = is_bob ? (tex[1] ? tex_scale[1] : tex_scale[0]) : 0.0f;

		g_gs_device->Interlace(fs, field ^ field2, mode, offset);
	}

	if (GSConfig.ShadeBoost)
		g_gs_device->ShadeBoost();

	if (GSConfig.FXAA)
		g_gs_device->FXAA();

	// Sharpens biinear at lower resolutions, almost nearest but with more uniform pixels.
	if (GSConfig.LinearPresent == GSPostBilinearMode::BilinearSharp && (g_gs_device->GetWindowWidth() > fs.x || g_gs_device->GetWindowHeight() > fs.y))
	{
		g_gs_device->Resize(g_gs_device->GetWindowWidth(), g_gs_device->GetWindowHeight());
	}

	if (m_scanmask_used)
		m_scanmask_used--;

	return true;
}

GSVector2i GSRenderer::GetInternalResolution()
{
	return m_real_size;
}

float GSRenderer::GetModXYOffset()
{
	float mod_xy = 0.0f;

	if (GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::Normal)
	{
		mod_xy = GetUpscaleMultiplier();
		switch (static_cast<int>(std::round(mod_xy)))
		{
			case 2: case 4: case 6: case 8: mod_xy += 0.2f; break;
			case 3: case 7:                 mod_xy += 0.1f; break;
			case 5:                         mod_xy += 0.3f; break;
			default:                        mod_xy = 0.0f; break;
		}
	}

	return mod_xy;
}

static float GetCurrentAspectRatioFloat(bool is_progressive)
{
	static constexpr std::array<float, static_cast<size_t>(AspectRatioType::MaxCount) + 1> ars = {{4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f, 16.0f / 9.0f, 3.0f / 2.0f}};
	return ars[static_cast<u32>(GSConfig.AspectRatio) + (3u * (is_progressive && GSConfig.AspectRatio == AspectRatioType::RAuto4_3_3_2))];
}

static GSVector4 CalculateDrawDstRect(s32 window_width, s32 window_height, const GSVector4i& src_rect, const GSVector2i& src_size, GSDisplayAlignment alignment, bool flip_y, bool is_progressive)
{
	const float f_width = static_cast<float>(window_width);
	const float f_height = static_cast<float>(window_height);
	const float clientAr = f_width / f_height;

	float targetAr = clientAr;
	if (EmuConfig.CurrentAspectRatio == AspectRatioType::RAuto4_3_3_2)
	{
		if (is_progressive)
			targetAr = 3.0f / 2.0f;
		else
			targetAr = 4.0f / 3.0f;
	}
	else if (EmuConfig.CurrentAspectRatio == AspectRatioType::R4_3)
	{
		targetAr = 4.0f / 3.0f;
	}
	else if (EmuConfig.CurrentAspectRatio == AspectRatioType::R16_9)
		targetAr = 16.0f / 9.0f;

	const float crop_adjust = (static_cast<float>(src_rect.width()) / static_cast<float>(src_size.x)) /
		(static_cast<float>(src_rect.height()) / static_cast<float>(src_size.y));

	const double arr = (targetAr * crop_adjust) / clientAr;
	float target_width = f_width;
	float target_height = f_height;
	if (arr < 1)
		target_width = std::floor(f_width * arr + 0.5f);
	else if (arr > 1)
		target_height = std::floor(f_height / arr + 0.5f);

	target_height *= GSConfig.StretchY / 100.0f;

	if (GSConfig.IntegerScaling)
	{
		// make target width/height an integer multiple of the texture width/height
		float t_width = static_cast<double>(src_rect.width());
		float t_height = static_cast<double>(src_rect.height());

		// If using Bilinear (Shape) the image will be prescaled to larger than the window, so we need to unscale it.
		if (GSConfig.LinearPresent == GSPostBilinearMode::BilinearSharp && src_rect.width() > 0 && src_rect.height() > 0)
		{
			const GSVector2i resolution = g_gs_renderer->PCRTCDisplays.GetResolution();
			const GSVector2i fs = GSVector2i(static_cast<int>(static_cast<float>(resolution.x) * g_gs_renderer->GetUpscaleMultiplier()),
				static_cast<int>(static_cast<float>(resolution.y) * g_gs_renderer->GetUpscaleMultiplier()));

			if (g_gs_device->GetWindowWidth() > fs.x || g_gs_device->GetWindowHeight() > fs.y)
			{
				t_width *= static_cast<float>(fs.x) / src_rect.width();
				t_height *= static_cast<float>(fs.y) / src_rect.height();
			}
		}

		float scale;
		if ((t_width / t_height) >= 1.0)
			scale = target_width / t_width;
		else
			scale = target_height / t_height;

		if (scale > 1.0)
		{
			const float adjust = std::floor(scale) / scale;
			target_width = target_width * adjust;
			target_height = target_height * adjust;
		}
	}

	float target_x, target_y;
	if (target_width >= f_width)
	{
		target_x = -((target_width - f_width) * 0.5f);
	}
	else
	{
		switch (alignment)
		{
			case GSDisplayAlignment::Center:
				target_x = (f_width - target_width) * 0.5f;
				break;
			case GSDisplayAlignment::RightOrBottom:
				target_x = (f_width - target_width);
				break;
			case GSDisplayAlignment::LeftOrTop:
			default:
				target_x = 0.0f;
				break;
		}
	}
	if (target_height >= f_height)
	{
		target_y = -((target_height - f_height) * 0.5f);
	}
	else
	{
		switch (alignment)
		{
			case GSDisplayAlignment::Center:
				target_y = (f_height - target_height) * 0.5f;
				break;
			case GSDisplayAlignment::RightOrBottom:
				target_y = (f_height - target_height);
				break;
			case GSDisplayAlignment::LeftOrTop:
			default:
				target_y = 0.0f;
				break;
		}
	}

	GSVector4 ret(target_x, target_y, target_x + target_width, target_y + target_height);

	if (flip_y)
	{
		const float height = ret.w - ret.y;
		ret.y = static_cast<float>(window_height) - ret.w;
		ret.w = ret.y + height;
	}

	return ret;
}

static GSVector4i CalculateDrawSrcRect(const GSTexture* src)
{
	const float upscale = GSConfig.UpscaleMultiplier;
	const GSVector2i size(src->GetSize());
	const int left = static_cast<int>(static_cast<float>(GSConfig.Crop[0]) * upscale);
	const int top = static_cast<int>(static_cast<float>(GSConfig.Crop[1]) * upscale);
	const int right =  size.x - static_cast<int>(static_cast<float>(GSConfig.Crop[2]) * upscale);
	const int bottom = size.y - static_cast<int>(static_cast<float>(GSConfig.Crop[3]) * upscale);
	return GSVector4i(left, top, right, bottom);
}

static const char* GetScreenshotSuffix()
{
	static constexpr const char* suffixes[static_cast<u8>(GSScreenshotFormat::Count)] = {
		"png", "jpg"};
	return suffixes[static_cast<u8>(GSConfig.ScreenshotFormat)];
}

static void CompressAndWriteScreenshot(std::string filename, u32 width, u32 height, std::vector<u32> pixels)
{
	Common::RGBA8Image image;
	image.SetPixels(width, height, std::move(pixels));

	std::string key(fmt::format("GSScreenshot_{}", filename));

	if (!GSDumpReplayer::IsRunner())
	{
		Host::AddIconOSDMessage(key, ICON_FA_CAMERA,
			fmt::format(TRANSLATE_FS("GS", "Saving screenshot to '{}'."), Path::GetFileName(filename)), 60.0f);
	}

	// maybe std::async would be better here.. but it's definitely worth threading, large screenshots take a while to compress.
	std::unique_lock lock(s_screenshot_threads_mutex);
	s_screenshot_threads.emplace_back([key = std::move(key), filename = std::move(filename), image = std::move(image),
										  quality = GSConfig.ScreenshotQuality]() {
		if (image.SaveToFile(filename.c_str(), quality))
		{
			if (!GSDumpReplayer::IsRunner())
			{
				Host::AddIconOSDMessage(std::move(key), ICON_FA_CAMERA,
					fmt::format(TRANSLATE_FS("GS", "Saved screenshot to '{}'."), Path::GetFileName(filename)),
					Host::OSD_INFO_DURATION);
			}
		}
		else
		{
			Host::AddIconOSDMessage(std::move(key), ICON_FA_CAMERA,
				fmt::format(TRANSLATE_FS("GS", "Failed to save screenshot to '{}'."), Path::GetFileName(filename),
					Host::OSD_ERROR_DURATION));
		}

		// remove ourselves from the list, if the GS thread is waiting for us, we won't be in there
		const auto this_id = std::this_thread::get_id();
		std::unique_lock lock(s_screenshot_threads_mutex);
		for (auto it = s_screenshot_threads.begin(); it != s_screenshot_threads.end(); ++it)
		{
			if (it->get_id() == this_id)
			{
				it->detach();
				s_screenshot_threads.erase(it);
				break;
			}
		}
	});
}

void GSJoinSnapshotThreads()
{
	std::unique_lock lock(s_screenshot_threads_mutex);
	while (!s_screenshot_threads.empty())
	{
		std::thread save_thread(std::move(s_screenshot_threads.front()));
		s_screenshot_threads.pop_front();
		lock.unlock();
		save_thread.join();
		lock.lock();
	}
}

bool GSRenderer::BeginPresentFrame(bool frame_skip)
{
	Host::BeginPresentFrame();

	const GSDevice::PresentResult res = g_gs_device->BeginPresent(frame_skip);
	if (res == GSDevice::PresentResult::FrameSkipped)
	{
		// If we're skipping a frame, we need to reset imgui's state, since
		// we won't be calling EndPresentFrame().
		ImGuiManager::SkipFrame();
		return false;
	}
	else if (res == GSDevice::PresentResult::OK)
	{
		// All good!
		return true;
	}

	// If we're constantly crashing on something in particular, we don't want to end up in an
	// endless reset loop.. that'd probably end up leaking memory and/or crashing us for other
	// reasons. So just abort in such case.
	const Common::Timer::Value current_time = Common::Timer::GetCurrentValue();
	if (s_last_gpu_reset_time != 0 &&
		Common::Timer::ConvertValueToSeconds(current_time - s_last_gpu_reset_time) < 15.0f)
	{
		pxFailRel("Host GPU lost too many times, device is probably completely wedged.");
	}
	s_last_gpu_reset_time = current_time;

	// Device lost, something went really bad.
	// Let's just toss out everything, and try to hobble on.
	if (!GSreopen(true, GSGetCurrentRenderer(), std::nullopt))
	{
		pxFailRel("Failed to recreate GS device after loss.");
		return false;
	}

	// First frame after reopening is definitely going to be trash, so skip it.
	Host::AddIconOSDMessage("GSDeviceLost", ICON_FA_EXCLAMATION_TRIANGLE,
		TRANSLATE_SV("GS", "Host GPU device encountered an error and was recovered. This may have broken rendering."),
		Host::OSD_CRITICAL_ERROR_DURATION);
	return false;
}

void GSRenderer::EndPresentFrame()
{
	if (GSDumpReplayer::IsReplayingDump())
		GSDumpReplayer::RenderUI();

	FullscreenUI::Render();
	ImGuiManager::RenderOSD();
	g_gs_device->EndPresent();
	ImGuiManager::NewFrame();
}

void GSRenderer::VSync(u32 field, bool registers_written, bool idle_frame)
{
	if (GSConfig.DumpGSData && s_n >= GSConfig.SaveN)
	{
		DumpGSPrivRegs(*m_regs, GetDrawDumpPath("vsync_%05d_f%lld_gs_reg.txt", s_n, g_perfmon.GetFrame()));
	}

	const int fb_sprite_blits = g_perfmon.GetDisplayFramebufferSpriteBlits();
	const bool fb_sprite_frame = (fb_sprite_blits > 0);

	bool skip_frame = false;
	if (GSConfig.SkipDuplicateFrames && !GSCapture::IsCapturingVideo())
	{
		bool is_unique_frame;
		switch (PerformanceMetrics::GetInternalFPSMethod())
		{
		case PerformanceMetrics::InternalFPSMethod::GSPrivilegedRegister:
			is_unique_frame = registers_written;
			break;
		case PerformanceMetrics::InternalFPSMethod::DISPFBBlit:
			is_unique_frame = fb_sprite_frame;
			break;
		default:
			is_unique_frame = true;
			break;
		}

		if (!is_unique_frame && m_skipped_duplicate_frames < MAX_SKIPPED_DUPLICATE_FRAMES)
		{
			m_skipped_duplicate_frames++;
			skip_frame = true;
		}
		else
		{
			m_skipped_duplicate_frames = 0;
		}
	}

	const bool blank_frame = !Merge(field);

	m_last_draw_n = s_n;
	m_last_transfer_n = s_transfer_n;

	if (skip_frame)
	{
		if (BeginPresentFrame(true))
			EndPresentFrame();

		PerformanceMetrics::Update(registers_written, fb_sprite_frame, true);
		return;
	}

	if (!idle_frame)
		g_gs_device->AgePool();


	g_perfmon.EndFrame(idle_frame);

	if ((g_perfmon.GetFrame() & 0x1f) == 0)
		g_perfmon.Update();

	// Little bit ugly, but we can't do CAS inside the render pass.
	GSVector4i src_rect;
	GSVector4 src_uv, draw_rect;
	GSTexture* current = g_gs_device->GetCurrent();
	if (current && !blank_frame)
	{
		src_rect = CalculateDrawSrcRect(current);
		src_uv = GSVector4(src_rect) / GSVector4(current->GetSize()).xyxy();
		draw_rect = CalculateDrawDstRect(g_gs_device->GetWindowWidth(), g_gs_device->GetWindowHeight(),
			src_rect, current->GetSize(), s_display_alignment, g_gs_device->UsesLowerLeftOrigin(),
			GetVideoMode() == GSVideoMode::SDTV_480P);
		s_last_draw_rect = draw_rect;

		if (GSConfig.CASMode != GSCASMode::Disabled)
		{
			static bool cas_log_once = false;
			if (g_gs_device->Features().cas_sharpening)
			{
				// sharpen only if the IR is higher than the display resolution
				const bool sharpen_only = (GSConfig.CASMode == GSCASMode::SharpenOnly ||
										   (current->GetWidth() > g_gs_device->GetWindowWidth() &&
											   current->GetHeight() > g_gs_device->GetWindowHeight()));
				g_gs_device->CAS(current, src_rect, src_uv, draw_rect, sharpen_only);
			}
			else if (!cas_log_once)
			{
				Host::AddIconOSDMessage("CASUnsupported", ICON_FA_EXCLAMATION_TRIANGLE,
					TRANSLATE_SV("GS",
						"CAS is not available, your graphics driver does not support the required functionality."),
					10.0f);
				cas_log_once = true;
			}
		}
	}

	if (BeginPresentFrame(false))
	{
		if (current && !blank_frame)
		{
			const u64 current_time = Common::Timer::GetCurrentValue();
			const float shader_time = static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - m_shader_time_start));

			g_gs_device->PresentRect(current, src_uv, nullptr, draw_rect,
				s_tv_shader_indices[GSConfig.TVShader], shader_time, GSConfig.LinearPresent != GSPostBilinearMode::Off);
		}

		EndPresentFrame();

		if (GSConfig.OsdShowGPU)
			PerformanceMetrics::OnGPUPresent(g_gs_device->GetAndResetAccumulatedGPUTime());
	}

	PerformanceMetrics::Update(registers_written, fb_sprite_frame, false);

	// snapshot
	if (!m_snapshot.empty())
	{
		u32 screenshot_width, screenshot_height;
		std::vector<u32> screenshot_pixels;

		if (!m_dump && m_dump_frames > 0)
		{
			if (GSConfig.UserHacks_ReadTCOnClose)
				ReadbackTextureCache();

			freezeData fd = {0, nullptr};
			Freeze(&fd, true);
			fd.data = new u8[fd.size];
			Freeze(&fd, false);

			// keep the screenshot relatively small so we don't bloat the dump
			static constexpr u32 DUMP_SCREENSHOT_WIDTH = 640;
			static constexpr u32 DUMP_SCREENSHOT_HEIGHT = 480;
			SaveSnapshotToMemory(DUMP_SCREENSHOT_WIDTH, DUMP_SCREENSHOT_HEIGHT, true, false,
				&screenshot_width, &screenshot_height, &screenshot_pixels);

			std::string_view compression_str;
			if (GSConfig.GSDumpCompression == GSDumpCompressionMethod::Uncompressed)
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpUncompressed(m_snapshot, VMManager::GetDiscSerial(),
					VMManager::GetDiscCRC(), screenshot_width, screenshot_height,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(), fd, m_regs));
				compression_str = TRANSLATE_SV("GS", "with no compression");
			}
			else if (GSConfig.GSDumpCompression == GSDumpCompressionMethod::LZMA)
			{
				m_dump = std::unique_ptr<GSDumpBase>(
					new GSDumpXz(m_snapshot, VMManager::GetDiscSerial(), VMManager::GetDiscCRC(), screenshot_width,
						screenshot_height, screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(), fd, m_regs));
				compression_str = TRANSLATE_SV("GS", "with LZMA compression");
			}
			else
			{
				m_dump = std::unique_ptr<GSDumpBase>(
					new GSDumpZst(m_snapshot, VMManager::GetDiscSerial(), VMManager::GetDiscCRC(), screenshot_width,
						screenshot_height, screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(), fd, m_regs));
				compression_str = TRANSLATE_SV("GS", "with Zstandard compression");
			}

			delete[] fd.data;

			Host::AddKeyedOSDMessage("GSDump",
				fmt::format(TRANSLATE_FS("GS", "Saving {0} GS dump {1} to '{2}'"),
					(m_dump_frames == 1) ? TRANSLATE_SV("GS", "single frame") : TRANSLATE_SV("GS", "multi-frame"), compression_str,
					Path::GetFileName(m_dump->GetPath())),
				Host::OSD_INFO_DURATION);
		}

		const bool internal_resolution = (GSConfig.ScreenshotSize >= GSScreenshotSize::InternalResolution);
		const bool aspect_correct = (GSConfig.ScreenshotSize != GSScreenshotSize::InternalResolutionUncorrected);

		if (g_gs_device->GetCurrent() && SaveSnapshotToMemory(
			internal_resolution ? 0 : g_gs_device->GetWindowWidth(),
			internal_resolution ? 0 : g_gs_device->GetWindowHeight(),
			aspect_correct, true,
			&screenshot_width, &screenshot_height, &screenshot_pixels))
		{
			CompressAndWriteScreenshot(fmt::format("{}.{}", m_snapshot, GetScreenshotSuffix()),
				screenshot_width, screenshot_height, std::move(screenshot_pixels));
		}
		else
		{
			Host::AddIconOSDMessage("GSScreenshot", ICON_FA_CAMERA,
				TRANSLATE_SV("GS", "Failed to render/download screenshot."), Host::OSD_ERROR_DURATION);
		}

		m_snapshot = {};
	}
	else if (m_dump)
	{
		const bool last = (m_dump_frames == 0);
		if (m_dump->VSync(field, last, m_regs))
		{
			Host::AddKeyedOSDMessage("GSDump",
				fmt::format(TRANSLATE_FS("GS", "Saved GS dump to '{}'."), Path::GetFileName(m_dump->GetPath())),
				Host::OSD_INFO_DURATION);
			m_dump.reset();
		}
		else if (!last)
		{
			m_dump_frames--;
		}
	}

	// capture
	if (GSCapture::IsCapturingVideo())
	{
		const GSVector2i size = GSCapture::GetSize();
		if (GSTexture* current = g_gs_device->GetCurrent())
		{
			// TODO: Maybe avoid this copy in the future? We can use swscale to fix it up on the dumping thread..
			if (current->GetSize() != size)
			{
				GSTexture* temp = g_gs_device->CreateRenderTarget(size.x, size.y, GSTexture::Format::Color, false);
				if (temp)
				{
					g_gs_device->StretchRect(current, temp, GSVector4(0, 0, size.x, size.y));
					GSCapture::DeliverVideoFrame(temp);
					g_gs_device->Recycle(temp);
				}
			}
			else
			{
				GSCapture::DeliverVideoFrame(current);
			}
		}
		else
		{
			// Bit janky, but unless we want to make variable frame rate files, we need to deliver *a* frame to
			// the video file, so just grab a blank RT.
			GSTexture* temp = g_gs_device->CreateRenderTarget(size.x, size.y, GSTexture::Format::Color, true);
			if (temp)
			{
				GSCapture::DeliverVideoFrame(temp);
				g_gs_device->Recycle(temp);
			}
		}
	}
}

void GSRenderer::QueueSnapshot(const std::string& path, u32 gsdump_frames)
{
	if (!m_snapshot.empty())
		return;

	// Allows for providing a complete path
	if (path.size() > 4 && StringUtil::EndsWithNoCase(path, ".png"))
	{
		m_snapshot = path.substr(0, path.size() - 4);
	}
	else
	{
		m_snapshot = GSGetBaseSnapshotFilename();
	}

	// this is really gross, but wx we get the snapshot request after shift...
	m_dump_frames = gsdump_frames;
}

static std::string GSGetBaseFilename()
{
	std::string filename;

	// append the game serial and title
	if (std::string name(VMManager::GetTitle(true)); !name.empty())
	{
		Path::SanitizeFileName(&name);
		if (name.length() > 219)
			name.resize(219);
		filename += name;
	}
	if (std::string serial = VMManager::GetDiscSerial(); !serial.empty())
	{
		Path::SanitizeFileName(&serial);
		filename += '_';
		filename += serial;
	}

	const time_t cur_time = time(nullptr);
	char local_time[16];

	if (strftime(local_time, sizeof(local_time), "%Y%m%d%H%M%S", localtime(&cur_time)))
	{
		static time_t prev_snap;
		// The variable 'n' is used for labelling the screenshots when multiple screenshots are taken in
		// a single second, we'll start using this variable for naming when a second screenshot request is detected
		// at the same time as the first one. Hence, we're initially setting this counter to 2 to imply that
		// the captured image is the 2nd image captured at this specific time.
		static int n = 2;

		filename += '_';

		if (cur_time == prev_snap)
			filename += fmt::format("{0}_({1})", local_time, n++);
		else
		{
			n = 2;
			filename += fmt::format("{}", local_time);
		}
		prev_snap = cur_time;
	}

	return filename;
}

std::string GSGetBaseSnapshotFilename()
{
	// prepend snapshots directory
	return Path::Combine(EmuFolders::Snapshots, GSGetBaseFilename());
}

std::string GSGetBaseVideoFilename()
{
	// prepend video directory
	return Path::Combine(EmuFolders::Videos, GSGetBaseFilename());
}

void GSRenderer::StopGSDump()
{
	m_snapshot = {};
	m_dump_frames = 0;
}

void GSRenderer::PresentCurrentFrame()
{
	if (BeginPresentFrame(false))
	{
		GSTexture* current = g_gs_device->GetCurrent();
		if (current)
		{
			const GSVector4i src_rect(CalculateDrawSrcRect(current));
			const GSVector4 src_uv(GSVector4(src_rect) / GSVector4(current->GetSize()).xyxy());
			const GSVector4 draw_rect(CalculateDrawDstRect(g_gs_device->GetWindowWidth(), g_gs_device->GetWindowHeight(),
				src_rect, current->GetSize(), s_display_alignment, g_gs_device->UsesLowerLeftOrigin(),
				GetVideoMode() == GSVideoMode::SDTV_480P));
			s_last_draw_rect = draw_rect;

			const u64 current_time = Common::Timer::GetCurrentValue();
			const float shader_time = static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - m_shader_time_start));

			g_gs_device->PresentRect(current, src_uv, nullptr, draw_rect,
				s_tv_shader_indices[GSConfig.TVShader], shader_time, GSConfig.LinearPresent != GSPostBilinearMode::Off);
		}

		EndPresentFrame();
	}
}

void GSTranslateWindowToDisplayCoordinates(float window_x, float window_y, float* display_x, float* display_y)
{
	const float draw_width = s_last_draw_rect.z - s_last_draw_rect.x;
	const float draw_height = s_last_draw_rect.w - s_last_draw_rect.y;
	const float rel_x = window_x - s_last_draw_rect.x;
	const float rel_y = window_y - s_last_draw_rect.y;
	if (rel_x < 0 || rel_x > draw_width || rel_y < 0 || rel_y > draw_height)
	{
		*display_x = -1.0f;
		*display_y = -1.0f;
		return;
	}

	*display_x = rel_x / draw_width;
	*display_y = rel_y / draw_height;
}

void GSSetDisplayAlignment(GSDisplayAlignment alignment)
{
	s_display_alignment = alignment;
}

bool GSRenderer::BeginCapture(std::string filename, const GSVector2i& size)
{
	const GSVector2i capture_resolution = (size.x != 0 && size.y != 0) ?
											  size :
											  (GSConfig.VideoCaptureAutoResolution ?
													  GetInternalResolution() :
													  GSVector2i(GSConfig.VideoCaptureWidth, GSConfig.VideoCaptureHeight));

	return GSCapture::BeginCapture(GetTvRefreshRate(), capture_resolution,
		GetCurrentAspectRatioFloat(GetVideoMode() == GSVideoMode::SDTV_480P),
		std::move(filename));
}

void GSRenderer::EndCapture()
{
	GSCapture::EndCapture();
}

GSTexture* GSRenderer::LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size)
{
	return nullptr;
}

bool GSRenderer::IsIdleFrame() const
{
	return (m_last_draw_n == s_n && m_last_transfer_n == s_transfer_n);
}

bool GSRenderer::SaveSnapshotToMemory(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
	u32* width, u32* height, std::vector<u32>* pixels)
{
	GSTexture* const current = g_gs_device->GetCurrent();
	if (!current)
	{
		*width = 0;
		*height = 0;
		pixels->clear();
		return false;
	}

	const GSVector4i src_rect(CalculateDrawSrcRect(current));
	const GSVector4 src_uv(GSVector4(src_rect) / GSVector4(current->GetSize()).xyxy());

	const bool is_progressive = (GetVideoMode() == GSVideoMode::SDTV_480P);
	GSVector4 draw_rect;
	if (window_width == 0 || window_height == 0)
	{
		if (apply_aspect)
		{
			// use internal resolution of the texture
			const float aspect = GetCurrentAspectRatioFloat(is_progressive);
			const int tex_width = current->GetWidth();
			const int tex_height = current->GetHeight();

			// expand to the larger dimension
			const float tex_aspect = static_cast<float>(tex_width) / static_cast<float>(tex_height);
			if (tex_aspect >= aspect)
				draw_rect = GSVector4(0.0f, 0.0f, static_cast<float>(tex_width), static_cast<float>(tex_width) / aspect);
			else
				draw_rect = GSVector4(0.0f, 0.0f, static_cast<float>(tex_height) * aspect, static_cast<float>(tex_height));
		}
		else
		{
			// uncorrected aspect is only available at internal resolution
			draw_rect = GSVector4(0.0f, 0.0f, static_cast<float>(current->GetWidth()), static_cast<float>(current->GetHeight()));
		}
	}
	else
	{
		draw_rect = CalculateDrawDstRect(window_width, window_height, src_rect, current->GetSize(),
			GSDisplayAlignment::LeftOrTop, false, is_progressive);
	}
	const u32 draw_width = static_cast<u32>(draw_rect.z - draw_rect.x);
	const u32 draw_height = static_cast<u32>(draw_rect.w - draw_rect.y);
	const u32 image_width = crop_borders ? draw_width : std::max(draw_width, window_width);
	const u32 image_height = crop_borders ? draw_height : std::max(draw_height, window_height);

	// We're not expecting screenshots to be fast, so just allocate a download texture on demand.
	GSTexture* rt = g_gs_device->CreateRenderTarget(draw_width, draw_height, GSTexture::Format::Color, false);
	if (rt)
	{
		std::unique_ptr<GSDownloadTexture> dl(g_gs_device->CreateDownloadTexture(draw_width, draw_height, GSTexture::Format::Color));
		if (dl)
		{
			const GSVector4i rc(0, 0, draw_width, draw_height);
			g_gs_device->StretchRect(current, src_uv, rt, GSVector4(rc), ShaderConvert::TRANSPARENCY_FILTER);
			dl->CopyFromTexture(rc, rt, rc, 0);
			dl->Flush();

			if (dl->Map(rc))
			{
				const u32 pad_x = (image_width - draw_width) / 2;
				const u32 pad_y = (image_height - draw_height) / 2;
				pixels->clear();
				pixels->resize(image_width * image_height, 0);
				*width = image_width;
				*height = image_height;
				StringUtil::StrideMemCpy(pixels->data() + pad_y * image_width + pad_x, image_width * sizeof(u32), dl->GetMapPointer(),
					dl->GetMapPitch(), draw_width * sizeof(u32), draw_height);

				g_gs_device->Recycle(rt);
				return true;
			}
		}

		g_gs_device->Recycle(rt);
	}

	*width = 0;
	*height = 0;
	pixels->clear();
	return false;
}

void DumpGSPrivRegs(const GSPrivRegSet& r, const std::string& filename)
{
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "wt");
	if (!fp)
		return;

	for (int i = 0; i < 2; i++)
	{
		if (i == 0 && !r.PMODE.EN1)
			continue;
		if (i == 1 && !r.PMODE.EN2)
			continue;

		std::fprintf(fp.get(), "DISPFB[%d] BP=%05x BW=%u PSM=%u DBX=%u DBY=%u\n",
			i,
			r.DISP[i].DISPFB.Block(),
			r.DISP[i].DISPFB.FBW,
			r.DISP[i].DISPFB.PSM,
			r.DISP[i].DISPFB.DBX,
			r.DISP[i].DISPFB.DBY);

		std::fprintf(fp.get(), "DISPLAY[%d] DX=%u DY=%u DW=%u DH=%u MAGH=%u MAGV=%u\n",
			i,
			r.DISP[i].DISPLAY.DX,
			r.DISP[i].DISPLAY.DY,
			r.DISP[i].DISPLAY.DW,
			r.DISP[i].DISPLAY.DH,
			r.DISP[i].DISPLAY.MAGH,
			r.DISP[i].DISPLAY.MAGV);
	}

	std::fprintf(fp.get(), "PMODE EN1=%u EN2=%u CRTMD=%u MMOD=%u AMOD=%u SLBG=%u ALP=%u\n",
		r.PMODE.EN1,
		r.PMODE.EN2,
		r.PMODE.CRTMD,
		r.PMODE.MMOD,
		r.PMODE.AMOD,
		r.PMODE.SLBG,
		r.PMODE.ALP);

	std::fprintf(fp.get(), "SMODE1 CLKSEL=%u CMOD=%u EX=%u GCONT=%u LC=%u NVCK=%u PCK2=%u PEHS=%u PEVS=%u PHS=%u PRST=%u PVS=%u RC=%u SINT=%u SLCK=%u SLCK2=%u SPML=%u T1248=%u VCKSEL=%u VHP=%u XPCK=%u\n",
		r.SMODE1.CLKSEL,
		r.SMODE1.CMOD,
		r.SMODE1.EX,
		r.SMODE1.GCONT,
		r.SMODE1.LC,
		r.SMODE1.NVCK,
		r.SMODE1.PCK2,
		r.SMODE1.PEHS,
		r.SMODE1.PEVS,
		r.SMODE1.PHS,
		r.SMODE1.PRST,
		r.SMODE1.PVS,
		r.SMODE1.RC,
		r.SMODE1.SINT,
		r.SMODE1.SLCK,
		r.SMODE1.SLCK2,
		r.SMODE1.SPML,
		r.SMODE1.T1248,
		r.SMODE1.VCKSEL,
		r.SMODE1.VHP,
		r.SMODE1.XPCK);

	std::fprintf(fp.get(), "SMODE2 INT=%u FFMD=%u DPMS=%u\n",
		r.SMODE2.INT,
		r.SMODE2.FFMD,
		r.SMODE2.DPMS);

	std::fprintf(fp.get(), "SRFSH %08x_%08x\n",
		r.SRFSH.U32[0],
		r.SRFSH.U32[1]);

	std::fprintf(fp.get(), "SYNCH1 %08x_%08x\n",
		r.SYNCH1.U32[0],
		r.SYNCH1.U32[1]);

	std::fprintf(fp.get(), "SYNCH2 %08x_%08x\n",
		r.SYNCH2.U32[0],
		r.SYNCH2.U32[1]);

	std::fprintf(fp.get(), "SYNCV VBP=%u VBPE=%u VDP=%u VFP=%u VFPE=%u VS=%u\n",
		r.SYNCV.VBP,
		r.SYNCV.VBPE,
		r.SYNCV.VDP,
		r.SYNCV.VFP,
		r.SYNCV.VFPE,
		r.SYNCV.VS);

	std::fprintf(fp.get(), "CSR %08x_%08x\n",
		r.CSR.U32[0],
		r.CSR.U32[1]);

	std::fprintf(fp.get(), "BGCOLOR B=%u G=%u R=%u\n",
		r.BGCOLOR.B,
		r.BGCOLOR.G,
		r.BGCOLOR.R);

	std::fprintf(fp.get(), "EXTBUF BP=0x%x BW=%u FBIN=%u WFFMD=%u EMODA=%u EMODC=%u WDX=%u WDY=%u\n",
		r.EXTBUF.EXBP, r.EXTBUF.EXBW, r.EXTBUF.FBIN, r.EXTBUF.WFFMD,
		r.EXTBUF.EMODA, r.EXTBUF.EMODC, r.EXTBUF.WDX, r.EXTBUF.WDY);

	std::fprintf(fp.get(), "EXTDATA SX=%u SY=%u SMPH=%u SMPV=%u WW=%u WH=%u\n",
		r.EXTDATA.SX, r.EXTDATA.SY, r.EXTDATA.SMPH, r.EXTDATA.SMPV, r.EXTDATA.WW, r.EXTDATA.WH);

	std::fprintf(fp.get(), "EXTWRITE EN=%u\n", r.EXTWRITE.WRITE);
}
