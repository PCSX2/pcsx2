/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Config.h"
#include "Counters.h"
#include "GS.h"
#include "GS/GS.h"
#include "GS/GSCapture.h"
#include "GS/GSVector.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "ImGui/FullscreenUI.h"
#include "ImGui/ImGuiFullscreen.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "Input/InputManager.h"
#include "PerformanceMetrics.h"
#include "Recording/InputRecording.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadBase.h"
#include "USB/USB.h"
#include "VMManager.h"

#include "common/BitUtils.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/core.h"
#include "imgui.h"

#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <span>
#include <tuple>
#include <unordered_map>

namespace ImGuiManager
{
	static void FormatProcessorStat(std::string& text, double usage, double time);
	static void DrawPerformanceOverlay(float& position_y);
	static void DrawSettingsOverlay();
	static void DrawInputsOverlay();
	static void DrawInputRecordingOverlay(float& position_y);
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

void ImGuiManager::FormatProcessorStat(std::string& text, double usage, double time)
{
	// Some values, such as GPU (and even CPU to some extent) can be out of phase with the wall clock,
	// which the processor time is divided by to get a utilization percentage. Let's clamp it at 100%,
	// so that people don't get confused, and remove the decimal places when it's there while we're at it.
	if (usage >= 99.95)
		fmt::format_to(std::back_inserter(text), "100% ({:.2f}ms)", time);
	else
		fmt::format_to(std::back_inserter(text), "{:.1f}% ({:.2f}ms)", usage, time);
}

void ImGuiManager::DrawPerformanceOverlay(float& position_y)
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);

	ImFont* const fixed_font = ImGuiManager::GetFixedFont();
	ImFont* const standard_font = ImGuiManager::GetStandardFont();

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	std::string text;
	ImVec2 text_size;

	text.reserve(128);

