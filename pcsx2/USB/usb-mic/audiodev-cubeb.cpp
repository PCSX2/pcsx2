/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "USB/usb-mic/audiodev-cubeb.h"
#include "USB/USB.h"
#include "Host.h"
#include "common/Assertions.h"
#include "common/Console.h"

#include "cubeb/cubeb.h"
#include "fmt/format.h"

// Since the context gets used to populate the device list, that unfortunately means
// we need locking around it, since the UI thread's gonna be saying hi. The settings
// callbacks don't actually modify the context itself, though, only look at the
// device list.
static cubeb* s_cubeb_context;
static cubeb_device_collection s_cubeb_input_devices;
static cubeb_device_collection s_cubeb_output_devices;
static u32 s_cubeb_refcount = 0;
static std::mutex s_cubeb_context_mutex;

static cubeb* GetCubebContext(const char* backend = nullptr)
{
	std::unique_lock lock(s_cubeb_context_mutex);

	if (!s_cubeb_context)
	{
		pxAssert(s_cubeb_refcount == 0);
		const int res = cubeb_init(&s_cubeb_context, "PCSX2_USB", backend);
		if (res != CUBEB_OK)
		{
			Console.Error("cubeb_init() failed: %d", res);
			return nullptr;
		}

		cubeb_enumerate_devices(s_cubeb_context, CUBEB_DEVICE_TYPE_INPUT, &s_cubeb_input_devices);
		cubeb_enumerate_devices(s_cubeb_context, CUBEB_DEVICE_TYPE_OUTPUT, &s_cubeb_output_devices);
	}

	if (s_cubeb_context)
		s_cubeb_refcount++;

	return s_cubeb_context;
}

static void ReleaseCubebContext()
{
	std::unique_lock lock(s_cubeb_context_mutex);

	pxAssert(s_cubeb_refcount > 0);
	if ((--s_cubeb_refcount) == 0)
	{
		cubeb_device_collection_destroy(s_cubeb_context, &s_cubeb_input_devices);
		s_cubeb_input_devices = {};
		cubeb_device_collection_destroy(s_cubeb_context, &s_cubeb_output_devices);
		s_cubeb_output_devices = {};

		cubeb_destroy(s_cubeb_context);
		s_cubeb_context = nullptr;
	}
}

static cubeb_devid FindCubebDevice(const char* devname, bool input)
{
	if (std::strcmp(devname, "cubeb_default") == 0)
		return nullptr;

	const cubeb_device_collection& col = input ? s_cubeb_input_devices : s_cubeb_output_devices;
	for (size_t i = 0; i < col.count; i++)
	{
		if (std::strcmp(devname, col.device[i].device_id) == 0)
			return col.device[i].devid;
	}

	Console.Warning("(audiodev_cubeb) Unable to find %s device %s", input ? "input" : "output", devname);
	return nullptr;
}

static void CubebStateCallback(cubeb_stream* stream, void* user_ptr, cubeb_state state)
{
}

namespace usb_mic
{
	namespace audiodev_cubeb
	{
		CubebAudioDevice::CubebAudioDevice(u32 port, AudioDir dir, u32 channels, std::string devname, s32 latency)
			: AudioDevice(port, dir, channels)
			, mLatency(latency)
			, mDeviceName(std::move(devname))
		{
			mContext = GetCubebContext();
			mDeviceId = FindCubebDevice(mDeviceName.c_str(), (dir == AUDIODIR_SOURCE));
		}

		CubebAudioDevice::~CubebAudioDevice()
		{
			if (mStream)
				CubebAudioDevice::Stop();

			if (mContext)
				ReleaseCubebContext();
		}

		std::vector<std::pair<std::string, std::string>> CubebAudioDevice::GetDeviceList(bool input)
		{
			std::vector<std::pair<std::string, std::string>> ret;
			ret.emplace_back("", TRANSLATE_SV("USB", "Not Connected"));
			ret.emplace_back("cubeb_default", input ? TRANSLATE_SV("USB", "Default Input Device") : TRANSLATE_SV("USB", "Default Output Device"));
			if (GetCubebContext())
			{
				const cubeb_device_collection& col = input ? s_cubeb_input_devices : s_cubeb_output_devices;
				for (size_t i = 0; i < col.count; i++)
					ret.emplace_back(col.device[i].device_id, col.device[i].friendly_name);

				ReleaseCubebContext();
			}
			return ret;
		}

