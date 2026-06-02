// SPDX-FileCopyrightText: 2026 Frente C / DKWDRV Hacking Project
// SPDX-License-Identifier: GPL-3.0+
//
// XA-ADPCM Inject for PCSX2 Ps1CD.cpp — v8 (xa-working-bgm-v2)
//
// Decoder: pwndrv validated (5 K0/K1 filters, separate L/R history,
//          correct stereo nibble layout, 16-bit history clamp).
// Output:  Stereo-interleaved ring buffer → drain-side resampler in ReadInput.cpp.
// Fixes:   From xa-fix-pause-drain: soft-pause, ADMA re-apply, audio sector filtering,
//          CdlInit filter reset, STRSND mode auto-activate.

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "SPU2/defs.h"
#include "R3000A.h"

extern void SPU2writeDMA4Mem(u16* pMem, u32 size);
extern s16 _spu2mem[];

// ─── XA ADPCM Decode Constants (pwndrv validated, 5 filters) ────────────────

static const int32_t XA_K0[5] = {  0,   60,  115,  98, 122 };
static const int32_t XA_K1[5] = {  0,    0,  -52, -55, -60 };

// ─── Ring buffer (stereo-interleaved: L,R,L,R,...) ──────────────────────────

static constexpr int XA_PCM_RING_SIZE = 524288;  // ~7s at 37800Hz stereo
static constexpr uint32_t XA_RING_MASK = XA_PCM_RING_SIZE - 1;

// ─── Decoder State ──────────────────────────────────────────────────────────

struct XAInjectState {
	bool     active;
	bool     adma_started;
	int32_t  h1_l, h2_l;    // separate L/R history (pwndrv)
	int32_t  h1_r, h2_r;
	uint32_t sectors;
	uint64_t last_decode_ticks;
	uint8_t  cur_file;
	uint8_t  cur_chan;
	int      freq;           // 37800 or 18900
	int      stereo;         // 0=mono, 1=stereo
	int      nbits;          // 4 or 8
};

static XAInjectState s_xa = {};

// Shared globals for cross-TU visibility (ReadInput.cpp, spu2sys.cpp)
extern bool g_xa_adma_active;
extern bool g_xa_adma_stereo;
extern int16_t g_xa_pcm_ring[];
extern uint32_t g_xa_pcm_write;
extern uint32_t g_xa_pcm_read;
extern int g_xa_last_half_filled;
extern int g_xa_freq;

// ─── XA ADPCM Decode (pwndrv validated — bit-exact with Python reference) ───
//
// Key differences from v6:
//  - 5 filter coefficients (not 4) — filter index can be 0-4
//  - Separate L/R ADPCM history
//  - History clamped to 16-bit before storage (prevents overflow artifacts)
//  - Stereo nibble layout: byte at 16+s*4+(u/2), even u=low nibble (L), odd u=high (R)

