// SPDX-FileCopyrightText: 2019-2024 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioStream.h"
#include "Host.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/ScopedGuard.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"

#include "cubeb/cubeb.h"
#include "fmt/format.h"
#include "IconsFontAwesome5.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <objbase.h>
#endif

namespace
{
	class CubebAudioStream : public AudioStream
	{
	public:
		CubebAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
		~CubebAudioStream();

		void SetPaused(bool paused) override;

		bool Initialize(const char* driver_name, const char* device_name, bool stretch_enabled, Error* error);

	private:
		static void LogCallback(const char* fmt, ...);
		static long DataCallback(cubeb_stream* stm, void* user_ptr, const void* input_buffer, void* output_buffer,
			long nframes);
		static void StateCallback(cubeb_stream* stream, void* user_ptr, cubeb_state state);

		void DestroyContextAndStream();

		cubeb* m_context = nullptr;
		cubeb_stream* stream = nullptr;
	};
} // namespace

static TinyString GetCubebErrorString(int rv)
{
	TinyString ret;
	switch (rv)
	{
		// clang-format off
#define C(e) case e: ret.assign(#e); break
		// clang-format on

		C(CUBEB_OK);
		C(CUBEB_ERROR);
		C(CUBEB_ERROR_INVALID_FORMAT);
		C(CUBEB_ERROR_INVALID_PARAMETER);
		C(CUBEB_ERROR_NOT_SUPPORTED);
		C(CUBEB_ERROR_DEVICE_UNAVAILABLE);

		default:
			return "CUBEB_ERROR_UNKNOWN";

#undef C
	}

	ret.append_format(" ({})", rv);
	return ret;
}

CubebAudioStream::CubebAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
	: AudioStream(sample_rate, parameters)
{
}

CubebAudioStream::~CubebAudioStream()
{
	DestroyContextAndStream();
}

void CubebAudioStream::LogCallback(const char* fmt, ...)
{
	SmallString str;
	std::va_list ap;
	va_start(ap, fmt);
	str.vsprintf(fmt, ap);
	va_end(ap);
	DEV_LOG(str);
}

void CubebAudioStream::DestroyContextAndStream()
{
	if (stream)
	{
		cubeb_stream_stop(stream);
		cubeb_stream_destroy(stream);
		stream = nullptr;
	}

	if (m_context)
	{
		cubeb_destroy(m_context);
		m_context = nullptr;
	}
}

