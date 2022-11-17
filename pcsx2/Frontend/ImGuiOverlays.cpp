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

#include <chrono>
#include <cmath>
#include <deque>
#include <mutex>
#include <unordered_map>

#include "fmt/core.h"

#include "common/StringUtil.h"
#include "common/Timer.h"
#include "imgui.h"

#include "Config.h"
#include "Counters.h"
#include "Frontend/ImGuiManager.h"
#include "Frontend/ImGuiOverlays.h"
#include "GS.h"
#include "GS/GS.h"
#include "Host.h"
#include "HostDisplay.h"
#include "IconsFontAwesome5.h"
#include "PerformanceMetrics.h"

#ifdef PCSX2_CORE
#include "PAD/Host/PAD.h"
#include "PAD/Host/KeyStatus.h"
#include "Frontend/FullscreenUI.h"
#include "Frontend/ImGuiManager.h"
#include "Frontend/ImGuiFullscreen.h"
#include "Frontend/InputManager.h"
#include "VMManager.h"
#endif

namespace ImGuiManager
{
	static void FormatProcessorStat(std::string& text, double usage, double time);
	static void DrawPerformanceOverlay();
#ifdef PCSX2_CORE
	static void DrawSettingsOverlay();
	static void DrawInputsOverlay();
#endif
} // namespace ImGuiManager

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

void ImGuiManager::DrawPerformanceOverlay()
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);
	float position_y = margin;

	ImFont* const fixed_font = ImGuiManager::GetFixedFont();
	ImFont* const standard_font = ImGuiManager::GetStandardFont();

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	std::string text;
	ImVec2 text_size;
	bool first = true;

	text.reserve(128);

#define DRAW_LINE(font, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		dl->AddText(font, font->FontSize, \
			ImVec2(ImGui::GetIO().DisplaySize.x - margin - text_size.x + shadow_offset, position_y + shadow_offset), \
			IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, font->FontSize, ImVec2(ImGui::GetIO().DisplaySize.x - margin - text_size.x, position_y), color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

#ifdef PCSX2_CORE
	const bool paused = (VMManager::GetState() == VMState::Paused);
	const bool fsui_active = FullscreenUI::HasActiveWindow();
#else
	const bool paused = false;
	const bool fsui_active = false;
