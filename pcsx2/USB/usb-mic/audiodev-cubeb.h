/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "USB/shared/ringbuffer.h"
#include "USB/USB.h"
#include "audiodev.h"

#include <mutex>
#include <string>
#include <vector>

struct cubeb;
struct cubeb_stream;

namespace usb_mic
{
	namespace audiodev_cubeb
	{
		class CubebAudioDevice final : public AudioDevice
		{
		public:
			CubebAudioDevice(u32 port, AudioDir dir, u32 channels, std::string devname, s32 latency);
			~CubebAudioDevice();

			static std::vector<std::pair<std::string, std::string>> GetDeviceList(bool input);

			uint32_t GetBuffer(short* buff, uint32_t frames) override;
			uint32_t SetBuffer(short* buff, uint32_t frames) override;
			bool GetFrames(uint32_t* size) override;
			void SetResampling(int samplerate) override;
			bool Start() override;
			void Stop() override;

		protected:
			void ResetBuffers();

			static long DataCallback(struct cubeb_stream* stream, void* user_ptr, void const* input_buffer,
				void* output_buffer, long nframes);

			u32 mSampleRate = 48000;
			u32 mLatency = 50;
			u32 mStreamLatency = 0;
			cubeb* mContext;
			cubeb_stream* mStream = nullptr;
			std::string mDeviceName;
			const void* mDeviceId;

			RingBuffer mBuffer;
			std::mutex mMutex;
		};
	} // namespace audiodev_cubeb
} // namespace usb_mic
