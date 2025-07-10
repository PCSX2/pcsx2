// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BuildVersion.h"
#include "Config.h"
#include "Counters.h"
#include "GS.h"
#include "GS/GS.h"
#include "GS/GSCapture.h"
#include "GS/GSVector.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "Host.h"
#include "IconsFontAwesome6.h"
#include "IconsPromptFont.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiAnimated.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "Input/InputManager.h"
#include "MTGS.h"
#include "PerformanceMetrics.h"
#include "Recording/InputRecording.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadBase.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "cpuinfo.h"

#include "common/BitUtils.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/chrono.h"
#include "fmt/format.h"
#include "imgui.h"

#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <limits>
#include <mutex>
#include <span>
#include <tuple>
#include <unordered_map>

InputRecordingUI::InputRecordingData g_InputRecordingData;

// OSD positioning funcs
ImVec2 CalculateOSDPosition(OsdOverlayPos position, float margin, const ImVec2& text_size, float window_width, float window_height)
{
	switch (position)
	{
		case OsdOverlayPos::TopLeft:
			return ImVec2(margin, margin);
		case OsdOverlayPos::TopCenter:
			return ImVec2((window_width - text_size.x) * 0.5f, margin);
		case OsdOverlayPos::TopRight:
			return ImVec2(window_width - margin - text_size.x, margin);
		case OsdOverlayPos::CenterLeft:
			return ImVec2(margin, (window_height - text_size.y) * 0.5f);
		case OsdOverlayPos::Center:
			return ImVec2((window_width - text_size.x) * 0.5f, (window_height - text_size.y) * 0.5f);
		case OsdOverlayPos::CenterRight:
			return ImVec2(window_width - margin - text_size.x, (window_height - text_size.y) * 0.5f);
		case OsdOverlayPos::BottomLeft:
			return ImVec2(margin, window_height - margin - text_size.y);
		case OsdOverlayPos::BottomCenter:
			return ImVec2((window_width - text_size.x) * 0.5f, window_height - margin - text_size.y);
		case OsdOverlayPos::BottomRight:
			return ImVec2(window_width - margin - text_size.x, window_height - margin - text_size.y);
		case OsdOverlayPos::None:
		default:
			return ImVec2(0.0f, 0.0f);
	}
}

ImVec2 CalculatePerformanceOverlayTextPosition(OsdOverlayPos position, float margin, const ImVec2& text_size, float window_width, float position_y)
{
	const float abs_margin = std::abs(margin);
	
	// Get the X position based on horizontal alignment
	float x_pos;
	switch (position)
	{
		case OsdOverlayPos::TopLeft:
		case OsdOverlayPos::CenterLeft:
		case OsdOverlayPos::BottomLeft:
			x_pos = abs_margin; // Left alignment
			break;
			
		case OsdOverlayPos::TopCenter:
		case OsdOverlayPos::Center:
		case OsdOverlayPos::BottomCenter:
			x_pos = (window_width - text_size.x) * 0.5f; // Center alignment
			break;
			
		case OsdOverlayPos::TopRight:
		case OsdOverlayPos::CenterRight:
		case OsdOverlayPos::BottomRight:
		default:
			x_pos = window_width - text_size.x - abs_margin; // Right alignment
			break;
	}
	
	return ImVec2(x_pos, position_y);
}

bool ShouldUseLeftAlignment(OsdOverlayPos position)
{
	return (position == OsdOverlayPos::TopLeft || position == OsdOverlayPos::CenterLeft || position == OsdOverlayPos::BottomLeft);
}

namespace ImGuiManager
{
	static void FormatProcessorStat(SmallStringBase& text, double usage, double time);
	static void DrawPerformanceOverlay(float& position_y, float scale, float margin, float spacing);
	static void DrawSettingsOverlay(float scale, float margin, float spacing);
	static void DrawInputsOverlay(float scale, float margin, float spacing);
	static void DrawInputRecordingOverlay(float& position_y, float scale, float margin, float spacing);
	static void DrawVideoCaptureOverlay(float& position_y, float scale, float margin, float spacing);
} // namespace ImGuiManager

static std::tuple<float, float> GetMinMax(std::span<const float> values)
{
	GSVector4 vmin(GSVector4::load<false>(values.data()));
	GSVector4 vmax(vmin);

	const u32 count = static_cast<u32>(values.size());
	const u32 aligned_count = Common::AlignDownPow2(count, 4);
	u32 i = 4;
	for (; i < aligned_count; i += 4)
	{
		const GSVector4 v(GSVector4::load<false>(&values[i]));
		vmin = vmin.min(v);
		vmax = vmax.max(v);
	}

	float min = std::min(vmin.x, std::min(vmin.y, std::min(vmin.z, vmin.w)));
	float max = std::max(vmax.x, std::max(vmax.y, std::max(vmax.z, vmax.w)));
	for (; i < count; i++)
	{
		min = std::min(min, values[i]);
		max = std::max(max, values[i]);
	}

	return std::tie(min, max);
}

__ri void ImGuiManager::FormatProcessorStat(SmallStringBase& text, double usage, double time)
{
	// Some values, such as GPU (and even CPU to some extent) can be out of phase with the wall clock,
	// which the processor time is divided by to get a utilization percentage. Let's clamp it at 100%,
	// so that people don't get confused, and remove the decimal places when it's there while we're at it.
	if (usage >= 99.95)
		text.append_format("100% ({:.2f}ms)", time);
	else
		text.append_format("{:.1f}% ({:.2f}ms)", usage, time);
}

