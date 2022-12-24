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

#include "PrecompiledHeader.h"
#include "GSRenderer.h"
#include "GS/GSCapture.h"
#include "GS/GSGL.h"
#include "Host.h"
#include "HostDisplay.h"
#include "PerformanceMetrics.h"
#include "pcsx2/Config.h"
#include "IconsFontAwesome5.h"
#include "VMManager.h"
#include "common/FileSystem.h"
#include "common/Image.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"
#include "fmt/core.h"
#include <algorithm>
#include <array>
#include <deque>
#include <thread>
#include <mutex>

static constexpr std::array<PresentShader, 6> s_tv_shader_indices = {
	PresentShader::COPY, PresentShader::SCANLINE,
	PresentShader::DIAGONAL_FILTER, PresentShader::TRIANGULAR_FILTER,
	PresentShader::COMPLEX_FILTER, PresentShader::LOTTES_FILTER};

static std::deque<std::thread> s_screenshot_threads;
static std::mutex s_screenshot_threads_mutex;

std::unique_ptr<GSRenderer> g_gs_renderer;

// Since we read this on the EE thread, we can't put it in the renderer, because
// we might be switching while the other thread reads it.
static GSVector4 s_last_draw_rect;


GSRenderer::GSRenderer()
	: m_shader_time_start(Common::Timer::GetCurrentValue())
{
	s_last_draw_rect = GSVector4::zero();
}

GSRenderer::~GSRenderer() = default;

void GSRenderer::Reset(bool hardware_reset)
{
	// clear the current display texture
	if (hardware_reset)
		g_gs_device->ClearCurrent();

	GSState::Reset(hardware_reset);
}

void GSRenderer::Destroy()
{
	GSCapture::EndCapture();
}

