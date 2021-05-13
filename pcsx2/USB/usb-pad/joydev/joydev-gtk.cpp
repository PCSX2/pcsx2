/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "joydev.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>
#include "USB/gtk.h"

namespace usb_pad
{
	namespace joydev
	{

		using sys_clock = std::chrono::system_clock;
		using ms = std::chrono::milliseconds;

#define JOYTYPE "joytype"
#define CFG "cfg"

		static bool GetEventName(const char* dev_type, int map, int event, const char** name)
		{
			static char buf[256] = {0};
			if (map < evdev::JOY_STEERING)
			{
				snprintf(buf, sizeof(buf), "Button %d", event);
			}
			else
			{
				// assuming that PS2 axes are always mapped to PC axes
				snprintf(buf, sizeof(buf), "Axis %d", event);
			}
			*name = buf;
			return true;
		}

		static bool PollInput(const std::vector<std::pair<std::string, usb_pad::evdev::ConfigMapping>>& fds, std::string& dev_name, bool isaxis, int& value, bool& inverted, int& initial)
		{
			int event_fd = -1;
			ssize_t len;
			struct js_event event;

			fd_set fdset;
			int maxfd = -1;

			FD_ZERO(&fdset);
			for (const auto& js : fds)
			{
				FD_SET(js.second.fd, &fdset);
				if (maxfd < js.second.fd)
					maxfd = js.second.fd;
			}

			inverted = false;

			// empty event queues
			for (const auto& js : fds)
				while ((len = read(js.second.fd, &event, sizeof(event))) > 0)
					;

			struct axis_value
			{
				int16_t value;
				bool initial;
			};
			axis_value axisVal[ABS_MAX + 1] = {0};

			struct timeval timeout
			{
			};
			timeout.tv_sec = 5;
			int result = select(maxfd + 1, &fdset, NULL, NULL, &timeout);

			if (!result)
				return false;

			if (result == -1)
			{
				return false;
			}

			for (const auto& js : fds)
			{
				if (FD_ISSET(js.second.fd, &fdset))
				{
					event_fd = js.second.fd;
					dev_name = js.first;
					break;
				}
			}

			if (event_fd == -1)
				return false;

			auto last = sys_clock::now();
			//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
			while (true)
			{
				auto dur = std::chrono::duration_cast<ms>(sys_clock::now() - last).count();
				if (dur > 5000)
					goto error;

				if ((len = read(event_fd, &event, sizeof(event))) > -1 && (len == sizeof(event)))
				{
					if (isaxis && event.type == JS_EVENT_AXIS)
					{
						auto& val = axisVal[event.number];

						if (!val.initial)
						{
							val.value = event.value;
							val.initial = true;
						}
						else
						{
							int diff = event.value - val.value;
							if (std::abs(diff) > 2047)
							{
								value = event.number;
								inverted = (diff < 0);
								initial = val.value;
								break;
							}
						}
					}
					else if (!isaxis && event.type == JS_EVENT_BUTTON)
					{
						if (event.value)
						{
							value = event.number;
							break;
						}
					}
				}
				else if (errno != EAGAIN)
				{
					goto error;
				}
				else
				{
					while (gtk_events_pending())
						gtk_main_iteration_do(FALSE);
					std::this_thread::sleep_for(ms(1));
				}
			}

			return true;

		error:
			return false;
		}

		int JoyDevPad::Configure(int port, const char* dev_type, void* data)
		{
			if (!strcmp(dev_type, BuzzDevice::TypeName()))
				return RESULT_CANCELED;

			evdev::ApiCallbacks apicbs{GetEventName, EnumerateDevices, PollInput};
			int ret = evdev::GtkPadConfigure(port, dev_type, "Joydev Settings", joydev::APINAME, GTK_WINDOW(data), apicbs);
			return ret;
		}

	} // namespace joydev
} // namespace usb_pad