static int xa_decode_sector_to_pcm(const uint8_t* raw_sector, int16_t* pcm_out, int max_samples)
{
	const uint8_t* subhdr = raw_sector + 4;
	const uint8_t* audio_data = raw_sector + 12;

	uint8_t coding = subhdr[3];
	int stereo = (coding & 0x01) ? 1 : 0;
	int freq = ((coding >> 2) & 0x03) ? 18900 : 37800;
	int nbits = ((coding >> 4) & 0x01) ? 8 : 4;

	// Reset history on format change
	if (freq != s_xa.freq || stereo != s_xa.stereo || nbits != s_xa.nbits) {
		s_xa.freq = freq;
		s_xa.stereo = stereo;
		s_xa.nbits = nbits;
		s_xa.h1_l = s_xa.h2_l = 0;
		s_xa.h1_r = s_xa.h2_r = 0;
		g_xa_freq = freq;
		g_xa_adma_stereo = stereo;
		Console.WriteLn("[XA-INJECT] Format: %dHz %dbit %s", freq, nbits, stereo ? "stereo" : "mono");
	}

	int out_idx = 0;
	int32_t h1l = s_xa.h1_l, h2l = s_xa.h2_l;
	int32_t h1r = s_xa.h1_r, h2r = s_xa.h2_r;

	for (int g = 0; g < 18; g++) {
		const uint8_t* grp = audio_data + g * 128;
		int num_units = (nbits == 4) ? 8 : 4;

		for (int u = 0; u < num_units; u++) {
			// Sound parameter index (pwndrv validated)
			int p_idx = (nbits == 4) ? ((u < 4) ? u : (u - 4 + 8)) : u;
			uint8_t sp = grp[p_idx];
			int filter = (sp >> 4) & 0x0F;
			if (filter > 4) filter = 0;  // safety: only 0-4 valid
			int range = sp & 0x0F;
			if (range > 12) range = 9;

			for (int s = 0; s < 28; s++) {
				int32_t sample;

				if (nbits == 4) {
					// Nibble layout (same for mono and stereo per pwndrv):
					// byte at 16 + s*4 + (u/2), even u = low nibble, odd u = high nibble
					int byte_pos = 16 + s * 4 + (u / 2);
					int get_high = (u & 1);
					uint8_t byte = grp[byte_pos];
					int nib = get_high ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
					sample = (int16_t)((uint16_t)(nib << 12)) >> range;
				} else {
					int byte_pos = 16 + s * 4 + u;
					uint8_t byte = grp[byte_pos];
					sample = (int16_t)((uint16_t)(byte << 8)) >> range;
				}

				// Apply filter + history (separate L/R channels)
				if (!stereo || (u % 2 == 0)) {
					// Left channel (or mono)
					sample += ((XA_K0[filter] * h1l) + (XA_K1[filter] * h2l) + 32) >> 6;
					h2l = h1l;
					h1l = std::clamp(sample, -32768, 32767);  // 16-bit clamp on history (ppc_decomp fix)
					if (out_idx < max_samples)
						pcm_out[out_idx++] = (int16_t)h1l;
				} else {
					// Right channel
					sample += ((XA_K0[filter] * h1r) + (XA_K1[filter] * h2r) + 32) >> 6;
					h2r = h1r;
					h1r = std::clamp(sample, -32768, 32767);  // 16-bit clamp on history
					if (out_idx < max_samples)
						pcm_out[out_idx++] = (int16_t)h1r;
				}
			}
		}
	}

	s_xa.h1_l = h1l; s_xa.h2_l = h2l;
	s_xa.h1_r = h1r; s_xa.h2_r = h2r;

	return out_idx;
}

// ─── Ring buffer feed (stereo-interleaved) ──────────────────────────────────

