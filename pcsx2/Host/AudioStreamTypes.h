// SPDX-FileCopyrightText: 2019-2024 Connor McLaughlin <stenzek@gmail.com>
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

class SettingsWrapper;

enum class AudioBackend : u8
{
	Null,
	Cubeb,
	SDL,
	Count
};

enum class AudioExpansionMode : u8
{
	Disabled,
	StereoLFE,
	Quadraphonic,
	QuadraphonicLFE,
	Surround51,
	Surround71,
	Count
};

struct AudioStreamParameters
{
	AudioExpansionMode expansion_mode = DEFAULT_EXPANSION_MODE;
	bool minimal_output_latency = DEFAULT_OUTPUT_LATENCY_MINIMAL;
	u16 buffer_ms = DEFAULT_BUFFER_MS;
	u16 output_latency_ms = DEFAULT_OUTPUT_LATENCY_MS;

	u16 stretch_sequence_length_ms = DEFAULT_STRETCH_SEQUENCE_LENGTH;
	u16 stretch_seekwindow_ms = DEFAULT_STRETCH_SEEKWINDOW;
	u16 stretch_overlap_ms = DEFAULT_STRETCH_OVERLAP;
	bool stretch_use_quickseek = DEFAULT_STRETCH_USE_QUICKSEEK;
	bool stretch_use_aa_filter = DEFAULT_STRETCH_USE_AA_FILTER;

	float expand_circular_wrap = DEFAULT_EXPAND_CIRCULAR_WRAP;
	float expand_shift = DEFAULT_EXPAND_SHIFT;
	float expand_depth = DEFAULT_EXPAND_DEPTH;
	float expand_focus = DEFAULT_EXPAND_FOCUS;
	float expand_center_image = DEFAULT_EXPAND_CENTER_IMAGE;
	float expand_front_separation = DEFAULT_EXPAND_FRONT_SEPARATION;
	float expand_rear_separation = DEFAULT_EXPAND_REAR_SEPARATION;
	u16 expand_block_size = DEFAULT_EXPAND_BLOCK_SIZE;
	u8 expand_low_cutoff = DEFAULT_EXPAND_LOW_CUTOFF;
	u8 expand_high_cutoff = DEFAULT_EXPAND_HIGH_CUTOFF;

	static constexpr AudioExpansionMode DEFAULT_EXPANSION_MODE = AudioExpansionMode::Disabled;
	static constexpr u16 DEFAULT_BUFFER_MS = 50;
	static constexpr u16 DEFAULT_OUTPUT_LATENCY_MS = 20;
	static constexpr bool DEFAULT_OUTPUT_LATENCY_MINIMAL = false;

	static constexpr u16 DEFAULT_EXPAND_BLOCK_SIZE = 2048;
	static constexpr float DEFAULT_EXPAND_CIRCULAR_WRAP = 90.0f;
	static constexpr float DEFAULT_EXPAND_SHIFT = 0.0f;
	static constexpr float DEFAULT_EXPAND_DEPTH = 1.0f;
	static constexpr float DEFAULT_EXPAND_FOCUS = 0.0f;
	static constexpr float DEFAULT_EXPAND_CENTER_IMAGE = 1.0f;
	static constexpr float DEFAULT_EXPAND_FRONT_SEPARATION = 1.0f;
	static constexpr float DEFAULT_EXPAND_REAR_SEPARATION = 1.0f;
	static constexpr u8 DEFAULT_EXPAND_LOW_CUTOFF = 40;
	static constexpr u8 DEFAULT_EXPAND_HIGH_CUTOFF = 90;

	static constexpr u16 DEFAULT_STRETCH_SEQUENCE_LENGTH = 30;
	static constexpr u16 DEFAULT_STRETCH_SEEKWINDOW = 20;
	static constexpr u16 DEFAULT_STRETCH_OVERLAP = 10;

	static constexpr bool DEFAULT_STRETCH_USE_QUICKSEEK = false;
	static constexpr bool DEFAULT_STRETCH_USE_AA_FILTER = false;

	void LoadSave(SettingsWrapper& wrap, const char* section);

	bool operator==(const AudioStreamParameters& rhs) const;
	bool operator!=(const AudioStreamParameters& rhs) const;
};
