// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioStream.h"
#include "Host/AudioRateController.h"
#include "Host/AudioRateResampler.h"
#include "FreeSurroundDecoder.h"
#include "Host.h"
#include "GS/GSVector.h"

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/Pcsx2Defs.h"
#include "common/SettingsWrapper.h"
#include "common/SmallString.h"
#include "common/Timer.h"

#include "SoundTouch.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>

//#define LOG_UNDERRUN(...) DEV_LOG(__VA_ARGS__)
#define LOG_UNDERRUN(...) (void)0
static constexpr bool LOG_TIMESTRETCH_STATS = false;

static constexpr const std::array<std::pair<u8, u8>, static_cast<size_t>(AudioExpansionMode::Count)>
	s_expansion_channel_count = {{
		{u8(2), u8(2)}, // Disabled
		{u8(3), u8(3)}, // StereoLFE
		{u8(5), u8(4)}, // Quadraphonic
		{u8(5), u8(5)}, // QuadraphonicLFE
		{u8(6), u8(6)}, // Surround51
		{u8(8), u8(8)}, // Surround71
	}};

AudioStream::DeviceInfo::DeviceInfo(std::string name_, std::string display_name_, u32 minimum_latency_)
	: name(std::move(name_))
	, display_name(std::move(display_name_))
	, minimum_latency_frames(minimum_latency_)
{
}

AudioStream::DeviceInfo::~DeviceInfo() = default;

AudioStream::AudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
	: m_sample_rate(sample_rate)
	, m_parameters(parameters)
	, m_internal_channels(s_expansion_channel_count[static_cast<size_t>(parameters.expansion_mode)].first)
	, m_output_channels(s_expansion_channel_count[static_cast<size_t>(parameters.expansion_mode)].second)
{
}

AudioStream::~AudioStream()
{
	DestroyBuffer();
}

std::unique_ptr<AudioStream> AudioStream::CreateNullStream(u32 sample_rate, u32 buffer_ms)
{
	// no point stretching with no output
	AudioStreamParameters params;
	params.expansion_mode = AudioExpansionMode::Disabled;
	params.buffer_ms = static_cast<u16>(buffer_ms);

	std::unique_ptr<AudioStream> stream(new AudioStream(sample_rate, params));
	stream->BaseInitialize(&StereoSampleReaderImpl, AudioSynchronizationMode::Disabled);
	stream->SetOutputVolume(0);
	return stream;
}

std::vector<std::pair<std::string, std::string>> AudioStream::GetDriverNames(AudioBackend backend)
{
	std::vector<std::pair<std::string, std::string>> ret;
	switch (backend)
	{
		case AudioBackend::Cubeb:
			ret = GetCubebDriverNames();
			break;

		default:
			break;
	}

	return ret;
}

std::vector<AudioStream::DeviceInfo> AudioStream::GetOutputDevices(AudioBackend backend, const char* driver)
{
	std::vector<AudioStream::DeviceInfo> ret;
	switch (backend)
	{
		case AudioBackend::Cubeb:
			ret = GetCubebOutputDevices(driver);
			break;

		default:
			break;
	}

	return ret;
}

std::unique_ptr<AudioStream> AudioStream::CreateStream(AudioBackend backend, u32 sample_rate, const AudioStreamParameters& parameters,
	const char* driver_name, const char* device_name, AudioSynchronizationMode sync_mode, Error* error)
{
	INFO_LOG("Creating {} audio stream, sample rate = {}, expansion = {}, buffer = {}, low latency buffer = {}, latency = {}, sync mode {}, driver = {}, device = {}",
		GetBackendName(backend), sample_rate, GetExpansionModeName(parameters.expansion_mode), parameters.buffer_ms,
		parameters.low_latency_buffer_ms, parameters.output_latency_ms, static_cast<u32>(sync_mode), driver_name, device_name);

	switch (backend)
	{
		case AudioBackend::Cubeb:
			return CreateCubebAudioStream(sample_rate, parameters, driver_name, device_name, sync_mode, error);

		case AudioBackend::SDL:
			return CreateSDLAudioStream(sample_rate, parameters, sync_mode, error);

		case AudioBackend::Null:
			return CreateNullStream(sample_rate, parameters.buffer_ms);

		default:
			Error::SetStringView(error, "Unknown audio backend.");
			return nullptr;
	}
}

u32 AudioStream::GetAlignedBufferSize(u32 size)
{
	static_assert(std::has_single_bit(CHUNK_SIZE));
	return Common::AlignUpPow2(size, CHUNK_SIZE);
}

u32 AudioStream::GetFrameCountForMS(u32 sample_rate, u32 milliseconds)
{
	const u64 frames = (static_cast<u64>(milliseconds) * sample_rate) / 1000u;
	return static_cast<u32>(std::clamp<u64>(frames, 1, std::numeric_limits<u32>::max()));
}

u32 AudioStream::GetMSForFrames(u32 sample_rate, u32 frames)
{
	return static_cast<u32>((static_cast<u64>(frames) * 1000u) / sample_rate);
}

