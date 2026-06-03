// SPDX-FileCopyrightText: 2026 Frente C / DKWDRV Hacking Project
// SPDX-License-Identifier: GPL-3.0+
//
// XA-ADPCM Inject for PCSX2 Ps1CD.cpp — v9 (xa-working-bgm-v2)
//
// Architecture: Sector FIFO + demand-driven decode.
//   CD read → raw sector FIFO (never dropped)
//   ADMA fill callback → decode from FIFO into PCM ring as needed
//   Resampler drains ring at correct pitch (37800/18900 → 48000 Hz)
//
// This eliminates both overflow (ring never overfills) and starvation
// (full stream is preserved in sector FIFO).

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

// ─── PCM Ring buffer (stereo-interleaved: L,R,L,R,...) ──────────────────────

static constexpr int XA_PCM_RING_SIZE = 524288;  // ~7s at 37800Hz stereo
static constexpr uint32_t XA_RING_MASK = XA_PCM_RING_SIZE - 1;

// ─── Sector FIFO (raw 2336-byte XA audio payloads) ──────────────────────────

static constexpr int XA_SECTOR_SIZE = 2336;        // raw audio payload per sector
static constexpr int XA_SECTOR_FIFO_COUNT = 4096;  // ~218s at 37800Hz stereo
static constexpr uint32_t XA_SECTOR_FIFO_MASK = XA_SECTOR_FIFO_COUNT - 1;

// Sector FIFO storage (defined in Ps1CD.cpp alongside other globals)
extern uint8_t g_xa_sector_fifo[];           // [XA_SECTOR_FIFO_COUNT * XA_SECTOR_SIZE]
extern uint32_t g_xa_sector_fifo_write;       // write index (sectors)
extern uint32_t g_xa_sector_fifo_read;        // read index (sectors)

// ─── Decoder State ──────────────────────────────────────────────────────────

struct XAInjectState {
	bool     active;
	bool     adma_started;
	int32_t  h1_l, h2_l;    // separate L/R history (pwndrv)
	int32_t  h1_r, h2_r;
	uint32_t sectors;        // total sectors received from CD
	uint32_t decoded;        // total sectors decoded from FIFO
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

static int xa_decode_sector_to_pcm(const uint8_t* raw_sector, int16_t* pcm_out, int max_samples)
{
	const uint8_t* subhdr = raw_sector + 4;
	const uint8_t* audio_data = raw_sector + 12;

	uint8_t coding = subhdr[3];
	int stereo = (coding & 0x01) ? 1 : 0;
	int freq = ((coding >> 2) & 0x03) ? 18900 : 37800;
	int nbits = ((coding >> 4) & 0x01) ? 8 : 4;

	// Update format on change
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
			int p_idx = (nbits == 4) ? ((u < 4) ? u : (u - 4 + 8)) : u;
			uint8_t sp = grp[p_idx];
			int filter = (sp >> 4) & 0x0F;
			if (filter > 4) filter = 0;
			int range = sp & 0x0F;
			if (range > 12) range = 9;

			for (int s = 0; s < 28; s++) {
				int32_t sample;

				if (nbits == 4) {
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

				if (!stereo || (u % 2 == 0)) {
					sample += ((XA_K0[filter] * h1l) + (XA_K1[filter] * h2l) + 32) >> 6;
					h2l = h1l;
					h1l = std::clamp(sample, -32768, 32767);
					if (out_idx < max_samples)
						pcm_out[out_idx++] = (int16_t)h1l;
				} else {
					sample += ((XA_K0[filter] * h1r) + (XA_K1[filter] * h2r) + 32) >> 6;
					h2r = h1r;
					h1r = std::clamp(sample, -32768, 32767);
					if (out_idx < max_samples)
						pcm_out[out_idx++] = (int16_t)h1r;
				}
			}
		}
	}

	s_xa.h1_l = h1l; s_xa.h2_l = h2l;
	s_xa.h1_r = h1r; s_xa.h2_r = h2r;