bool GSRenderer::Merge(int field)
{
	bool en[2];

	GSVector4i fr[2];
	GSVector4i dr[2];
	GSVector2i display_offsets[2];

	GSVector2i display_baseline = {INT_MAX, INT_MAX};
	GSVector2i frame_baseline = {INT_MAX, INT_MAX};
	GSVector2i display_combined = {0, 0};
	bool feedback_merge = m_regs->EXTWRITE.WRITE == 1;
	bool display_offset = false;

	for (int i = 0; i < 2; i++)
	{
		en[i] = IsEnabled(i) || (m_regs->EXTBUF.FBIN == i && feedback_merge);

		if (en[i])
		{
			fr[i] = GetFrameRect(i);
			dr[i] = GetDisplayRect(i);
			display_offsets[i] = GetResolutionOffset(i);

			display_combined.x = std::max(((dr[i].right) - dr[i].left) + display_offsets[i].x, display_combined.x);
			display_combined.y = std::max((dr[i].bottom - dr[i].top) + display_offsets[i].y, display_combined.y);

			display_baseline.x = std::min(display_offsets[i].x, display_baseline.x);
			display_baseline.y = std::min(display_offsets[i].y, display_baseline.y);
			frame_baseline.x = std::min(std::max(fr[i].left, 0), frame_baseline.x);
			frame_baseline.y = std::min(std::max(fr[i].top, 0), frame_baseline.y);

			display_offset |= std::abs(display_baseline.y - display_offsets[i].y) == 1;
			/*DevCon.Warning("Read offset was X %d(left %d) Y %d(top %d)", display_baseline.x, dr[i].left, display_baseline.y, dr[i].top);
			DevCon.Warning("[%d]: %d %d %d %d, %d %d %d %d\n", i, fr[i].x,fr[i].y,fr[i].z,fr[i].w , dr[i].x,dr[i].y,dr[i].z,dr[i].w);
			DevCon.Warning("Offset X %d Offset Y %d", display_offsets[i].x, display_offsets[i].y);*/
		}
	}

	if (!en[0] && !en[1])
	{
		return false;
	}

	GL_PUSH("Renderer Merge %d (0: enabled %d 0x%x, 1: enabled %d 0x%x)", s_n, en[0], m_regs->DISP[0].DISPFB.Block(), en[1], m_regs->DISP[1].DISPFB.Block());

	// try to avoid fullscreen blur, could be nice on tv but on a monitor it's like double vision, hurts my eyes (persona 4, guitar hero)
	//
	// NOTE: probably the technique explained in graphtip.pdf (Antialiasing by Supersampling / 4. Reading Odd/Even Scan Lines Separately with the PCRTC then Blending)

	const bool samesrc =
		en[0] && en[1] &&
		m_regs->DISP[0].DISPFB.FBP == m_regs->DISP[1].DISPFB.FBP &&
		m_regs->DISP[0].DISPFB.FBW == m_regs->DISP[1].DISPFB.FBW &&
		GSUtil::HasCompatibleBits(m_regs->DISP[0].DISPFB.PSM, m_regs->DISP[1].DISPFB.PSM);
	bool single_fetch = false;

	GSVector2i fs(0, 0);
	GSVector2i ds(0, 0);

	GSTexture* tex[3] = {NULL, NULL, NULL};
	int y_offset[3] = {0, 0, 0};

	s_n++;

	// Only need to check the right/bottom on software renderer, hardware always gets the full texture then cuts a bit out later.
	if (samesrc && !feedback_merge && (GSConfig.UseHardwareRenderer() || (fr[0].right == fr[1].right && fr[0].bottom == fr[1].bottom)))
	{
		tex[0] = GetOutput(0, y_offset[0]);
		tex[1] = tex[0]; // saves one texture fetch
		y_offset[1] = y_offset[0];
		single_fetch = true;
	}
	else
	{
		if (en[0])
			tex[0] = GetOutput(0, y_offset[0]);
		if (en[1])
			tex[1] = GetOutput(1, y_offset[1]);
		if (feedback_merge)
			tex[2] = GetFeedbackOutput();
	}

	GSVector4 src_out_rect[2];
	GSVector4 src_gs_read[2];
	GSVector4 dst[3];

	const bool slbg = m_regs->PMODE.SLBG;

	GSVector2i resolution(GetResolution());
	bool scanmask_frame = m_scanmask_used && !display_offset;
	const bool ignore_offset = !GSConfig.PCRTCOffsets;
	const bool is_bob = GSConfig.InterlaceMode == GSInterlaceMode::BobTFF || GSConfig.InterlaceMode == GSInterlaceMode::BobBFF;

	// Use offset for bob deinterlacing always, extra offset added later for FFMD mode.
	float offset = is_bob ? (tex[1] ? tex[1]->GetScale().y : tex[0]->GetScale().y) : 0.0f;

	int field2 = 0;
	int mode = 3;

	// FFMD (half frames) requires blend deinterlacing, so automatically use that. Same when SCANMSK is used but not blended in the merge circuit (Alpine Racer 3)
	if (GSConfig.InterlaceMode != GSInterlaceMode::Automatic || (!m_regs->SMODE2.FFMD && !scanmask_frame))
	{
		field2 = ((static_cast<int>(GSConfig.InterlaceMode) - 2) & 1);
		mode = ((static_cast<int>(GSConfig.InterlaceMode) - 2) >> 1);
	}

	for (int i = 0; i < 2; i++)
	{
		if (!en[i] || !tex[i])
			continue;

		GSVector4i r = GetFrameMagnifiedRect(i);
		GSVector4 scale = GSVector4(tex[i]->GetScale()).xyxy();


		GSVector2i off(ignore_offset ? 0 : display_offsets[i]);
		GSVector2i display_diff(display_offsets[i].x - display_baseline.x, display_offsets[i].y - display_baseline.y);
		GSVector2i frame_diff(fr[i].left - frame_baseline.x, fr[i].top - frame_baseline.y);

		if (!GSConfig.UseHardwareRenderer())
		{
			// Clear any frame offsets, offset is already done in the software GetOutput.
			fr[i].right -= fr[i].left;
			fr[i].left = 0;
			fr[i].bottom -= fr[i].top;
			fr[i].top = 0;

			// Put any frame offset difference back if we aren't anti-blurring on a single fetch (not offset).
			if (!GSConfig.PCRTCAntiBlur && single_fetch)
			{
				fr[i].right += frame_diff.x;
				fr[i].left += frame_diff.x;
				fr[i].bottom += frame_diff.y;
				fr[i].top += frame_diff.y;
			}
			
		}

		// If using scanmsk we have to keep the single line offset, regardless of upscale
		// so we handle this separately after the rect calculations.
		float interlace_offset = 0.0f;

		if ((!GSConfig.PCRTCAntiBlur || m_scanmask_used) && display_offset)
		{
			interlace_offset = static_cast<float>(display_diff.y & 1);

			// When the displays are offset by 1 we need to adjust for upscale to handle it (reduces bounce in MGS2 when upscaling)
			interlace_offset += (tex[i]->GetScale().y - 1.0f)  / 2;

			if (interlace_offset >= 1.0f)
			{
				if (!ignore_offset)
					off.y -= 1;

				display_diff.y -= 1;
			}
		}

		// Start of Anti-Blur code.
		if (!ignore_offset)
		{
			if (GSConfig.PCRTCAntiBlur)
			{
				if (samesrc)
				{
					// Offset by DISPLAY setting
					if (display_diff.x < 4)
						off.x -= display_diff.x;
					if (display_diff.y < 4)
						off.y -= display_diff.y;

					// Only functional in HW mode, software clips/positions the framebuffer on read.
					if (GSConfig.UseHardwareRenderer())
					{
						// Offset by DISPFB setting
						if (abs(frame_diff.x) < 4)
							off.x += frame_diff.x;
						if (abs(frame_diff.y) < 4)
							off.y += frame_diff.y;
					}
				}
			}
		}
		else
		{
			if (!slbg || !feedback_merge)
			{
				// If the offsets between the two displays are quite large, it's probably intended for an effect.
				if (display_diff.x >= 4 || !GSConfig.PCRTCAntiBlur)
					off.x = display_diff.x;

				if (display_diff.y >= 4 || !GSConfig.PCRTCAntiBlur)
					off.y = display_diff.y;

				// Need to check if only circuit 2 is enabled. Stuntman toggles circuit 1 on and off every other frame.
				if (samesrc || m_regs->PMODE.EN == 2)
				{
					// Adjusting the screen offset when using a negative offset.
					const int videomode = static_cast<int>(GetVideoMode()) - 1;
					const GSVector4i offsets = !GSConfig.PCRTCOverscan ? VideoModeOffsets[videomode] : VideoModeOffsetsOverscan[videomode];
					GSVector2i base_resolution(offsets.x, offsets.y);

					if (isinterlaced() && !m_regs->SMODE2.FFMD)
						base_resolution.y *= 2;

					// Offset by DISPLAY setting
					if (display_diff.x < 0)
					{
						off.x = 0;
						if (base_resolution.x > resolution.x)
							resolution.x -= display_diff.x;
					}
					if (display_diff.y < 0)
					{
						off.y = 0;
						if (base_resolution.y > resolution.y)
							resolution.y -= display_diff.y;
					}

					// Don't do X, we only care about height, this would need to be tailored for games using X (Black does -5).
					// Mainly for Hokuto no Ken which does -14 Y offset.
					if (display_baseline.y < -4)
						off.y += display_baseline.y;

					// Anti-Blur stuff
					// Only functional in HW mode, software clips/positions the framebuffer on read.
					if (GSConfig.PCRTCAntiBlur && GSConfig.UseHardwareRenderer())
					{
						// Offset by DISPFB setting
						if (abs(frame_diff.x) < 4)
							off.x += frame_diff.x;
						if (abs(frame_diff.y) < 4)
							off.y += frame_diff.y;
					}
				}
			}
		}
		// End of Anti-Blur code.

		if (isinterlaced() && m_regs->SMODE2.FFMD)
			off.y >>= 1;

		// dst is the final destination rect with offset on the screen.
		dst[i] = scale * (GSVector4(off).xyxy() + GSVector4(r.rsize()));

		// src_gs_read is the size which we're really reading from GS memory.
		src_gs_read[i] = ((GSVector4(fr[i]) + GSVector4(0, y_offset[i], 0, y_offset[i])) * scale) / GSVector4(tex[i]->GetSize()).xyxy();

		// src_out_rect is the resized rect for output. (Not really used)
		src_out_rect[i] = (GSVector4(r) * scale) / GSVector4(tex[i]->GetSize()).xyxy();

		if (m_regs->SMODE2.FFMD && !is_bob && !GSConfig.DisableInterlaceOffset && GSConfig.InterlaceMode != GSInterlaceMode::Off)
		{
			// We do half because FFMD is a half sized framebuffer, then we offset by 1 in the shader for the actual interlace
			if(GetUpscaleMultiplier() > 1.0f)
				interlace_offset += ((((tex[1] ? tex[1]->GetScale().y : tex[0]->GetScale().y) + 0.5f) * 0.5f) - 1.0f) * static_cast<float>(field ^ field2);
			offset = 1.0f;
		}
		// Restore manually offset "interlace" lines
		dst[i] += GSVector4(0.0f, interlace_offset, 0.0f, interlace_offset);
	}

	if (feedback_merge && tex[2])
	{
		GSVector4 scale = GSVector4(tex[2]->GetScale()).xyxy();
		GSVector4i feedback_rect;

		feedback_rect.left = m_regs->EXTBUF.WDX;
		feedback_rect.right = feedback_rect.left + ((m_regs->EXTDATA.WW + 1) / ((m_regs->EXTDATA.SMPH - m_regs->DISP[m_regs->EXTBUF.FBIN].DISPLAY.MAGH) + 1));
		feedback_rect.top = m_regs->EXTBUF.WDY;
		feedback_rect.bottom = ((m_regs->EXTDATA.WH + 1) * (2 - m_regs->EXTBUF.WFFMD)) / ((m_regs->EXTDATA.SMPV - m_regs->DISP[m_regs->EXTBUF.FBIN].DISPLAY.MAGV) + 1);

		dst[2] = GSVector4(scale * GSVector4(feedback_rect.rsize()));
	}

	// Set the resolution to the height of the displays (kind of a saturate height)
	if (ignore_offset && !feedback_merge)
	{
		GSVector2i max_resolution = GetResolution();
		resolution.x = display_combined.x - display_baseline.x;
		resolution.y = display_combined.y - display_baseline.y;

		if (isinterlaced() && m_regs->SMODE2.FFMD)
		{
			resolution.y >>= 1;
		}

		resolution.x = std::min(max_resolution.x, resolution.x);
		resolution.y = std::min(max_resolution.y, resolution.y);
	}

	fs = GSVector2i(static_cast<int>(static_cast<float>(resolution.x) * GetUpscaleMultiplier()),
		static_cast<int>(static_cast<float>(resolution.y) * GetUpscaleMultiplier()));
	ds = fs;

	// When interlace(FRAME) mode, the rect is half height, so it needs to be stretched.
	const bool is_interlaced_resolution = m_regs->SMODE2.INT || (isReallyInterlaced() && IsAnalogue() && GSConfig.InterlaceMode != GSInterlaceMode::Off);

	if (is_interlaced_resolution && m_regs->SMODE2.FFMD)
		ds.y *= 2;

	m_real_size = GSVector2i(fs.x, is_interlaced_resolution ? ds.y : fs.y);

	if (!tex[0] && !tex[1])
		return false;

	if ((tex[0] == tex[1]) && (src_out_rect[0] == src_out_rect[1]).alltrue() && (dst[0] == dst[1]).alltrue() && !feedback_merge && !slbg)
	{
		// the two outputs are identical, skip drawing one of them (the one that is alpha blended)

		tex[0] = NULL;
	}

	GSVector4 c = GSVector4((int)m_regs->BGCOLOR.R, (int)m_regs->BGCOLOR.G, (int)m_regs->BGCOLOR.B, (int)m_regs->PMODE.ALP) / 255;

	g_gs_device->Merge(tex, src_gs_read, dst, fs, m_regs->PMODE, m_regs->EXTBUF, c);

	if (isReallyInterlaced() && GSConfig.InterlaceMode != GSInterlaceMode::Off)
		g_gs_device->Interlace(ds, field ^ field2, mode, offset);

	if (GSConfig.ShadeBoost)
		g_gs_device->ShadeBoost();

	if (GSConfig.FXAA)
		g_gs_device->FXAA();

	// Sharpens biinear at lower resolutions, almost nearest but with more uniform pixels.
	if (GSConfig.LinearPresent == GSPostBilinearMode::BilinearSharp && (g_host_display->GetWindowWidth() > fs.x || g_host_display->GetWindowHeight() > fs.y))
	{
		g_gs_device->Resize(g_host_display->GetWindowWidth(), g_host_display->GetWindowHeight());
	}

	if (m_scanmask_used)
		m_scanmask_used--;

	return true;
}

