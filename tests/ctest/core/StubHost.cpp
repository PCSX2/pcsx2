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

#include "pcsx2/Frontend/CommonHost.h"
#include "pcsx2/Frontend/ImGuiManager.h"
#include "pcsx2/Frontend/InputManager.h"
#include "pcsx2/GS.h"
#include "pcsx2/Host.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/VMManager.h"

#ifdef ENABLE_ACHIEVEMENTS
#include "pcsx2/Frontend/Achievements.h"
#endif

void Host::CommitBaseSettingChanges()
{
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
	return false;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
}

std::optional<std::vector<u8>> Host::ReadResourceFile(const char* filename)
{
    return std::nullopt;
}

std::optional<std::string> Host::ReadResourceFileToString(const char* filename)
{
	return std::nullopt;
}

std::optional<std::time_t> Host::GetResourceFileTimestamp(const char* filename)
{
	return std::nullopt;
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
}

bool Host::ConfirmMessage(const std::string_view& title, const std::string_view& message)
{
	return true;
}

void Host::OpenURL(const std::string_view& url)
{
}

bool Host::CopyTextToClipboard(const std::string_view& text)
{
	return false;
}

void Host::BeginTextInput()
{
}

void Host::EndTextInput()
{
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
    return std::nullopt;
}

void Host::OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name)
{
}

void Host::OnInputDeviceDisconnected(const std::string_view& identifier)
{
}

void Host::SetRelativeMouseMode(bool enabled)
{
}

bool Host::AcquireHostDisplay(RenderAPI api, bool clear_state_on_fail)
{
    return false;
}

void Host::ReleaseHostDisplay(bool clear_state)
{
}

HostDisplay::PresentResult Host::BeginPresentFrame(bool frame_skip)
{
	return HostDisplay::PresentResult::FrameSkipped;
}

void Host::EndPresentFrame()
{
}

void Host::ResizeHostDisplay(u32 new_window_width, u32 new_window_height, float new_window_scale)
{
}

void Host::UpdateHostDisplay()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
}

void Host::OnVMStarting()
{
}

void Host::OnVMStarted()
{
}

void Host::OnVMDestroyed()
{
}

void Host::OnVMPaused()
{
}

void Host::OnVMResumed()
{
}

void Host::OnGameChanged(const std::string& disc_path, const std::string& elf_override, const std::string& game_serial,
	const std::string& game_name, u32 game_crc)
{
}

void Host::OnPerformanceMetricsUpdated()
{
}

void Host::OnSaveStateLoading(const std::string_view& filename)
{
}

void Host::OnSaveStateLoaded(const std::string_view& filename, bool was_successful)
{
}

void Host::OnSaveStateSaved(const std::string_view& filename)
{
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
}

void Host::RefreshGameListAsync(bool invalidate_cache)
{
}

void Host::CancelGameListRefresh()
{
}

bool Host::IsFullscreen()
{
	return false;
}

void Host::SetFullscreen(bool enabled)
{
}

void Host::RequestExit(bool save_state_if_running)
{
}

void Host::RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state)
{
}

void Host::CPUThreadVSync()
{
}

#ifdef ENABLE_ACHIEVEMENTS
void Host::OnAchievementsRefreshed()
{
}
#endif

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view& str)
{
	return std::nullopt;
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
	return std::nullopt;
}

SysMtgsThread& GetMTGS()
{
    throw std::exception();
}

//////////////////////////////////////////////////////////////////////////
// Interface Stuff
//////////////////////////////////////////////////////////////////////////

const IConsoleWriter* PatchesCon = &Console;
BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()