	// [INSTRUMENT] Log raw ADPCM decode output (first 8 samples) periodically
	static uint32_t decode_log_ctr = 0;
	decode_log_ctr++;
	if (decode_log_ctr <= 5 || decode_log_ctr % 200 == 0) {
		Console.WriteLn("[XA-DECODE] #%u out_idx=%d filt=%d range=%d samples: %d %d %d %d %d %d %d %d",
			decode_log_ctr, out_idx,
			(audio_data[0] >> 4) & 0xF, audio_data[0] & 0xF,
			out_idx > 0 ? (int)pcm_out[0] : 0, out_idx > 1 ? (int)pcm_out[1] : 0,
			out_idx > 2 ? (int)pcm_out[2] : 0, out_idx > 3 ? (int)pcm_out[3] : 0,
			out_idx > 4 ? (int)pcm_out[4] : 0, out_idx > 5 ? (int)pcm_out[5] : 0,
			out_idx > 6 ? (int)pcm_out[6] : 0, out_idx > 7 ? (int)pcm_out[7] : 0);
	}

	return out_idx;
}

// ─── Feed decoded PCM to ring (stereo-interleaved) ──────────────────────────

static void xa_feed_to_ring(int16_t* pcm, int num_samples)
{
	if (!s_xa.stereo) {
		// Mono: duplicate each sample as L,R pair
		for (int i = 0; i < num_samples; i++) {
			g_xa_pcm_ring[g_xa_pcm_write] = pcm[i];
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
			g_xa_pcm_ring[g_xa_pcm_write] = pcm[i];
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
		}
	} else {
		// Stereo: already L,R,L,R
		for (int i = 0; i < num_samples; i++) {
			g_xa_pcm_ring[g_xa_pcm_write] = pcm[i];
			g_xa_pcm_write = (g_xa_pcm_write + 1) & XA_RING_MASK;
		}
	}
}

// ─── Demand-driven decode: called from ADMA fill to keep ring fed ───────────
// Decodes sectors from FIFO until ring has at least target_samples available
// or FIFO is empty. Called from ReadInput.cpp ADMA fill path.

