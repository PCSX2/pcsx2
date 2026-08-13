// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioRateController.h"

#include <algorithm>
#include <cmath>

constexpr float CORRECTION_LIMIT = 0.01f;
constexpr float FILTER_TIME_CONSTANT_SECONDS = 0.025f;
constexpr float PROPORTIONAL_GAIN = 0.012f;
constexpr float INTEGRAL_GAIN_PER_SECOND = 0.040f;
constexpr float CORRECTION_SLEW_PER_SECOND = 0.040f;
constexpr float INTEGRAL_DECAY_SECONDS = 0.75f;
constexpr float SATURATION_RELEASE_MARGIN = 0.0005f;

AudioRateController::AudioRateController(u32 sample_rate, u32 target_frames)
	: m_sample_rate(std::max(sample_rate, 1u))
	, m_target_frames(std::max(target_frames, 1u))
{
	Reset();
}

void AudioRateController::Reset()
{
	m_filtered_buffered_frames = static_cast<float>(m_target_frames);
	m_integral = 0.0f;
	m_correction = 1.0f;
	m_saturation_direction = 0;
}

float AudioRateController::Update(
	u32 buffered_frames, u32 callback_frames, u32 input_frames, float nominal_rate, bool discontinuity)
{
	const float target_frames = static_cast<float>(m_target_frames);
	const float dt = static_cast<float>(std::max(input_frames, 1u)) / static_cast<float>(m_sample_rate);
	if (discontinuity)
	{
		m_filtered_buffered_frames = static_cast<float>(buffered_frames);
		m_integral = 0.0f;
		m_saturation_direction = 0;
	}

	const float filter_alpha = 1.0f - std::exp(-dt / FILTER_TIME_CONSTANT_SECONDS);
	m_filtered_buffered_frames +=
		(static_cast<float>(buffered_frames) - m_filtered_buffered_frames) * filter_alpha;

	float error_frames = m_filtered_buffered_frames - target_frames;
	const float deadband_frames = std::max(static_cast<float>(m_sample_rate) * 0.001f,
		static_cast<float>(callback_frames) * 0.20f);
	if (std::abs(error_frames) <= deadband_frames)
	{
		error_frames = 0.0f;
		m_integral *= std::exp(-dt / INTEGRAL_DECAY_SECONDS);
	}
	else
	{
		const float normalized_error = error_frames / target_frames;
		const float proposed_integral = m_integral + normalized_error * INTEGRAL_GAIN_PER_SECOND * dt;
		const float proposed_delta = PROPORTIONAL_GAIN * normalized_error + proposed_integral;
		// Stop integrating while the error would push farther beyond a correction limit.
		if (std::abs(proposed_delta) <= CORRECTION_LIMIT ||
			(proposed_delta > CORRECTION_LIMIT && normalized_error < 0.0f) ||
			(proposed_delta < -CORRECTION_LIMIT && normalized_error > 0.0f))
		{
			m_integral = proposed_integral;
		}
	}

	const float normalized_error = error_frames / target_frames;
	const float unclamped_correction = 1.0f + PROPORTIONAL_GAIN * normalized_error + m_integral;
	const float lower_limit = 1.0f - CORRECTION_LIMIT;
	const float upper_limit = 1.0f + CORRECTION_LIMIT;
	if ((m_saturation_direction < 0 && unclamped_correction >= (lower_limit + SATURATION_RELEASE_MARGIN)) ||
		(m_saturation_direction > 0 && unclamped_correction <= (upper_limit - SATURATION_RELEASE_MARGIN)))
	{
		m_saturation_direction = 0;
	}
	if (m_saturation_direction == 0)
	{
		if (unclamped_correction <= lower_limit)
			m_saturation_direction = -1;
		else if (unclamped_correction >= upper_limit)
			m_saturation_direction = 1;
	}

	// Apply hysteresis at the correction limits to prevent callback jitter from repeatedly entering and leaving saturation.
	const float requested_correction = (m_saturation_direction < 0) ? lower_limit :
	                                   (m_saturation_direction > 0) ? upper_limit :
	                                                                  unclamped_correction;
	const float max_slew = CORRECTION_SLEW_PER_SECOND * dt;
	m_correction += std::clamp(requested_correction - m_correction, -max_slew, max_slew);
	m_correction = std::clamp(m_correction, lower_limit, upper_limit);

	return std::max(nominal_rate, 0.0f) * m_correction;
}