		bool CubebAudioDevice::Start()
		{
			if (mStream)
				Stop();

			if (!mDeviceName.empty() && mDeviceName != "cubeb_default" && !mDeviceId)
			{
				Console.Error("(audiodev_cubeb) Device '%s' is not available.", mDeviceName.c_str());
				return false;
			}

			cubeb_stream_params params;
			params.format = CUBEB_SAMPLE_S16LE;
			params.rate = mSampleRate;
			params.channels = mChannels;
			params.layout = CUBEB_LAYOUT_UNDEFINED;
			params.prefs = CUBEB_STREAM_PREF_NONE;

			// Prefer minimum latency, reduces the chance of dropped samples due to the extra buffer.
			if (cubeb_get_min_latency(mContext, &params, &mStreamLatency) != CUBEB_OK)
				mStreamLatency = (mLatency * mSampleRate) / 1000u;

			const bool input = (mAudioDir == AUDIODIR_SOURCE);
			int res = cubeb_stream_init(mContext, &mStream, fmt::format("{}", (void*)this).c_str(),
				input ? mDeviceId : nullptr, input ? &params : nullptr, input ? nullptr : mDeviceId,
				input ? nullptr : &params, mStreamLatency,
				&CubebAudioDevice::DataCallback, &CubebStateCallback, this);
			if (res != CUBEB_OK)
			{
				Console.Error("(audiodev_cubeb) cubeb_stream_init() failed: %d", res);
				return false;
			}

			ResetBuffers();

			res = cubeb_stream_start(mStream);
			if (res != CUBEB_OK)
			{
				Console.Error("(audiodev_cubeb) cubeb_stream_start() failed: %d", res);
				cubeb_stream_destroy(mStream);
				mStream = nullptr;
				return false;
			}

			return true;
		}

		void CubebAudioDevice::Stop()
		{
			if (!mStream)
				return;

			int res = cubeb_stream_stop(mStream);
			if (res != CUBEB_OK)
				Console.Error("cubeb_stream_stop() returned %d", res);

			cubeb_stream_destroy(mStream);
			mStream = nullptr;
		}

		uint32_t CubebAudioDevice::GetBuffer(short* buff, uint32_t frames)
		{
			if (!mStream)
				return frames;

			std::lock_guard<std::mutex> lk(mMutex);
			u32 samples_to_read = frames * GetChannels();
			short* pDst = (short*)buff;
			pxAssert(samples_to_read <= mBuffer.size<short>());

			while (samples_to_read > 0)
			{
				u32 samples = std::min(samples_to_read, static_cast<u32>(mBuffer.peek_read<short>()));
				if (!samples)
					break;
				memcpy(pDst, mBuffer.front(), samples * sizeof(short));
				mBuffer.read<short>(samples);
				pDst += samples;
				samples_to_read -= samples;
			}
			return (frames - (samples_to_read / GetChannels()));
		}

		uint32_t CubebAudioDevice::SetBuffer(short* buff, uint32_t frames)
		{
			if (!mStream)
				return frames;

			std::lock_guard<std::mutex> lk(mMutex);
			size_t nbytes = frames * sizeof(short) * GetChannels();
			mBuffer.write((uint8_t*)buff, nbytes);

			return frames;
		}

		bool CubebAudioDevice::GetFrames(uint32_t* size)
		{
			if (!mStream)
				return true;

			std::lock_guard<std::mutex> lk(mMutex);
			*size = mBuffer.size<short>() / GetChannels();
			return true;
		}

		void CubebAudioDevice::SetResampling(int samplerate)
		{
			const bool was_started = (mStream != nullptr);

			Stop();

			mSampleRate = samplerate;

			if (was_started)
				Start();

			ResetBuffers();
		}

		void CubebAudioDevice::ResetBuffers()
		{
			std::lock_guard<std::mutex> lk(mMutex);
			const u32 samples = std::max(((mSampleRate * mChannels) * mLatency) / 1000u, mStreamLatency * mChannels);
			mBuffer.reserve(sizeof(u16) * samples);
		}

		long CubebAudioDevice::DataCallback(
			cubeb_stream* stream, void* user_ptr, void const* input_buffer, void* output_buffer, long nframes)
		{
			CubebAudioDevice* const ad = static_cast<CubebAudioDevice*>(user_ptr);
			const size_t bytes = ad->mChannels * sizeof(short) * static_cast<size_t>(nframes);

			std::lock_guard<std::mutex> lk(ad->mMutex);
			if (ad->mAudioDir == AUDIODIR_SOURCE)
				ad->mBuffer.write((u8*)input_buffer, bytes);
			else
				ad->mBuffer.read((u8*)output_buffer, bytes);

			return nframes;
		}
	} // namespace audiodev_cubeb
} // namespace usb_mic
