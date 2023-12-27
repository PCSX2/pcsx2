// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "audiodev.h"
#include <cstring>

namespace usb_mic
{
	namespace audiodev_noop
	{
		class NoopAudioDevice : public AudioDevice
		{
		public:
			NoopAudioDevice(
				u32 port, AudioDir dir, u32 channels)
				: AudioDevice(port, dir, channels)
			{
			}
			~NoopAudioDevice() override {}
			bool Start() override
			{
				return true;
			}
			void Stop() override {}
			bool GetFrames(uint32_t* size) override { return true; }
			uint32_t GetBuffer(int16_t* outBuf, uint32_t outFrames) override
			{
				std::memset(outBuf, 0, outFrames * sizeof(int16_t));
				return outFrames;
			}
			uint32_t SetBuffer(int16_t* inBuf, uint32_t inFrames) override { return inFrames; }
			void SetResampling(int samplerate) override {}
		};
	} // namespace audiodev_noop
} // namespace usb_mic