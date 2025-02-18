// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Pcsx2Types.h"
#include "common/Console.h"
#include "common/HostSys.h"
#include "common/Path.h"
#include "common/ScopedGuard.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/WindowInfo.h"

#include "fmt/format.h"

#include <dbus/dbus.h>
#include <spawn.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <ctype.h>
#include <optional>
#include <thread>

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
	FILE* file = fopen("/etc/os-release", "r");
	if (file)
	{
		char line[256];
		std::string distro;
		std::string version = "";
		while (fgets(line, sizeof(line), file))
		{
			std::string_view line_view(line);
			if (line_view.starts_with("NAME="))
			{
				distro = line_view.substr(5, line_view.size() - 6);
			}
			else if (line_view.starts_with("BUILD_ID="))
			{
				version = line_view.substr(9, line_view.size() - 10);
			}
			else if (line_view.starts_with("VERSION_ID="))
			{
				version = line_view.substr(11, line_view.size() - 12);
			}
		}
		fclose(file);

		// Some distros put quotes around the name and or version.
		if (distro.starts_with("\"") && distro.ends_with("\""))
			distro = distro.substr(1, distro.size() - 2);

		if (version.starts_with("\"") && version.ends_with("\""))
					version = version.substr(1, version.size() - 2);

		if (!distro.empty() && !version.empty())
			return fmt::format("{} {}", distro, version);
	}

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
	char* desktop_session = nullptr;

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

	desktop_session = std::getenv("DESKTOP_SESSION");
	if (desktop_session && std::strncmp(desktop_session, "mate", 4) == 0)
	{
		message = dbus_message_new_method_call("org.mate.ScreenSaver", "/org/mate/ScreenSaver", "org.mate.ScreenSaver", bus_method);
	}
	else
	{
		message = dbus_message_new_method_call("org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver", "org.freedesktop.ScreenSaver", bus_method);
	}

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

bool Common::InhibitScreensaver(bool inhibit)
{
	return SetScreensaverInhibitDBus(inhibit, "PCSX2", "PCSX2 VM is running.");
}

void Common::SetMousePosition(int x, int y)
{
	Display* display = XOpenDisplay(nullptr);
	if (!display)
		return;

	Window root = DefaultRootWindow(display);
	XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
	XFlush(display);

	XCloseDisplay(display);
}

static std::function<void(int, int)> fnMouseMoveCb;
static std::atomic<bool> trackingMouse = false;
static std::thread mouseThread;

void mouseEventLoop()
{
	Threading::SetNameOfCurrentThread("X11 Mouse Thread");
	Display* display = XOpenDisplay(nullptr);
	if (!display)
	{
		return;
	}

	int opcode, eventcode, error;
	if (!XQueryExtension(display, "XInputExtension", &opcode, &eventcode, &error))
	{
		XCloseDisplay(display);
		return;
	}

	const Window root = DefaultRootWindow(display);
	XIEventMask evmask;
	unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};

	evmask.deviceid = XIAllDevices;
	evmask.mask_len = sizeof(mask);
	evmask.mask = mask;
	XISetMask(mask, XI_RawMotion);

	XISelectEvents(display, root, &evmask, 1);
	XSync(display, False);

	XEvent event;
	while (trackingMouse)
	{
		// XNextEvent is blocking, this is a zombie process risk if no events arrive
		// while we are trying to shutdown.
		// https://nrk.neocities.org/articles/x11-timeout-with-xsyncalarm might be
		// a better solution than using XPending.
		if (!XPending(display))
		{
			Threading::Sleep(1);
			Threading::SpinWait();
			continue;
		}

		XNextEvent(display, &event);
		if (event.xcookie.type == GenericEvent &&
			event.xcookie.extension == opcode &&
			XGetEventData(display, &event.xcookie))
		{
			XIRawEvent* raw_event = reinterpret_cast<XIRawEvent*>(event.xcookie.data);
			if (raw_event->evtype == XI_RawMotion)
			{
				Window w;
				int root_x, root_y, win_x, win_y;
				unsigned int mask;
				XQueryPointer(display, root, &w, &w, &root_x, &root_y, &win_x, &win_y, &mask);

				if (fnMouseMoveCb)
					fnMouseMoveCb(root_x, root_y);
			}
			XFreeEventData(display, &event.xcookie);
		}
	}

	XCloseDisplay(display);
}

bool Common::AttachMousePositionCb(std::function<void(int, int)> cb)
{
	fnMouseMoveCb = cb;

	if (trackingMouse)
		return true;

	trackingMouse = true;
	mouseThread = std::thread(mouseEventLoop);
	mouseThread.detach();
	return true;
}

void Common::DetachMousePositionCb()
{
	trackingMouse = false;
	fnMouseMoveCb = nullptr;
	if (mouseThread.joinable())
	{
		mouseThread.join();
	}
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
	if (res == 0)
		return true;

	// Try gst-play-1.0.
	const char* gst_play_cmdname = "gst-play-1.0";
	const char* gst_play_argv[] = {cmdname, path, nullptr};
	res = posix_spawnp(&pid, gst_play_cmdname, nullptr, nullptr, const_cast<char**>(gst_play_argv), environ);
	if (res == 0)
		return true;

	// gst-launch? Bit messier for sure.
	TinyString location_str = TinyString::from_format("location={}", path);
	TinyString parse_str = TinyString::from_format("{}parse", Path::GetExtension(path));
	const char* gst_launch_cmdname = "gst-launch-1.0";
	const char* gst_launch_argv[] = {
		gst_launch_cmdname, "filesrc", location_str.c_str(), "!", parse_str.c_str(), "!", "alsasink", nullptr};
	res = posix_spawnp(&pid, gst_launch_cmdname, nullptr, nullptr, const_cast<char**>(gst_launch_argv), environ);
	if (res == 0)
		return true;

	Console.ErrorFmt("Failed to play sound effect {}. Make sure you have aplay, gst-play-1.0, or gst-launch-1.0 available.", path);
	return false;
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