u32 AudioStream::GetMSForFramesCeil(u32 sample_rate, u32 frames)
{
	return static_cast<u32>((static_cast<u64>(frames) * 1000u + sample_rate - 1u) / sample_rate);
}

static constexpr const std::array s_backend_names = {
	"Null",
	"Cubeb",
	"SDL",
};
static constexpr const std::array s_backend_display_names = {
	TRANSLATE_NOOP("AudioStream", "Null (No Output)"),
	TRANSLATE_NOOP("AudioStream", "Cubeb"),
	TRANSLATE_NOOP("AudioStream", "SDL"),
};

std::optional<AudioBackend> AudioStream::ParseBackendName(const char* str)
{
	int index = 0;
	for (const char* name : s_backend_names)
	{
		if (std::strcmp(name, str) == 0)
			return static_cast<AudioBackend>(index);

		index++;
	}

	return std::nullopt;
}

const char* AudioStream::GetBackendName(AudioBackend backend)
{
	return s_backend_names[static_cast<int>(backend)];
}

const char* AudioStream::GetBackendDisplayName(AudioBackend backend)
{
	return Host::TranslateToCString("AudioStream", s_backend_display_names[static_cast<int>(backend)]);
}

static constexpr const std::array s_expansion_mode_names = {
	"Disabled",
	"StereoLFE",
	"Quadraphonic",
	"QuadraphonicLFE",
	"Surround51",
	"Surround71",
};
static constexpr const std::array s_expansion_mode_display_names = {
	TRANSLATE_NOOP("AudioStream", "Disabled (Stereo)"),
	TRANSLATE_NOOP("AudioStream", "Stereo with LFE"),
	TRANSLATE_NOOP("AudioStream", "Quadraphonic"),
	TRANSLATE_NOOP("AudioStream", "Quadraphonic with LFE"),
	TRANSLATE_NOOP("AudioStream", "5.1 Surround"),
	TRANSLATE_NOOP("AudioStream", "7.1 Surround"),
};

const char* AudioStream::GetExpansionModeName(AudioExpansionMode mode)
{
	return (static_cast<u32>(mode) < s_expansion_mode_names.size()) ? s_expansion_mode_names[static_cast<u32>(mode)] : "";
}

const char* AudioStream::GetExpansionModeDisplayName(AudioExpansionMode mode)
{
	return (static_cast<u32>(mode) < s_expansion_mode_display_names.size()) ?
			   Host::TranslateToCString("AudioStream", s_expansion_mode_display_names[static_cast<u32>(mode)]) :
			   "";
}

std::optional<AudioExpansionMode> AudioStream::ParseExpansionMode(const char* name)
{
	for (u8 i = 0; i < static_cast<u8>(AudioExpansionMode::Count); i++)
	{
		if (std::strcmp(name, s_expansion_mode_names[i]) == 0)
			return static_cast<AudioExpansionMode>(i);
	}

	return std::nullopt;
}

u32 AudioStream::GetBufferedFramesRelaxed() const
{
	const u32 rpos = m_rpos.load(std::memory_order_relaxed);
	const u32 wpos = m_wpos.load(std::memory_order_relaxed);
	return (wpos + m_buffer_size - rpos) % m_buffer_size;
}

void AudioStream::ReadFrames(SampleType* samples, u32 num_frames)
{
	m_callback_frames.store(num_frames, std::memory_order_relaxed);
	const u32 available_frames = GetBufferedFramesRelaxed();
	u32 frames_to_read = num_frames;
	u32 silence_frames = 0;

	if (m_filling)
	{
		u32 toFill = m_buffer_size / (IsStretchEnabled() ? 32 : 400);
		toFill = GetAlignedBufferSize(toFill);

		if (available_frames < toFill)
		{
			silence_frames = num_frames;
			frames_to_read = 0;
		}
		else
		{
			m_filling = false;
			LOG_UNDERRUN("Underrun compensation done ({} frames buffered)", toFill);
		}
	}

	if (available_frames < frames_to_read)
	{
		silence_frames = frames_to_read - available_frames;
		frames_to_read = available_frames;
		m_filling = true;
		m_discontinuity_count.fetch_add(1, std::memory_order_relaxed);

		if (IsStretchEnabled())
			StretchUnderrun();
	}

	if (frames_to_read > 0)
	{
		u32 rpos = m_rpos.load(std::memory_order_acquire);

		u32 end = m_buffer_size - rpos;
		if (end > frames_to_read)
			end = frames_to_read;

		// towards the end of the buffer
		if (end > 0)
		{
			m_sample_reader(samples, &m_buffer[rpos * m_internal_channels], end);
			rpos += end;
			rpos = (rpos == m_buffer_size) ? 0 : rpos;
		}

		// after wrapping around
		const u32 start = frames_to_read - end;
		if (start > 0)
		{
			m_sample_reader(&samples[end * m_output_channels], &m_buffer[0], start);
			rpos = start;
		}

		m_rpos.store(rpos, std::memory_order_release);
	}

	if (silence_frames > 0)
	{
		if (frames_to_read > 0)
		{
			// super basic resampler - spread the input samples evenly across the output samples. will sound like ass and have
			// aliasing, but better than popping by inserting silence.
			const u32 increment =
				static_cast<u32>(65536.0f * (static_cast<float>(frames_to_read) / static_cast<float>(num_frames)));

			SampleType* resample_ptr =
				static_cast<SampleType*>(alloca(frames_to_read * m_output_channels * sizeof(SampleType)));
			std::memcpy(resample_ptr, samples, frames_to_read * m_output_channels * sizeof(SampleType));

			SampleType* out_ptr = samples;
			const u32 copy_stride = sizeof(SampleType) * m_output_channels;
			u32 resample_subpos = 0;
			for (u32 i = 0; i < num_frames; i++)
			{
				std::memcpy(out_ptr, resample_ptr, copy_stride);
				out_ptr += m_output_channels;

				resample_subpos += increment;
				resample_ptr += (resample_subpos >> 16) * m_output_channels;
				resample_subpos %= 65536u;
			}

			LOG_UNDERRUN("Audio buffer underflow, resampled {} frames to {}", frames_to_read, num_frames);
		}
		else
		{
			// no data, fall back to silence
			std::memset(samples + (frames_to_read * m_output_channels), 0, silence_frames * m_output_channels * sizeof(SampleType));
		}
	}

	if (m_volume != 100)
	{
		const int num_samples = num_frames * m_output_channels;
		const float volume_mult = static_cast<float>(m_volume) / 100.0f;

		for (int i = 0; i < num_samples; i++)
		{
			samples[i] *= volume_mult;
		}
	}
}