GSVector2i GSRenderer::GetInternalResolution()
{
	return m_real_size;
}

static float GetCurrentAspectRatioFloat(bool is_progressive)
{
	static constexpr std::array<float, static_cast<size_t>(AspectRatioType::MaxCount) + 1> ars = {{4.0f / 3.0f, 4.0f / 3.0f, 4.0f / 3.0f, 16.0f / 9.0f, 3.0f / 2.0f}};
	return ars[static_cast<u32>(GSConfig.AspectRatio) + (3u * (is_progressive && GSConfig.AspectRatio == AspectRatioType::RAuto4_3_3_2))];
}

static GSVector4 CalculateDrawDstRect(s32 window_width, s32 window_height, const GSVector4i& src_rect, const GSVector2i& src_size, HostDisplay::Alignment alignment, bool flip_y, bool is_progressive)
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
		const float t_width = static_cast<double>(src_rect.width());
		const float t_height = static_cast<double>(src_rect.height());

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
			case HostDisplay::Alignment::Center:
				target_x = (f_width - target_width) * 0.5f;
				break;
			case HostDisplay::Alignment::RightOrBottom:
				target_x = (f_width - target_width);
				break;
			case HostDisplay::Alignment::LeftOrTop:
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
			case HostDisplay::Alignment::Center:
				target_y = (f_height - target_height) * 0.5f;
				break;
			case HostDisplay::Alignment::RightOrBottom:
				target_y = (f_height - target_height);
				break;
			case HostDisplay::Alignment::LeftOrTop:
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
	Host::AddIconOSDMessage(key, ICON_FA_CAMERA, fmt::format("Saving screenshot to '{}'.", Path::GetFileName(filename)), 60.0f);

	// maybe std::async would be better here.. but it's definitely worth threading, large screenshots take a while to compress.
	std::unique_lock lock(s_screenshot_threads_mutex);
	s_screenshot_threads.emplace_back([key = std::move(key), filename = std::move(filename), image = std::move(image), quality = GSConfig.ScreenshotQuality]() {
		if (image.SaveToFile(filename.c_str(), quality))
		{
			Host::AddIconOSDMessage(std::move(key), ICON_FA_CAMERA,
				fmt::format("Saved screenshot to '{}'.", Path::GetFileName(filename)), Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage(std::move(key), ICON_FA_CAMERA,
				fmt::format("Failed to save screenshot to '{}'.", Path::GetFileName(filename), Host::OSD_ERROR_DURATION));
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

void GSRenderer::VSync(u32 field, bool registers_written)
{
	Flush(GSFlushReason::VSYNC);

	if (GSConfig.DumpGSData && s_n >= GSConfig.SaveN)
	{
		m_regs->Dump(GetDrawDumpPath("vsync_%05d_f%lld_gs_reg.txt", s_n, g_perfmon.GetFrame()));
	}

	const int fb_sprite_blits = g_perfmon.GetDisplayFramebufferSpriteBlits();
	const bool fb_sprite_frame = (fb_sprite_blits > 0);

	bool skip_frame = false;
	if (GSConfig.SkipDuplicateFrames)
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

	if (skip_frame)
	{
		g_gs_device->ResetAPIState();
		if (Host::BeginPresentFrame(true))
			Host::EndPresentFrame();
		g_gs_device->RestoreAPIState();
		PerformanceMetrics::Update(registers_written, fb_sprite_frame, true);
		return;
	}

	g_gs_device->AgePool();

	g_perfmon.EndFrame();
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
		draw_rect = CalculateDrawDstRect(g_host_display->GetWindowWidth(), g_host_display->GetWindowHeight(),
			src_rect, current->GetSize(), g_host_display->GetDisplayAlignment(), g_host_display->UsesLowerLeftOrigin(),
			GetVideoMode() == GSVideoMode::SDTV_480P || (GSConfig.PCRTCOverscan && GSConfig.PCRTCOffsets));
		s_last_draw_rect = draw_rect;

		if (GSConfig.CASMode != GSCASMode::Disabled)
		{
			static bool cas_log_once = false;
			if (g_gs_device->Features().cas_sharpening)
			{
				// sharpen only if the IR is higher than the display resolution
				const bool sharpen_only = (GSConfig.CASMode == GSCASMode::SharpenOnly ||
										   (current->GetWidth() > g_host_display->GetWindowWidth() &&
											   current->GetHeight() > g_host_display->GetWindowHeight()));
				g_gs_device->CAS(current, src_rect, src_uv, draw_rect, sharpen_only);
			}
			else if (!cas_log_once)
			{
				Host::AddIconOSDMessage("CASUnsupported", ICON_FA_EXCLAMATION_TRIANGLE,
					"CAS is not available, your graphics driver does not support the required functionality.", 10.0f);
				cas_log_once = true;
			}
		}
	}

	g_gs_device->ResetAPIState();
	if (Host::BeginPresentFrame(false))
	{
		if (current && !blank_frame)
		{
			const u64 current_time = Common::Timer::GetCurrentValue();
			const float shader_time = static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - m_shader_time_start));

			g_gs_device->PresentRect(current, src_uv, nullptr, draw_rect,
				s_tv_shader_indices[GSConfig.TVShader], shader_time, GSConfig.LinearPresent != GSPostBilinearMode::Off);
		}

		Host::EndPresentFrame();

		if (GSConfig.OsdShowGPU)
			PerformanceMetrics::OnGPUPresent(g_host_display->GetAndResetAccumulatedGPUTime());
	}
	g_gs_device->RestoreAPIState();
	PerformanceMetrics::Update(registers_written, fb_sprite_frame, false);

	// snapshot
	if (!m_snapshot.empty())
	{
		u32 screenshot_width, screenshot_height;
		std::vector<u32> screenshot_pixels;

		if (!m_dump && m_dump_frames > 0)
		{
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
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpUncompressed(m_snapshot, VMManager::GetGameSerial(), m_crc,
					screenshot_width, screenshot_height,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with no compression";
			}
			else if (GSConfig.GSDumpCompression == GSDumpCompressionMethod::LZMA)
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpXz(m_snapshot, VMManager::GetGameSerial(), m_crc,
					screenshot_width, screenshot_height,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with LZMA compression";
			}
			else
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpZst(m_snapshot, VMManager::GetGameSerial(), m_crc,
					screenshot_width, screenshot_height,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with Zstandard compression";
			}

			delete[] fd.data;

			Host::AddKeyedOSDMessage("GSDump", fmt::format("Saving {0} GS dump {1} to '{2}'",
				(m_dump_frames == 1) ? "single frame" : "multi-frame", compression_str,
				Path::GetFileName(m_dump->GetPath())), Host::OSD_INFO_DURATION);
		}

		const bool internal_resolution = (GSConfig.ScreenshotSize >= GSScreenshotSize::InternalResolution);
		const bool aspect_correct = (GSConfig.ScreenshotSize != GSScreenshotSize::InternalResolutionUncorrected);

		if (g_gs_device->GetCurrent() && SaveSnapshotToMemory(
			internal_resolution ? 0 : g_host_display->GetWindowWidth(),
			internal_resolution ? 0 : g_host_display->GetWindowHeight(),
			aspect_correct, true,
			&screenshot_width, &screenshot_height, &screenshot_pixels))
		{
			CompressAndWriteScreenshot(fmt::format("{}.{}", m_snapshot, GetScreenshotSuffix()),
				screenshot_width, screenshot_height, std::move(screenshot_pixels));
		}
		else
		{
			Host::AddIconOSDMessage("GSScreenshot", ICON_FA_CAMERA, "Failed to render/download screenshot.", Host::OSD_ERROR_DURATION);
		}

		m_snapshot = {};
	}
	else if (m_dump)
	{
		const bool last = (m_dump_frames == 0);
		if (m_dump->VSync(field, last, m_regs))
		{
			Host::AddKeyedOSDMessage("GSDump", fmt::format("Saved GS dump to '{}'.", Path::GetFileName(m_dump->GetPath())), Host::OSD_INFO_DURATION);
			m_dump.reset();
		}
		else if (!last)
		{
			m_dump_frames--;
		}
	}

	// capture
	if (GSCapture::IsCapturing())
	{
		if (GSTexture* current = g_gs_device->GetCurrent())
		{
			GSVector2i size = GSCapture::GetSize();

			bool res;
			GSTexture::GSMap m;
			if (size == current->GetSize())
				res = g_gs_device->DownloadTexture(current, GSVector4i(0, 0, size.x, size.y), m);
			else
				res = g_gs_device->DownloadTextureConvert(current, GSVector4(0, 0, 1, 1), size, GSTexture::Format::Color, ShaderConvert::COPY, m, true);

			if (res)
			{
				GSCapture::DeliverFrame(m.bits, m.pitch, !g_gs_device->IsRBSwapped());
				g_gs_device->DownloadTextureComplete();
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

std::string GSGetBaseSnapshotFilename()
{
	std::string filename;

	// append the game serial and title
	if (std::string name(VMManager::GetGameName()); !name.empty())
	{
		Path::SanitizeFileName(&name);
		if (name.length() > 219)
			name.resize(219);
		filename += name;
	}
	if (std::string serial(VMManager::GetGameSerial()); !serial.empty())
	{
		Path::SanitizeFileName(&serial);
		filename += '_';
		filename += serial;
	}

	time_t cur_time = time(nullptr);
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

	// prepend snapshots directory
	return Path::Combine(EmuFolders::Snapshots, filename);
}

void GSRenderer::StopGSDump()
{
	m_snapshot = {};
	m_dump_frames = 0;
}

void GSRenderer::PresentCurrentFrame()
{
	g_gs_device->ResetAPIState();
	if (Host::BeginPresentFrame(false))
	{
		GSTexture* current = g_gs_device->GetCurrent();
		if (current)
		{
			const GSVector4i src_rect(CalculateDrawSrcRect(current));
			const GSVector4 src_uv(GSVector4(src_rect) / GSVector4(current->GetSize()).xyxy());
			const GSVector4 draw_rect(CalculateDrawDstRect(g_host_display->GetWindowWidth(), g_host_display->GetWindowHeight(),
				src_rect, current->GetSize(), g_host_display->GetDisplayAlignment(), g_host_display->UsesLowerLeftOrigin(),
				GetVideoMode() == GSVideoMode::SDTV_480P || (GSConfig.PCRTCOverscan && GSConfig.PCRTCOffsets)));
			s_last_draw_rect = draw_rect;

			const u64 current_time = Common::Timer::GetCurrentValue();
			const float shader_time = static_cast<float>(Common::Timer::ConvertValueToSeconds(current_time - m_shader_time_start));

			g_gs_device->PresentRect(current, src_uv, nullptr, draw_rect,
				s_tv_shader_indices[GSConfig.TVShader], shader_time, GSConfig.LinearPresent != GSPostBilinearMode::Off);
		}

		Host::EndPresentFrame();
	}
	g_gs_device->RestoreAPIState();
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

bool GSRenderer::BeginCapture(std::string filename)
{
	return GSCapture::BeginCapture(GetTvRefreshRate(), GetInternalResolution(),
		GetCurrentAspectRatioFloat(GetVideoMode() == GSVideoMode::SDTV_480P || (GSConfig.PCRTCOverscan && GSConfig.PCRTCOffsets)),
		std::move(filename));
}

void GSRenderer::EndCapture()
{
	GSCapture::EndCapture();
}

void GSRenderer::PurgePool()
{
	g_gs_device->PurgePool();
}

void GSRenderer::PurgeTextureCache()
{
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

	const bool is_progressive = (GetVideoMode() == GSVideoMode::SDTV_480P || (GSConfig.PCRTCOverscan && GSConfig.PCRTCOffsets));
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
			HostDisplay::Alignment::LeftOrTop, false, is_progressive);
	}
	const u32 draw_width = static_cast<u32>(draw_rect.z - draw_rect.x);
	const u32 draw_height = static_cast<u32>(draw_rect.w - draw_rect.y);
	const u32 image_width = crop_borders ? draw_width : std::max(draw_width, window_width);
	const u32 image_height = crop_borders ? draw_height : std::max(draw_height, window_height);

	GSTexture::GSMap map;
	const bool result = g_gs_device->DownloadTextureConvert(
		current, src_uv,
		GSVector2i(draw_width, draw_height), GSTexture::Format::Color,
		ShaderConvert::TRANSPARENCY_FILTER, map, true);
	if (result)
	{
		const u32 pad_x = (image_width - draw_width) / 2;
		const u32 pad_y = (image_height - draw_height) / 2;
		pixels->clear();
		pixels->resize(image_width * image_height, 0);
		*width = image_width;
		*height = image_height;
		StringUtil::StrideMemCpy(pixels->data() + pad_y * image_width + pad_x, image_width * sizeof(u32),
			map.bits, map.pitch, draw_width * sizeof(u32), draw_height);

		g_gs_device->DownloadTextureComplete();
	}

	return result;
}
