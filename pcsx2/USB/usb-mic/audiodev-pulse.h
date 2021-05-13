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

#include <cstdint>
#include <cstring>
#include <pulse/pulseaudio.h>
#include "USB/shared/ringbuffer.h"
#include "audiodeviceproxy.h"
#include <samplerate.h>
//#include <typeinfo>
//#include <thread>
#include <mutex>
#include <chrono>

namespace usb_mic
{
	namespace audiodev_pulse
	{

// macros for string concat
#undef APINAME_
#define APINAME_ "pulse"

		static const char* APINAME = "pulse";

		using hrc = std::chrono::high_resolution_clock;
		using ms = std::chrono::milliseconds;
		using us = std::chrono::microseconds;
		using ns = std::chrono::nanoseconds;
		using sec = std::chrono::seconds;

		class PulseAudioDevice : public AudioDevice
		{
		public:
			PulseAudioDevice(int port, const char* dev_type, int device, AudioDir dir)
				: AudioDevice(port, dev_type, device, dir)
				, mBuffering(50)
				, mPaused(true)
				, mQuit(false)
				, mPMainLoop(nullptr)
				, mPContext(nullptr)
				, mStream(nullptr)
				, mServer(nullptr)
				, mPAready(0)
				, mResampleRatio(1.0)
				, mTimeAdjust(1.0)
				, mSamplesPerSec(48000)
				, mResampler(nullptr)
				, mOutSamples(0)
			{
				int i = dir == AUDIODIR_SOURCE ? 0 : 2;
				const char* var_names[] = {
					N_AUDIO_SOURCE0,
					N_AUDIO_SOURCE1,
					N_AUDIO_SINK0,
					N_AUDIO_SINK1};

				if (!LoadSetting(mDevType, mPort, APINAME, (device ? var_names[i + 1] : var_names[i]), mDeviceName) || mDeviceName.empty())
					throw AudioDeviceError(APINAME_ ": failed to load device settings");

				LoadSetting(mDevType, mPort, APINAME, (dir == AUDIODIR_SOURCE ? N_BUFFER_LEN_SRC : N_BUFFER_LEN_SINK), mBuffering);
				mBuffering = MIN(1000, MAX(1, mBuffering));

				if (!AudioInit())
					throw AudioDeviceError(APINAME_ ": failed to bind pulseaudio library");

				mSSpec.format = PA_SAMPLE_FLOAT32LE; //PA_SAMPLE_S16LE;
				mSSpec.channels = 2;
				mSSpec.rate = 48000;

				if (!Init())
					throw AudioDeviceError(APINAME_ ": failed to init");
			}

			~PulseAudioDevice()
			{
				mQuit = true;
				std::lock_guard<std::mutex> lock(mMutex);
				Uninit();
				AudioDeinit();
				mResampler = src_delete(mResampler);
				if (file)
					fclose(file);
			}

			uint32_t GetBuffer(short* buff, uint32_t frames);
			uint32_t SetBuffer(short* buff, uint32_t frames);
			bool GetFrames(uint32_t* size);
			void SetResampling(int samplerate);
			void Start();
			void Stop();
			virtual bool Compare(AudioDevice* compare);
			void Uninit();
			bool Init();
			void ResetBuffers();

			inline uint32_t GetChannels()
			{
				return mSSpec.channels;
			}

			static const char* TypeName()
			{
				return APINAME;
			}

			static const TCHAR* Name()
			{
				return "PulseAudio";
			}

			static int Configure(int port, const char* dev_type, void* data);

			static void AudioDevices(std::vector<AudioDeviceInfo>& devices, AudioDir& dir);

			static bool AudioInit();
			static void AudioDeinit();

			static void context_state_cb(pa_context* c, void* userdata);
			static void stream_state_cb(pa_stream* s, void* userdata);
			static void stream_read_cb(pa_stream* p, size_t nbytes, void* userdata);
			static void stream_write_cb(pa_stream* p, size_t nbytes, void* userdata);
			static void stream_success_cb(pa_stream* p, int success, void* userdata) {}

		protected:
			int mChannels;
			int mBuffering;
			std::string mDeviceName;
			pa_sample_spec mSSpec;

			RingBuffer mOutBuffer;
			RingBuffer mInBuffer;
			//std::thread mThread;
			//std::condition_variable mEvent;
			std::mutex mMutex;
			bool mPaused;
			bool mQuit;
			hrc::time_point mLastGetBuffer;

			pa_threaded_mainloop* mPMainLoop;
			pa_context* mPContext;
			pa_stream* mStream;
			char* mServer; //TODO add server selector?
			int mPAready;
			double mResampleRatio;
			// Speed up or slow down audio
			double mTimeAdjust;
			int mSamplesPerSec;
			SRC_STATE* mResampler;

			int mOutSamples;
			hrc::time_point mLastOut;
			FILE* file = nullptr;
		};
	} // namespace audiodev_pulse
} // namespace usb_mic