void AudioStream::StereoSampleReaderImpl(SampleType* dest, const SampleType* src, u32 num_frames)
{
	std::memcpy(dest, src, num_frames * 2 * sizeof(SampleType));
}

void AudioStream::InternalWriteFrames(const SampleType* data, u32 num_frames)
{
	const u32 free = m_buffer_size - GetBufferedFramesRelaxed();
	if (free <= num_frames)
	{
		if (IsStretchEnabled())
		{
			StretchOverrun();
		}
		else
		{
			m_discontinuity_count.fetch_add(1, std::memory_order_relaxed);
			LOG_UNDERRUN("Buffer overrun, chunk dropped");
			return;
		}
	}

	u32 wpos = m_wpos.load(std::memory_order_acquire);

	// wrapping around the end of the buffer?
	if ((m_buffer_size - wpos) <= num_frames)
	{
		// needs to be written in two parts
		const u32 end = m_buffer_size - wpos;
		const u32 start = num_frames - end;

		// start is zero when this chunk reaches exactly the end
		std::memcpy(&m_buffer[wpos * m_internal_channels], data, end * m_internal_channels * sizeof(SampleType));
		if (start > 0)
			std::memcpy(&m_buffer[0], data + end * m_internal_channels, start * m_internal_channels * sizeof(SampleType));

		wpos = start;
	}
	else
	{
		// no split
		std::memcpy(&m_buffer[wpos * m_internal_channels], data, num_frames * m_internal_channels * sizeof(SampleType));
		wpos += num_frames;
	}

	m_wpos.store(wpos, std::memory_order_release);
}

void AudioStream::BaseInitialize(SampleReader sample_reader, AudioSynchronizationMode sync_mode)
{
	m_sync_mode = sync_mode;
	switch (sync_mode)
	{
		case AudioSynchronizationMode::Disabled:
			m_processing_mode = ProcessingMode::None;
			break;

		case AudioSynchronizationMode::TimeStretch:
			m_processing_mode = ProcessingMode::TimeStretch;
			break;

		case AudioSynchronizationMode::LowLatency:
			m_processing_mode = ProcessingMode::Resample;
			break;

		case AudioSynchronizationMode::Count:
			break;
	}
	m_paused = false;
	m_sample_reader = sample_reader;

	AllocateBuffer();
	ExpandAllocate();
	ProcessorAllocate();
	if (m_processing_mode == ProcessingMode::Resample)
		PrimeLowLatencyBuffer();
}

void AudioStream::AllocateBuffer()
{
	// use a larger buffer when time stretching, since we need more input
	u32 target_ms;
	u32 multiplier;
	switch (m_processing_mode)
	{
		case ProcessingMode::None:
			target_ms = m_parameters.buffer_ms;
			multiplier = 1;
			break;

		case ProcessingMode::Resample:
			target_ms = m_parameters.low_latency_buffer_ms;
			multiplier = 4;
			break;

		case ProcessingMode::TimeStretch:
			target_ms = m_parameters.buffer_ms;
			multiplier = 16;
			break;
	}
	m_target_buffer_size = std::max(GetAlignedBufferSize((m_sample_rate * target_ms) / 1000u), CHUNK_SIZE);
	m_buffer_size = GetAlignedBufferSize(((target_ms * multiplier) * m_sample_rate) / 1000u);
	if (m_processing_mode == ProcessingMode::Resample)
		m_buffer_size = std::max(m_buffer_size, m_target_buffer_size + CHUNK_SIZE);

	m_buffer = std::make_unique<float[]>(m_buffer_size * m_internal_channels);
	m_staging_buffer = std::make_unique<float[]>(CHUNK_SIZE * 2 * m_internal_channels);

	if (IsExpansionEnabled())
		m_expand_buffer = std::make_unique<float[]>(m_parameters.expand_block_size * NUM_INPUT_CHANNELS);

	DEV_LOG("Allocated audio buffer: {} frames, {} ms, expansion {}, block size {}, mode {}, target {}.",
		m_buffer_size, target_ms, GetExpansionModeName(m_parameters.expansion_mode), m_parameters.expand_block_size,
		static_cast<u32>(m_processing_mode), m_target_buffer_size);
}