static void xa_feed_to_ring(int16_t* pcm, int num_samples)
{
	// For mono: duplicate each sample to L,R in ring
	// For stereo: samples already interleaved L,R,L,R from decoder
	int16_t peak = 0;

	if (!s_xa.stereo) {
		// Mono: write each sample as L,R pair
		for (int i = 0; i < num_samples; i++) {
			int16_t s = pcm[i];
			if (s > peak) peak = s;
			if (-s > peak) peak = -s;
			g_xa_pcm_ring[g_xa_pcm_write] = s;
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
			g_xa_pcm_ring[g_xa_pcm_write] = s;  // duplicate to R
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
			// Overflow: discard oldest
			if (((g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK) >= (XA_PCM_RING_SIZE - 2)) {
				g_xa_pcm_read = (g_xa_pcm_read + 2) & XA_RING_MASK;
			}
		}
	} else {
		// Stereo: already L,R,L,R
		for (int i = 0; i < num_samples; i++) {
			int16_t s = pcm[i];
			if (s > peak) peak = s;
			if (-s > peak) peak = -s;
			g_xa_pcm_ring[g_xa_pcm_write] = s;
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
			if (((g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK) >= (XA_PCM_RING_SIZE - 1)) {
				g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
			}
		}
	}

	// Log
	static int feed_count = 0;
	feed_count++;
	if (feed_count <= 10 || feed_count % 100 == 0) {
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
		Console.WriteLn("[XA-FEED] #%d: %d samples, peak=%d, ring_avail=%u, pcm[0..3]=%d %d %d %d",
			feed_count, num_samples, (int)peak, ring_avail,
			(int)pcm[0], (int)pcm[1], (int)pcm[2], (int)pcm[3]);
	}
}

// ─── ADMA Configuration ─────────────────────────────────────────────────────

static void xa_configure_adma()
{
	if (s_xa.adma_started) return;
	s_xa.adma_started = true;

	g_xa_pcm_write = 0;
	g_xa_pcm_read = 0;
	g_xa_last_half_filled = -1;
	g_xa_adma_stereo = s_xa.stereo;
	g_xa_adma_active = true;
	g_xa_freq = s_xa.freq;

	// Pre-fill ADMA input area with silence
	for (int i = 0; i < 0x200; i++) {
		_spu2mem[0x2000 + i] = 0;
		_spu2mem[0x2200 + i] = 0;
	}

	// Enable ADMA on Core 0
	Cores[0].AutoDMACtrl |= 1;
	Cores[0].InpVol.Left = 0x7FFF;
	Cores[0].InpVol.Right = 0x7FFF;
	Cores[0].InputDataLeft = 0x200;
	Cores[0].AdmaInProgress = 1;

	Console.WriteLn("[XA-INJECT] ADMA configured: AutoDMACtrl=%d InpVol=0x7FFF stereo=%d freq=%d",
		Cores[0].AutoDMACtrl, s_xa.stereo, s_xa.freq);
}

// ─── Reset ADMA flag (called from spu2sys.cpp when game clears AutoDMACtrl) ─

static void xa_inject_reset_adma_flag()
{
	s_xa.adma_started = false;
}

// ─── Process one XA sector ──────────────────────────────────────────────────

static void xa_inject_process(const uint8_t* transfer, [[maybe_unused]] int mode,
                              [[maybe_unused]] uint8_t file_filter, [[maybe_unused]] uint8_t chan_filter)
{
	if (!s_xa.active) return;

	// Check subheader: is this an audio sector?
	uint8_t submode = transfer[6];
	if (!(submode & 0x04)) return;  // bit 2 = audio

	// Filter check
	uint8_t file = transfer[4];
	uint8_t chan = transfer[5];
	if (s_xa.cur_file != 0 && file != s_xa.cur_file) return;
	if (s_xa.cur_chan != 0 && chan != s_xa.cur_chan) return;

	// Decode XA sector to PCM
	static int16_t pcm_buf[8192];
	int num_samples = xa_decode_sector_to_pcm(transfer, pcm_buf, 8192);
	if (num_samples <= 0) return;

	s_xa.sectors++;

	// Configure ADMA on first sector
	xa_configure_adma();

	// Feed to ring buffer
	xa_feed_to_ring(pcm_buf, num_samples);

	if (s_xa.sectors <= 5 || (s_xa.sectors % 200) == 0) {
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
		Console.WriteLn("[XA-INJECT] Sector %u: %d samples (file=%d ch=%d) ring_avail=%u ADMA_ctrl=%d InpVol=%04x",
			s_xa.sectors, num_samples, file, chan, ring_avail,
			Cores[0].AutoDMACtrl, (u16)Cores[0].InpVol.Left);
	}
}

// ─── Public API ─────────────────────────────────────────────────────────────

static void xa_inject_init()
{
	s_xa.active = true;
	s_xa.adma_started = false;
	s_xa.h1_l = s_xa.h2_l = 0;
	s_xa.h1_r = s_xa.h2_r = 0;
	g_xa_pcm_write = 0;
	g_xa_pcm_read = 0;
	g_xa_adma_active = false;
	s_xa.sectors = 0;
	s_xa.last_decode_ticks = 0;
	s_xa.freq = 0;
	s_xa.stereo = 0;
	s_xa.nbits = 0;
	Console.WriteLn("[XA-INJECT] Started XA decode, ADMA output to Core 0");
}

static void xa_inject_stop()
{
	if (!s_xa.active) return;
	s_xa.active = false;
	s_xa.adma_started = false;
	g_xa_adma_active = false;
	// Restore Core 0 ADMA state
	Cores[0].AutoDMACtrl &= ~1;
	Cores[0].InputDataLeft = 0;
	Cores[0].AdmaInProgress = 0;
	Cores[0].InpVol.Left = 0;
	Cores[0].InpVol.Right = 0;
	// Flush ring
	g_xa_pcm_write = 0;
	g_xa_pcm_read = 0;
	Console.WriteLn("[XA-INJECT] Stopped (%u sectors decoded)", s_xa.sectors);
}

static void xa_inject_setfilter(uint8_t file, uint8_t chan)
{
	if (file != s_xa.cur_file || chan != s_xa.cur_chan) {
		s_xa.cur_file = file;
		s_xa.cur_chan = chan;
		// Reset history on filter change (new track)
		s_xa.h1_l = s_xa.h2_l = 0;
		s_xa.h1_r = s_xa.h2_r = 0;
		// Preserve ADMA — don't reset adma_started (xa-fix-pause-drain lesson)
		Console.WriteLn("[XA-INJECT] SetFilter: file=%d channel=%d (history reset, ADMA preserved)", file, chan);
	}
}