__ri void ImGuiManager::DrawPerformanceOverlay(float& position_y, float scale, float margin, float spacing)
{
	const float shadow_offset = std::ceil(scale);

	ImFont* const fixed_font = ImGuiManager::GetFixedFont();
	ImFont* const standard_font = ImGuiManager::GetStandardFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	SmallString text;
	ImVec2 text_size;

	// Adjust initial Y position based on vertical alignment
	switch (GSConfig.OsdPerformancePos)
	{
		case OsdOverlayPos::CenterLeft:
		case OsdOverlayPos::Center:
		case OsdOverlayPos::CenterRight:

			position_y = (GetWindowHeight() - (font_size * 8.0f)) * 0.5f;
			break;
			
		case OsdOverlayPos::BottomLeft:
		case OsdOverlayPos::BottomCenter:
		case OsdOverlayPos::BottomRight:

			position_y = GetWindowHeight() - margin - (font_size * 15.0f + spacing * 14.0f);
			break;
			
		case OsdOverlayPos::TopLeft:
		case OsdOverlayPos::TopCenter:
		case OsdOverlayPos::TopRight:
		default:
			// Top alignment keeps the passed position_y
			break;
	}

#define DRAW_LINE(font, size, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		const ImVec2 text_pos = CalculatePerformanceOverlayTextPosition(GSConfig.OsdPerformancePos, margin, text_size, GetWindowWidth(), position_y); \
		dl->AddText(font, size, ImVec2(text_pos.x + shadow_offset, text_pos.y + shadow_offset), IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, size, text_pos, color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

	const bool paused = (VMManager::GetState() == VMState::Paused);
	const bool fsui_active = FullscreenUI::HasActiveWindow();

	if (!paused)
	{
		bool first = true;
		const float speed = PerformanceMetrics::GetSpeed();

		if (GSConfig.OsdShowFPS)
		{
			switch (PerformanceMetrics::GetInternalFPSMethod())
			{
				case PerformanceMetrics::InternalFPSMethod::GSPrivilegedRegister:
					text.append_format("FPS: {:.2f} [P]", PerformanceMetrics::GetInternalFPS(),
						PerformanceMetrics::GetFPS());
					break;

				case PerformanceMetrics::InternalFPSMethod::DISPFBBlit:
					text.append_format("FPS: {:.2f} [B]", PerformanceMetrics::GetInternalFPS(),
						PerformanceMetrics::GetFPS());
					break;

				case PerformanceMetrics::InternalFPSMethod::None:
				default:
					text.append_format("FPS: {:.2f}", PerformanceMetrics::GetFPS());
					break;
			}
			first = false;
		}

		if (GSConfig.OsdShowVPS)
		{
			text.append_format("{}VPS: {:.2f}", first ? "" : " | ", PerformanceMetrics::GetFPS(),
				PerformanceMetrics::GetFPS());
			first = false;
		}

		if (GSConfig.OsdShowSpeed)
		{
			text.append_format("{}Speed: {}%", first ? "" : " | ", static_cast<u32>(std::round(speed)));

			const float target_speed = VMManager::GetTargetSpeed();
			if (target_speed == 0.0f)
				text.append(" (T: Max)");
			else
				text.append_format(" (T: {:.0f}%)", target_speed * 100.0f);
			first = false;
		}

		if (GSConfig.OsdShowVersion)
		{
			text.append_format("{}PCSX2 {}", first ? "" : " | ", BuildVersion::GitRev);
		}

		if (!text.empty())
		{
			ImU32 color;
			if (speed < 95.0f)
				color = IM_COL32(255, 100, 100, 255);
			else if (speed > 105.0f)
				color = IM_COL32(100, 255, 100, 255);
			else
				color = IM_COL32(255, 255, 255, 255);

			DRAW_LINE(fixed_font, font_size, text.c_str(), color);
		}

		if (GSConfig.OsdShowGSStats)
		{
			text.clear();
			GSgetStats(text);
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

			text.clear();
			GSgetMemoryStats(text);
			if (!text.empty())
				DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

			text.clear();
			text.append_format("{} QF | Min: {:.2f}ms | Avg: {:.2f}ms | Max: {:.2f}ms",
				MTGS::GetCurrentVsyncQueueSize() - 1, // we subtract one for the current frame
				PerformanceMetrics::GetMinimumFrameTime(),
				PerformanceMetrics::GetAverageFrameTime(),
				PerformanceMetrics::GetMaximumFrameTime());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowResolution)
		{
			int width, height;
			GSgetInternalResolution(&width, &height);

			text.clear();
			text.append_format("{}x{} {} {}", width, height, ReportVideoMode(), ReportInterlaceMode());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowHardwareInfo)
		{
			// CPU
			text.clear();
			text.append_format("CPU: {} ({}C/{}T)",
				cpuinfo_get_package(0)->name,
				cpuinfo_get_cores_count(),
				cpuinfo_get_processors_count());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

			// GPU
			text.clear();
			text.append_format("GPU: {}{}", g_gs_device->GetName(), GSConfig.UseDebugDevice ? " (Debug)" : "");
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowCPU)
		{
			text.clear();
			if (EmuConfig.Speedhacks.EECycleRate != 0 || EmuConfig.Speedhacks.EECycleSkip != 0)
				text.append_format("EE[{}/{}]: ", EmuConfig.Speedhacks.EECycleRate, EmuConfig.Speedhacks.EECycleSkip);
			else
				text = "EE: ";
			FormatProcessorStat(text, PerformanceMetrics::GetCPUThreadUsage(), PerformanceMetrics::GetCPUThreadAverageTime());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

			text = "GS: ";
			FormatProcessorStat(text, PerformanceMetrics::GetGSThreadUsage(), PerformanceMetrics::GetGSThreadAverageTime());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

			if (THREAD_VU1)
			{
				text = "VU: ";
				FormatProcessorStat(text, PerformanceMetrics::GetVUThreadUsage(), PerformanceMetrics::GetVUThreadAverageTime());
				DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
			}

			const u32 gs_sw_threads = PerformanceMetrics::GetGSSWThreadCount();
			for (u32 i = 0; i < gs_sw_threads; i++)
			{
				text.clear();
				text.append_format("SW-{}: ", i);
				FormatProcessorStat(text, PerformanceMetrics::GetGSSWThreadUsage(i), PerformanceMetrics::GetGSSWThreadAverageTime(i));
				DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
			}

			if (GSCapture::IsCapturing())
			{
				text = "CAP: ";
				FormatProcessorStat(text, PerformanceMetrics::GetCaptureThreadUsage(), PerformanceMetrics::GetCaptureThreadAverageTime());
				DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
			}
		}

		if (GSConfig.OsdShowGPU)
		{
			text = "GPU: ";
			FormatProcessorStat(text, PerformanceMetrics::GetGPUUsage(), PerformanceMetrics::GetGPUAverageTime());
			DRAW_LINE(fixed_font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowIndicators)
		{
			const float target_speed = VMManager::GetTargetSpeed();
			const bool is_normal_speed = (target_speed == EmuConfig.EmulationSpeed.NominalScalar ||
										  VMManager::IsTargetSpeedAdjustedToHost());
			if (!is_normal_speed)
			{
				if (target_speed == EmuConfig.EmulationSpeed.SlomoScalar) // Slow-Motion
					DRAW_LINE(standard_font, font_size, ICON_PF_SLOW_MOTION, IM_COL32(255, 255, 255, 255));
				else if (target_speed == EmuConfig.EmulationSpeed.TurboScalar) // Turbo
					DRAW_LINE(standard_font, font_size, ICON_FA_FORWARD_FAST, IM_COL32(255, 255, 255, 255));
				else // Unlimited
					DRAW_LINE(standard_font, font_size, ICON_FA_FORWARD, IM_COL32(255, 255, 255, 255));
			}
		}

		if (GSConfig.OsdShowFrameTimes)
		{
			const ImVec2 history_size(200.0f * scale, 50.0f * scale);
			ImGui::SetNextWindowSize(ImVec2(history_size.x, history_size.y));
			
			const ImVec2 window_pos = CalculatePerformanceOverlayTextPosition(GSConfig.OsdPerformancePos, margin, history_size, GetWindowWidth(), position_y);
			ImGui::SetNextWindowPos(window_pos);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.25f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::PushFont(fixed_font, font_size);
			if (ImGui::Begin("##frame_times", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs))
			{
				auto [min, max] = GetMinMax(PerformanceMetrics::GetFrameTimeHistory());

				// add a little bit of space either side, so we're not constantly resizing
				if ((max - min) < 4.0f)
				{
					min = min - std::fmod(min, 1.0f);
					max = max - std::fmod(max, 1.0f) + 1.0f;
					min = std::max(min - 2.0f, 0.0f);
					max += 2.0f;
				}

				ImGui::PlotEx(
					ImGuiPlotType_Lines, "##frame_times",
					[](void*, int idx) -> float {
						return PerformanceMetrics::GetFrameTimeHistory()[(
							(PerformanceMetrics::GetFrameTimeHistoryPos() + idx) % PerformanceMetrics::NUM_FRAME_TIME_SAMPLES)];
					},
					nullptr, PerformanceMetrics::NUM_FRAME_TIME_SAMPLES, 0, nullptr, min, max, history_size);

				ImDrawList* win_dl = ImGui::GetCurrentWindow()->DrawList;
				const ImVec2 wpos(ImGui::GetCurrentWindow()->Pos);

				text.clear();
				text.append_format("Max: {:.1f} ms", max);
				text_size = fixed_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str(), text.c_str() + text.length());
				
				float text_x;
				switch (GSConfig.OsdPerformancePos)
				{
					case OsdOverlayPos::TopLeft:
					case OsdOverlayPos::CenterLeft:
					case OsdOverlayPos::BottomLeft:
						text_x = wpos.x + 2.0f * spacing; // Left alignment within window
						break;
					case OsdOverlayPos::TopCenter:
					case OsdOverlayPos::Center:
					case OsdOverlayPos::BottomCenter:
						text_x = wpos.x + (history_size.x - text_size.x) * 0.5f; // Center alignment within window
						break;
					case OsdOverlayPos::TopRight:
					case OsdOverlayPos::CenterRight:
					case OsdOverlayPos::BottomRight:
					default:
						text_x = wpos.x + history_size.x - text_size.x - spacing; // Right alignment within window
						break;
				}
				win_dl->AddText(ImVec2(text_x + shadow_offset, wpos.y + shadow_offset),
					IM_COL32(0, 0, 0, 100), text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(text_x, wpos.y),
					IM_COL32(255, 255, 255, 255), text.c_str(), text.c_str() + text.length());

				text.clear();
				text.append_format("Min: {:.1f} ms", min);
				text_size = fixed_font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text.c_str(), text.c_str() + text.length());
				
				float min_text_x;
				switch (GSConfig.OsdPerformancePos)
				{
					case OsdOverlayPos::TopLeft:
					case OsdOverlayPos::CenterLeft:
					case OsdOverlayPos::BottomLeft:
						min_text_x = wpos.x + 2.0f * spacing; // Left alignment within window
						break;
					case OsdOverlayPos::TopCenter:
					case OsdOverlayPos::Center:
					case OsdOverlayPos::BottomCenter:
						min_text_x = wpos.x + (history_size.x - text_size.x) * 0.5f; // Center alignment within window
						break;
					case OsdOverlayPos::TopRight:
					case OsdOverlayPos::CenterRight:
					case OsdOverlayPos::BottomRight:
					default:
						min_text_x = wpos.x + history_size.x - text_size.x - spacing; // Right alignment within window
						break;
				}
				win_dl->AddText(ImVec2(min_text_x + shadow_offset, wpos.y + history_size.y - font_size + shadow_offset),
					IM_COL32(0, 0, 0, 100), text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(min_text_x, wpos.y + history_size.y - font_size),
					IM_COL32(255, 255, 255, 255), text.c_str(), text.c_str() + text.length());
			}
			ImGui::End();
			ImGui::PopFont();
			ImGui::PopStyleVar(5);
			ImGui::PopStyleColor(3);
		}
	}
	else if (!fsui_active)
	{
		if (GSConfig.OsdShowIndicators)
		{
			// We should put the Pause icon in the top right regardless of performance overlay position
			text = ICON_FA_PAUSE;
			text_size = standard_font->CalcTextSizeA(font_size, std::numeric_limits<float>::max(), -1.0f, text.c_str(), nullptr, nullptr);
			const ImVec2 pause_pos(GetWindowWidth() - margin - text_size.x, margin);
			dl->AddText(standard_font, font_size, ImVec2(pause_pos.x + shadow_offset, pause_pos.y + shadow_offset), IM_COL32(0, 0, 0, 100), text.c_str());
			dl->AddText(standard_font, font_size, pause_pos, IM_COL32(255, 255, 255, 255), text.c_str());
		}
	}

#undef DRAW_LINE
}