void AudioStream::DestroyBuffer()
{
	m_expand_buffer.reset();
	m_staging_buffer.reset();
	m_buffer.reset();
	m_buffer_size = 0;
	m_wpos.store(0, std::memory_order_release);
	m_rpos.store(0, std::memory_order_release);
}

void AudioStream::EmptyBuffer()
{
	if (IsExpansionEnabled())
	{
		m_expander->Flush();
		m_expand_output_buffer = nullptr;
		m_expand_buffer_pos = 0;
	}

	if (m_processing_mode == ProcessingMode::Resample)
	{
		ResetLowLatencyState(true);
		return;
	}

	if (IsStretchEnabled())
	{
		m_soundtouch->clear();
		if (IsStretchEnabled())
			m_soundtouch->setTempo(m_nominal_rate);
	}

	m_wpos.store(m_rpos.load(std::memory_order_acquire), std::memory_order_release);
}

void AudioStream::ResetForSavestateLoad()
{
	if (m_processing_mode == ProcessingMode::Resample)
		ResetLowLatencyState(true);
}

void AudioStream::SetNominalRate(float tempo)
{
	m_nominal_rate = tempo;
}

AudioStream::ProcessingMode AudioStream::SelectProcessingMode(float speed) const
{
	// Hysteresis prevents mode changes near the fallback boundary.
	const float minimum = (m_processing_mode == ProcessingMode::Resample) ? 0.95f : 0.97f;
	const float maximum = (m_processing_mode == ProcessingMode::Resample) ? 1.05f : 1.03f;
	return (speed >= minimum && speed <= maximum) ? ProcessingMode::Resample : ProcessingMode::TimeStretch;
}

void AudioStream::SetTargetSpeed(float speed)
{
	m_target_speed = speed;
	if (m_sync_mode == AudioSynchronizationMode::LowLatency)
		SetProcessingMode(SelectProcessingMode(speed));
}

void AudioStream::UpdateTargetTempo(float tempo)
{
	if (!IsStretchEnabled())
		return;

	// undo sqrt()
	if (tempo)
		tempo *= tempo;

	m_average_position = AVERAGING_WINDOW;
	m_average_available = AVERAGING_WINDOW;
	std::fill_n(m_average_fullness.data(), AVERAGING_WINDOW, tempo);
	m_soundtouch->setTempo(tempo);
	m_stretch_reset = 0;
	m_stretch_inactive = false;
	m_stretch_ok_count = 0;
	m_dynamic_target_usage = static_cast<float>(m_target_buffer_size) * m_nominal_rate;
}

void AudioStream::SetSynchronizationMode(AudioSynchronizationMode mode)
{
	if (m_sync_mode == mode)
		return;

	m_sync_mode = mode;
	switch (mode)
	{
		case AudioSynchronizationMode::Disabled:
			SetProcessingMode(ProcessingMode::None);
			break;

		case AudioSynchronizationMode::TimeStretch:
			SetProcessingMode(ProcessingMode::TimeStretch);
			break;

		case AudioSynchronizationMode::LowLatency:
			SetProcessingMode(SelectProcessingMode(m_target_speed));
			break;

		case AudioSynchronizationMode::Count:
			break;
	}
}

void AudioStream::SetProcessingMode(ProcessingMode mode)
{
	if (m_processing_mode == mode)
		return;

	// can't resize the buffers while paused
	const bool paused = m_paused;
	if (!paused)
		SetPaused(true);
	if (IsExpansionEnabled())
	{
		m_expander->Flush();
		m_expand_output_buffer = nullptr;
		m_expand_buffer_pos = 0;
	}

	ProcessorDestroy();
	DestroyBuffer();
	m_processing_mode = mode;
	AllocateBuffer();
	ProcessorAllocate();
	if (mode == ProcessingMode::Resample)
		ResetLowLatencyState(true);

	if (!paused)
		SetPaused(false);
}

void AudioStream::SetPaused(bool paused)
{
	if (m_paused == paused)
		return;

	m_paused = paused;
	if (m_processing_mode == ProcessingMode::Resample)
	{
		if (paused)
			ResetLowLatencyState(false);
		else if (GetBufferedFramesRelaxed() == 0)
			ResetLowLatencyState(true);
		else
			m_rate_controller->Reset();
	}
}

void AudioStream::SetOutputVolume(u32 volume)
{
	m_volume = volume;
}