static void xa_pump_fifo_to_ring(uint32_t target_ring_avail)
{
	static int16_t pcm_buf[8192];

	while (true) {
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
		if (ring_avail >= target_ring_avail)
			break;

		// Check if FIFO has sectors
		if (g_xa_sector_fifo_read == g_xa_sector_fifo_write)
			break;  // FIFO empty

		// Decode next sector from FIFO
		uint8_t* sector = &g_xa_sector_fifo[g_xa_sector_fifo_read * XA_SECTOR_SIZE];
		g_xa_sector_fifo_read = (g_xa_sector_fifo_read + 1) & XA_SECTOR_FIFO_MASK;

		int num_samples = xa_decode_sector_to_pcm(sector, pcm_buf, 8192);
		if (num_samples <= 0)
			continue;

		s_xa.decoded++;
		xa_feed_to_ring(pcm_buf, num_samples);
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

	// Route ADMA to dry output only — disable wet (reverb) path
	// On real PS1, XA audio goes through CD input volume, not SPU reverb
	Cores[0].DryGate.InpL = -1;
	Cores[0].DryGate.InpR = -1;
	Cores[0].WetGate.InpL = 0;
	Cores[0].WetGate.InpR = 0;

	Console.WriteLn("[XA-INJECT] ADMA configured: AutoDMACtrl=%d InpVol=0x7FFF stereo=%d freq=%d",
		Cores[0].AutoDMACtrl, s_xa.stereo, s_xa.freq);
}

// ─── Process one XA sector from CD read (push to FIFO) ─────────────────────

static void xa_inject_process(const uint8_t* transfer, [[maybe_unused]] int mode,
                              [[maybe_unused]] uint8_t file_filter, [[maybe_unused]] uint8_t chan_filter)
{
	if (!s_xa.active) return;

	// Check subheader: is this an audio sector?
	uint8_t submode = transfer[6];
	if (!(submode & 0x04)) return;  // bit 2 = audio

	// Filter check — only apply if MODE_SF (bit 3) is set in cd mode
	uint8_t file = transfer[4];
	uint8_t chan = transfer[5];
	if (mode & 0x08) {  // MODE_SF: channel filter enabled
		if (s_xa.cur_file != 0 && file != s_xa.cur_file) return;
		if (s_xa.cur_chan != 0 && chan != s_xa.cur_chan) return;
	}

	s_xa.sectors++;

	// First sector: detect format and configure ADMA
	if (s_xa.sectors == 1) {
		// Parse format from this sector to set freq/stereo before ADMA configure
		uint8_t coding = transfer[7];  // subheader byte 3
		int stereo = (coding & 0x01) ? 1 : 0;
		int freq = ((coding >> 2) & 0x03) ? 18900 : 37800;
		int nbits = ((coding >> 4) & 0x01) ? 8 : 4;
		s_xa.freq = freq;
		s_xa.stereo = stereo;
		s_xa.nbits = nbits;
		g_xa_freq = freq;
		g_xa_adma_stereo = stereo;
		Console.WriteLn("[XA-INJECT] Format: %dHz %dbit %s", freq, nbits, stereo ? "stereo" : "mono");
		xa_configure_adma();
	}

	// Push raw sector to FIFO
	uint32_t next_write = (g_xa_sector_fifo_write + 1) & XA_SECTOR_FIFO_MASK;
	if (next_write == g_xa_sector_fifo_read) {
		// FIFO full — drop oldest (shouldn't happen with 4096 sectors)
		g_xa_sector_fifo_read = (g_xa_sector_fifo_read + 1) & XA_SECTOR_FIFO_MASK;
	}
	memcpy(&g_xa_sector_fifo[g_xa_sector_fifo_write * XA_SECTOR_SIZE], transfer, XA_SECTOR_SIZE);
	g_xa_sector_fifo_write = next_write;

	if (s_xa.sectors <= 5 || (s_xa.sectors % 200) == 0) {
		uint32_t fifo_avail = (g_xa_sector_fifo_write - g_xa_sector_fifo_read) & XA_SECTOR_FIFO_MASK;
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
		Console.WriteLn("[XA-INJECT] Sector %u: (file=%d ch=%d) fifo=%u ring_avail=%u",
			s_xa.sectors, file, chan, fifo_avail, ring_avail);
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
	s_xa.decoded = 0;
	s_xa.freq = 0;
	s_xa.stereo = 0;
	s_xa.nbits = 0;
	g_xa_sector_fifo_write = 0;
	g_xa_sector_fifo_read = 0;
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
	// Zero ADMA buffer to prevent stale looping
	for (int i = 0; i < 0x200; i++) {
		_spu2mem[0x2000 + i] = 0;  // Left
		_spu2mem[0x2200 + i] = 0;  // Right
	}
	// Flush ring + FIFO
	g_xa_pcm_write = 0;
	g_xa_pcm_read = 0;
	g_xa_sector_fifo_write = 0;
	g_xa_sector_fifo_read = 0;
	Console.WriteLn("[XA-INJECT] Stopped (%u sectors received, %u decoded)", s_xa.sectors, s_xa.decoded);
}

static void xa_inject_setfilter(uint8_t file, uint8_t chan)
{
	if (file != s_xa.cur_file || chan != s_xa.cur_chan) {
		s_xa.cur_file = file;
		s_xa.cur_chan = chan;
		// Reset history on filter change (new track)
		s_xa.h1_l = s_xa.h2_l = 0;
		s_xa.h1_r = s_xa.h2_r = 0;
		// Flush ring + FIFO on track change
		g_xa_pcm_write = 0;
		g_xa_pcm_read = 0;
		g_xa_sector_fifo_write = 0;
		g_xa_sector_fifo_read = 0;
		s_xa.sectors = 0;
		s_xa.decoded = 0;
		s_xa.adma_started = false;  // force re-configure on next sector
		s_xa.freq = 0;
		Console.WriteLn("[XA-INJECT] SetFilter: file=%d channel=%d (full reset)", file, chan);
	}
}