__ri void ImGuiManager::DrawSettingsOverlay(float scale, float margin, float spacing)
{
	if (!GSConfig.OsdShowSettings ||
		FullscreenUI::HasActiveWindow())
		return;

	std::string text;
	text.reserve(512);

#define APPEND(...) \
	do \
	{ \
		fmt::format_to(std::back_inserter(text), __VA_ARGS__); \
	} while (0)

	if (EmuConfig.Speedhacks.EECycleRate != 0)
		APPEND("CR={} ", EmuConfig.Speedhacks.EECycleRate);
	if (EmuConfig.Speedhacks.EECycleSkip != 0)
		APPEND("CS={} ", EmuConfig.Speedhacks.EECycleSkip);
	if (EmuConfig.Speedhacks.fastCDVD)
		APPEND("FCDVD ");
	if (EmuConfig.Speedhacks.vu1Instant)
		APPEND("IVU ");
	if (EmuConfig.Speedhacks.vuThread)
		APPEND("MTVU ");
	if (EmuConfig.GS.VsyncEnable)
		APPEND("VSYNC ");

	APPEND("EER={} EEC={} VUR={} VUC={} VQS={} ", static_cast<unsigned>(EmuConfig.Cpu.FPUFPCR.GetRoundMode()),
		EmuConfig.Cpu.Recompiler.GetEEClampMode(), static_cast<unsigned>(EmuConfig.Cpu.VU0FPCR.GetRoundMode()),
		EmuConfig.Cpu.Recompiler.GetVUClampMode(), EmuConfig.GS.VsyncQueueSize);

	if (EmuConfig.EnableCheats || EmuConfig.EnableWideScreenPatches || EmuConfig.EnableNoInterlacingPatches)
	{
		APPEND("C={}{}{} ", EmuConfig.EnableCheats ? "C" : "", EmuConfig.EnableWideScreenPatches ? "W" : "",
			EmuConfig.EnableNoInterlacingPatches ? "N" : "");
	}

	if (GSIsHardwareRenderer())
	{
		if ((GSConfig.UpscaleMultiplier - std::floor(GSConfig.UpscaleMultiplier)) > 0.01)
			APPEND("IR={:.2f} ", static_cast<float>(GSConfig.UpscaleMultiplier));
		else
			APPEND("IR={} ", static_cast<unsigned>(GSConfig.UpscaleMultiplier));

		APPEND("B={} PL={} ", static_cast<unsigned>(GSConfig.AccurateBlendingUnit), static_cast<unsigned>(GSConfig.TexturePreloading));
		if (GSConfig.GPUPaletteConversion)
			APPEND("PLTX ");

		if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
			APPEND("HWDM={} ", static_cast<unsigned>(GSConfig.HWDownloadMode));

		if (GSConfig.HWMipmap)
			APPEND("MM ");

		// deliberately test global and print local here for auto values
		if (EmuConfig.GS.TextureFiltering != BiFiltering::PS2)
			APPEND("BF={} ", static_cast<unsigned>(GSConfig.TextureFiltering));
		if (EmuConfig.GS.TriFilter != TriFiltering::Automatic)
			APPEND("TF={} ", static_cast<unsigned>(GSConfig.TriFilter));
		if (GSConfig.MaxAnisotropy > 1)
			APPEND("AF={} ", EmuConfig.GS.MaxAnisotropy);
		if (GSConfig.Dithering != 2)
			APPEND("DI={} ", GSConfig.Dithering);
		if (GSConfig.UserHacks_HalfPixelOffset != GSHalfPixelOffset::Off)
			APPEND("HPO={} ", static_cast<u32>(GSConfig.UserHacks_HalfPixelOffset));
		if (GSConfig.UserHacks_RoundSprite > 0)
			APPEND("RS={} ", GSConfig.UserHacks_RoundSprite);
		if (GSConfig.UserHacks_NativeScaling > GSNativeScaling::Off)
			APPEND("NS={} ", static_cast<unsigned>(GSConfig.UserHacks_NativeScaling));
		if (GSConfig.UserHacks_TCOffsetX != 0 || GSConfig.UserHacks_TCOffsetY != 0)
			APPEND("TCO={}/{} ", GSConfig.UserHacks_TCOffsetX, GSConfig.UserHacks_TCOffsetY);
		if (GSConfig.UserHacks_CPUSpriteRenderBW != 0)
			APPEND("CSBW={}/{} ", GSConfig.UserHacks_CPUSpriteRenderBW, GSConfig.UserHacks_CPUSpriteRenderLevel);
		if (GSConfig.UserHacks_CPUCLUTRender != 0)
			APPEND("CCLUT={} ", GSConfig.UserHacks_CPUCLUTRender);
		if (GSConfig.UserHacks_GPUTargetCLUTMode != GSGPUTargetCLUTMode::Disabled)
			APPEND("GCLUT={} ", static_cast<unsigned>(GSConfig.UserHacks_GPUTargetCLUTMode));
		if (GSConfig.SkipDrawStart != 0 || GSConfig.SkipDrawEnd != 0)
			APPEND("SD={}/{} ", GSConfig.SkipDrawStart, GSConfig.SkipDrawEnd);
		if (GSConfig.UserHacks_TextureInsideRt != GSTextureInRtMode::Disabled)
			APPEND("TexRT={} ", static_cast<unsigned>(GSConfig.UserHacks_TextureInsideRt));
		if (GSConfig.UserHacks_BilinearHack != GSBilinearDirtyMode::Automatic)
			APPEND("BLU={} ", static_cast<unsigned>(GSConfig.UserHacks_BilinearHack));
		if (GSConfig.UserHacks_ForceEvenSpritePosition)
			APPEND("FESP ");
		if (GSConfig.UserHacks_NativePaletteDraw)
			APPEND("NPD ");
		if (GSConfig.UserHacks_MergePPSprite)
			APPEND("MS ");
		if (GSConfig.UserHacks_AlignSpriteX)
			APPEND("AS ");
		if (GSConfig.UserHacks_AutoFlush != GSHWAutoFlushLevel::Disabled)
			APPEND("ATFL={} ", static_cast<unsigned>(GSConfig.UserHacks_AutoFlush));
		if (GSConfig.UserHacks_CPUFBConversion)
			APPEND("FBC ");
		if (GSConfig.UserHacks_ReadTCOnClose)
			APPEND("RTOC ");
		if (GSConfig.UserHacks_DisableDepthSupport)
			APPEND("DDC ");
		if (GSConfig.UserHacks_DisablePartialInvalidation)
			APPEND("DPIV ");
		if (GSConfig.UserHacks_DisableSafeFeatures)
			APPEND("DSF ");
		if (GSConfig.UserHacks_DisableRenderFixes)
			APPEND("DRF ");
		if (GSConfig.PreloadFrameWithGSData)
			APPEND("PLFD ");
		if (GSConfig.UserHacks_EstimateTextureRegion)
			APPEND("ETR ");
		if (GSConfig.HWSpinGPUForReadbacks)
			APPEND("RBSG ");
		if (GSConfig.HWSpinCPUForReadbacks)
			APPEND("RBSC ");
	}

#undef APPEND

	if (text.empty())
		return;
	else if (text.back() == ' ')
		text.pop_back();

	const float shadow_offset = std::ceil(scale);
	ImFont* const font = ImGuiManager::GetFixedFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();
	const float position_y = GetWindowHeight() - margin - font_size;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 text_size =
		font->CalcTextSizeA(font_size, std::numeric_limits<float>::max(), -1.0f, text.c_str(), text.c_str() + text.length(), nullptr);
	dl->AddText(font, font_size,
		ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset), IM_COL32(0, 0, 0, 100),
		text.c_str(), text.c_str() + text.length());
	dl->AddText(font, font_size, ImVec2(GetWindowWidth() - margin - text_size.x, position_y), IM_COL32(255, 255, 255, 255),
		text.c_str(), text.c_str() + text.length());
}