bool CubebAudioStream::Initialize(const char* driver_name, const char* device_name, bool stretch_enabled, Error* error)
{
	cubeb_set_log_callback(CUBEB_LOG_NORMAL, LogCallback);

	int rv = cubeb_init(&m_context, "PCSX2", (driver_name && *driver_name) ? driver_name : nullptr);
	if (rv != CUBEB_OK)
	{
		Error::SetStringFmt(error, "Could not initialize cubeb context: {}", GetCubebErrorString(rv));
		return false;
	}

	static constexpr const std::array<std::pair<cubeb_channel_layout, SampleReader>,
		static_cast<size_t>(AudioExpansionMode::Count)>
		channel_setups = {{
			// Disabled
			{CUBEB_LAYOUT_STEREO, StereoSampleReaderImpl},
			// StereoLFE
			{CUBEB_LAYOUT_STEREO_LFE, &SampleReaderImpl<AudioExpansionMode::StereoLFE, READ_CHANNEL_FRONT_LEFT,
										  READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_LFE>},
			// Quadraphonic
			{CUBEB_LAYOUT_QUAD, &SampleReaderImpl<AudioExpansionMode::Quadraphonic, READ_CHANNEL_FRONT_LEFT,
									READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>},
			// QuadraphonicLFE
			{CUBEB_LAYOUT_QUAD_LFE,
				&SampleReaderImpl<AudioExpansionMode::QuadraphonicLFE, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
					READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>},
			// Surround51
			{CUBEB_LAYOUT_3F2_LFE_BACK,
				&SampleReaderImpl<AudioExpansionMode::Surround51, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
					READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>},
			// Surround71
			{CUBEB_LAYOUT_3F4_LFE,
				&SampleReaderImpl<AudioExpansionMode::Surround71, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
					READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT,
					READ_CHANNEL_SIDE_LEFT, READ_CHANNEL_SIDE_RIGHT>},
		}};

	cubeb_stream_params params = {};
	params.format = CUBEB_SAMPLE_S16LE;
	params.rate = m_sample_rate;
	params.channels = m_output_channels;
	params.layout = channel_setups[static_cast<size_t>(m_parameters.expansion_mode)].first;
	params.prefs = CUBEB_STREAM_PREF_NONE;

	u32 latency_frames = GetBufferSizeForMS(
		m_sample_rate, (m_parameters.minimal_output_latency) ? m_parameters.buffer_ms : m_parameters.output_latency_ms);
	u32 min_latency_frames = 0;
	rv = cubeb_get_min_latency(m_context, &params, &min_latency_frames);
	if (rv == CUBEB_ERROR_NOT_SUPPORTED)
	{
		DEV_LOG("Cubeb backend does not support latency queries, using latency of {} ms ({} frames).",
			m_parameters.buffer_ms, latency_frames);
	}
	else
	{
		if (rv != CUBEB_OK)
		{
			Error::SetStringFmt(error, "cubeb_get_min_latency() failed: {}", GetCubebErrorString(rv));
			DestroyContextAndStream();
			return false;
		}

		const u32 minimum_latency_ms = GetMSForBufferSize(m_sample_rate, min_latency_frames);
		DEV_LOG("Minimum latency: {} ms ({} audio frames)", minimum_latency_ms, min_latency_frames);
		if (m_parameters.minimal_output_latency)
		{
			// use minimum
			latency_frames = min_latency_frames;
		}
		else if (minimum_latency_ms > m_parameters.output_latency_ms)
		{
			WARNING_LOG("Minimum latency is above requested latency: {} vs {}, adjusting to compensate.",
				min_latency_frames, latency_frames);
			latency_frames = min_latency_frames;
		}
	}

	cubeb_devid selected_device = nullptr;
	cubeb_device_collection devices;
	bool devices_valid = false;
	if (device_name && *device_name)
	{
		rv = cubeb_enumerate_devices(m_context, CUBEB_DEVICE_TYPE_OUTPUT, &devices);
		devices_valid = (rv == CUBEB_OK);
		if (rv == CUBEB_OK)
		{
			for (size_t i = 0; i < devices.count; i++)
			{
				const cubeb_device_info& di = devices.device[i];
				if (di.device_id && std::strcmp(device_name, di.device_id) == 0)
				{
					INFO_LOG("Using output device '{}' ({}).", di.device_id,
						di.friendly_name ? di.friendly_name : di.device_id);
					selected_device = di.devid;
					break;
				}
			}

			if (!selected_device)
			{
				Host::AddIconOSDMessage("AudioDeviceUnavailable", ICON_FA_VOLUME_UP,
					fmt::format("Requested audio output device '{}' not found, using default.", device_name),
					Host::OSD_ERROR_DURATION);
			}
		}
		else
		{
			WARNING_LOG("cubeb_enumerate_devices() returned {}, using default device.", GetCubebErrorString(rv));
		}
	}

	BaseInitialize(channel_setups[static_cast<size_t>(m_parameters.expansion_mode)].second, stretch_enabled);

	char stream_name[32];
	std::snprintf(stream_name, sizeof(stream_name), "%p", this);

	rv = cubeb_stream_init(m_context, &stream, stream_name, nullptr, nullptr, selected_device, &params, latency_frames,
		&CubebAudioStream::DataCallback, StateCallback, this);

	if (devices_valid)
		cubeb_device_collection_destroy(m_context, &devices);

	if (rv != CUBEB_OK)
	{
		Error::SetStringFmt(error, "cubeb_stream_init() failed: {}", GetCubebErrorString(rv));
		DestroyContextAndStream();
		return false;
	}

	rv = cubeb_stream_start(stream);
	if (rv != CUBEB_OK)
	{
		Error::SetStringFmt(error, "cubeb_stream_start() failed: {}", GetCubebErrorString(rv));
		DestroyContextAndStream();
		return false;
	}

	return true;
}