void AudioStream::BeginWrite(SampleType** buffer_ptr, u32* num_frames)
{
	*buffer_ptr = &m_staging_buffer[m_staging_buffer_pos];
	*num_frames = CHUNK_SIZE - (m_staging_buffer_pos / NUM_INPUT_CHANNELS);
}

void AudioStream::WriteFrame(const SampleType* frame)
{
	pxAssert((CHUNK_SIZE - (m_staging_buffer_pos / NUM_INPUT_CHANNELS)) > 0);
	std::memcpy(&m_staging_buffer[m_staging_buffer_pos], frame, sizeof(SampleType) * NUM_INPUT_CHANNELS);
	EndWrite(1);
}

void AudioStream::ExpandAllocate()
{
	pxAssert(!m_expander);
	if (m_parameters.expansion_mode == AudioExpansionMode::Disabled)
		return;

	static constexpr std::array<std::pair<FreeSurroundDecoder::ChannelSetup, bool>,
		static_cast<size_t>(AudioExpansionMode::Count)>
		channel_setup_mapping = {{
			{FreeSurroundDecoder::ChannelSetup::Stereo, false}, // Disabled
			{FreeSurroundDecoder::ChannelSetup::Stereo, true}, // StereoLFE
			{FreeSurroundDecoder::ChannelSetup::Surround41, false}, // Quadraphonic
			{FreeSurroundDecoder::ChannelSetup::Surround41, true}, // QuadraphonicLFE
			{FreeSurroundDecoder::ChannelSetup::Surround51, true}, // Surround51
			{FreeSurroundDecoder::ChannelSetup::Surround71, true}, // Surround71
		}};

	const auto [fs_setup, fs_lfe] = channel_setup_mapping[static_cast<size_t>(m_parameters.expansion_mode)];

	m_expander = std::make_unique<FreeSurroundDecoder>(fs_setup, m_parameters.expand_block_size);
	m_expander->SetBassRedirection(fs_lfe);
	m_expander->SetCircularWrap(m_parameters.expand_circular_wrap);
	m_expander->SetShift(m_parameters.expand_shift);
	m_expander->SetDepth(m_parameters.expand_depth);
	m_expander->SetFocus(m_parameters.expand_focus);
	m_expander->SetCenterImage(m_parameters.expand_center_image);
	m_expander->SetFrontSeparation(m_parameters.expand_front_separation);
	m_expander->SetRearSeparation(m_parameters.expand_rear_separation);
	m_expander->SetLowCutoff(static_cast<float>(m_parameters.expand_low_cutoff) / m_sample_rate * 2);
	m_expander->SetHighCutoff(static_cast<float>(m_parameters.expand_high_cutoff) / m_sample_rate * 2);
}

void AudioStream::EndWrite(u32 num_frames)
{
	// don't bother committing anything when muted
	if (m_volume == 0)
		return;

	m_staging_buffer_pos += num_frames * NUM_INPUT_CHANNELS;
	pxAssert(m_staging_buffer_pos <= (CHUNK_SIZE * NUM_INPUT_CHANNELS));
	if ((m_staging_buffer_pos / NUM_INPUT_CHANNELS) < CHUNK_SIZE)
		return;

	m_staging_buffer_pos = 0;
	WriteChunk(m_staging_buffer.get());
}

void AudioStream::WriteChunk(const SampleType* chunk)
{
	if (!IsExpansionEnabled() && m_processing_mode == ProcessingMode::None)
	{
		InternalWriteFrames(chunk, CHUNK_SIZE);
		return;
	}

	if (IsExpansionEnabled())
	{
		// ProcessWriteBlock() overwrites the staging buffer on output, so we need to copy into the expand buffer first.
		std::memcpy(m_expand_buffer.get() + m_expand_buffer_pos * NUM_INPUT_CHANNELS, chunk, CHUNK_SIZE * NUM_INPUT_CHANNELS * sizeof(SampleType));

		// Output the corresponding block.
		if (m_expand_output_buffer)
			ProcessWriteBlock(m_expand_output_buffer + m_expand_buffer_pos * m_internal_channels);

		// Decode the next block if we buffered enough.
		m_expand_buffer_pos += CHUNK_SIZE;
		if (m_expand_buffer_pos == m_parameters.expand_block_size)
		{
			m_expand_buffer_pos = 0;
			m_expand_output_buffer = m_expander->Decode(m_expand_buffer.get());
		}
	}
	else
	{
		ProcessWriteBlock(chunk);
	}
}

template <class T>
__fi static bool IsInRange(const T& val, const T& min, const T& max)
{
	return (min <= val && val <= max);
}

void AudioStream::ProcessorAllocate()
{
	switch (m_processing_mode)
	{
		case ProcessingMode::None:
			break;

		case ProcessingMode::Resample:
			ResampleAllocate();
			break;

		case ProcessingMode::TimeStretch:
			StretchAllocate();
			break;
	}
}

void AudioStream::ProcessorDestroy()
{
	ResampleDestroy();
	StretchDestroy();
}