#endif

	if (!paused)
	{
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
			std::string gs_stats;
			GSgetStats(gs_stats);
			DRAW_LINE(fixed_font, gs_stats.c_str(), IM_COL32(255, 255, 255, 255));
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
			fmt::format_to(std::back_inserter(text), "{:.2f}ms ({:.2f}ms worst)", PerformanceMetrics::GetAverageFrameTime(),
				PerformanceMetrics::GetWorstFrameTime());
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

#ifdef PCSX2_CORE

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
		EmuConfig.Cpu.Recompiler.GetEEClampMode(), static_cast<unsigned>(EmuConfig.Cpu.sseVUMXCSR.GetRoundMode()),
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
		if (EmuConfig.GS.CRCHack != CRCHackLevel::Automatic)
			APPEND("CRC={} ", static_cast<unsigned>(EmuConfig.GS.CRCHack));
		if (GSConfig.UserHacks_HalfBottomOverride >= 0)
			APPEND("HBO={} ", GSConfig.UserHacks_HalfBottomOverride);
		if (GSConfig.UserHacks_HalfPixelOffset > 0)
			APPEND("HPO={} ", GSConfig.UserHacks_HalfPixelOffset);
		if (GSConfig.UserHacks_RoundSprite > 0)
			APPEND("RS={} ", GSConfig.UserHacks_RoundSprite);
		if (GSConfig.UserHacks_TCOffsetX != 0 || GSConfig.UserHacks_TCOffsetY != 0)
			APPEND("TCO={}/{} ", GSConfig.UserHacks_TCOffsetX, GSConfig.UserHacks_TCOffsetY);
		if (GSConfig.UserHacks_CPUSpriteRenderBW != 0)
			APPEND("CSBW={} ", GSConfig.UserHacks_CPUSpriteRenderBW);
		if (GSConfig.UserHacks_CPUCLUTRender != 0)
			APPEND("CCD={} ", GSConfig.UserHacks_CPUCLUTRender);
		if (GSConfig.SkipDrawStart != 0 || GSConfig.SkipDrawEnd != 0)
			APPEND("SD={}/{} ", GSConfig.SkipDrawStart, GSConfig.SkipDrawEnd);
		if (GSConfig.UserHacks_TextureInsideRt)
			APPEND("TexRT ");
		if (GSConfig.UserHacks_WildHack)
			APPEND("WA ");
		if (GSConfig.UserHacks_MergePPSprite)
			APPEND("MS ");
		if (GSConfig.UserHacks_AlignSpriteX)
			APPEND("AS ");
		if (GSConfig.UserHacks_AutoFlush)
			APPEND("AF ");
		if (GSConfig.UserHacks_CPUFBConversion)
			APPEND("FBC ");
		if(GSConfig.UserHacks_DisableDepthSupport)
			APPEND("DDE ");
		if (GSConfig.UserHacks_DisablePartialInvalidation)
			APPEND("DPIV ");
		if (GSConfig.UserHacks_DisableSafeFeatures)
			APPEND("DSF ");
		if (GSConfig.WrapGSMem)
			APPEND("WGSM ");
		if (GSConfig.PreloadFrameWithGSData)
			APPEND("PLFD ");
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
	const float position_y = ImGui::GetIO().DisplaySize.y - margin - font->FontSize;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImVec2 text_size =
		font->CalcTextSizeA(font->FontSize, std::numeric_limits<float>::max(), -1.0f, text.c_str(), text.c_str() + text.length(), nullptr);
	dl->AddText(font, font->FontSize,
		ImVec2(ImGui::GetIO().DisplaySize.x - margin - text_size.x + shadow_offset, position_y + shadow_offset), IM_COL32(0, 0, 0, 100),
		text.c_str(), text.c_str() + text.length());
	dl->AddText(font, font->FontSize, ImVec2(ImGui::GetIO().DisplaySize.x - margin - text_size.x, position_y), IM_COL32(255, 255, 255, 255),
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
	for (u32 port = 0; port < PAD::NUM_CONTROLLER_PORTS; port++)
	{
		const PAD::ControllerType ctype = g_key_status.GetType(port);
		if (ctype != PAD::ControllerType::NotConnected)
			num_ports++;
	}

	float current_x = margin;
	float current_y = display_size.y - margin - ((static_cast<float>(num_ports) * (font->FontSize + spacing)) - spacing);

	const ImVec4 clip_rect(current_x, current_y, display_size.x - margin, display_size.y - margin);

	std::string text;
	text.reserve(256);

	for (u32 port = 0; port < PAD::NUM_CONTROLLER_PORTS; port++)
	{
		const PAD::ControllerType ctype = g_key_status.GetType(port);
		if (ctype == PAD::ControllerType::NotConnected)
			continue;

		const PAD::ControllerInfo* cinfo = PAD::GetControllerInfo(ctype);
		if (!cinfo)
			continue;

		text.clear();
		fmt::format_to(std::back_inserter(text), "P{} |", port + 1u);

		for (u32 bind = 0; bind < cinfo->num_bindings; bind++)
		{
			const PAD::ControllerBindingInfo& bi = cinfo->bindings[bind];
			switch (bi.type)
			{
				case PAD::ControllerBindingType::Axis:
				case PAD::ControllerBindingType::HalfAxis:
				{
					// axes are always shown
					const float value = static_cast<float>(g_key_status.GetRawPressure(port, bind)) * (1.0f / 255.0f);
					if (value >= (254.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
					else if (value > (1.0f / 255.0f))
						fmt::format_to(std::back_inserter(text), " {}: {:.2f}", bi.name, value);
				}
				break;

				case PAD::ControllerBindingType::Button:
				{
					// buttons only shown when active
					const float value = static_cast<float>(g_key_status.GetRawPressure(port, bind)) * (1.0f / 255.0f);
					if (value >= 0.5f)
						fmt::format_to(std::back_inserter(text), " {}", bi.name);
				}
				break;

				case PAD::ControllerBindingType::Motor:
				case PAD::ControllerBindingType::Macro:
				case PAD::ControllerBindingType::Unknown:
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

#endif

void ImGuiManager::RenderOverlays()
{
	DrawPerformanceOverlay();
#ifdef PCSX2_CORE
	DrawSettingsOverlay();
	DrawInputsOverlay();
#endif
}
