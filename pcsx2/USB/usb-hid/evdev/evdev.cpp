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

#include "evdev.h"
#include <cassert>
#include <sstream>
#include "../hidproxy.h"
#include "../../qemu-usb/input-keymap-linux-to-qcode.h"

#ifdef USING_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
extern Display* g_GSdsp;
extern Window g_GSwin;
#endif

namespace usb_hid
{
	namespace evdev
	{

#define test_bit(nr, addr) \
	(((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1) / (sizeof(long) * 8)) + 1)

		bool FindHid(const std::string& evphys, std::string& hid_dev)
		{
			int fd;
			char buf[256];

			std::stringstream str;
			struct dirent* dp;

			DIR* dirp = opendir("/dev/input/");
			if (dirp == NULL)
			{
				Console.Warning("Error opening /dev/input/");
				return false;
			}

			while ((dp = readdir(dirp)) != NULL)
			{
				if (strncmp(dp->d_name, "hidraw", 6) == 0)
				{

					str.clear();
					str.str("");
					str << "/dev/input/" << dp->d_name;
					fd = open(str.str().c_str(), O_RDWR | O_NONBLOCK);

					if (fd < 0)
					{
						Console.Warning("Unable to open device");
						continue;
					}

					memset(buf, 0x0, sizeof(buf));
					//res = ioctl(fd, HIDIOCGRAWNAME(256), buf);

					/*			res = ioctl(fd, HIDIOCGRAWPHYS(256), buf);
			if (res < 0)
				Console.Warning("HIDIOCGRAWPHYS");
			else
			*/
					close(fd);
					if (evphys == buf)
					{
						closedir(dirp);
						hid_dev = str.str();
						return true;
					}
				}
			}
		//quit:
			closedir(dirp);
			return false;
		}

		int EvDev::TokenOut(const uint8_t* data, int len)
		{
			return len;
		}

		int EvDev::Open()
		{
			// Make sure there is atleast two types so we won't go beyond array length
			assert((int)HIDTYPE_MOUSE == 1);
			std::stringstream name;

			mHandle = -1;

			std::string path;
			if (!LoadSetting(mDevType, mPort, APINAME, N_DEVICE, path))
			{
				return 1;
			}

			if (path.empty() || !file_exists(path))
				goto quit;

			if ((mHandle = open(path.c_str(), O_RDWR | O_NONBLOCK)) < 0)
			{
				goto quit;
			}

			if (!mReaderThreadIsRunning)
			{
				if (mReaderThread.joinable())
					mReaderThread.join();
				mReaderThread = std::thread(&EvDev::ReaderThread, this);
			}
			return 0;

		quit:
			Close();
			return 1;
		}

		int EvDev::Close()
		{
			if (mHandle != -1)
				close(mHandle);

			mHandle = -1;
			return 0;
		}

