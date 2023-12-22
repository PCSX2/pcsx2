// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
