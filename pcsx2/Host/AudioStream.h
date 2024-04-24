// SPDX-FileCopyrightText: 2019-2024 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Host/AudioStreamTypes.h"

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Error;

class FreeSurroundDecoder;
namespace soundtouch
{
	class SoundTouch;
}

class AudioStream
{
public:
	using SampleType = s16;

	static constexpr u32 NUM_INPUT_CHANNELS = 2;
	static constexpr u32 MAX_OUTPUT_CHANNELS = 8;
	static constexpr u32 CHUNK_SIZE = 64;
	static constexpr u32 MIN_EXPANSION_BLOCK_SIZE = 256;
	static constexpr u32 MAX_EXPANSION_BLOCK_SIZE = 4096;

	struct DeviceInfo
	{
		std::string name;
		std::string display_name;
		u32 minimum_latency_frames;

		DeviceInfo(std::string name_, std::string display_name_, u32 minimum_latency_);
		~DeviceInfo();
	};

public:
	virtual ~AudioStream();

	static u32 GetAlignedBufferSize(u32 size);
	static u32 GetBufferSizeForMS(u32 sample_rate, u32 ms);
	static u32 GetMSForBufferSize(u32 sample_rate, u32 buffer_size);

	static std::optional<AudioBackend> ParseBackendName(const char* str);
	static const char* GetBackendName(AudioBackend backend);
	static const char* GetBackendDisplayName(AudioBackend backend);

	static const char* GetExpansionModeName(AudioExpansionMode mode);
	static const char* GetExpansionModeDisplayName(AudioExpansionMode mode);
	static std::optional<AudioExpansionMode> ParseExpansionMode(const char* name);

	__fi u32 GetSampleRate() const { return m_sample_rate; }
	__fi u32 GetInternalChannels() const { return m_internal_channels; }
	__fi u32 GetOutputChannels() const { return m_internal_channels; }
	__fi u32 GetBufferSize() const { return m_buffer_size; }
	__fi u32 GetTargetBufferSize() const { return m_target_buffer_size; }
	__fi u32 GetOutputVolume() const { return m_volume; }
	__fi float GetNominalTempo() const { return m_nominal_rate; }
	__fi AudioExpansionMode GetExpansionMode() const { return m_parameters.expansion_mode; }
	__fi bool IsExpansionEnabled() const { return m_parameters.expansion_mode != AudioExpansionMode::Disabled; }
	__fi bool IsStretchEnabled() const { return m_stretch_enabled; }
	__fi bool IsPaused() const { return m_paused; }

	u32 GetBufferedFramesRelaxed() const;

	/// Temporarily pauses the stream, preventing it from requesting data.
	virtual void SetPaused(bool paused);

	void SetOutputVolume(u32 volume);

	void WriteChunk(const SampleType* chunk);

	void BeginWrite(SampleType** buffer_ptr, u32* num_frames);
	void WriteFrame(const SampleType* frame);
	void EndWrite(u32 num_frames);
	void EmptyBuffer();

	/// Nominal rate is used for both resampling and timestretching, input samples are assumed to be this amount faster
	/// than the sample rate.
	void SetNominalRate(float tempo);
	void UpdateTargetTempo(float tempo);

	void SetStretchEnabled(bool enabled);

	static std::vector<std::pair<std::string, std::string>> GetDriverNames(AudioBackend backend);
	static std::vector<DeviceInfo> GetOutputDevices(AudioBackend backend, const char* driver);
	static std::unique_ptr<AudioStream> CreateStream(AudioBackend backend, u32 sample_rate, const AudioStreamParameters& parameters,
		const char* driver_name, const char* device_name, bool stretch_enabled, Error* error = nullptr);
	static std::unique_ptr<AudioStream> CreateNullStream(u32 sample_rate, u32 buffer_ms);

protected:
	enum ReadChannel : u8
	{
		READ_CHANNEL_FRONT_LEFT,
		READ_CHANNEL_FRONT_CENTER,
		READ_CHANNEL_FRONT_RIGHT,
		READ_CHANNEL_SIDE_LEFT,
		READ_CHANNEL_SIDE_RIGHT,
		READ_CHANNEL_REAR_LEFT,
		READ_CHANNEL_REAR_RIGHT,
		READ_CHANNEL_LFE,
		READ_CHANNEL_NONE
	};

	using SampleReader = void (*)(SampleType* dest, const SampleType* src, u32 num_frames);

	AudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
	void BaseInitialize(SampleReader sample_reader, bool stretch_enabled);

	void ReadFrames(SampleType* samples, u32 num_frames);

	template <AudioExpansionMode mode, ReadChannel c0 = READ_CHANNEL_NONE, ReadChannel c1 = READ_CHANNEL_NONE,
		ReadChannel c2 = READ_CHANNEL_NONE, ReadChannel c3 = READ_CHANNEL_NONE, ReadChannel c4 = READ_CHANNEL_NONE,
		ReadChannel c5 = READ_CHANNEL_NONE, ReadChannel c6 = READ_CHANNEL_NONE, ReadChannel c7 = READ_CHANNEL_NONE>
	static void SampleReaderImpl(SampleType* dest, const SampleType* src, u32 num_frames);
	static void StereoSampleReaderImpl(SampleType* dest, const SampleType* src, u32 num_frames);

