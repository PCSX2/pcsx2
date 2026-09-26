// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <memory>

namespace soundtouch
{
	class RateTransposer;
}

class AudioRateResampler
{
public:
	explicit AudioRateResampler(u32 channels);
	~AudioRateResampler();

	AudioRateResampler(const AudioRateResampler&) = delete;
	AudioRateResampler& operator=(const AudioRateResampler&) = delete;

	void Reset();
	void SetRate(float rate);
	void PutFrames(const float* frames, u32 num_frames);
	u32 ReceiveFrames(float* frames, u32 max_frames);

	u32 GetAvailableFrames() const;

private:
	std::unique_ptr<soundtouch::RateTransposer> m_transposer;
};
