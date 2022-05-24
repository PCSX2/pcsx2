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

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "Host.h"
#include "HostSettings.h"
#include "HostDisplay.h"
#include "GS/GS.h"

#include "common/Assertions.h"
#include "Frontend/ImGuiManager.h"

#include "gui/App.h"
#include "gui/AppHost.h"
#include "gui/pxEvents.h"

#include <atomic>
#include <mutex>

#include "gui/App.h"
#include "gui/AppConfig.h"
#include "gui/pxEvents.h"

static auto OpenResourceCFile(const char* filename, const char* mode)
{
	const std::string full_filename(Path::Combine(EmuFolders::Resources, filename));
	auto fp = FileSystem::OpenManagedCFile(full_filename.c_str(), mode);
	if (!fp)
		Console.Error("Failed to open resource file '%s'", filename);

	return fp;
}

std::optional<std::vector<u8>> Host::ReadResourceFile(const char* filename)
{
	auto fp = OpenResourceCFile(filename, "rb");
	if (!fp)
		return std::nullopt;

	const size_t size = FileSystem::FSize64(fp.get());
	std::vector<u8> ret(size);
	if (std::fread(ret.data(), size, 1, fp.get()) != 1)
	{
		Console.Error("Failed to read resource file '%s'", filename);
		return std::nullopt;
	}

	return ret;
}

std::optional<std::string> Host::ReadResourceFileToString(const char* filename)
{
	auto fp = OpenResourceCFile(filename, "rb");
	if (!fp)
		return std::nullopt;

	const size_t size = FileSystem::FSize64(fp.get());
	std::string ret;
	ret.resize(size);
	if (std::fread(ret.data(), size, 1, fp.get()) != 1)
	{
		Console.Error("Failed to read resource file '%s' to string", filename);
		return std::nullopt;
	}

	return ret;
}

bool Host::GetBoolSettingValue(const char* section, const char* key, bool default_value /* = false */)
{
	return default_value;
}

std::string Host::GetStringSettingValue(const char* section, const char* key, const char* default_value /* = "" */)
{
	return default_value;
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
	wxGetApp().PostEvent(pxMessageBoxEvent(
		title.empty() ? wxString() : wxString::FromUTF8(title.data(), title.length()),
		message.empty() ? wxString() : wxString::FromUTF8(message.data(), message.length()),
		MsgButtons().OK()));
}

static std::unique_ptr<HostDisplay> s_host_display;

HostDisplay* Host::AcquireHostDisplay(HostDisplay::RenderAPI api)
{
	sApp.OpenGsPanel();

	// can't go anywhere if we don't have a window to render into!
	if (g_gs_window_info.type == WindowInfo::Type::Surfaceless)
		return nullptr;

	s_host_display = HostDisplay::CreateDisplayForAPI(api);
	if (!s_host_display)
		return nullptr;

	if (!s_host_display->CreateRenderDevice(g_gs_window_info, GSConfig.Adapter, EmuConfig.GetEffectiveVsyncMode(),
			GSConfig.ThreadedPresentation, GSConfig.UseDebugDevice) ||
		!s_host_display->InitializeRenderDevice(EmuFolders::Cache, GSConfig.UseDebugDevice) ||
		!ImGuiManager::Initialize())
	{
		s_host_display->DestroyRenderDevice();
		s_host_display.reset();
		return nullptr;
	}

	Console.WriteLn(Color_StrongGreen, "%s Graphics Driver Info:", HostDisplay::RenderAPIToString(s_host_display->GetRenderAPI()));
	Console.Indent().WriteLn(s_host_display->GetDriverInfo());

	return s_host_display.get();
}

void Host::ReleaseHostDisplay()
{
	ImGuiManager::Shutdown();

	if (s_host_display)
	{
		s_host_display->DestroyRenderDevice();
		s_host_display.reset();
	}

	sApp.CloseGsPanel();
}

HostDisplay* Host::GetHostDisplay()
{
	return s_host_display.get();
}

bool Host::BeginPresentFrame(bool frame_skip)
{
	CheckForGSWindowResize();

	if (!s_host_display->BeginPresent(frame_skip))
	{
		// if we're skipping a frame, we need to reset imgui's state, since
		// we won't be calling EndPresentFrame().
		ImGuiManager::NewFrame();
		return false;
	}

	return true;
}

void Host::EndPresentFrame()
{
	ImGuiManager::RenderOSD();
	s_host_display->EndPresent();
	ImGuiManager::NewFrame();
}

void Host::UpdateHostDisplay()
{
	// not used for wx
}

void Host::ResizeHostDisplay(u32 new_window_width, u32 new_window_height, float new_window_scale)
{
	// not used for wx (except for osd scale changes)
	ImGuiManager::WindowResized();
}

static std::atomic_bool s_gs_window_resized{false};
static std::mutex s_gs_window_resized_lock;
static int s_new_gs_window_width = 0;
static int s_new_gs_window_height = 0;
static float s_new_gs_window_scale = 1;

void Host::GSWindowResized(int width, int height, float scale)
{
	std::unique_lock lock(s_gs_window_resized_lock);
	s_new_gs_window_width = width;
	s_new_gs_window_height = height;
	s_new_gs_window_scale = scale;
	s_gs_window_resized.store(true);
}

void Host::CheckForGSWindowResize()
{
	if (!s_gs_window_resized.load())
		return;

	int width, height;
	float scale;
	{
		std::unique_lock lock(s_gs_window_resized_lock);
		width = s_new_gs_window_width;
		height = s_new_gs_window_height;
		scale = s_new_gs_window_scale;
		s_gs_window_resized.store(false);
	}

	if (!s_host_display)
		return;

	GSResetAPIState();
	s_host_display->ResizeRenderWindow(width, height, scale);
	GSRestoreAPIState();
	ImGuiManager::WindowResized();
}

