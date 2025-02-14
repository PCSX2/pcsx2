// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioStream.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"

#include <SDL3/SDL.h>

namespace
{
	class SDLAudioStream final : public AudioStream
	{
	public:
		SDLAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
		~SDLAudioStream();

		void SetPaused(bool paused) override;

		bool OpenDevice(bool stretch_enabled, Error* error);
		void CloseDevice();

	protected:
		__fi bool IsOpen() const { return (m_stream != nullptr); }

		static void AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);

		SDL_AudioStream* m_stream = nullptr;
	};
} // namespace

static bool InitializeSDLAudio(Error* error)
{
	static bool initialized = false;
	if (initialized)
		return true;

	// Set the name that shows up in the audio mixers on some platforms
	SDL_SetHint("SDL_AUDIO_DEVICE_APP_NAME", "PCSX2");

	// May as well keep it alive until the process exits.
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		Error::SetStringFmt(error, "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
		return false;
	}

	std::atexit([]() { SDL_QuitSubSystem(SDL_INIT_AUDIO); });

	initialized = true;
	return true;
}

SDLAudioStream::SDLAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
	: AudioStream(sample_rate, parameters)
{
}

SDLAudioStream::~SDLAudioStream()
{
	if (IsOpen())
		SDLAudioStream::CloseDevice();
}

std::unique_ptr<AudioStream> AudioStream::CreateSDLAudioStream(u32 sample_rate, const AudioStreamParameters& parameters,
	bool stretch_enabled, Error* error)
{
	if (!InitializeSDLAudio(error))
		return {};

	std::unique_ptr<SDLAudioStream> stream = std::make_unique<SDLAudioStream>(sample_rate, parameters);
	if (!stream->OpenDevice(stretch_enabled, error))
		stream.reset();

	return stream;
}

bool SDLAudioStream::OpenDevice(bool stretch_enabled, Error* error)
{
	pxAssert(!IsOpen());

	static constexpr const std::array<SampleReader, static_cast<size_t>(AudioExpansionMode::Count)> sample_readers = {{
		// Disabled
		&StereoSampleReaderImpl,
		// StereoLFE
		&SampleReaderImpl<AudioExpansionMode::StereoLFE, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_LFE>,
		// Quadraphonic
		&SampleReaderImpl<AudioExpansionMode::Quadraphonic, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		// QuadraphonicLFE
		&SampleReaderImpl<AudioExpansionMode::QuadraphonicLFE, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		// Surround51
		&SampleReaderImpl<AudioExpansionMode::Surround51, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		// Surround71
		&SampleReaderImpl<AudioExpansionMode::Surround71, READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_FRONT_CENTER, READ_CHANNEL_LFE, READ_CHANNEL_SIDE_LEFT, READ_CHANNEL_SIDE_RIGHT,
			READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
	}};

	uint samples = GetBufferSizeForMS(
		m_sample_rate, (m_parameters.minimal_output_latency) ? m_parameters.buffer_ms : m_parameters.output_latency_ms);

	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, fmt::format("{}", samples).c_str());

	const SDL_AudioSpec spec = {SDL_AUDIO_S16LE, m_output_channels, static_cast<int>(m_sample_rate)};
	m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, static_cast<void*>(this));

	SDL_AudioSpec obtained_spec = {};
	int obtained_samples = 0;

	if (SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &obtained_spec, &obtained_samples))
		DEV_LOG("Requested {} frame buffer, got {} frame buffer", samples, obtained_samples);
	else
		DEV_LOG("SDL_GetAudioDeviceFormat() failed {}", SDL_GetError());

	BaseInitialize(sample_readers[static_cast<size_t>(m_parameters.expansion_mode)], stretch_enabled);
	SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_stream));

	return true;
}

void SDLAudioStream::SetPaused(bool paused)
{
	if (m_paused == paused)
		return;

	if (paused)
		SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(m_stream));
	else
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(m_stream));

	m_paused = paused;
}

void SDLAudioStream::CloseDevice()
{
	SDL_DestroyAudioStream(m_stream);
	m_stream = nullptr;
}

void SDLAudioStream::AudioCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
	if (additional_amount > 0)
	{
		SDLAudioStream* const this_ptr = static_cast<SDLAudioStream*>(userdata);

		const u32 num_frames = additional_amount / sizeof(SampleType) / this_ptr->m_output_channels;
		SampleType* buffer = SDL_stack_alloc(SampleType, additional_amount / sizeof(SampleType));
		if (buffer)
		{
			this_ptr->ReadFrames(buffer, num_frames);
			SDL_PutAudioStreamData(stream, buffer, additional_amount);
			SDL_stack_free(buffer);
		}
	}
}
