// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/Global.h"
#include "SPU2/spu2.h" // hopefully temporary, until I resolve lClocks depdendency
#include "IopMem.h"

#include <cstring>

namespace SPU2Savestate
{
	// Arbitrary ID to identify SPU2 saves.
	static constexpr u32 SAVE_ID = 0x1227521;

	// versioning for saves.
	// Increment this when changes to the savestate system are made.
	static constexpr u32 SAVE_VERSION = 0x000e;

	static void wipe_the_cache()
	{
		memset(pcm_cache_data, 0, pcm_BlockCount * sizeof(PcmCacheEntry));
	}
} // namespace SPU2Savestate

struct SPU2Savestate::DataBlock
{
	u32 spu2id;          // SPU2 state identifier lets ZeroGS/PeopsSPU2 know this isn't their state)
	u8 unkregs[0x10000]; // SPU2 raw register memory
	u8 mem[0x200000];    // SPU2 raw sample memory

	u32 version; // SPU2 version identifier
	V_Core Cores[2];
	V_SPDIF Spdif;
	u16 OutPos;
	u16 InputPos;
	u32 Cycles;
	u32 lClocks;
	int PlayMode;
};

s32 SPU2Savestate::FreezeIt(DataBlock& spud)
{
	spud.spu2id = SAVE_ID;
	spud.version = SAVE_VERSION;

	memcpy(spud.unkregs, spu2regs, sizeof(spud.unkregs));
	memcpy(spud.mem, _spu2mem, sizeof(spud.mem));

	memcpy(spud.Cores, Cores, sizeof(Cores));
	memcpy(&spud.Spdif, &Spdif, sizeof(Spdif));

	// Convert pointers to offsets so we can safely restore them when loading.
	// We use -1 for null, and anything else as an offset from iop memory.
#define FIX_POINTER(x) \
	if (!(x)) \
	{ \
		x = reinterpret_cast<decltype(x)>(-1); \
	} \
	else \
	{ \
		pxAssert(reinterpret_cast<const u8*>((x)) >= iopPhysMem(0) && reinterpret_cast<const u8*>((x)) < iopPhysMem(0x1fffff)); \
		x = reinterpret_cast<decltype(x)>(reinterpret_cast<const u8*>((x)) - iopPhysMem(0)); \
	}

	for (u32 i = 0; i < 2; i++)
	{
		V_Core& core = spud.Cores[i];
		FIX_POINTER(core.DMAPtr);
		FIX_POINTER(core.DMARPtr);
	}

#undef FIX_POINTER

	spud.OutPos = OutPos;
	spud.InputPos = InputPos;
	spud.Cycles = Cycles;
	spud.lClocks = lClocks;
	spud.PlayMode = PlayMode;

	// note: Don't save the cache.  PCSX2 doesn't offer a safe method of predicting
	// the required size of the savestate prior to saving, plus this is just too
	// "implementation specific" for the intended spec of a savestate.  Let's just
	// force the user to rebuild their cache instead.

	return 0;
}

s32 SPU2Savestate::ThawIt(DataBlock& spud)
{
	if (spud.spu2id != SAVE_ID || spud.version < SAVE_VERSION)
	{
		fprintf(stderr, "\n*** SPU2 Warning:\n");
		if (spud.spu2id == SAVE_ID)
			fprintf(stderr, "\tSavestate version is from an older version of PCSX2.\n");
		else
			fprintf(stderr, "\tThe savestate you are trying to load is incorrect or corrupted.\n");

		fprintf(stderr,
				"\tAudio may not recover correctly.  Save your game to memorycard, reset,\n\n"
				"\tand then continue from there.\n\n");

		// Do *not* reset the cores.
		// We'll need some "hints" as to how the cores should be initialized, and the
		// only way to get that is to use the game's existing core settings and hope
		// they kinda match the settings for the savestate (IRQ enables and such).

		// adpcm cache : Clear all the cache flags and buffers.

		wipe_the_cache();
	}
	else
	{
		SndBuffer::ClearContents();

		memcpy(spu2regs, spud.unkregs, sizeof(spud.unkregs));
		memcpy(_spu2mem, spud.mem, sizeof(spud.mem));

		memcpy(Cores, spud.Cores, sizeof(Cores));
		memcpy(&Spdif, &spud.Spdif, sizeof(Spdif));

		// Reverse the pointer offset from above.
#define FIX_POINTER(x) \
	if ((x) == reinterpret_cast<decltype(x)>(-1)) \
	{ \
		x = nullptr; \
	} \
	else \
	{ \
		pxAssert(reinterpret_cast<size_t>((x)) <= 0x1fffff); \
		x = reinterpret_cast<decltype(x)>(iopPhysMem(0) + reinterpret_cast<size_t>((x))); \
	}

		for (u32 i = 0; i < 2; i++)
		{
			V_Core& core = Cores[i];
			FIX_POINTER(core.DMAPtr);
			FIX_POINTER(core.DMARPtr);
		}

#undef FIX_POINTER

		OutPos = spud.OutPos;
		InputPos = spud.InputPos;
		Cycles = spud.Cycles;
		lClocks = spud.lClocks;
		PlayMode = spud.PlayMode;

		wipe_the_cache();

		// Go through the V_Voice structs and recalculate SBuffer pointer from
		// the NextA setting.

		for (int c = 0; c < 2; c++)
		{
			for (int v = 0; v < 24; v++)
			{
				const int cacheIdx = Cores[c].Voices[v].NextA / pcm_WordsPerBlock;
				Cores[c].Voices[v].SBuffer = pcm_cache_data[cacheIdx].Sampledata;
			}
		}
	}
	return 0;
}

s32 SPU2Savestate::SizeIt()
{
	return sizeof(DataBlock);
}