	u32 m_sample_rate = 0;
	u32 m_volume = 100;
	AudioStreamParameters m_parameters;
	u8 m_internal_channels = 0;
	u8 m_output_channels = 0;
	bool m_stretch_enabled = false;
	bool m_stretch_inactive = false;
	bool m_filling = false;
	bool m_paused = false;

private:
	static constexpr u32 AVERAGING_BUFFER_SIZE = 256;
	static constexpr u32 AVERAGING_WINDOW = 50;
	static constexpr u32 STRETCH_RESET_THRESHOLD = 5;
	static constexpr u32 TARGET_IPS = 691;

	static std::vector<std::pair<std::string, std::string>> GetCubebDriverNames();
	static std::vector<DeviceInfo> GetCubebOutputDevices(const char* driver);
	static std::unique_ptr<AudioStream> CreateCubebAudioStream(u32 sample_rate, const AudioStreamParameters& parameters,
		const char* driver_name, const char* device_name, bool stretch_enabled, Error* error);

	static std::unique_ptr<AudioStream> CreateSDLAudioStream(u32 sample_rate, const AudioStreamParameters& parameters,
		bool stretch_enabled, Error* error);

	void AllocateBuffer();
	void DestroyBuffer();

	void InternalWriteFrames(const SampleType* samples, u32 num_frames);

	void ExpandAllocate();

	void StretchAllocate();
	void StretchDestroy();
	void StretchWriteBlock(const float* block);
	void StretchUnderrun();
	void StretchOverrun();

	float AddAndGetAverageTempo(float val);
	void UpdateStretchTempo();

	u32 m_buffer_size = 0;
	std::unique_ptr<s16[]> m_buffer;
	SampleReader m_sample_reader = nullptr;

	std::atomic<u32> m_rpos{0};
	std::atomic<u32> m_wpos{0};

	std::unique_ptr<soundtouch::SoundTouch> m_soundtouch;

	u32 m_target_buffer_size = 0;
	u32 m_stretch_reset = STRETCH_RESET_THRESHOLD;

	u32 m_stretch_ok_count = 0;
	float m_nominal_rate = 1.0f;
	float m_dynamic_target_usage = 0.0f;

	u32 m_average_position = 0;
	u32 m_average_available = 0;
	u32 m_staging_buffer_pos = 0;

	std::array<float, AVERAGING_BUFFER_SIZE> m_average_fullness = {};

	// temporary staging buffer, used for timestretching
	std::unique_ptr<s16[]> m_staging_buffer;

	// float buffer, soundtouch only accepts float samples as input
	std::unique_ptr<float[]> m_float_buffer;

	std::unique_ptr<FreeSurroundDecoder> m_expander;

	// block buffer for expansion
	std::unique_ptr<float[]> m_expand_buffer;
	float* m_expand_output_buffer = nullptr;
	u32 m_expand_buffer_pos = 0;
};

template <AudioExpansionMode mode, AudioStream::ReadChannel c0, AudioStream::ReadChannel c1, AudioStream::ReadChannel c2,
	AudioStream::ReadChannel c3, AudioStream::ReadChannel c4, AudioStream::ReadChannel c5,
	AudioStream::ReadChannel c6, AudioStream::ReadChannel c7>
void AudioStream::SampleReaderImpl(SampleType* dest, const SampleType* src, u32 num_frames)
{
	static_assert(READ_CHANNEL_NONE == MAX_OUTPUT_CHANNELS);
	static constexpr const std::array<std::pair<std::array<s8, MAX_OUTPUT_CHANNELS>, u8>,
		static_cast<size_t>(AudioExpansionMode::Count)>
		luts = {{
			// FL FC FR SL SR RL RR LFE
			{{0, -1, 1, -1, -1, -1, -1, -1}, 2}, // Disabled
			{{0, -1, 1, -1, -1, -1, -1, 2}, 3}, // StereoLFE
			{{0, -1, 1, -1, -1, 2, 3, -1}, 5}, // Quadraphonic
			{{0, -1, 2, -1, -1, 2, 3, 4}, 5}, // QuadraphonicLFE
			{{0, 1, 2, -1, -1, 3, 4, 5}, 6}, // Surround51
			{{0, 1, 2, 3, 4, 5, 6, 7}, 8}, // Surround71
		}};
	constexpr const auto& lut = luts[static_cast<size_t>(mode)].first;
	for (u32 i = 0; i < num_frames; i++)
	{
		if constexpr (c0 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c0] >= 0 && lut[c0] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c0]];
		}
		if constexpr (c1 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c1] >= 0 && lut[c1] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c1]];
		}
		if constexpr (c2 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c2] >= 0 && lut[c2] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c2]];
		}
		if constexpr (c3 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c3] >= 0 && lut[c3] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c3]];
		}
		if constexpr (c4 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c4] >= 0 && lut[c4] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c4]];
		}
		if constexpr (c5 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c5] >= 0 && lut[c5] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c5]];
		}
		if constexpr (c6 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c6] >= 0 && lut[c6] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c6]];
		}
		if constexpr (c7 != READ_CHANNEL_NONE)
		{
			static_assert(lut[c7] >= 0 && lut[c7] < MAX_OUTPUT_CHANNELS);
			*(dest++) = src[lut[c7]];
		}

		src += luts[static_cast<size_t>(mode)].second;
	}
}
