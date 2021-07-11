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

#ifndef USBPADRAW_H
#define USBPADRAW_H
#include <thread>
#include <array>
#include <atomic>
#include "USB/usb-pad/padproxy.h"
#include "USB/usb-pad/usb-pad.h"
#include "USB/shared/rawinput_usb.h"
#include "USB/readerwriterqueue/readerwriterqueue.h"

namespace usb_pad
{
	namespace raw
	{

		static const char* APINAME = "rawinput";

		class RawInputPad : public Pad, shared::rawinput::ParseRawInputCB
		{
		public:
			RawInputPad(int port, const char* dev_type)
				: Pad(port, dev_type)
				, mDoPassthrough(false)
				, mUsbHandle(INVALID_HANDLE_VALUE)
				, mWriterThreadIsRunning(false)
				, mReaderThreadIsRunning(false)
			{
				if (!InitHid())
					throw PadError("InitHid() failed!");
			}
			~RawInputPad()
			{
				Close();
				if (mWriterThread.joinable())
					mWriterThread.join();
			}
			int Open();
			int Close();
			int TokenIn(uint8_t* buf, int len);
			int TokenOut(const uint8_t* data, int len);
			int Reset()
			{
				uint8_t reset[7] = {0};
				reset[0] = 0xF3; //stop forces
				TokenOut(reset, sizeof(reset));
				return 0;
			}
			void ParseRawInput(PRAWINPUT pRawInput);

			static const TCHAR* Name()
			{
				return TEXT("Raw Input");
			}

			static int Configure(int port, const char* dev_type, void* data);

		protected:
			static void WriterThread(void* ptr);
			static void ReaderThread(void* ptr);
			HIDP_CAPS mCaps;
			HANDLE mUsbHandle = (HANDLE)-1;
			OVERLAPPED mOLRead;
			OVERLAPPED mOLWrite;

			int32_t mDoPassthrough;
			wheel_data_t mDataCopy;
			std::thread mWriterThread;
			std::thread mReaderThread;
			std::atomic<bool> mWriterThreadIsRunning;
			std::atomic<bool> mReaderThreadIsRunning;
			moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 8>, 32> mFFData;
			moodycamel::BlockingReaderWriterQueue<std::array<uint8_t, 32>, 16> mReportData; //TODO 32 is random
		};

/*
Layout:
	0x8000 bit means it is a valid mapping,
	where value is PS2 button/axis and 
	array (Mappings::btnMap etc.) index is physical button/axis
	(reversed for keyboard mappings).
	[31..16] bits player 2 mapping
	[15..0]  bits player 1 mapping
*/
//Maybe getting too convoluted
//Check for which player(s) the mapping is for
//Using MSB (right? :P) to indicate if valid mapping
#define PLY_IS_MAPPED(p, x) (x & (0x8000 << (16 * p)))
// Add owning player bits to mapping
#define PLY_SET_MAPPED(p, x) (((x & 0x7FFF) | 0x8000) << (16 * p))
#define PLY_UNSET_MAPPED(p, x) (x & ~(0xFFFF << (16 * p)))
#define PLY_GET_VALUE(p, x) ((x >> (16 * p)) & 0x7FFF)

		//Joystick: btnMap/axisMap/hatMap[physical button] = ps2 button
		//Keyboard: btnMap/axisMap/hatMap[ps2 button] = vkey
		struct Mappings
		{
			uint32_t btnMap[MAX_BUTTONS];
			uint32_t axisMap[MAX_AXES];
			uint32_t hatMap[8];
			wheel_data_t data[2];
			std::wstring devName;
#if _WIN32
			std::wstring hidPath;
#endif
		};

		struct RawDlgConfig
		{
			int port;
			const char* dev_type;
			std::wstring player_joys[2];
			int32_t pt[2]{};
			RawDlgConfig(int p, const char* dev_type_)
				: port(p)
				, dev_type(dev_type_)
			{
			}
		};

		typedef std::vector<Mappings> MapVector;
		static MapVector mapVector;
		static std::map<HANDLE, Mappings*> mappings;

		void LoadMappings(const char* dev_type, MapVector& maps);
		void SaveMappings(const char* dev_type, MapVector& maps);

	} // namespace raw
} // namespace usb_pad
#endif
