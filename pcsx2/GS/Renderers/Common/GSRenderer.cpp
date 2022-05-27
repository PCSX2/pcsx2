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
#include "GS/GSGL.h"
#include "Host.h"
#include "HostDisplay.h"
#include "PerformanceMetrics.h"
#include "pcsx2/Config.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "fmt/core.h"

#ifndef PCSX2_CORE
#include "gui/AppCoreThread.h"
#if defined(__unix__)
#include <X11/keysym.h>
#elif defined(__APPLE__)
#include <Carbon/Carbon.h>
#endif

static std::string GetDumpName()
{
	return StringUtil::wxStringToUTF8String(GameInfo::gameName);
}
static std::string GetDumpSerial()
{
	return StringUtil::wxStringToUTF8String(GameInfo::gameSerial);
}
#else
#include "VMManager.h"

static std::string GetDumpName()
{
	return VMManager::GetGameName();
}
static std::string GetDumpSerial()
{
	return VMManager::GetGameSerial();
}
#endif

std::unique_ptr<GSRenderer> g_gs_renderer;

GSRenderer::GSRenderer() = default;

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
			frame_baseline.x = std::min(fr[i].left, frame_baseline.x);
			frame_baseline.y = std::min(fr[i].top, frame_baseline.y);

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

	bool samesrc =
		en[0] && en[1] &&
		m_regs->DISP[0].DISPFB.FBP == m_regs->DISP[1].DISPFB.FBP &&
		m_regs->DISP[0].DISPFB.FBW == m_regs->DISP[1].DISPFB.FBW &&
		m_regs->DISP[0].DISPFB.PSM == m_regs->DISP[1].DISPFB.PSM;

	GSVector2i fs(0, 0);
	GSVector2i ds(0, 0);

	GSTexture* tex[3] = {NULL, NULL, NULL};
	int y_offset[3] = {0, 0, 0};

	s_n++;

	if (samesrc && fr[0].bottom == fr[1].bottom && !feedback_merge)
	{
		tex[0] = GetOutput(0, y_offset[0]);
		tex[1] = tex[0]; // saves one texture fetch
		y_offset[1] = y_offset[0];
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
	bool scanmask_frame = true;
	const bool ignore_offset = !GSConfig.PCRTCOffsets;

	for (int i = 0; i < 2; i++)
	{
		if (!en[i] || !tex[i])
			continue;

		GSVector4i r = GetFrameMagnifiedRect(i);
		GSVector4 scale = GSVector4(tex[i]->GetScale()).xyxy();


		GSVector2i off(ignore_offset ? 0 : display_offsets[i]);
		GSVector2i display_diff(display_offsets[i].x - display_baseline.x, display_offsets[i].y - display_baseline.y);
		GSVector2i frame_diff(fr[i].left - frame_baseline.x, fr[i].top - frame_baseline.y);

		// If using scanmsk we have to keep the single line offset, regardless of upscale
		// so we handle this separately after the rect calculations.
		const int interlace_offset = display_diff.y & 1;

		if (m_scanmask_used && interlace_offset)
		{
			display_diff.y &= ~1;
			scanmask_frame = false;
			if (!ignore_offset)
				off.y &= ~1;
		}

		// Start of Anti-Blur code.
		if (!ignore_offset)
		{
			if (samesrc)
			{
				// Offset by DISPLAY setting
				if (display_diff.x < 4)
					off.x -= display_diff.x;
				if (display_diff.y < 4)
					off.y -= display_diff.y;

				// Offset by DISPFB setting
				if (frame_diff.x == 1)
					off.x += 1;
				if (frame_diff.y == 1)
					off.y += 1;
			}
		}
		else
		{
			if (!slbg || !feedback_merge)
			{
				const int videomode = static_cast<int>(GetVideoMode()) - 1;
				GSVector2i base_resolution(VideoModeOffsets[videomode].x, VideoModeOffsets[videomode].y);

				if (isinterlaced() && !m_regs->SMODE2.FFMD)
					base_resolution.y *= 2;

				// If the offsets between the two displays are quite large, it's probably intended for an effect.
				if (display_diff.x >= 4)
					off.x = display_diff.x;

				if (display_diff.y >= 4)
					off.y = display_diff.y;

				// Anti blur hax.
				if (samesrc)
				{
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

					// Offset by DISPFB setting
					if (frame_diff.x == 1)
						off.x += 1;

					if (frame_diff.y == 1)
						off.y += 1;

					// Don't do X, we only care about height, this would need to be tailored for games using X (Black does -5).
					// Mainly for Hokuto no Ken which does -14 Y offset.
					if (display_baseline.y < -4)
						off.y += display_baseline.y;
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

		// Restore the single line offset for scanmsk.
		if (m_scanmask_used && interlace_offset)
			dst[i] += GSVector4(0.0f, 1.0f, 0.0f, 1.0f);
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

	fs = resolution * GSVector2i(GetUpscaleMultiplier());
	ds = fs;

	// When interlace(FRAME) mode, the rect is half height, so it needs to be stretched.
	if (m_regs->SMODE2.INT && m_regs->SMODE2.FFMD)
	{
		ds.y *= 2;
	}

	m_real_size = ds;

	if (tex[0] || tex[1])
	{
		if ((tex[0] == tex[1]) && (src_out_rect[0] == src_out_rect[1]).alltrue() && (dst[0] == dst[1]).alltrue() && !feedback_merge && !slbg)
		{
			// the two outputs are identical, skip drawing one of them (the one that is alpha blended)

			tex[0] = NULL;
		}

		GSVector4 c = GSVector4((int)m_regs->BGCOLOR.R, (int)m_regs->BGCOLOR.G, (int)m_regs->BGCOLOR.B, (int)m_regs->PMODE.ALP) / 255;

		g_gs_device->Merge(tex, src_gs_read, dst, fs, m_regs->PMODE, m_regs->EXTBUF, c);

		// Offset is not compatible with scanmsk, as scanmsk renders every other line, but at x7 the interlace offset will be 7 lines
		const int offset = (m_scanmask_used || !m_regs->SMODE2.FFMD) ? 0 : (int)(tex[1] ? tex[1]->GetScale().y : tex[0]->GetScale().y);

		if (m_regs->SMODE2.INT && GSConfig.InterlaceMode != GSInterlaceMode::Off)
		{
			const bool scanmask = m_scanmask_used && scanmask_frame && GSConfig.InterlaceMode == GSInterlaceMode::Automatic;

			if (GSConfig.InterlaceMode == GSInterlaceMode::Automatic && (m_regs->SMODE2.FFMD)) // Auto interlace enabled / Odd frame interlace setting
			{
				constexpr int field2 = 1;
				constexpr int mode = 2;

				g_gs_device->Interlace(ds, field ^ field2, mode, offset);
			}
			else
			{
				const int field2 = scanmask ? 0 : 1 - ((static_cast<int>(GSConfig.InterlaceMode) - 1) & 1);
				const int mode = scanmask ? 2 : ((static_cast<int>(GSConfig.InterlaceMode) - 1) >> 1);

				g_gs_device->Interlace(ds, field ^ field2, mode, offset);
			}
		}

		if (GSConfig.ShadeBoost)
		{
			g_gs_device->ShadeBoost();
		}

		if (GSConfig.ShaderFX)
		{
			g_gs_device->ExternalFX();
		}

		if (GSConfig.FXAA)
		{
			g_gs_device->FXAA();
		}
	}

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

static GSVector4 CalculateDrawRect(s32 window_width, s32 window_height, s32 texture_width, s32 texture_height, HostDisplay::Alignment alignment, bool flip_y, bool is_progressive)
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

	const double arr = targetAr / clientAr;
	float target_width = f_width;
	float target_height = f_height;
	if (arr < 1)
		target_width = std::floor(f_width * arr + 0.5f);
	else if (arr > 1)
		target_height = std::floor(f_height / arr + 0.5f);

	float zoom = GSConfig.Zoom / 100.0;
	if (zoom == 0) //auto zoom in untill black-bars are gone (while keeping the aspect ratio).
		zoom = std::max((float)arr, (float)(1.0 / arr));

	target_width *= zoom;
	target_height *= zoom * GSConfig.StretchY / 100.0f;

	if (GSConfig.IntegerScaling)
	{
		// make target width/height an integer multiple of the texture width/height
		const float t_width = static_cast<double>(texture_width);
		const float t_height = static_cast<double>(texture_height);

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

	const float unit = .01f * std::min(target_x, target_y);
	target_x += unit * GSConfig.OffsetX;
	target_y += unit * GSConfig.OffsetY;

	GSVector4 ret(target_x, target_y, target_x + target_width, target_y + target_height);

	if (flip_y)
	{
		const float height = ret.w - ret.y;
		ret.y = static_cast<float>(window_height) - ret.w;
		ret.w = ret.y + height;
	}

	return ret;
}

void GSRenderer::VSync(u32 field, bool registers_written)
{
	Flush();

	if (s_dump && s_n >= s_saven)
	{
		m_regs->Dump(root_sw + StringUtil::StdStringFromFormat("%05d_f%lld_gs_reg.txt", s_n, g_perfmon.GetFrame()));
	}

	const int fb_sprite_blits = g_perfmon.GetDisplayFramebufferSpriteBlits();
	const bool fb_sprite_frame = (fb_sprite_blits > 0);
	PerformanceMetrics::Update(registers_written, fb_sprite_frame);

	bool skip_frame = m_frameskip;
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
		return;
	}

	g_gs_device->AgePool();

	g_perfmon.EndFrame();
	if ((g_perfmon.GetFrame() & 0x1f) == 0)
		g_perfmon.Update();

	g_gs_device->ResetAPIState();
	if (Host::BeginPresentFrame(false))
	{
		GSTexture* current = g_gs_device->GetCurrent();
		if (current && !blank_frame)
		{
			HostDisplay* const display = g_gs_device->GetDisplay();
			const GSVector4 draw_rect(CalculateDrawRect(display->GetWindowWidth(), display->GetWindowHeight(),
				current->GetWidth(), current->GetHeight(), display->GetDisplayAlignment(), display->UsesLowerLeftOrigin(), GetVideoMode() == GSVideoMode::SDTV_480P));

			static constexpr ShaderConvert s_shader[5] = {ShaderConvert::COPY, ShaderConvert::SCANLINE,
				ShaderConvert::DIAGONAL_FILTER, ShaderConvert::TRIANGULAR_FILTER,
				ShaderConvert::COMPLEX_FILTER}; // FIXME

			g_gs_device->StretchRect(current, nullptr, draw_rect, s_shader[GSConfig.TVShader], GSConfig.LinearPresent);
		}

		Host::EndPresentFrame();

		if (GSConfig.OsdShowGPU)
			PerformanceMetrics::OnGPUPresent(Host::GetHostDisplay()->GetAndResetAccumulatedGPUTime());
	}
	g_gs_device->RestoreAPIState();

	// snapshot
	// wx is dumb and call this from the UI thread...
#ifndef PCSX2_CORE
	std::unique_lock snapshot_lock(m_snapshot_mutex);
#endif

	if (!m_snapshot.empty())
	{
		if (!m_dump && m_dump_frames > 0)
		{
			freezeData fd = {0, nullptr};
			Freeze(&fd, true);
			fd.data = new u8[fd.size];
			Freeze(&fd, false);

			// keep the screenshot relatively small so we don't bloat the dump
			static constexpr u32 DUMP_SCREENSHOT_WIDTH = 640;
			static constexpr u32 DUMP_SCREENSHOT_HEIGHT = 480;
			std::vector<u32> screenshot_pixels;
			SaveSnapshotToMemory(DUMP_SCREENSHOT_WIDTH, DUMP_SCREENSHOT_HEIGHT, &screenshot_pixels);

			std::string_view compression_str;
			if (GSConfig.GSDumpCompression == GSDumpCompressionMethod::Uncompressed)
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpUncompressed(m_snapshot, GetDumpSerial(), m_crc,
					DUMP_SCREENSHOT_WIDTH, DUMP_SCREENSHOT_HEIGHT,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with no compression";
			}
			else if (GSConfig.GSDumpCompression == GSDumpCompressionMethod::LZMA)
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpXz(m_snapshot, GetDumpSerial(), m_crc,
					DUMP_SCREENSHOT_WIDTH, DUMP_SCREENSHOT_HEIGHT,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with LZMA compression";
			}
			else
			{
				m_dump = std::unique_ptr<GSDumpBase>(new GSDumpZst(m_snapshot, GetDumpSerial(), m_crc,
					DUMP_SCREENSHOT_WIDTH, DUMP_SCREENSHOT_HEIGHT,
					screenshot_pixels.empty() ? nullptr : screenshot_pixels.data(),
					fd, m_regs));
				compression_str = "with Zstandard compression";
			}

			delete[] fd.data;

			Host::AddKeyedOSDMessage("GSDump", fmt::format("Saving {0} GS dump {1} to '{2}'",
				(m_dump_frames == 1) ? "single frame" : "multi-frame", compression_str,
				Path::GetFileName(m_dump->GetPath())), 10.0f);
		}

		if (GSTexture* t = g_gs_device->GetCurrent())
		{
			const std::string path(m_snapshot + ".png");
			if (t->Save(path))
			{
				Host::AddKeyedOSDMessage("GSScreenshot",
					fmt::format("Screenshot saved to '{}'.", Path::GetFileName(path)), 10.0f);
			}
			else
			{
				Host::AddFormattedOSDMessage(10.0f, "Failed to save screenshot to '%s'.", path.c_str());
			}
		}

		m_snapshot = {};
	}
	else if (m_dump)
	{
		const bool last = (m_dump_frames == 0);
		if (m_dump->VSync(field, last, m_regs))
		{
			Host::AddKeyedOSDMessage("GSDump", fmt::format("Saved GS dump to '{}'.", Path::GetFileName(m_dump->GetPath())), 10.0f);
			m_dump.reset();
		}
		else if (!last)
		{
			m_dump_frames--;
		}
	}

#ifndef PCSX2_CORE
	// capture
	if (m_capture.IsCapturing())
	{
		if (GSTexture* current = g_gs_device->GetCurrent())
		{
			GSVector2i size = m_capture.GetSize();

			bool res;
			GSTexture::GSMap m;
			if (size == current->GetSize())
				res = g_gs_device->DownloadTexture(current, GSVector4i(0, 0, size.x, size.y), m);
			else
				res = g_gs_device->DownloadTextureConvert(current, GSVector4(0, 0, 1, 1), size, GSTexture::Format::Color, ShaderConvert::COPY, m, true);

			if (res)
			{
				m_capture.DeliverFrame(m.bits, m.pitch, !g_gs_device->IsRBSwapped());
				g_gs_device->DownloadTextureComplete();
			}
		}
	}
#endif
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
		time_t cur_time = time(nullptr);
		static time_t prev_snap;
		// The variable 'n' is used for labelling the screenshots when multiple screenshots are taken in
		// a single second, we'll start using this variable for naming when a second screenshot request is detected
		// at the same time as the first one. Hence, we're initially setting this counter to 2 to imply that
		// the captured image is the 2nd image captured at this specific time.
		static int n = 2;
		char local_time[16];

		if (strftime(local_time, sizeof(local_time), "%Y%m%d%H%M%S", localtime(&cur_time)))
		{
			if (cur_time == prev_snap)
				m_snapshot = fmt::format("gs_{0}_({1})", local_time, n++);
			else
			{
				n = 2;
				m_snapshot = fmt::format("gs_{}", local_time);
			}
			prev_snap = cur_time;
		}

		// append the game serial and title
		if (std::string name(GetDumpName()); !name.empty())
		{
			Path::SanitizeFileName(name);
			m_snapshot += '_';
			m_snapshot += name;
		}
		if (std::string serial(GetDumpSerial()); !serial.empty())
		{
			Path::SanitizeFileName(serial);
			m_snapshot += '_';
			m_snapshot += serial;
		}

		// prepend snapshots directory
		m_snapshot = Path::Combine(EmuFolders::Snapshots, m_snapshot);
	}

	// this is really gross, but wx we get the snapshot request after shift...
#ifdef PCSX2_CORE
	m_dump_frames = gsdump_frames;
#endif
}

void GSRenderer::StopGSDump()
{
	m_snapshot = {};
	m_dump_frames = 0;
}

#ifndef PCSX2_CORE

bool GSRenderer::BeginCapture(std::string& filename)
{
	return m_capture.BeginCapture(GetTvRefreshRate(), GetInternalResolution(), GetCurrentAspectRatioFloat(GetVideoMode() == GSVideoMode::SDTV_480P), filename);
}

void GSRenderer::EndCapture()
{
	m_capture.EndCapture();
}

void GSRenderer::KeyEvent(const HostKeyEvent& e)
{
#ifdef _WIN32
	m_shift_key = !!(::GetAsyncKeyState(VK_SHIFT) & 0x8000);
	m_control_key = !!(::GetAsyncKeyState(VK_CONTROL) & 0x8000);
#elif defined(__APPLE__)
	m_shift_key = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Shift)
	           || CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightShift);
	m_control_key = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Control)
	             || CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightControl)
	             || CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_Command)
	             || CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, kVK_RightCommand);
