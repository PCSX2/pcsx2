// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

class AudioRateController
{
public:
	AudioRateController(u32 sample_rate, u32 target_frames);

	void Reset();
	float Update(u32 buffered_frames, u32 callback_frames, u32 input_frames, float nominal_rate, bool discontinuity);

private:
	u32 m_sample_rate;
	u32 m_target_frames;
	float m_filtered_buffered_frames = 0.0f;
	float m_integral = 0.0f;
	float m_correction = 1.0f;
	s8 m_saturation_direction = 0;
};
