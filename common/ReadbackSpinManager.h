// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Pcsx2Defs.h"

#include <vector>

/// A class for calculating optimal spin values to trick OSes into not powering down GPUs while waiting for readbacks
class ReadbackSpinManager
{
public:
	struct Event
	{
		s64 size;
		u32 begin;
		u32 end;
	};

private:
	double m_spins_per_unit_time = 0;
	double m_total_spin_time = 0;
	double m_total_spin_cycles = 0;
	std::vector<Event> m_frames[3];
	u32 m_current_frame = 0;
	u32 m_reference_frame = 0;
	u32 m_reference_frame_idx = 0;

public:
	struct DrawSubmittedReturn
	{
		u32 id;
		u32 recommended_spin;
	};

	/// Call when a readback is requested
	void ReadbackRequested();
	/// Call at the end of a frame
	void NextFrame();
	/// Call when a command buffer is submitted to the GPU
	/// `size` is used to attempt to find patterns in submissions, and can be any metric that approximates the amount of work in a submission (draw calls, command encoders, etc)
	/// Returns an id to be passed to `DrawCompleted`, and the recommended number of spin cycles to perform on the GPU in order to keep it busy
	DrawSubmittedReturn DrawSubmitted(u64 size);
	/// Call once a draw has been finished by the GPU and you have begin/end data for it
	/// `begin_time` and `end_time` can be in any unit as long as it's consistent.  It's okay if they roll over, as long as it happens less than once every few frames.
	void DrawCompleted(u32 id, u32 begin_time, u32 end_time);
	/// Call when a spin completes to help the manager figure out how quickly your GPU spins
	void SpinCompleted(u32 cycles, u32 begin_time, u32 end_time);
	/// Get the calculated number of spins per unit of time
	/// Note: May be zero when there's insufficient data
	double SpinsPerUnitTime() const { return m_spins_per_unit_time; }
};