#else
	switch (e.key)
	{
		case XK_Shift_L:
		case XK_Shift_R:
			m_shift_key = (e.type == HostKeyEvent::Type::KeyPressed);
			return;
		case XK_Control_L:
		case XK_Control_R:
			m_control_key = (e.type == HostKeyEvent::Type::KeyReleased);
			return;
	}
#endif

	if (m_dump_frames == 0)
	{
		// start dumping
		if (m_shift_key)
			m_dump_frames = m_control_key ? std::numeric_limits<u32>::max() : 1;
	}
	else
	{
		// stop dumping
		if (m_dump && !m_control_key)
			m_dump_frames = 0;
	}

	if (e.type == HostKeyEvent::Type::KeyPressed)
	{

		int step = m_shift_key ? -1 : 1;

#if defined(__unix__)
#define VK_F5 XK_F5
#define VK_DELETE XK_Delete
#define VK_NEXT XK_Next
#elif defined(__APPLE__)
#define VK_F5 kVK_F5
#define VK_DELETE kVK_ForwardDelete
#define VK_NEXT kVK_PageDown
#endif

		// NOTE: These are all BROKEN! They mess with GS thread state from the UI thread.

		switch (e.key)
		{
			case VK_F5:
				GSConfig.InterlaceMode = static_cast<GSInterlaceMode>((static_cast<int>(GSConfig.InterlaceMode) + static_cast<int>(GSInterlaceMode::Count) + step) % static_cast<int>(GSInterlaceMode::Count));
				theApp.SetConfig("deinterlace", static_cast<int>(GSConfig.InterlaceMode));
				printf("GS: Set deinterlace mode to %d (%s).\n", static_cast<int>(GSConfig.InterlaceMode), theApp.m_gs_deinterlace.at(static_cast<int>(GSConfig.InterlaceMode)).name.c_str());
				return;
			case VK_DELETE:
				GSConfig.AA1 = !GSConfig.AA1;
				theApp.SetConfig("aa1", GSConfig.AA1);
				printf("GS: (Software) Edge anti-aliasing is now %s.\n", GSConfig.AA1 ? "enabled" : "disabled");
				return;
			case VK_NEXT: // As requested by Prafull, to be removed later
				char dither_msg[3][16] = {"disabled", "auto", "auto unscaled"};
				GSConfig.Dithering = (GSConfig.Dithering + 1) % 3;
				printf("GS: Dithering is now %s.\n", dither_msg[GSConfig.Dithering]);
				return;
		}
	}
}