#define DRAW_LINE(font, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		dl->AddText(font, font->FontSize, \
			ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset), \
			IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, font->FontSize, ImVec2(GetWindowWidth() - margin - text_size.x, position_y), color, (text)); \
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
					fmt::format_to(std::back_inserter(text), "G: {:.2f} [P] | V: {:.2f}", PerformanceMetrics::GetInternalFPS(),
						PerformanceMetrics::GetFPS());
					break;

				case PerformanceMetrics::InternalFPSMethod::DISPFBBlit:
					fmt::format_to(std::back_inserter(text), "G: {:.2f} [B] | V: {:.2f}", PerformanceMetrics::GetInternalFPS(),
						PerformanceMetrics::GetFPS());
					break;

				case PerformanceMetrics::InternalFPSMethod::None:
				default:
					fmt::format_to(std::back_inserter(text), "V: {:.2f}", PerformanceMetrics::GetFPS());
					break;
			}
			first = false;
		}
		if (GSConfig.OsdShowSpeed)
		{
			fmt::format_to(std::back_inserter(text), "{}{}%", first ? "" : " | ", static_cast<u32>(std::round(speed)));

			// We read the main config here, since MTGS doesn't get updated with speed toggles.
			if (EmuConfig.GS.LimitScalar == 0.0f)
				text += " (Max)";
			else
				fmt::format_to(std::back_inserter(text), " ({:.0f}%)", EmuConfig.GS.LimitScalar * 100.0f);
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

			DRAW_LINE(fixed_font, text.c_str(), color);
		}

		if (GSConfig.OsdShowGSStats)
		{
			text.clear();
			GSgetStats(text);
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));

			text.clear();
			GSgetMemoryStats(text);
			if (!text.empty())
				DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowResolution)
		{
			int width, height;
			GSgetInternalResolution(&width, &height);

			text.clear();
			fmt::format_to(std::back_inserter(text), "{}x{} {} {}", width, height, ReportVideoMode(), ReportInterlaceMode());
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowCPU)
		{
			text.clear();
			fmt::format_to(std::back_inserter(text), "{:.2f}ms | {:.2f}ms | {:.2f}ms", PerformanceMetrics::GetMinimumFrameTime(),
				PerformanceMetrics::GetAverageFrameTime(), PerformanceMetrics::GetMaximumFrameTime());
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));

			text.clear();
			if (EmuConfig.Speedhacks.EECycleRate != 0 || EmuConfig.Speedhacks.EECycleSkip != 0)
				fmt::format_to(std::back_inserter(text), "EE[{}/{}]: ", EmuConfig.Speedhacks.EECycleRate, EmuConfig.Speedhacks.EECycleSkip);
			else
				text = "EE: ";
			FormatProcessorStat(text, PerformanceMetrics::GetCPUThreadUsage(), PerformanceMetrics::GetCPUThreadAverageTime());
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));

			text = "GS: ";
			FormatProcessorStat(text, PerformanceMetrics::GetGSThreadUsage(), PerformanceMetrics::GetGSThreadAverageTime());
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));

			const u32 gs_sw_threads = PerformanceMetrics::GetGSSWThreadCount();
			for (u32 i = 0; i < gs_sw_threads; i++)
			{
				text.clear();
				fmt::format_to(std::back_inserter(text), "SW-{}: ", i);
				FormatProcessorStat(text, PerformanceMetrics::GetGSSWThreadUsage(i), PerformanceMetrics::GetGSSWThreadAverageTime(i));
				DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
			}

			if (THREAD_VU1)
			{
				text = "VU: ";
				FormatProcessorStat(text, PerformanceMetrics::GetVUThreadUsage(), PerformanceMetrics::GetVUThreadAverageTime());
				DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
			}

			if (GSCapture::IsCapturing())
			{
				text = "CAP: ";
				FormatProcessorStat(text, PerformanceMetrics::GetCaptureThreadUsage(), PerformanceMetrics::GetCaptureThreadAverageTime());
				DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
			}
		}

		if (GSConfig.OsdShowGPU)
		{
			text = "GPU: ";
			FormatProcessorStat(text, PerformanceMetrics::GetGPUUsage(), PerformanceMetrics::GetGPUAverageTime());
			DRAW_LINE(fixed_font, text.c_str(), IM_COL32(255, 255, 255, 255));
		}

		if (GSConfig.OsdShowIndicators)
		{
			const bool is_normal_speed = (EmuConfig.GS.LimitScalar == EmuConfig.Framerate.NominalScalar);
			if (!is_normal_speed)
			{
				const bool is_slowmo = (EmuConfig.GS.LimitScalar < EmuConfig.Framerate.NominalScalar);
				DRAW_LINE(standard_font, is_slowmo ? ICON_FA_FORWARD : ICON_FA_FAST_FORWARD, IM_COL32(255, 255, 255, 255));
			}
		}

		if (GSConfig.OsdShowFrameTimes)
		{
			const ImVec2 history_size(200.0f * scale, 50.0f * scale);
			ImGui::SetNextWindowSize(ImVec2(history_size.x, history_size.y));
			ImGui::SetNextWindowPos(ImVec2(GetWindowWidth() - margin - history_size.x, position_y));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.25f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			ImGui::PushFont(fixed_font);
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
				fmt::format_to(std::back_inserter(text), "{:.1f} ms", max);
				text_size = fixed_font->CalcTextSizeA(fixed_font->FontSize, FLT_MAX, 0.0f, text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(wpos.x + history_size.x - text_size.x - spacing + shadow_offset, wpos.y + shadow_offset),
					IM_COL32(0, 0, 0, 100), text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(wpos.x + history_size.x - text_size.x - spacing, wpos.y), IM_COL32(255, 255, 255, 255), text.c_str(),
					text.c_str() + text.length());

				text.clear();
				fmt::format_to(std::back_inserter(text), "{:.1f} ms", min);
				text_size = fixed_font->CalcTextSizeA(fixed_font->FontSize, FLT_MAX, 0.0f, text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(wpos.x + history_size.x - text_size.x - spacing + shadow_offset,
									wpos.y + history_size.y - fixed_font->FontSize + shadow_offset),
					IM_COL32(0, 0, 0, 100), text.c_str(), text.c_str() + text.length());
				win_dl->AddText(ImVec2(wpos.x + history_size.x - text_size.x - spacing, wpos.y + history_size.y - fixed_font->FontSize),
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
			DRAW_LINE(standard_font, ICON_FA_PAUSE, IM_COL32(255, 255, 255, 255));
		}
	}