void AudioStream::ProcessWriteBlock(const float* block)
{
	switch (m_processing_mode)
	{
		case ProcessingMode::None:
			InternalWriteFrames(block, CHUNK_SIZE);
			break;

		case ProcessingMode::Resample:
			ResampleBlock(block);
			break;

		case ProcessingMode::TimeStretch:
			StretchWriteBlock(block);
			break;
	}
}

void AudioStream::ResampleAllocate()
{
	m_resampler = std::make_unique<AudioRateResampler>(m_internal_channels);
	m_rate_controller = std::make_unique<AudioRateController>(m_sample_rate, m_target_buffer_size);
	m_seen_discontinuity_count = m_discontinuity_count.load(std::memory_order_relaxed);
	m_fade_in_frames_remaining = std::min(m_sample_rate / 200u, m_target_buffer_size);
}

void AudioStream::ResampleDestroy()
{
	m_rate_controller.reset();
	m_resampler.reset();
}

void AudioStream::PrimeLowLatencyBuffer()
{
	pxAssert(m_processing_mode == ProcessingMode::Resample);
	std::fill_n(m_staging_buffer.get(), CHUNK_SIZE * m_internal_channels, 0.0f);
	for (u32 frames = 0; frames < m_target_buffer_size; frames += CHUNK_SIZE)
		InternalWriteFrames(m_staging_buffer.get(), CHUNK_SIZE);
}

void AudioStream::ResetLowLatencyState(bool prime_buffer)
{
	if (!m_resampler || !m_rate_controller)
		return;

	m_wpos.store(m_rpos.load(std::memory_order_acquire), std::memory_order_release);
	if (IsExpansionEnabled())
	{
		m_expander->Flush();
		m_expand_output_buffer = nullptr;
		m_expand_buffer_pos = 0;
	}
	m_staging_buffer_pos = 0;
	m_resampler->Reset();
	m_rate_controller->Reset();
	m_seen_discontinuity_count = m_discontinuity_count.load(std::memory_order_relaxed);
	m_fade_in_frames_remaining = std::min(m_sample_rate / 200u, m_target_buffer_size);
	m_filling = false;
	if (prime_buffer)
		PrimeLowLatencyBuffer();
}

void AudioStream::ResampleBlock(const float* block)
{
	const float* input = block;
	if (m_fade_in_frames_remaining > 0)
	{
		const u32 fade_total = std::max(m_sample_rate / 200u, 1u);
		for (u32 frame = 0; frame < CHUNK_SIZE; frame++)
		{
			const float gain =
				1.0f - (static_cast<float>(m_fade_in_frames_remaining) / static_cast<float>(fade_total));
			for (u32 channel = 0; channel < m_internal_channels; channel++)
				m_staging_buffer[frame * m_internal_channels + channel] =
					block[frame * m_internal_channels + channel] * gain;
			if (m_fade_in_frames_remaining > 0)
				m_fade_in_frames_remaining--;
		}
		input = m_staging_buffer.get();
	}

	const u32 discontinuity_count = m_discontinuity_count.load(std::memory_order_relaxed);
	const float rate = m_rate_controller->Update(
		GetBufferedFramesRelaxed(), m_callback_frames.load(std::memory_order_relaxed), CHUNK_SIZE, m_target_speed,
		discontinuity_count != m_seen_discontinuity_count);
	m_seen_discontinuity_count = discontinuity_count;
	m_resampler->SetRate(rate);
	m_resampler->PutFrames(input, CHUNK_SIZE);

	while (m_resampler->GetAvailableFrames() > 0)
	{
		const u32 output_frames = m_resampler->ReceiveFrames(m_staging_buffer.get(), CHUNK_SIZE * 2);
		if (output_frames == 0)
			break;
		InternalWriteFrames(m_staging_buffer.get(), output_frames);
	}
}

void AudioStream::StretchAllocate()
{
	if (!IsStretchEnabled())
		return;

	m_soundtouch = std::make_unique<soundtouch::SoundTouch>();
	m_soundtouch->setSampleRate(m_sample_rate);
	m_soundtouch->setChannels(m_internal_channels);

	m_soundtouch->setSetting(SETTING_USE_QUICKSEEK, m_parameters.stretch_use_quickseek);
	m_soundtouch->setSetting(SETTING_USE_AA_FILTER, m_parameters.stretch_use_aa_filter);

	m_soundtouch->setSetting(SETTING_SEQUENCE_MS, m_parameters.stretch_sequence_length_ms);
	m_soundtouch->setSetting(SETTING_SEEKWINDOW_MS, m_parameters.stretch_seekwindow_ms);
	m_soundtouch->setSetting(SETTING_OVERLAP_MS, m_parameters.stretch_overlap_ms);

	m_soundtouch->setTempo(m_nominal_rate);

	m_stretch_reset = STRETCH_RESET_THRESHOLD;
	m_stretch_inactive = false;
	m_stretch_ok_count = 0;
	m_dynamic_target_usage = 0.0f;
	m_average_position = 0;
	m_average_available = 0;

	m_staging_buffer_pos = 0;
}

void AudioStream::StretchDestroy()
{
	m_soundtouch.reset();
}