#endif // PCSX2_CORE

void GSRenderer::PurgePool()
{
	g_gs_device->PurgePool();
}

void GSRenderer::PurgeTextureCache()
{
}

bool GSRenderer::SaveSnapshotToMemory(u32 width, u32 height, std::vector<u32>* pixels)
{
	GSTexture* const current = g_gs_device->GetCurrent();
	if (!current)
		return false;

	GSVector4 draw_rect(CalculateDrawRect(width, height, current->GetWidth(), current->GetHeight(),
		HostDisplay::Alignment::LeftOrTop, false, GetVideoMode() == GSVideoMode::SDTV_480P));
	u32 draw_width = static_cast<u32>(draw_rect.z - draw_rect.x);
	u32 draw_height = static_cast<u32>(draw_rect.w - draw_rect.y);
	if (draw_width > width)
	{
		draw_width = width;
		draw_rect.left = 0;
		draw_rect.right = width;
	}
	if (draw_height > height)
	{
		draw_height = height;
		draw_rect.top = 0;
		draw_rect.bottom = height;
	}

	GSTexture::GSMap map;
	const bool result = g_gs_device->DownloadTextureConvert(
		current, GSVector4(0.0f, 0.0f, 1.0f, 1.0f),
		GSVector2i(draw_width, draw_height), GSTexture::Format::Color,
		ShaderConvert::COPY, map, true);
	if (result)
	{
		const u32 pad_x = (width - draw_width) / 2;
		const u32 pad_y = (height - draw_height) / 2;
		pixels->resize(width * height, 0);
		StringUtil::StrideMemCpy(pixels->data() + pad_y * width + pad_x, width * sizeof(u32),
			map.bits, map.pitch, draw_width * sizeof(u32), draw_height);

		g_gs_device->DownloadTextureComplete();
	}

	return result;
}