#undef DRAW_LINE
}

void ImGuiManager::DrawSettingsOverlay()
{
	if (!GSConfig.OsdShowSettings || VMManager::GetState() != VMState::Running)
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

	APPEND("EER={} EEC={} VUR={} VUC={} VQS={} ", static_cast<unsigned>(EmuConfig.Cpu.sseMXCSR.GetRoundMode()),
		EmuConfig.Cpu.Recompiler.GetEEClampMode(), static_cast<unsigned>(EmuConfig.Cpu.sseVU0MXCSR.GetRoundMode()),
		EmuConfig.Cpu.Recompiler.GetVUClampMode(), EmuConfig.GS.VsyncQueueSize);

	if (EmuConfig.EnableCheats || EmuConfig.EnableWideScreenPatches || EmuConfig.EnableNoInterlacingPatches)
	{
		APPEND("C={}{}{} ", EmuConfig.EnableCheats ? "C" : "", EmuConfig.EnableWideScreenPatches ? "W" : "",
			EmuConfig.EnableNoInterlacingPatches ? "N" : "");
	}

	if (GSConfig.UseHardwareRenderer())
	{
		if ((GSConfig.UpscaleMultiplier - std::floor(GSConfig.UpscaleMultiplier)) > 0.01)
			APPEND("IR={:.2f} ", static_cast<float>(GSConfig.UpscaleMultiplier));
		else
			APPEND("IR={} ", static_cast<unsigned>(GSConfig.UpscaleMultiplier));

		APPEND("B={} PL={} ", static_cast<unsigned>(GSConfig.AccurateBlendingUnit), static_cast<unsigned>(GSConfig.TexturePreloading));
		if (GSConfig.GPUPaletteConversion)
			APPEND("PT ");

		if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
			APPEND("DL={} ", static_cast<unsigned>(GSConfig.HWDownloadMode));

		if (GSConfig.HWMipmap != HWMipmapLevel::Automatic)
			APPEND("MM={} ", static_cast<unsigned>(GSConfig.HWMipmap));

		// deliberately test global and print local here for auto values
		if (EmuConfig.GS.TextureFiltering != BiFiltering::PS2)
			APPEND("BF={} ", static_cast<unsigned>(GSConfig.TextureFiltering));
		if (EmuConfig.GS.TriFilter != TriFiltering::Automatic)
			APPEND("TF={} ", static_cast<unsigned>(GSConfig.TriFilter));
		if (GSConfig.MaxAnisotropy > 1)
			APPEND("AF={} ", EmuConfig.GS.MaxAnisotropy);
		if (GSConfig.Dithering != 2)
			APPEND("DI={} ", GSConfig.Dithering);
		if (GSConfig.UserHacks_HalfPixelOffset > 0)
			APPEND("HPO={} ", GSConfig.UserHacks_HalfPixelOffset);
		if (GSConfig.UserHacks_RoundSprite > 0)
			APPEND("RS={} ", GSConfig.UserHacks_RoundSprite);
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
		if (GSConfig.UserHacks_WildHack)
			APPEND("WA ");
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
			APPEND("FTC ");
		if(GSConfig.UserHacks_DisableDepthSupport)
			APPEND("DDE ");
		if (GSConfig.UserHacks_DisablePartialInvalidation)
			APPEND("DPIV ");
		if (GSConfig.UserHacks_TargetPartialInvalidation)
			APPEND("TPI ");
		if (GSConfig.UserHacks_DisableSafeFeatures)
			APPEND("DSF ");
		if (GSConfig.UserHacks_DisableRenderFixes)
			APPEND("DRF ");
		if (GSConfig.PreloadFrameWithGSData)
			APPEND("PLFD ");
		if (GSConfig.UserHacks_EstimateTextureRegion)
			APPEND("ETR ");
	}

#undef APPEND

	if (text.empty())
		return;
	else if (text.back() == ' ')
		text.pop_back();

	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = 1.0f * scale;
	const float margin = 10.0f * scale;
	ImFont* font = ImGuiManager::GetFixedFont();
	const float position_y = GetWindowHeight() - margin - font->FontSize;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 text_size =
		font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, text.c_str(), text.c_str() + text.length(), nullptr);
	dl->AddText(font, font->FontSize,
		ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset), IM_COL32(0, 0, 0, 100),
		text.c_str(), text.c_str() + text.length());
	dl->AddText(font, font->FontSize, ImVec2(GetWindowWidth() - margin - text_size.x, position_y), IM_COL32(255, 255, 255, 255),
		text.c_str(), text.c_str() + text.length());
}

