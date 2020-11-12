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
#include "audiodeviceproxy.h"

namespace usb_mic
{
	namespace audiodev_noop
	{

		static const char* APINAME = "noop";

		class NoopAudioDevice : public AudioDevice
		{
		public:
			NoopAudioDevice(int port, const char* dev_type, int mic, AudioDir dir)
				: AudioDevice(port, dev_type, mic, dir)
			{
			}
			~NoopAudioDevice() {}
			void Start() {}
			void Stop() {}
			virtual bool GetFrames(uint32_t* size)
			{
				return true;
			}
			virtual uint32_t GetBuffer(int16_t* outBuf, uint32_t outFrames)
			{
				return outFrames;
			}
			virtual uint32_t SetBuffer(int16_t* inBuf, uint32_t inFrames)
			{
				return inFrames;
			}
			virtual void SetResampling(int samplerate) {}
			virtual uint32_t GetChannels()
			{
				return 1;
			}

			virtual bool Compare(AudioDevice* compare)
			{
				return false;
			}

			static const TCHAR* Name()
			{
				return TEXT("NOOP");
			}

			static bool AudioInit()
			{
				return true;
			}

			static void AudioDeinit()
			{
			}

			static void AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir)
			{
				AudioDeviceInfo info;
				info.strID = TEXT("silence");
				info.strName = TEXT("Silence");
				devices.push_back(info);
			}

			static int Configure(int port, const char* dev_type, void* data)
			{
				return RESULT_OK;
			}
		};

	} // namespace audiodev_noop
} // namespace usb_mic