void AudioStream::StretchWriteBlock(const float* block)
{
	m_soundtouch->putSamples(block, CHUNK_SIZE);

	u32 tempProgress;
	while (tempProgress = m_soundtouch->receiveSamples(m_staging_buffer.get(), CHUNK_SIZE), tempProgress != 0)
	{
		InternalWriteFrames(m_staging_buffer.get(), tempProgress);
	}

	UpdateStretchTempo();
}

float AudioStream::AddAndGetAverageTempo(float val)
{
	if (m_stretch_reset >= STRETCH_RESET_THRESHOLD)
		m_average_available = 0;
	if (m_average_available < AVERAGING_BUFFER_SIZE)
		m_average_available++;

	m_average_fullness[m_average_position] = val;
	m_average_position = (m_average_position + 1U) % AVERAGING_BUFFER_SIZE;

	const u32 actual_window = std::min<u32>(m_average_available, AVERAGING_WINDOW);
	const u32 first_index = (m_average_position - actual_window + AVERAGING_BUFFER_SIZE) % AVERAGING_BUFFER_SIZE;

	float sum = 0;
	for (u32 i = first_index; i < first_index + actual_window; i++)
		sum += m_average_fullness[i % AVERAGING_BUFFER_SIZE];
	sum = sum / actual_window;

	return (sum != 0.0f) ? sum : 1.0f;
}

void AudioStream::UpdateStretchTempo()
{
	static constexpr float MIN_TEMPO = 0.05f;
	static constexpr float MAX_TEMPO = 50.0f;

	// Which range we will run in 1:1 mode for.
	static constexpr float INACTIVE_GOOD_FACTOR = 1.04f;
	static constexpr float INACTIVE_BAD_FACTOR = 1.2f;
	static constexpr u32 INACTIVE_MIN_OK_COUNT = 50;
	static constexpr u32 COMPENSATION_DIVIDER = 100;

	float base_target_usage = static_cast<float>(m_target_buffer_size) * m_nominal_rate;

	// state vars
	if (m_stretch_reset >= STRETCH_RESET_THRESHOLD)
	{
		LOG_UNDERRUN("___ Stretcher is being reset.");
		m_stretch_inactive = false;
		m_stretch_ok_count = 0;
		m_dynamic_target_usage = base_target_usage;
	}

	const u32 ibuffer_usage = GetBufferedFramesRelaxed();
	float buffer_usage = static_cast<float>(ibuffer_usage);
	float tempo = buffer_usage / m_dynamic_target_usage;
	tempo = AddAndGetAverageTempo(tempo);

	// Dampening when we get close to target.
	if (tempo < 2.0f)
		tempo = std::sqrt(tempo);

	tempo = std::clamp(tempo, MIN_TEMPO, MAX_TEMPO);

	if (tempo < 1.0f)
		base_target_usage /= std::sqrt(tempo);

	m_dynamic_target_usage +=
		static_cast<float>(base_target_usage / tempo - m_dynamic_target_usage) / static_cast<float>(COMPENSATION_DIVIDER);
	if (IsInRange(tempo, 0.9f, 1.1f) &&
		IsInRange(m_dynamic_target_usage, base_target_usage * 0.9f, base_target_usage * 1.1f))
	{
		m_dynamic_target_usage = base_target_usage;
	}

	if (!m_stretch_inactive)
	{
		if (IsInRange(tempo, 1.0f / INACTIVE_GOOD_FACTOR, INACTIVE_GOOD_FACTOR))
			m_stretch_ok_count++;
		else
			m_stretch_ok_count = 0;

		if (m_stretch_ok_count >= INACTIVE_MIN_OK_COUNT)
		{
			LOG_UNDERRUN("=== Stretcher is now inactive.");
			m_stretch_inactive = true;
		}
	}
	else if (!IsInRange(tempo, 1.0f / INACTIVE_BAD_FACTOR, INACTIVE_BAD_FACTOR))
	{
		LOG_UNDERRUN("~~~ Stretcher is now active @ tempo {}.", tempo);
		m_stretch_inactive = false;
		m_stretch_ok_count = 0;
	}

	if (m_stretch_inactive)
		tempo = m_nominal_rate;

	if constexpr (LOG_TIMESTRETCH_STATS)
	{
		static int iterations = 0;
		static u64 last_log_time = 0;

		const u64 now = Common::Timer::GetCurrentValue();

		if (Common::Timer::ConvertValueToSeconds(now - last_log_time) > 1.0f)
		{
			DEV_LOG("buffers: {:4} ms ({:3.0f}%), tempo: {}, comp: {:2.3f}, iters: {}, reset:{}",
				(ibuffer_usage * 1000u) / m_sample_rate, 100.0f * buffer_usage / base_target_usage, tempo,
				m_dynamic_target_usage / base_target_usage, iterations, m_stretch_reset);

			last_log_time = now;
			iterations = 0;
		}

		iterations++;
	}

	m_soundtouch->setTempo(tempo);

	if (m_stretch_reset >= STRETCH_RESET_THRESHOLD)
		m_stretch_reset = 0;
}