void ImGuiManager::DrawInputsOverlay()
{
	// Technically this is racing the CPU thread.. but it doesn't really matter, at worst, the inputs get displayed onscreen late.
	if (!GSConfig.OsdShowInputs || VMManager::GetState() != VMState::Running)
		return;

	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = 1.0f * scale;
	const float margin = 10.0f * scale;
	const float spacing = 5.0f * scale;
	ImFont* font = ImGuiManager::GetFixedFont();

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

	float current_x = margin;
	float current_y = display_size.y - margin - ((static_cast<float>(num_ports) * (font->FontSize + spacing)) - spacing);

	const ImVec4 clip_rect(current_x, current_y, display_size.x - margin, display_size.y - margin);

	std::string text;
	text.reserve(256);

	for (u32 slot = 0; slot < Pad::NUM_CONTROLLER_PORTS; slot++)
	{
		const PadBase* const pad = Pad::GetPad(slot);
		const Pad::ControllerType ctype = pad->GetType();
		if (ctype == Pad::ControllerType::NotConnected)
			continue;
	
		text.clear();
		fmt::format_to(std::back_inserter(text), "P{} |", slot + 1u);

		const Pad::ControllerInfo& cinfo = pad->GetInfo();
		for (u32 bind = 0; bind < static_cast<u32>(cinfo.bindings.size()); bind++)
		{
			const InputBindingInfo& bi = cinfo.bindings[bind];
			switch (bi.bind_type)
			{
				case InputBindingInfo::Type::Axis:
				case InputBindingInfo::Type::HalfAxis:
				{
					// axes are always shown
					const float value = static_cast<float>(pad->GetRawInput(bind)) * (1.0f / 255.0f);
					if (value >= (254.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
					else if (value > (1.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}: {:.2f}", bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Button:
				{
					// buttons only shown when active
					const float value = static_cast<float>(pad->GetRawInput(bind)) * (1.0f / 255.0f);
					if (value >= 0.5f)
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
				}
				break;

				case InputBindingInfo::Type::Motor:
				case InputBindingInfo::Type::Macro:
				case InputBindingInfo::Type::Unknown:
				default:
					break;
			}
		}

		dl->AddText(font, font->FontSize, ImVec2(current_x + shadow_offset, current_y + shadow_offset), shadow_color, text.c_str(),
			text.c_str() + text.length(), 0.0f, &clip_rect);
		dl->AddText(
			font, font->FontSize, ImVec2(current_x, current_y), text_color, text.c_str(), text.c_str() + text.length(), 0.0f, &clip_rect);

		current_y += font->FontSize + spacing;
	}

	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		if (EmuConfig.USB.Ports[port].DeviceType < 0)
			continue;

		const std::span<const InputBindingInfo> bindings(USB::GetDeviceBindings(port));
		if (bindings.empty())
			continue;

		text.clear();
		fmt::format_to(std::back_inserter(text), "USB{} |", port + 1u);

		for (const InputBindingInfo& bi : bindings)
		{
			switch (bi.bind_type)
			{
				case InputBindingInfo::Type::Axis:
				case InputBindingInfo::Type::HalfAxis:
				{
					// axes are always shown
					const float value = static_cast<float>(USB::GetDeviceBindValue(port, bi.bind_index));
					if (value >= (254.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
					else if (value > (1.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}: {:.2f}", bi.name, value);
				}
				break;

				case InputBindingInfo::Type::Button:
				{
					// buttons only shown when active
					const float value = static_cast<float>(USB::GetDeviceBindValue(port, bi.bind_index));
					if (value >= 0.5f)
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
				}
				break;

				case InputBindingInfo::Type::Motor:
				case InputBindingInfo::Type::Macro:
				case InputBindingInfo::Type::Unknown:
				default:
					break;
			}
		}

		dl->AddText(font, font->FontSize, ImVec2(current_x + shadow_offset, current_y + shadow_offset), shadow_color, text.c_str(),
			text.c_str() + text.length(), 0.0f, &clip_rect);
		dl->AddText(
			font, font->FontSize, ImVec2(current_x, current_y), text_color, text.c_str(), text.c_str() + text.length(), 0.0f, &clip_rect);

		current_y += font->FontSize + spacing;
	}
}

void ImGuiManager::DrawInputRecordingOverlay(float& position_y)
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);
	position_y += margin;

	ImFont* const fixed_font = ImGuiManager::GetFixedFont();
	ImFont* const standard_font = ImGuiManager::GetStandardFont();

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	std::string text;
	ImVec2 text_size;

	text.reserve(128);
#define DRAW_LINE(font, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		dl->AddText(font, font->FontSize, \
			ImVec2(GetWindowWidth() - margin - text_size.x + shadow_offset, position_y + shadow_offset), \
			IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, font->FontSize, ImVec2(GetWindowWidth() - margin - text_size.x, position_y), color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)
	// TODO - icon list that would be nice to add
	// - 'video' when screen capturing
	if (g_InputRecording.isActive())
	{
		// Status Indicators
		if (g_InputRecording.getControls().isRecording())
		{
			DRAW_LINE(standard_font, fmt::format("{} Recording", ICON_FA_RECORD_VINYL).c_str(), IM_COL32(255, 0, 0, 255));
		}
		else
		{
			DRAW_LINE(standard_font, fmt::format("{} Replaying", ICON_FA_PLAY).c_str(), IM_COL32(97, 240, 84, 255));
		}

		// Input Recording Metadata
		DRAW_LINE(fixed_font, fmt::format("Input Recording Active: {}", g_InputRecording.getData().getFilename()).c_str(), IM_COL32(117, 255, 241, 255));
		DRAW_LINE(fixed_font, fmt::format("Frame: {}/{} ({})", g_InputRecording.getFrameCounter() + 1, g_InputRecording.getData().getTotalFrames(), g_FrameCount).c_str(), IM_COL32(117, 255, 241, 255));
		DRAW_LINE(fixed_font, fmt::format("Undo Count: {}", g_InputRecording.getData().getUndoCount()).c_str(), IM_COL32(117, 255, 241, 255));
	}

#undef DRAW_LINE
}

void ImGuiManager::RenderOverlays()
{
	float position_y = 0;
	DrawInputRecordingOverlay(position_y);
	DrawPerformanceOverlay(position_y);
	DrawSettingsOverlay();
	DrawInputsOverlay();
}
