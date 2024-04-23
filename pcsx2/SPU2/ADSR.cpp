// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/defs.h"

#include "common/Assertions.h"

static constexpr s32 ADSR_MAX_VOL = 0x7fff;

void V_ADSR::UpdateCache()
{
	CachedPhases[PHASE_ATTACK].Decr = false;
	CachedPhases[PHASE_ATTACK].Exp = AttackMode;
	CachedPhases[PHASE_ATTACK].Shift = AttackShift;
	CachedPhases[PHASE_ATTACK].Step = 7 - AttackStep;
	CachedPhases[PHASE_ATTACK].Target = ADSR_MAX_VOL;

	CachedPhases[PHASE_DECAY].Decr = true;
	CachedPhases[PHASE_DECAY].Exp = true;
	CachedPhases[PHASE_DECAY].Shift = DecayShift;
	CachedPhases[PHASE_DECAY].Step = -8;
	CachedPhases[PHASE_DECAY].Target = (SustainLevel + 1) << 11;

	CachedPhases[PHASE_SUSTAIN].Decr = SustainDir;
	CachedPhases[PHASE_SUSTAIN].Exp = SustainMode;
	CachedPhases[PHASE_SUSTAIN].Shift = SustainShift;
	CachedPhases[PHASE_SUSTAIN].Step = 7 - SustainStep;

	if (CachedPhases[PHASE_SUSTAIN].Decr)
		CachedPhases[PHASE_SUSTAIN].Step = ~CachedPhases[PHASE_SUSTAIN].Step;

	CachedPhases[PHASE_SUSTAIN].Target = 0;

	CachedPhases[PHASE_RELEASE].Decr = true;
	CachedPhases[PHASE_RELEASE].Exp = ReleaseMode;
	CachedPhases[PHASE_RELEASE].Shift = ReleaseShift;
	CachedPhases[PHASE_RELEASE].Step = -8;
	CachedPhases[PHASE_RELEASE].Target = 0;
}

bool V_ADSR::Calculate(int voiceidx)
{
	pxAssume(Phase != PHASE_STOPPED);

	auto& p = CachedPhases.at(Phase);

	// maybe not correct for the "infinite" settings
	u32 counter_inc = 0x8000 >> std::max(0, p.Shift - 11);
	s32 level_inc = p.Step << std::max(0, 11 - p.Shift);

	if (p.Exp)
	{
		if (!p.Decr && Value > 0x6000)
		{
			counter_inc >>= 2;
		}

		if (p.Decr)
		{
			level_inc = (s16)((level_inc * Value) >> 15);
		}
	}

	counter_inc = std::max<u32>(1, counter_inc);
	Counter += counter_inc;

	if (Counter >= 0x8000)
	{
		Counter = 0;
		Value = std::clamp<s32>(Value + level_inc, 0, INT16_MAX);
	}

	// Stay in sustain until key off or silence
	if (Phase == PHASE_SUSTAIN)
	{
		return Value != 0;
	}

	// Check if target is reached to advance phase
	if ((!p.Decr && Value >= p.Target) || (p.Decr && Value <= p.Target))
	{
		Phase++;
	}

	// All phases done, stop the voice
	if (Phase > PHASE_RELEASE)
	{
		return false;
	}

	return true;
}

void V_ADSR::Attack()
{
	Phase = PHASE_ATTACK;
	Counter = 0;
	Value = 0;
}

void V_ADSR::Release()
{
	if (Phase != PHASE_STOPPED)
	{
		Phase = PHASE_RELEASE;
		Counter = 0;
	}
}

void V_VolumeSlide::RegSet(u16 src)
{
	Reg_VOL = src;
	if (!Enable)
	{
		Value = SignExtend16(src << 1);
	}
}

void V_VolumeSlide::Update()
{
	if (!Enable)
		return;

	s32 step_size = 7 - Step;

	if (Decr)
	{
		step_size = ~step_size;
	}

	u32 counter_inc = 0x8000 >> std::max(0, Shift - 11);
	s32 level_inc = step_size << std::max(0, 11 - Shift);

	if (Exp)
	{
		if (!Decr && Value > 0x6000)
		{
			counter_inc >>= 2;
		}

		if (Decr)
		{
			level_inc = (s16)((level_inc * Value) >> 15);
		}
	}

	// Allow counter_inc to be zero only in when all bits
	// of the rate field are set
	if (Step != 3 && Shift != 0x1f)
	{
		counter_inc = std::max<u32>(1, counter_inc);
	}
	Counter += counter_inc;

	// If negative phase "increase" to -0x8000 or "decrease" towards 0
	// Unless in Exp + Decr modes
	if (!(Exp && Decr))
	{
		level_inc = Phase ? -level_inc : level_inc;
	}

	if (Counter >= 0x8000)
	{
		Counter = 0;

		if (!Decr)
		{
			Value = std::clamp<s32>(Value + level_inc, INT16_MIN, INT16_MAX);
		}
		else
		{
			s32 low = Phase ? INT16_MIN : 0;
			s32 high = Phase ? 0 : INT16_MAX;
			if (Exp)
			{
				low = 0;
				high = INT16_MAX;
			}
			Value = std::clamp<s32>(Value + level_inc, low, high);
		}
	}
}
