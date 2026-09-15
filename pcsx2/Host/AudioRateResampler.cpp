// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioRateResampler.h"

#include "../../3rdparty/soundtouch/source/SoundTouch/RateTransposer.h"

#include <cmath>
#include <vector>

AudioRateResampler::AudioRateResampler(u32 channels)
{
	m_transposer = std::make_unique<soundtouch::RateTransposer>();
	m_transposer->setChannels(static_cast<int>(channels));
	m_transposer->enableAAFilter(false);
	m_transposer->setRate(1.0);

	// Preallocate SoundTouch's internal FIFOs before entering the real-time path.
	static constexpr u32 PREWARM_FRAMES = 4096;
	std::vector<float> silence(PREWARM_FRAMES * channels, 0.0f);
	for (const double rate : {0.94, 1.07})
	{
		m_transposer->setRate(rate);
		m_transposer->putSamples(silence.data(), PREWARM_FRAMES);
		m_transposer->receiveSamples(m_transposer->numSamples());
		m_transposer->clear();
	}

	m_transposer->setRate(1.0);
	m_transposer->clear();
}

AudioRateResampler::~AudioRateResampler() = default;

void AudioRateResampler::Reset()
{
	m_transposer->clear();
}

void AudioRateResampler::SetRate(float rate)
{
	if (!std::isfinite(rate) || rate <= 0.0f)
		return;

	m_transposer->setRate(static_cast<double>(rate));
}

void AudioRateResampler::PutFrames(const float* frames, u32 num_frames)
{
	m_transposer->putSamples(frames, num_frames);
}

u32 AudioRateResampler::ReceiveFrames(float* frames, u32 max_frames)
{
	return m_transposer->receiveSamples(frames, max_frames);
}

u32 AudioRateResampler::GetAvailableFrames() const
{
	return m_transposer->numSamples();
}
