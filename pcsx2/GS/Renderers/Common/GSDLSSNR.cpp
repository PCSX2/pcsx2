// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSDLSSNR.h"

#include "common/Console.h"

#ifdef ENABLE_DLSSNR

#include "nr_frame.h"

#include <cmath>
#include <vector>

namespace
{
	struct DLSSNRState
	{
		nr_frame* frame = nullptr;
		bool open_failed = false;
		bool error_reported = false;
		int frame_index = 0;

		u32 width = 0;
		u32 height = 0;
		bool have_history = false;

		std::vector<float> colour;
		std::vector<float> output;
		std::vector<float> history; // previous output
		std::vector<float> previous; // previous input
	};

	static DLSSNRState s_state;
} // namespace

bool GSDLSSNR::IsAvailable()
{
	return true;
}

static bool OpenFrame()
{
	if (s_state.frame)
		return true;
	if (s_state.open_failed)
		return false;

	// NULL: the weights compiled into libdlssnr.
	s_state.frame = nr_frame_open(nullptr);
	if (!s_state.frame)
	{
		Console.Error("DLSS-NR: nr_frame_open() failed: %s", nr_frame_error());
		s_state.open_failed = true;
		return false;
	}

	Console.WriteLn("DLSS-NR: running on %s (%s runtime, %s)", nr_frame_device(s_state.frame), nr_frame_runtime(),
		nr_frame_gemm_path(s_state.frame));
	return true;
}

bool GSDLSSNR::Process(u8* rgba, u32 width, u32 height, u32 stride, float intensity)
{
	if (width == 0 || height == 0 || !OpenFrame())
		return false;

	const size_t pixels = static_cast<size_t>(width) * height;
	if (width != s_state.width || height != s_state.height)
	{
		s_state.width = width;
		s_state.height = height;
		s_state.have_history = false;
		s_state.colour.resize(pixels * 3);
		s_state.output.resize(pixels * 3);
		s_state.history.resize(pixels * 3);
		s_state.previous.resize(pixels * 3);
	}

	// byte / 255, like image_io.py and nr_frame_main.c
	for (u32 y = 0; y < height; y++)
	{
		const u8* row = rgba + static_cast<size_t>(y) * stride;
		float* out = s_state.colour.data() + static_cast<size_t>(y) * width * 3;
		for (u32 x = 0; x < width; x++)
		{
			out[x * 3 + 0] = static_cast<float>(row[x * 4 + 0]) * (1.0f / 255.0f);
			out[x * 3 + 1] = static_cast<float>(row[x * 4 + 1]) * (1.0f / 255.0f);
			out[x * 3 + 2] = static_cast<float>(row[x * 4 + 2]) * (1.0f / 255.0f);
		}
	}

	nr_frame_params params;
	nr_frame_defaults(&params);
	params.intensity = intensity;
	params.frame_index = s_state.frame_index++;
	// A PS2 frame is small, run the graph at its own size rather than padding it out to 320.
	params.min_extent = 128;

	const float* history = s_state.have_history ? s_state.history.data() : nullptr;
	const float* previous = s_state.have_history ? s_state.previous.data() : nullptr;
	if (nr_frame_update(s_state.frame, s_state.colour.data(), static_cast<int>(height), static_cast<int>(width), history,
			previous, &params, s_state.output.data(), nullptr) != 0)
	{
		if (!s_state.error_reported)
		{
			Console.Error("DLSS-NR: nr_frame_update() failed: %s", nr_frame_error());
			s_state.error_reported = true;
		}
		return false;
	}

	s_state.history.swap(s_state.output);
	s_state.previous.swap(s_state.colour);
	s_state.have_history = true;

	// value * 255 + 0.5, the output is now in history
	const float* result = s_state.history.data();
	for (u32 y = 0; y < height; y++)
	{
		u8* row = rgba + static_cast<size_t>(y) * stride;
		const float* in = result + static_cast<size_t>(y) * width * 3;
		for (u32 x = 0; x < width; x++)
		{
			for (u32 c = 0; c < 3; c++)
			{
				const float v = std::fmin(std::fmax(in[x * 3 + c], 0.0f), 1.0f);
				row[x * 4 + c] = static_cast<u8>(v * 255.0f + 0.5f);
			}
		}
	}

	return true;
}

void GSDLSSNR::ResetHistory()
{
	s_state.have_history = false;
}

void GSDLSSNR::Shutdown()
{
	if (s_state.frame)
	{
		nr_frame_close(s_state.frame);
		nr_frame_shutdown();
		s_state.frame = nullptr;
	}
	s_state.have_history = false;
	s_state.error_reported = false;
}

#else

bool GSDLSSNR::IsAvailable()
{
	return false;
}

bool GSDLSSNR::Process(u8* rgba, u32 width, u32 height, u32 stride, float intensity)
{
	return false;
}

void GSDLSSNR::ResetHistory()
{
}

void GSDLSSNR::Shutdown()
{
}

#endif
