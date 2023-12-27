// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#if !defined(_WIN32) && !defined(__APPLE__)

#include "common/Pcsx2Types.h"
#include "common/HostSys.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

#include "fmt/core.h"

#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <optional>
#include <spawn.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dbus/dbus.h>

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

bool WindowInfo::InhibitScreensaver(const WindowInfo& wi, bool inhibit)
{
	return SetScreensaverInhibitDBus(inhibit, "PCSX2", "PCSX2 VM is running.");
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