__ri void ImGuiManager::DrawInputsOverlay(float scale, float margin, float spacing)
{
	// Technically this is racing the CPU thread.. but it doesn't really matter, at worst, the inputs get displayed onscreen late.
	if (!GSConfig.OsdShowInputs ||
		FullscreenUI::HasActiveWindow())
		return;

	const float shadow_offset = std::ceil(scale);
	ImFont* const font = ImGuiManager::GetStandardFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();

	static constexpr u32 text_color = IM_COL32(0xff, 0xff, 0xff, 255);
	static constexpr u32 shadow_color = IM_COL32(0x00, 0x00, 0x00, 100);

	const ImVec2& display_size = ImGui::GetIO().DisplaySize;
	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	u32 num_ports = 0;

	for (u32 slot = 0; slot < Pad::NUM_CONTROLLER_PORTS; slot++)
	{
		if (Pad::HasConnectedPad(slot))
			num_ports++;
	}

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		if (EmuConfig.USB.Ports[port].DeviceType >= 0 && !USB::GetDeviceBindings(port).empty())
			num_ports++;
	}

	float current_x = ImFloor(margin);
	float current_y = ImFloor(display_size.y - margin - ((static_cast<float>(num_ports) * (font_size + spacing)) - spacing));
	const ImVec4 clip_rect(current_x, current_y, display_size.x - margin, display_size.y);

	SmallString text;

	for (u32 slot = 0; slot < Pad::NUM_CONTROLLER_PORTS; slot++)
	{
		const PadBase* const pad = Pad::GetPad(slot);
		const Pad::ControllerType ctype = pad->GetType();
		if (ctype == Pad::ControllerType::NotConnected)
			continue;

		const Pad::ControllerInfo& cinfo = pad->GetInfo();
		if (cinfo.icon_name)
			text.format("{} {}", cinfo.icon_name, slot + 1u);
		else
			text.format("{} |", slot + 1u);

		for (u32 bind = 0; bind < static_cast<u32>(cinfo.bindings.size()); bind++)
		{
			const InputBindingInfo& bi = cinfo.bindings[bind];
			switch (bi.bind_type)
			{
				case InputBindingInfo::Type::Axis:
				case InputBindingInfo::Type::HalfAxis:
				{
					// axes are only shown if not resting/past deadzone. values are normalized.
					const float value = pad->GetEffectiveInput(bind);
					const float abs_value = std::abs(value);
					if (abs_value >= (254.0f / 255.0f))
						text.append_format(" {}", bi.icon_name ? bi.icon_name : bi.name);
					else if (abs_value >= (1.0f / 255.0f))
						text.append_format(" {}: {:.2f}", bi.icon_name ? bi.icon_name : bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Button:
				{
					// buttons display the value from 0 through 255.
					const float value = pad->GetEffectiveInput(bind);
					if (value >= 254.0f)
						text.append_format(" {}", bi.icon_name ? bi.icon_name : bi.name);
					else if (value > 0.0f)
						text.append_format(" {}: {:.0f}", bi.icon_name ? bi.icon_name : bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Motor:
				case InputBindingInfo::Type::Macro:
				case InputBindingInfo::Type::Unknown:
				default:
					break;
			}
		}

		dl->AddText(font, font_size, ImVec2(current_x + shadow_offset, current_y + shadow_offset), shadow_color, text.c_str(),
			text.c_str() + text.length(), 0.0f, &clip_rect);
		dl->AddText(
			font, font_size, ImVec2(current_x, current_y), text_color, text.c_str(), text.c_str() + text.length(), 0.0f, &clip_rect);

		current_y += font_size + spacing;
	}

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		if (EmuConfig.USB.Ports[port].DeviceType < 0)
			continue;

		const std::span<const InputBindingInfo> bindings(USB::GetDeviceBindings(port));
		if (bindings.empty())
			continue;

		text.format("{} {} ", ICON_PF_USB, port + 1u);

		for (const InputBindingInfo& bi : bindings)
		{
			switch (bi.bind_type)
			{
				case InputBindingInfo::Type::Axis:
				case InputBindingInfo::Type::HalfAxis:
				{
					// axes are only shown if not resting/past deadzone. values are normalized.
					const float value = static_cast<float>(USB::GetDeviceBindValue(port, bi.bind_index));
					if (value >= (254.0f / 255.0f))
						text.append_format(" {}", bi.icon_name ? bi.icon_name : bi.name);
					else if (value > (1.0f / 255.0f))
						text.append_format(" {}: {:.2f}", bi.icon_name ? bi.icon_name : bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Button:
				{
					// buttons display the value from 0 through 255. values are normalized, so denormalize them.
					const float value = static_cast<float>(USB::GetDeviceBindValue(port, bi.bind_index)) * 255.0f;
					if (value >= 254.0f)
						text.append_format(" {}", bi.icon_name ? bi.icon_name : bi.name);
					else if (value > 0.0f)
						text.append_format(" {}: {:.0f}", bi.icon_name ? bi.icon_name : bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Motor:
				case InputBindingInfo::Type::Macro:
				case InputBindingInfo::Type::Unknown:
				default:
					break;
			}
		}

		dl->AddText(font, font_size, ImVec2(current_x + shadow_offset, current_y + shadow_offset), shadow_color, text.c_str(),
			text.c_str() + text.length(), 0.0f, &clip_rect);
		dl->AddText(
			font, font_size, ImVec2(current_x, current_y), text_color, text.c_str(), text.c_str() + text.length(), 0.0f, &clip_rect);

		current_y += font_size + spacing;
	}
}

__ri void ImGuiManager::DrawInputRecordingOverlay(float& position_y, float scale, float margin, float spacing)
{
	if (!GSConfig.OsdShowInputRec ||
		!g_InputRecording.isActive() ||
		FullscreenUI::HasActiveWindow())
		return;

	const float shadow_offset = std::ceil(scale);

	ImFont* const fixed_font = ImGuiManager::GetFixedFont();
	ImFont* const standard_font = ImGuiManager::GetStandardFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	std::string text;
	ImVec2 text_size;

	text.reserve(128);
#define DRAW_LINE(font, size, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		dl->AddText(font, size, \
			ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset), \
			IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, size, ImVec2(GetWindowWidth() - margin - text_size.x, position_y), color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

	// Status Indicators
	if (g_InputRecordingData.is_recording)
	{
		DRAW_LINE(standard_font, font_size, TinyString::from_format(TRANSLATE_FS("ImGuiOverlays", "{} Recording Input"), ICON_PF_CIRCLE).c_str(), IM_COL32(255, 0, 0, 255));
	}
	else
	{
		DRAW_LINE(standard_font, font_size, TinyString::from_format(TRANSLATE_FS("ImGuiOverlays", "{} Replaying"), ICON_FA_PLAY).c_str(), IM_COL32(97, 240, 84, 255));
	}

	// Input Recording Metadata
	DRAW_LINE(fixed_font, font_size, g_InputRecordingData.recording_active_message.c_str(), IM_COL32(117, 255, 241, 255));
	DRAW_LINE(fixed_font, font_size, g_InputRecordingData.frame_data_message.c_str(), IM_COL32(117, 255, 241, 255));
	DRAW_LINE(fixed_font, font_size, g_InputRecordingData.undo_count_message.c_str(), IM_COL32(117, 255, 241, 255));

#undef DRAW_LINE
}

__ri void ImGuiManager::DrawVideoCaptureOverlay(float& position_y, float scale, float margin, float spacing)
{
	if (!GSConfig.OsdShowVideoCapture ||
		!GSCapture::IsCapturing() ||
		FullscreenUI::HasActiveWindow())
		return;

	const float shadow_offset = std::ceil(scale);
	ImFont* const standard_font = ImGuiManager::GetStandardFont();
	float font_size = ImGuiManager::GetFontSizeStandard();
	ImDrawList* dl = ImGui::GetBackgroundDrawList();

	static constexpr const char* ICON = ICON_PF_CIRCLE;
	const TinyString text_msg = TinyString::from_format(" {}", GSCapture::GetElapsedTime());
	const ImVec2 icon_size = standard_font->CalcTextSizeA(font_size, std::numeric_limits<float>::max(),
		-1.0f, ICON, nullptr, nullptr);
	const ImVec2 text_size = standard_font->CalcTextSizeA(font_size, std::numeric_limits<float>::max(),
		-1.0f, text_msg.c_str(), text_msg.end_ptr(), nullptr);

	// Shadow
	dl->AddText(standard_font, font_size,
		ImVec2(GetWindowWidth() - margin - text_size.x - icon_size.x + shadow_offset, position_y + shadow_offset),
		IM_COL32(0, 0, 0, 100), ICON);
	dl->AddText(standard_font, font_size,
		ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset),
		IM_COL32(0, 0, 0, 100), text_msg.c_str(), text_msg.end_ptr());

	// Text
	dl->AddText(standard_font, font_size,
		ImVec2(GetWindowWidth() - margin - text_size.x - icon_size.x, position_y), IM_COL32(255, 0, 0, 255), ICON);
	dl->AddText(standard_font, font_size,
		ImVec2(GetWindowWidth() - margin - text_size.x, position_y), IM_COL32(255, 255, 255, 255), text_msg.c_str(),
		text_msg.end_ptr());

	position_y += std::max(icon_size.y, text_size.y) + spacing;
}

namespace SaveStateSelectorUI
{
	namespace
	{
		struct ListEntry
		{
			std::string title;
			std::string summary;
			std::string filename;
			std::unique_ptr<GSTexture> preview_texture;
		};
	} // namespace

	static void InitializePlaceholderListEntry(ListEntry* li, std::string path, s32 slot);
	static void InitializeListEntry(const std::string& serial, u32 crc, ListEntry* li, s32 slot);

	static void RefreshHotkeyLegend();
	static void Draw();
	static void ShowSlotOSDMessage();
	bool IsOpen();

	static constexpr const char* DATE_TIME_FORMAT = TRANSLATE_NOOP("ImGuiOverlays", "Saved at {0:%H:%M} on {0:%a} {0:%Y/%m/%d}.");

	static std::shared_ptr<GSTexture> s_placeholder_texture;
	static std::string s_load_legend;
	static std::string s_save_legend;
	static std::string s_prev_legend;
	static std::string s_next_legend;
	static std::string s_close_legend;

	static std::array<ListEntry, VMManager::NUM_SAVE_STATE_SLOTS> s_slots;
	static std::atomic_int32_t s_current_slot{0};

	static float s_open_time = 0.0f;
	static float s_close_time = 0.0f;

	static ImAnimatedFloat s_scroll_animated;
	static ImAnimatedFloat s_background_animated;

	static bool s_open = false;
} // namespace SaveStateSelectorUI

void SaveStateSelectorUI::Open(float open_time /* = DEFAULT_OPEN_TIME */)
{
	const std::string serial = VMManager::GetDiscSerial();
	if (serial.empty())
	{
		Host::AddIconOSDMessage("SaveStateSelectorUIUnavailable", ICON_PF_MEMORY_CARD,
			TRANSLATE_SV("ImGuiOverlays", "Save state selector is unavailable without a valid game serial."));
		return;
	}

	s_open_time = 0.0f;
	s_close_time = open_time;

	if (s_open)
		return;


	if (!s_placeholder_texture)
		s_placeholder_texture = ImGuiFullscreen::LoadTexture("fullscreenui/no-save.png");

	s_scroll_animated.Reset(0.0f);
	s_background_animated.Reset(0.0f);
	s_open = true;
	RefreshList(serial, VMManager::GetDiscCRC());
	RefreshHotkeyLegend();
}

bool SaveStateSelectorUI::IsOpen(){
	return s_open;
}

void SaveStateSelectorUI::Close()
{
	s_open = false;
	s_load_legend = {};
	s_save_legend = {};
	s_prev_legend = {};
	s_next_legend = {};
	s_close_legend = {};
}

void SaveStateSelectorUI::RefreshList(const std::string& serial, u32 crc)
{
	for (ListEntry& entry : s_slots)
	{
		if (entry.preview_texture)
			g_gs_device->Recycle(entry.preview_texture.release());
	}

	for (u32 i = 0; i < VMManager::NUM_SAVE_STATE_SLOTS; i++)
		InitializeListEntry(serial, crc, &s_slots[i], static_cast<s32>(i + 1));
}

void SaveStateSelectorUI::Clear()
{
	// called on CPU thread at shutdown, textures should already be deleted, unless running
	// big picture UI, in which case we have to delete them here...
	for (ListEntry& li : s_slots)
	{
		if (li.preview_texture)
		{
			MTGS::RunOnGSThread([tex = li.preview_texture.release()]() {
				g_gs_device->Recycle(tex);
			});
		}

		li = {};
	}

	s_current_slot.store(0, std::memory_order_release);
}

void SaveStateSelectorUI::DestroyTextures()
{
	Close();

	for (ListEntry& entry : s_slots)
	{
		if (entry.preview_texture)
			g_gs_device->Recycle(entry.preview_texture.release());
	}

	s_placeholder_texture.reset();
}

void SaveStateSelectorUI::RefreshHotkeyLegend()
{
	auto format_legend_entry = [](SmallString binding, std::string_view caption) {
		InputManager::PrettifyInputBinding(binding);
		return fmt::format("{} - {}", binding, caption);
	};

	s_load_legend = format_legend_entry(Host::GetSmallStringSettingValue("Hotkeys", "LoadStateFromSlot"),
		TRANSLATE_STR("ImGuiOverlays", "Load"));
	s_save_legend = format_legend_entry(Host::GetSmallStringSettingValue("Hotkeys", "SaveStateToSlot"),
		TRANSLATE_STR("ImGuiOverlays", "Save"));
	s_prev_legend = format_legend_entry(Host::GetSmallStringSettingValue("Hotkeys", "PreviousSaveStateSlot"),
		TRANSLATE_STR("ImGuiOverlays", "Select Previous"));
	s_next_legend = format_legend_entry(Host::GetSmallStringSettingValue("Hotkeys", "NextSaveStateSlot"),
		TRANSLATE_STR("ImGuiOverlays", "Select Next"));
	s_close_legend = format_legend_entry(Host::GetSmallStringSettingValue("Hotkeys", "OpenPauseMenu"),
		TRANSLATE_STR("ImGuiOverlays", "Close Menu"));
}

void SaveStateSelectorUI::SelectNextSlot(bool open_selector)
{
	const s32 current_slot = s_current_slot.load(std::memory_order_acquire);
	s_current_slot.store((current_slot == (VMManager::NUM_SAVE_STATE_SLOTS - 1)) ? 0 : (current_slot + 1), std::memory_order_release);

	if (open_selector)
	{
		MTGS::RunOnGSThread([]() {
			if (!s_open)
				Open();

			s_open_time = 0.0f;
		});
	}
	else
	{
		ShowSlotOSDMessage();
	}
}

void SaveStateSelectorUI::SelectPreviousSlot(bool open_selector)
{
	const s32 current_slot = s_current_slot.load(std::memory_order_acquire);
	s_current_slot.store((current_slot == 0) ? (VMManager::NUM_SAVE_STATE_SLOTS - 1) : (current_slot - 1), std::memory_order_release);

	if (open_selector)
	{
		MTGS::RunOnGSThread([]() {
			if (!s_open)
				Open();

			s_open_time = 0.0f;
		});
	}
	else
	{
		ShowSlotOSDMessage();
	}
}

void SaveStateSelectorUI::InitializeListEntry(const std::string& serial, u32 crc, ListEntry* li, s32 slot)
{
	std::string path = VMManager::GetSaveStateFileName(serial.c_str(), crc, slot);
	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(path.c_str(), &sd))
	{
		InitializePlaceholderListEntry(li, std::move(path), slot);
		return;
	}

	li->title = fmt::format(TRANSLATE_FS("ImGuiOverlays", "Save Slot {0}"), slot);
	li->summary = fmt::format(TRANSLATE_FS("ImGuiOverlays", DATE_TIME_FORMAT), fmt::localtime(sd.ModificationTime));
	li->filename = Path::GetFileName(path);

	u32 screenshot_width, screenshot_height;
	std::vector<u32> screenshot_pixels;
	if (SaveState_ReadScreenshot(path, &screenshot_width, &screenshot_height, &screenshot_pixels))
	{
		li->preview_texture =
			std::unique_ptr<GSTexture>(g_gs_device->CreateTexture(screenshot_width, screenshot_height, 1, GSTexture::Format::Color));
		if (!li->preview_texture || !li->preview_texture->Update(GSVector4i(0, 0, screenshot_width, screenshot_height),
										screenshot_pixels.data(), sizeof(u32) * screenshot_width))
		{
			Console.Error("Failed to upload save state image to GPU");
			if (li->preview_texture)
				g_gs_device->Recycle(li->preview_texture.release());
		}
	}
}

void SaveStateSelectorUI::InitializePlaceholderListEntry(ListEntry* li, std::string path, s32 slot)
{
	li->title = fmt::format(TRANSLATE_FS("ImGuiOverlays", "Save Slot {0}"), slot);
	li->summary = TRANSLATE_STR("ImGuiOverlays", "No save present in this slot.");
	li->filename = Path::GetFileName(path);
}

void SaveStateSelectorUI::Draw()
{
	static constexpr float SCROLL_ANIMATION_TIME = 0.25f;
	static constexpr float BG_ANIMATION_TIME = 0.15f;

	const auto& io = ImGui::GetIO();
	const float scale = ImGuiManager::GetGlobalScale();
	const float width = (600.0f * scale);
	const float height = (430.0f * scale);

	const float padding_and_rounding = 10.0f * scale;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, padding_and_rounding);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding_and_rounding, padding_and_rounding));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.15f, 0.17f, 0.8f));
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
		ImVec2(0.5f, 0.5f));

	if (ImGui::Begin("##save_state_selector", nullptr,
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoScrollbar))
	{
		// Leave 2 lines for the legend
		const float legend_margin = ImGui::GetFontSize() * 3.0f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
		const float padding = 10.0f * scale;

		ImGui::BeginChild("##item_list", ImVec2(0, -legend_margin), false,
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoBackground);
		{
			const s32 current_slot = s_current_slot.load(std::memory_order_acquire);
			const ImVec2 image_size = ImVec2(128.0f * scale, (128.0f / (4.0f / 3.0f)) * scale);
			const float item_width = std::floor(width - (padding_and_rounding * 2.0f) - ImGui::GetStyle().ScrollbarSize);
			const float item_height = std::floor(image_size.y + padding * 2.0f);
			const float text_indent = image_size.x + padding + padding;

			for (size_t i = 0; i < s_slots.size(); i++)
			{
				const ListEntry& entry = s_slots[i];
				const float y_start = item_height * static_cast<float>(i);

				if (i == static_cast<size_t>(current_slot))
				{
					ImGui::SetCursorPosY(y_start);

					const ImVec2 p_start(ImGui::GetCursorScreenPos());
					const ImVec2 p_end(p_start.x + item_width, p_start.y + item_height);
					const ImRect item_rect(p_start, p_end);
					const ImRect& window_rect = ImGui::GetCurrentWindow()->ClipRect;
					if (!window_rect.Contains(item_rect))
					{
						float scroll_target = ImGui::GetScrollY();
						if (item_rect.Min.y < window_rect.Min.y)
							scroll_target = (ImGui::GetScrollY() - (window_rect.Min.y - item_rect.Min.y));
						else if (item_rect.Max.y > window_rect.Max.y)
							scroll_target = (ImGui::GetScrollY() + (item_rect.Max.y - window_rect.Max.y));

						if (scroll_target != s_scroll_animated.GetEndValue())
							s_scroll_animated.Start(ImGui::GetScrollY(), scroll_target, SCROLL_ANIMATION_TIME);
					}

					if (s_scroll_animated.IsActive())
						ImGui::SetScrollY(s_scroll_animated.UpdateAndGetValue());

					if (s_background_animated.GetEndValue() != p_start.y)
						s_background_animated.Start(s_background_animated.UpdateAndGetValue(), p_start.y, BG_ANIMATION_TIME);

					ImVec2 highlight_pos;
					if (s_background_animated.IsActive())
						highlight_pos = ImVec2(p_start.x, s_background_animated.UpdateAndGetValue());
					else
						highlight_pos = p_start;

					ImGui::GetWindowDrawList()->AddRectFilled(highlight_pos,
						ImVec2(highlight_pos.x + item_width, highlight_pos.y + item_height),
						ImColor(0.22f, 0.30f, 0.34f, 0.9f), padding_and_rounding);
				}

				if (GSTexture* preview_texture = entry.preview_texture ? entry.preview_texture.get() : s_placeholder_texture.get())
				{
					ImGui::SetCursorPosY(y_start + padding);
					ImGui::SetCursorPosX(padding);
					ImGui::Image(reinterpret_cast<ImTextureID>(preview_texture->GetNativeHandle()), image_size);
				}

				ImGui::SetCursorPosY(y_start + padding);

				ImGui::Indent(text_indent);

				ImGui::TextUnformatted(entry.title.c_str(), entry.title.c_str() + entry.title.length());
				ImGui::TextUnformatted(entry.summary.c_str(), entry.summary.c_str() + entry.summary.length());
				ImGui::PushFont(ImGuiManager::GetFixedFont(), ImGuiManager::GetFontSizeStandard());
				ImGui::TextUnformatted(entry.filename.c_str(), entry.filename.c_str() + entry.filename.length());
				ImGui::PopFont();

				ImGui::Unindent(text_indent);
				ImGui::SetCursorPosY(y_start);
				ImGui::ItemSize(ImVec2(item_width, item_height));
			}
		}
		ImGui::EndChild();

		ImGui::BeginChild("##legend", ImVec2(0, 0), false,
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
		{
			ImGui::SetCursorPosX(padding);
			if (ImGui::BeginTable("table", 2))
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s_load_legend.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s_prev_legend.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s_save_legend.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s_next_legend.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s_close_legend.c_str());

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	// auto-close
	s_open_time += io.DeltaTime;
	if (s_open_time >= s_close_time)
		Close();
}

s32 SaveStateSelectorUI::GetCurrentSlot()
{
	return s_current_slot.load(std::memory_order_acquire) + 1;
}

void SaveStateSelectorUI::LoadCurrentSlot()
{
	Host::RunOnCPUThread([slot = GetCurrentSlot()]() {
		VMManager::LoadStateFromSlot(slot);
	});
	Close();
}

void SaveStateSelectorUI::LoadCurrentBackupSlot()
{
	Host::RunOnCPUThread([slot = GetCurrentSlot()]() {
		VMManager::LoadStateFromSlot(slot, true);
	});
	Close();
}

void SaveStateSelectorUI::SaveCurrentSlot()
{
	Host::RunOnCPUThread([slot = GetCurrentSlot()]() {
		VMManager::SaveStateToSlot(slot);
	});
	Close();
}

void SaveStateSelectorUI::ShowSlotOSDMessage()
{
	const s32 slot = GetCurrentSlot();
	const u32 crc = VMManager::GetDiscCRC();
	const std::string serial = VMManager::GetDiscSerial();
	const std::string filename = VMManager::GetSaveStateFileName(serial.c_str(), crc, slot);
	FILESYSTEM_STAT_DATA sd;
	std::string date;
	if (!filename.empty() && FileSystem::StatFile(filename.c_str(), &sd))
		date = fmt::format(TRANSLATE_FS("ImGuiOverlays", DATE_TIME_FORMAT), fmt::localtime(sd.ModificationTime));
	else
		date = TRANSLATE_STR("ImGuiOverlays", "no save yet");

	Host::AddIconOSDMessage("ShowSlotOSDMessage", ICON_FA_MAGNIFYING_GLASS,
		fmt::format(TRANSLATE_FS("Hotkeys", "Save slot {0} selected ({1})."), slot, date),
		Host::OSD_QUICK_DURATION);
}

void ImGuiManager::RenderOverlays()
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);	
	float position_y = margin;

	DrawVideoCaptureOverlay(position_y, scale, margin, spacing);
	DrawInputRecordingOverlay(position_y, scale, margin, spacing);
	if (GSConfig.OsdPerformancePos != OsdOverlayPos::None)
		DrawPerformanceOverlay(position_y, scale, margin, spacing);
	DrawSettingsOverlay(scale, margin, spacing);
	DrawInputsOverlay(scale, margin, spacing);
	if (SaveStateSelectorUI::s_open)
		SaveStateSelectorUI::Draw();
}
