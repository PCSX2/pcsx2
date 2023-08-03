/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#if !defined(_WIN32) && !defined(__APPLE__)
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <optional>
#include <spawn.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fmt/core.h"

#include "common/Pcsx2Types.h"
#include "common/General.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

#ifdef DBUS_API
#include <dbus/dbus.h>
#endif

// Returns 0 on failure (not supported by the operating system).
u64 GetPhysicalMemory()
{
	u64 pages = 0;

#ifdef _SC_PHYS_PAGES
	pages = sysconf(_SC_PHYS_PAGES);
#endif

	return pages * getpagesize();
}

u64 GetTickFrequency()
{
	return 1000000000; // unix measures in nanoseconds
}

u64 GetCPUTicks()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (static_cast<u64>(ts.tv_sec) * 1000000000ULL) + ts.tv_nsec;
}

std::string GetOSVersionString()
{
#if defined(__linux__)
	return "Linux";
#else // freebsd
	return "Other Unix";
#endif
}

#ifdef DBUS_API

static bool SetScreensaverInhibitDBus(const bool inhibit_requested, const char* program_name, const char* reason)
{
	static dbus_uint32_t s_cookie;
	const char* bus_method = (inhibit_requested) ? "Inhibit" : "UnInhibit";
	DBusError error_dbus;
	DBusConnection* connection = nullptr;
	static DBusConnection* s_comparison_connection;
	DBusMessage* message = nullptr;
	DBusMessage* response = nullptr;
	DBusMessageIter message_itr;

	ScopedGuard cleanup = [&]() {
		if (dbus_error_is_set(&error_dbus))
			dbus_error_free(&error_dbus);
		if (message)
			dbus_message_unref(message);
		if (response)
			dbus_message_unref(response);
	};

	dbus_error_init(&error_dbus);
	// Calling dbus_bus_get() after the first time returns a pointer to the existing connection.
	connection = dbus_bus_get(DBUS_BUS_SESSION, &error_dbus);
	if (!connection || (dbus_error_is_set(&error_dbus)))
		return false;
	if (s_comparison_connection != connection)
	{
		dbus_connection_set_exit_on_disconnect(connection, false);
		s_cookie = 0;
		s_comparison_connection = connection;
	}
	message = dbus_message_new_method_call("org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", bus_method);
	if (!message)
		return false;
	// Initialize an append iterator for the message, gets freed with the message.
	dbus_message_iter_init_append(message, &message_itr);
	if (inhibit_requested)
	{
		// Guard against repeat inhibitions which would add extra inhibitors each generating a different cookie.
		if (s_cookie)
			return false;
		// Append process/window name.
		if (!dbus_message_iter_append_basic(&message_itr, DBUS_TYPE_STRING, &program_name))
			return false;
		// Append reason for inhibiting the screensaver.
		if (!dbus_message_iter_append_basic(&message_itr, DBUS_TYPE_STRING, &reason))
			return false;
	}
	else
	{
		// Only Append the cookie.
		if (!dbus_message_iter_append_basic(&message_itr, DBUS_TYPE_UINT32, &s_cookie))
			return false;
	}
	// Send message and get response.
	response = dbus_connection_send_with_reply_and_block(connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error_dbus);
	if (!response || dbus_error_is_set(&error_dbus))
		return false;
	s_cookie = 0;
	if (inhibit_requested)
	{
		// Get the cookie from the response message.
		if (!dbus_message_get_args(response, &error_dbus, DBUS_TYPE_UINT32, &s_cookie, DBUS_TYPE_INVALID) || dbus_error_is_set(&error_dbus))
			return false;
	}
	return true;
}

#endif

#if !defined(DBUS_API) && defined(X11_API)

static bool SetScreensaverInhibitX11(const WindowInfo& wi, bool inhibit)
{
	extern char** environ;

	const char* command = "xdg-screensaver";
	const char* operation = inhibit ? "suspend" : "resume";
	std::string id = fmt::format("0x{:X}", static_cast<u64>(reinterpret_cast<uintptr_t>(wi.window_handle)));

	char* argv[4] = {const_cast<char*>(command), const_cast<char*>(operation), const_cast<char*>(id.c_str()),
		nullptr};

	// Since we set SA_NOCLDWAIT in Qt, we don't need to wait here.
	pid_t pid;
	int res = posix_spawnp(&pid, "xdg-screensaver", nullptr, nullptr, argv, environ);
	return (res == 0);
}

static bool SetScreensaverInhibit(const WindowInfo& wi, bool inhibit)
{
	switch (wi.type)
	{
#ifdef X11_API
		case WindowInfo::Type::X11:
			return SetScreensaverInhibitX11(wi, inhibit);
#endif

		default:
			return false;
	}
}

static std::optional<WindowInfo> s_inhibit_window_info;

#endif

bool WindowInfo::InhibitScreensaver(const WindowInfo& wi, bool inhibit)
{

#ifdef DBUS_API

	return SetScreensaverInhibitDBus(inhibit, "PCSX2", "PCSX2 VM is running.");

#else

	if (s_inhibit_window_info.has_value())
	{
		// Bit of extra logic here, because wx spams it and we don't want to
		// spawn processes unnecessarily.
		if (s_inhibit_window_info->type == wi.type &&
			s_inhibit_window_info->window_handle == wi.window_handle &&
			s_inhibit_window_info->surface_handle == wi.surface_handle)
		{
			return true;
		}
		// Clear the old.
		SetScreensaverInhibit(s_inhibit_window_info.value(), false);
		s_inhibit_window_info.reset();
	}

	if (!inhibit)
		return true;

	// New window.
	if (!SetScreensaverInhibit(wi, true))
		return false;

	s_inhibit_window_info = wi;
	return true;

#endif

}

bool Common::PlaySoundAsync(const char* path)
{
#ifdef __linux__
	// This is... pretty awful. But I can't think of a better way without linking to e.g. gstreamer.
	const char* cmdname = "aplay";
	const char* argv[] = {cmdname, path, nullptr};
	pid_t pid;

	// Since we set SA_NOCLDWAIT in Qt, we don't need to wait here.
	int res = posix_spawnp(&pid, cmdname, nullptr, nullptr, const_cast<char**>(argv), environ);
	return (res == 0);
#else
	return false;
#endif
}

void Threading::Sleep(int ms)
{
	usleep(1000 * ms);
}

void Threading::SleepUntil(u64 ticks)
{
	struct timespec ts;
	ts.tv_sec = static_cast<time_t>(ticks / 1000000000ULL);
	ts.tv_nsec = static_cast<long>(ticks % 1000000000ULL);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr);
}

#endif