void AudioStream::StretchUnderrun()
{
	// Didn't produce enough frames in time.
	m_stretch_reset++;
}

void AudioStream::StretchOverrun()
{
	// Produced more frames than can fit in the buffer.
	m_stretch_reset++;
	m_discontinuity_count.fetch_add(1, std::memory_order_relaxed);

	// Drop two packets to give the time stretcher a bit more time to slow things down.
	const u32 discard = CHUNK_SIZE * 2;
	m_rpos.store((m_rpos.load(std::memory_order_acquire) + discard) % m_buffer_size, std::memory_order_release);
}

void AudioStreamParameters::LoadSave(SettingsWrapper& wrap, const char* section)
{
	wrap.EnumEntry(section, "ExpansionMode", expansion_mode, &AudioStream::ParseExpansionMode, &AudioStream::GetExpansionModeName, DEFAULT_EXPANSION_MODE);
	buffer_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "BufferMS", buffer_ms, DEFAULT_BUFFER_MS), 0, std::numeric_limits<u16>::max()));
	low_latency_buffer_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "LowLatencyBufferMS",
																 low_latency_buffer_ms, DEFAULT_LOW_LATENCY_BUFFER_MS),
		10, 100));
	output_latency_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "OutputLatencyMS",
															 output_latency_ms, DEFAULT_OUTPUT_LATENCY_MS),
		1, std::numeric_limits<u16>::max()));

	stretch_sequence_length_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "StretchSequenceLengthMS", DEFAULT_STRETCH_SEQUENCE_LENGTH), 0, std::numeric_limits<u16>::max()));
	stretch_seekwindow_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "StretchSeekWindowMS", DEFAULT_STRETCH_SEEKWINDOW), 0, std::numeric_limits<u16>::max()));
	stretch_overlap_ms = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "StretchOverlapMS", DEFAULT_STRETCH_OVERLAP), 0, std::numeric_limits<u16>::max()));
	stretch_use_quickseek = wrap.EntryBitBool(section, "StretchUseQuickSeek", DEFAULT_STRETCH_USE_QUICKSEEK);
	stretch_use_aa_filter = wrap.EntryBitBool(section, "StretchUseAAFilter", DEFAULT_STRETCH_USE_AA_FILTER);

	expand_block_size = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "ExpandBlockSize", DEFAULT_EXPAND_BLOCK_SIZE), 0, std::numeric_limits<u16>::max()));
	wrap.Entry(section, "ExpandCircularWrap", expand_circular_wrap, DEFAULT_EXPAND_CIRCULAR_WRAP);
	wrap.Entry(section, "ExpandShift", expand_shift, DEFAULT_EXPAND_SHIFT);
	wrap.Entry(section, "ExpandDepth", expand_depth, DEFAULT_EXPAND_DEPTH);
	wrap.Entry(section, "ExpandFocus", expand_focus, DEFAULT_EXPAND_FOCUS);
	wrap.Entry(section, "ExpandCenterImage", expand_center_image, DEFAULT_EXPAND_CENTER_IMAGE);
	wrap.Entry(section, "ExpandFrontSeparation", expand_front_separation, DEFAULT_EXPAND_FRONT_SEPARATION);
	wrap.Entry(section, "ExpandRearSeparation", expand_rear_separation, DEFAULT_EXPAND_REAR_SEPARATION);
	expand_low_cutoff = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "ExpandLowCutoff", DEFAULT_EXPAND_LOW_CUTOFF), 0, std::numeric_limits<u8>::max()));
	expand_high_cutoff = static_cast<u16>(std::clamp<int>(wrap.EntryBitfield(section, "ExpandHighCutoff", DEFAULT_EXPAND_HIGH_CUTOFF), 0, std::numeric_limits<u8>::max()));

	// Clamping of values.
	if (wrap.IsLoading())
	{
		stretch_sequence_length_ms = std::clamp<u16>(stretch_sequence_length_ms, 20, 100);
		stretch_seekwindow_ms = std::clamp<u16>(stretch_seekwindow_ms, 10, 30);
		stretch_overlap_ms = std::clamp<u16>(stretch_overlap_ms, 5, 15);

		expand_block_size = std::clamp<u16>(std::has_single_bit(expand_block_size) ? expand_block_size : std::bit_ceil(expand_block_size), 128, 8192);
		expand_circular_wrap = std::clamp(expand_circular_wrap, 0.0f, 360.0f);
		expand_shift = std::clamp(expand_shift, -1.0f, 1.0f);
		expand_depth = std::clamp(expand_depth, 0.0f, 5.0f);
		expand_focus = std::clamp(expand_focus, -1.0f, 1.0f);
		expand_center_image = std::clamp(expand_center_image, 0.0f, 1.0f);
		expand_front_separation = std::clamp(expand_front_separation, 0.0f, 10.0f);
		expand_rear_separation = std::clamp(expand_rear_separation, 0.0f, 10.0f);
		expand_low_cutoff = std::min<u8>(expand_low_cutoff, 100);
		expand_high_cutoff = std::min<u8>(expand_high_cutoff, 100);
	}
}