		void EvDev::ReaderThread()
		{
			ssize_t len;
			input_event events[32];

			mReaderThreadIsRunning = true;

			while (mHandle != -1)
			{
				//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
				while ((len = read(mHandle, &events, sizeof(events))) > -1)
				{
					InputEvent ev{};
					len /= sizeof(events[0]);
					for (int i = 0; i < len; i++)
					{
						input_event& event = events[i];
						switch (event.type)
						{
							case EV_ABS:
							{

								if (mHIDState->kind == HID_MOUSE || mHIDState->kind == HID_KEYBOARD) // usually mouse position is expected to be relative
									continue;

								if (!mHIDState->ptr.eh_entry)
									continue;

								ev.type = INPUT_EVENT_KIND_ABS;
								if (event.code == ABS_X)
								{
									ev.u.abs.axis = INPUT_AXIS_X;
									ev.u.abs.value = event.value;
									mHIDState->ptr.eh_entry(mHIDState, &ev);
								}
								else if (event.code == ABS_Y)
								{
									ev.u.abs.axis = INPUT_AXIS_Y;
									ev.u.abs.value = event.value;
									mHIDState->ptr.eh_entry(mHIDState, &ev);
								}
							}
							break;
							case EV_REL:
							{
								if (mHIDState->kind == HID_KEYBOARD || !mHIDState->ptr.eh_entry)
									continue;

								ev.type = INPUT_EVENT_KIND_REL;
								ev.u.rel.value = event.value;
								if (event.code == ABS_X)
								{
									ev.u.rel.axis = INPUT_AXIS_X;
									mHIDState->ptr.eh_entry(mHIDState, &ev);
								}
								else if (event.code == ABS_Y)
								{
									ev.u.rel.axis = INPUT_AXIS_Y;
									mHIDState->ptr.eh_entry(mHIDState, &ev);
								}
							}
							break;
							case EV_KEY:
							{

#ifdef USING_X11 //FIXME not thread-safe
								if (event.code == KEY_LEFTSHIFT || event.code == KEY_RIGHTSHIFT)
									shift = (event.value > 0);

								if (event.code == KEY_F12 && (event.value == 1) && shift)
								{
									if (!grabbed)
									{
										grabbed = true;
										XGrabPointer(g_GSdsp, g_GSwin, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, g_GSwin, None, CurrentTime);
										XGrabKeyboard(g_GSdsp, g_GSwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
										// Hides globally :(
										XFixesHideCursor(g_GSdsp, g_GSwin);
									}
									else
									{
										grabbed = false;
										XUngrabPointer(g_GSdsp, CurrentTime);
										XUngrabKeyboard(g_GSdsp, CurrentTime);
										XFixesShowCursor(g_GSdsp, g_GSwin);
									}
								}
#endif

								if (mHIDState->kind == HID_KEYBOARD && mHIDState->kbd.eh_entry)
								{

									QKeyCode qcode = Q_KEY_CODE_UNMAPPED;
									if (event.code < (uint16_t)qemu_input_map_linux_to_qcode_len)
										qcode = qemu_input_map_linux_to_qcode[event.code];

									if (event.value < 2)
									{
										ev.type = INPUT_EVENT_KIND_KEY;
										ev.u.key.down = (event.value == 1); // 2 if repeat
										ev.u.key.key.type = KEY_VALUE_KIND_QCODE;
										ev.u.key.key.u.qcode = qcode;

										mHIDState->kbd.eh_entry(mHIDState, &ev);
									}
								}

								if (mHIDState->kind != HID_KEYBOARD && mHIDState->ptr.eh_entry)
								{
									ev.type = INPUT_EVENT_KIND_BTN;
									switch (event.code)
									{
										case BTN_LEFT:
											ev.u.btn.button = INPUT_BUTTON_LEFT;
											ev.u.btn.down = (event.value == 1);
											mHIDState->ptr.eh_entry(mHIDState, &ev);
											break;
										case BTN_RIGHT:
											ev.u.btn.button = INPUT_BUTTON_RIGHT;
											ev.u.btn.down = (event.value == 1);
											mHIDState->ptr.eh_entry(mHIDState, &ev);
											break;
										case BTN_MIDDLE:
											ev.u.btn.button = INPUT_BUTTON_MIDDLE;
											ev.u.btn.down = (event.value == 1);
											mHIDState->ptr.eh_entry(mHIDState, &ev);
											break;
										default:
											break;
									}
								}
							}
							break;
							case EV_SYN: //TODO useful?
							{
								switch (event.code)
								{
									case SYN_REPORT:
										if (mHIDState->ptr.eh_sync) //TODO sync here?
											mHIDState->ptr.eh_sync(mHIDState);
										break;
									case SYN_DROPPED:
										//restore last good state
										break;
								}
							}
							break;
							default:
								break;
						}
					}

					if (len < (ssize_t)sizeof(input_event) && errno != EAGAIN)
					{
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			mReaderThreadIsRunning = false;
		}

	} // namespace evdev
} // namespace usb_hid