void CubebAudioStream::StateCallback(cubeb_stream* stream, void* user_ptr, cubeb_state state)
{
	// noop
}

long CubebAudioStream::DataCallback(cubeb_stream* stm, void* user_ptr, const void* input_buffer, void* output_buffer,
	long nframes)
{
	static_cast<CubebAudioStream*>(user_ptr)->ReadFrames(static_cast<s16*>(output_buffer), static_cast<u32>(nframes));
	return nframes;
}

void CubebAudioStream::SetPaused(bool paused)
{
	if (paused == m_paused || !stream)
		return;

	const int rv = paused ? cubeb_stream_stop(stream) : cubeb_stream_start(stream);
	if (rv != CUBEB_OK)
	{
		ERROR_LOG("Could not {} stream: {}", paused ? "pause" : "resume", GetCubebErrorString(rv));
		return;
	}

	m_paused = paused;
}

std::unique_ptr<AudioStream> AudioStream::CreateCubebAudioStream(u32 sample_rate, const AudioStreamParameters& parameters,
	const char* driver_name, const char* device_name, bool stretch_enabled, Error* error)
{
	std::unique_ptr<CubebAudioStream> stream = std::make_unique<CubebAudioStream>(sample_rate, parameters);
	if (!stream->Initialize(driver_name, device_name, stretch_enabled, error))
		stream.reset();
	return stream;
}

std::vector<std::pair<std::string, std::string>> AudioStream::GetCubebDriverNames()
{
	std::vector<std::pair<std::string, std::string>> names;
	names.emplace_back(std::string(), TRANSLATE_STR("AudioStream", "Default"));

	const char** cubeb_names = cubeb_get_backend_names();
	for (u32 i = 0; cubeb_names[i] != nullptr; i++)
		names.emplace_back(cubeb_names[i], cubeb_names[i]);

	return names;
}

std::vector<AudioStream::DeviceInfo> AudioStream::GetCubebOutputDevices(const char* driver)
{
	std::vector<AudioStream::DeviceInfo> ret;
	ret.emplace_back(std::string(), TRANSLATE_STR("AudioStream", "Default"), 0);

	cubeb* context;
	int rv = cubeb_init(&context, "PCSX2", (driver && *driver) ? driver : nullptr);
	if (rv != CUBEB_OK)
	{
		ERROR_LOG("cubeb_init() failed: {}", GetCubebErrorString(rv));
		return ret;
	}

	ScopedGuard context_cleanup([context]() { cubeb_destroy(context); });

	cubeb_device_collection devices;
	rv = cubeb_enumerate_devices(context, CUBEB_DEVICE_TYPE_OUTPUT, &devices);
	if (rv != CUBEB_OK)
	{
		ERROR_LOG("cubeb_enumerate_devices() failed: {}", GetCubebErrorString(rv));
		return ret;
	}

	ScopedGuard devices_cleanup([context, &devices]() { cubeb_device_collection_destroy(context, &devices); });

	// we need stream parameters to query latency
	cubeb_stream_params params = {};
	params.format = CUBEB_SAMPLE_S16LE;
	params.rate = 48000;
	params.channels = 2;
	params.layout = CUBEB_LAYOUT_UNDEFINED;
	params.prefs = CUBEB_STREAM_PREF_NONE;

	u32 min_latency = 0;
	cubeb_get_min_latency(context, &params, &min_latency);
	ret[0].minimum_latency_frames = min_latency;

	for (size_t i = 0; i < devices.count; i++)
	{
		const cubeb_device_info& di = devices.device[i];
		if (!di.device_id)
			continue;

		ret.emplace_back(di.device_id, di.friendly_name ? di.friendly_name : di.device_id, min_latency);
	}

	return ret;
}
