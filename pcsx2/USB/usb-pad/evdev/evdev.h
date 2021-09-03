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

#pragma once
#include "evdev-ff.h"
#include "shared.h"
#include "USB/linux/util.h"
#include "USB/readerwriterqueue/readerwriterqueue.h"
#include "common/Console.h"
//#include <dirent.h> //gtk.h pulls in?
#include <thread>
#include <array>
#include <atomic>

namespace usb_pad
{
	namespace evdev
	{

#define test_bit(nr, addr) \
	(((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1) / (sizeof(long) * 8)) + 1)

		void EnumerateDevices(device_list& list);

		static constexpr const char* APINAME = "evdev";

		class EvDevPad : public Pad
		{
		public:
			EvDevPad(int port, const char* dev_type)
				: Pad(port, dev_type)
				, mWriterThreadIsRunning(false)
			{
			}

			~EvDevPad() { Close(); }
			int Open();
			int Close();
			int TokenIn(uint8_t* buf, int len);
			int TokenOut(const uint8_t* data, int len);
			int Reset() { return 0; }

			static const TCHAR* Name()
			{
				return "Evdev";
			}

			static int Configure(int port, const char* dev_type, void* data);

		protected:
			void PollAxesValues(const device_data& device);
			void SetAxis(const device_data& device, int code, int value);
			void WriterThread();

			int mHidHandle = -1;
			EvdevFF* mEvdevFF = nullptr;
			struct wheel_data_t mWheelData
			{
			};
			std::vector<device_data> mDevices;
			int32_t mUseRawFF = 0;
			std::thread mWriterThread;
			std::atomic<bool> mWriterThreadIsRunning;
			moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 8>, 32> mFFData;
		};

		template <size_t _Size>
		bool GetEvdevName(const std::string& path, char (&name)[_Size])
		{
			int fd = 0;
			if ((fd = open(path.c_str(), O_RDONLY)) < 0)
			{
				Console.Warning("Cannot open %s\n", path.c_str());
			}
			else
			{
				if (ioctl(fd, EVIOCGNAME(_Size), name) < -1)
				{
					Console.Warning("Cannot get controller's name\n");
					close(fd);
					return false;
				}
				close(fd);
				return true;
			}
			return false;
		}

		static void CalcAxisCorr(axis_correct& abs_correct, struct input_absinfo absinfo)
		{
			int t;
			// convert values into 16 bit range
			if (absinfo.minimum == absinfo.maximum)
			{
				abs_correct.used = 0;
			}
			else
			{
				abs_correct.used = 1;
				abs_correct.coef[0] =
					(absinfo.maximum + absinfo.minimum) - 2 * absinfo.flat;
				abs_correct.coef[1] =
					(absinfo.maximum + absinfo.minimum) + 2 * absinfo.flat;
				t = ((absinfo.maximum - absinfo.minimum) - 4 * absinfo.flat);
				if (t != 0)
				{
					abs_correct.coef[2] =
						(1 << 28) / t;
				}
				else
				{
					abs_correct.coef[2] = 0;
				}
			}
		}

		// SDL2
		// convert values into 16 bit range
		static int AxisCorrect(const axis_correct& correct, int value)
		{
			if (correct.used)
			{
				value *= 2;
				if (value > correct.coef[0])
				{
					if (value < correct.coef[1])
					{
						return 0;
					}
					value -= correct.coef[1];
				}
				else
				{
					value -= correct.coef[0];
				}
				value *= correct.coef[2];
				value >>= 13;
			}

			/* Clamp and return */
			if (value < -32768)
				return -32768;
			if (value > 32767)
				return 32767;

			return value;
		}

	} // namespace evdev
} // namespace usb_pad
