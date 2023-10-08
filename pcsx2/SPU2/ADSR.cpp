/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "SPU2/Global.h"

#include "common/Assertions.h"

#include <array>

static constexpr s32 ADSR_MAX_VOL = 0x7fff;

static const int InvExpOffsets[] = {0, 4, 6, 8, 9, 10, 11, 12};

using PSXRateTable = std::array<u32, 160>;

static constexpr PSXRateTable ComputePSXRates()
{
	PSXRateTable rates = {};
	for (int i = 0; i < (32 + 128); i++)
	{
		const int shift = (i - 32) >> 2;
		s64 rate = (i & 3) + 4;
		if (shift < 0)
			rate >>= -shift;
		else
			rate <<= shift;

		// Maximum rate is 0x4000.
		rates[i] = (int)std::min(rate, (s64)0x40000000LL);
	}
	return rates;
}

static constexpr const PSXRateTable PsxRates = ComputePSXRates();

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
	s16 level_inc = (s16)(p.Step << std::max(0, 11 - p.Shift));

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
		// Shift and sign extend;
		Value = (s16)(src << 1);
	}
}

void V_VolumeSlide::Update()
{
	if (!Enable)
	{
		return;
	}
}
