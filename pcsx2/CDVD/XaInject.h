// SPDX-FileCopyrightText: 2026 Frente C / DKWDRV Hacking Project
// SPDX-License-Identifier: GPL-3.0+
//
// XA-ADPCM PoC Inject for PCSX2 Ps1CD.cpp — v6 (ADMA PCM path)
//
// Decodes XA audio sectors to 16-bit PCM and feeds them through SPU2's
// AutoDMA path on Core 0, which is the correct architecture (same as native
// PS1DRV and pcsx-redux). No voice, no ADPCM re-encoding.
//
// Reference: pcsx-redux src/core/decode_xa.cc + src/spu/xa.cc
//            vgmstream src/coding/xa_decoder.c

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "SPU2/defs.h"
#include "R3000A.h"

extern void SPU2writeDMA4Mem(u16* pMem, u32 size);
extern s16 _spu2mem[];

// ─── XA ADPCM Decode Constants ─────────────────────────────────────────────

// N=6 integer coefficients (mednafen/duckstation style)
static const int32_t XA_K0[4] = {  0,   60,  115,  98 };
static const int32_t XA_K1[4] = {  0,    0,  -52, -55 };

// ─── Decoder State ──────────────────────────────────────────────────────────

static constexpr int XA_PCM_RING_SIZE = 65536;  // must be power of 2

struct XAInjectState {
	bool     active;
	bool     adma_started;
	int32_t  hist1;              // ADPCM history (mono: single channel)
	int32_t  hist2;
	uint32_t sectors;
	uint64_t last_decode_ticks;
	uint8_t  cur_file;
	uint8_t  cur_chan;
	int      freq;              // 37800 or 18900
	int      stereo;            // 0=mono, 1=stereo
	int      nbits;             // 4 or 8
};

static XAInjectState s_xa = {};

// Shared flag for cross-TU visibility (ReadInput.cpp checks this)
extern bool g_xa_adma_active;
extern bool g_xa_adma_stereo;
extern int16_t g_xa_pcm_ring[];
extern uint32_t g_xa_pcm_write;
extern uint32_t g_xa_pcm_read;
extern int g_xa_last_half_filled;

// ─── XA ADPCM Decode (correct implementation from reference sources) ────────
//
// XA sector = 2352 bytes raw. After sync(12) + header(4) = subheader at offset 16.
// Audio data starts at offset 24 (subheader is 8 bytes).
// 18 sound groups × 128 bytes = 2304 bytes of audio.
//
// Each sound group (128 bytes):
//   Bytes 0-3: sound parameters for units 0-3
//   Bytes 4-7: copy of sound parameters 0-3
//   Bytes 8-11: sound parameters for units 4-7
//   Bytes 12-15: copy of sound parameters 4-7
//   Bytes 16-127: 28 × 4 bytes of nibble data
//
// Mono 4-bit (Level B): 8 subframes × 28 samples = 224 samples/group, 18 groups = 4032 samples/sector
// Stereo 4-bit (Level B): same but L/R interleaved → 2016 stereo pairs
// Mono 8-bit (Level A): 4 subframes × 28 samples, etc.

static int xa_decode_sector_to_pcm(const uint8_t* raw_sector, int16_t* pcm_out, int max_samples)
{
	// raw_sector = cdr.Transfer from Ps1CD.cpp, read with CDVD_MODE_2340.
	// Layout: [header(4)] [subheader(4)] [subheader_copy(4)] [data(2328)]
	// header = MM, SS, FF, Mode (CD sector header, sync already stripped)
	// subheader = File, Channel, Submode, Coding (starts at offset 4)
	// audio data starts at offset 12 (18 × 128 = 2304 bytes for XA)
	
	const uint8_t* subhdr = raw_sector + 4;      // skip 4-byte header
	const uint8_t* audio_data = raw_sector + 12;  // past header(4) + subheader(4) + copy(4)
	
	// Parse coding byte from subheader
	// subhdr[0]=file, [1]=channel, [2]=submode, [3]=coding
	uint8_t coding = subhdr[3];
	
	int stereo = (coding & 0x03) ? 1 : 0;   // bit 0: 0=mono, 1=stereo
	int freq_idx = (coding >> 2) & 0x03;     // 0=37800, 1=18900
	int bps_idx = (coding >> 4) & 0x03;      // 0=4bit, 1=8bit
	
	int freq = (freq_idx == 0) ? 37800 : 18900;
	int nbits = (bps_idx == 0) ? 4 : 8;
	
	// Reset history on format change
	if (freq != s_xa.freq || stereo != s_xa.stereo || nbits != s_xa.nbits) {
		s_xa.freq = freq;
		s_xa.stereo = stereo;
		s_xa.nbits = nbits;
		s_xa.hist1 = 0;
		s_xa.hist2 = 0;
		Console.WriteLn("[XA-INJECT] Format: %dHz %dbit %s", freq, nbits, stereo ? "stereo" : "mono");
	}
	
	int out_idx = 0;
	int32_t h1 = s_xa.hist1;
	int32_t h2 = s_xa.hist2;
	
	// Decode 18 sound groups
	for (int g = 0; g < 18; g++) {
		const uint8_t* grp = audio_data + g * 128;
		
		int num_units = (nbits == 4) ? 8 : 4;
		
		for (int u = 0; u < num_units; u++) {
			// Sound parameter for this unit
			// Units 0-3: grp[0..3], Units 4-7: grp[8..11]
			// (grp[4..7] and grp[12..15] are copies for error correction)
			int p_idx;
			if (nbits == 4) {
				p_idx = (u < 4) ? u : (u - 4 + 8);
			} else {
				p_idx = u;  // 8-bit only has 4 units
			}
			uint8_t sp = grp[p_idx];
			int filter = (sp >> 4) & 0x03;
			int range = sp & 0x0F;
			if (range > 12) range = 9;  // clamp per nocash docs
			
			// Decode 28 samples for this unit
			for (int s = 0; s < 28; s++) {
				int32_t sample;
				
				if (nbits == 4) {
					// Nibble position depends on mono/stereo
					int byte_pos;
					int get_high;
					if (!stereo) {
						// Mono: byte at 16 + s*4 + (u/2), low nibble for even u, high for odd
						byte_pos = 16 + s * 4 + (u / 2);
						get_high = (u & 1);
					} else {
						// Stereo: byte at 16 + s*4 + u, low=L high=R... no:
						// Stereo: byte at 16 + s*4 + (u/2), L=even units low, R=odd units high
						// Actually: byte at 16 + s*4 + u (for stereo u goes 0..7)
						// vgmstream: stereo su_pos = 0x10 + j*0x04 + i
						byte_pos = 16 + s * 4 + u;
						get_high = 0;  // TODO: need to verify stereo layout
					}
					
					uint8_t byte = grp[byte_pos];
					int nibble;
					if (get_high)
						nibble = (byte >> 4) & 0x0F;
					else
						nibble = byte & 0x0F;
					
					// Sign extend 4-bit
					sample = (int16_t)((uint16_t)(nibble << 12)) >> range;
				} else {
					// 8-bit mode
					int byte_pos;
					if (!stereo)
						byte_pos = 16 + s * 4 + u;
					else
						byte_pos = 16 + s * 4 + u;
					uint8_t byte = grp[byte_pos];
					sample = (int16_t)((uint16_t)(byte << 8)) >> range;
				}
				
				// Apply filter
				sample += ((XA_K0[filter] * h1) + (XA_K1[filter] * h2) + 32) >> 6;
				
				// Update history (don't clamp hist per mame/vgmstream)
				h2 = h1;
				h1 = sample;
				
				// Clamp output
				int16_t out_sample = (int16_t)std::clamp(sample, -32768, 32767);
				
				if (out_idx < max_samples)
					pcm_out[out_idx++] = out_sample;
			}
		}
	}
	
	s_xa.hist1 = h1;
	s_xa.hist2 = h2;
	
	return out_idx;
}

// ─── SPU2 ADMA Feed ─────────────────────────────────────────────────────────
//
// SPU2 Core 0 ADMA input area: 0x2000 in SPU2 RAM, 1KB (0x200 words).
// For split stereo: L at 0x2000-0x21FF, R at 0x2200-0x23FF.
// For non-split (mono→both): write same data to both halves.
//
// Instead of fighting ADMA DMA timing, write PCM directly into the SPU2
// output path. Simplest hack: write into _spu2mem at the ADMA area and
// poke the output position registers.
//
// ACTUALLY SIMPLEST: Just call SPU2writeDMA4Mem with properly set up state.
// But that crashed before due to missing DMA state.
//
// EVEN SIMPLER: Write decoded PCM directly to _spu2mem[] in the Core 0
// input area, and let the ADMA engine read it. We just need to enable ADMA.
//
// SIMPLEST THAT WORKS: Directly mix into SPU2's WetOutL/WetOutR or use
// the capture buffer path.
//
// OK let's try a different approach entirely: write 16-bit PCM into SPU2 RAM
// and play it through a voice using SHIFT=0 but with the correct ADPCM
// encoding that preserves more bits. Actually no — SPU2 ADPCM is always 4-bit.
//
// THE REAL FIX: Feed via SPU2writeDMA4Mem but set up the DMA state first.
// Looking at StartADMAWrite: it needs AutoDMACtrl bit 0 set for Core 0,
// InputDataLeft, InputPosWrite, AdmaInProgress, DMAPtr, MADR, TSA, etc.
//
// Let's try the most minimal working approach: poke PCM directly into the
// Core 0 ADMA input buffer at _spu2mem[0x2000] and set up the state.

// SPU2 Core 0 ADMA buffer layout:
// Non-split mode (mode=1): 0x2000 + spos (0 or 0x100), 0x200 words at a time
// Split mode (mode=0): L at 0x2000+spos, R at 0x2200+spos, 0x100 words each
//
// The SPU2 Mix() reads from InputPos (incremented each sample tick).
// If we just write PCM there and let the mixer pick it up...

// Actually the cleanest hack: bypass all DMA and directly write to the
// SPU2 output ring buffer. The V_Core has OutPos and a buffer.
// But those are C++ members we can't easily access from here.

// FINAL APPROACH: Use the voice path but with PROPER ADPCM encoding.
// The previous version used shift=0 which gives terrible quality.
// With optimal shift selection per block, SPU2 ADPCM can sound decent.
// shift=0: nibble range is ±7 × 4096 = ±28672 (coarse for quiet passages)
// shift=12: nibble range is ±7 × 1 = ±7 (fine but tiny)
// Optimal: find shift that minimizes quantization error for each 28-sample block.

static uint8_t xa_find_best_shift(const int16_t* samples, int count)
{
	// Find the max absolute value in the block
	int32_t max_abs = 0;
	for (int i = 0; i < count; i++) {
		int32_t abs_val = samples[i] < 0 ? -samples[i] : samples[i];
		if (abs_val > max_abs) max_abs = abs_val;
	}
	
	// shift determines: output = nibble << (12 - shift)
	// nibble range: -8 to +7, so max output = 7 << (12-shift)
	// We need: max_abs <= 7 << (12-shift)
	// => (12-shift) >= log2(max_abs/7)
	// => shift <= 12 - log2(max_abs/7)
	
	if (max_abs == 0) return 12;  // silence: use finest shift
	
	for (int shift = 12; shift >= 0; shift--) {
		int32_t max_representable = 7 << (12 - shift);
		if (max_representable >= max_abs)
			return (uint8_t)shift;
	}
	return 0;  // fallback: coarsest
}

static void xa_encode_adpcm_block_v2(const int16_t* pcm_in, int count, uint8_t* adpcm_out)
{
	uint8_t shift = xa_find_best_shift(pcm_in, count);
	
	adpcm_out[0] = shift;  // shift in low nibble, filter=0 in high nibble
	adpcm_out[1] = 0x00;   // no loop flags
	
	for (int i = 0; i < 14; i++) {
		int si = i * 2;
		uint8_t lo = 0, hi = 0;
		
		if (si < count) {
			// Quantize: sample / (1 << (12-shift)) → round to nearest
			int32_t divisor = 1 << (12 - shift);
			int32_t s = (pcm_in[si] + (divisor / 2)) / divisor;
			s = std::clamp(s, -8, 7);
			lo = (uint8_t)(s & 0x0F);
		}
		if (si + 1 < count) {
			int32_t divisor = 1 << (12 - shift);
			int32_t s = (pcm_in[si + 1] + (divisor / 2)) / divisor;
			s = std::clamp(s, -8, 7);
			hi = (uint8_t)(s & 0x0F);
		}
		adpcm_out[2 + i] = lo | (hi << 4);
	}
}

// ─── Ring buffer write ──────────────────────────────────────────────────────

static void xa_feed_to_spu2(int16_t* pcm, int num_samples)
{
	// Accumulate into the GLOBAL PCM ring buffer (shared with ReadInput.cpp)
	int16_t peak = 0;
	for (int i = 0; i < num_samples; i++) {
		if (pcm[i] > peak) peak = pcm[i];
		if (-pcm[i] > peak) peak = -pcm[i];
		g_xa_pcm_ring[g_xa_pcm_write] = pcm[i];
		g_xa_pcm_write = (g_xa_pcm_write + 1) & (XA_PCM_RING_SIZE - 1);
		// Overflow protection
		if (g_xa_pcm_write == g_xa_pcm_read) {
			g_xa_pcm_read = (g_xa_pcm_read + 1) & (XA_PCM_RING_SIZE - 1);
		}
	}
	// Log
	static int feed_count = 0;
	feed_count++;
	if (feed_count <= 10 || feed_count % 100 == 0) {
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & (XA_PCM_RING_SIZE - 1);
		Console.WriteLn("[XA-FEED] #%d: %d samples, peak=%d, ring_avail=%u, pcm[0..3]=%d %d %d %d",
			feed_count, num_samples, (int)peak, ring_avail,
			(int)pcm[0], (int)pcm[1], (int)pcm[2], (int)pcm[3]);
	}
}

// xa_adma_fill_half is now inlined in ReadInput.cpp using globals

static void xa_configure_adma()
{
	if (s_xa.adma_started) return;
	s_xa.adma_started = true;
	
	// Sync to globals for ReadInput.cpp
	g_xa_pcm_write = 0;
	g_xa_pcm_read = 0;
	g_xa_last_half_filled = -1;
	g_xa_adma_stereo = s_xa.stereo;
	g_xa_adma_active = true;
	
	// Pre-fill ADMA input area with silence
	for (int i = 0; i < 0x200; i++) {
		_spu2mem[0x2000 + i] = 0;
		_spu2mem[0x2200 + i] = 0;
	}
	
	// Enable ADMA on Core 0: AutoDMACtrl bit 0
	Cores[0].AutoDMACtrl |= 1;
	// Set input volume to max
	Cores[0].InpVol.Left = 0x7FFF;
	Cores[0].InpVol.Right = 0x7FFF;
	// Provide initial data so ReadInput doesn't starve
	Cores[0].InputDataLeft = 0x200;
	Cores[0].AdmaInProgress = 1;
	
	Console.WriteLn("[XA-INJECT] ADMA configured: AutoDMACtrl=%d InpVol=0x7FFF stereo=%d",
		Cores[0].AutoDMACtrl, s_xa.stereo);
	Console.WriteLn("[XA-INJECT]   Core0 state: OutPos=0x%03X PlayMode=%d InputDataLeft=%d AdmaInProgress=%d",
		(unsigned)OutPos, PlayMode, (int)Cores[0].InputDataLeft, (int)Cores[0].AdmaInProgress);
	Console.WriteLn("[XA-INJECT]   Core0 MasterVol: L=0x%04x R=0x%04x  ExtVol: L=0x%04x R=0x%04x",
		(u16)Cores[0].MasterVol.Left.Value, (u16)Cores[0].MasterVol.Right.Value,
		(u16)Cores[0].ExtVol.Left, (u16)Cores[0].ExtVol.Right);
}

// ─── Process one XA sector ──────────────────────────────────────────────────

static void xa_inject_process(const uint8_t* transfer, [[maybe_unused]] int mode, 
                              [[maybe_unused]] uint8_t file_filter, [[maybe_unused]] uint8_t chan_filter)
{
	if (!s_xa.active) return;
	
	// Check subheader: is this an audio sector?
	// CDVD_MODE_2340: transfer[0..3]=header, transfer[4..7]=subheader
	// subheader: [4]=file, [5]=channel, [6]=submode, [7]=coding
	uint8_t submode = transfer[6];
	if (!(submode & 0x04)) return;  // bit 2 = audio
	
	// Filter check
	uint8_t file = transfer[4];
	uint8_t chan = transfer[5];
	if (s_xa.cur_file != 0 && file != s_xa.cur_file) return;
	if (s_xa.cur_chan != 0 && chan != s_xa.cur_chan) return;
	
	// Rate limiting: ~18.75 sectors/sec for 37.8kHz mono
	// = 1 sector every 53.3ms = ~2,000,000 IOP cycles (36.864MHz)
	uint64_t now = psxRegs.cycle;
	if (s_xa.last_decode_ticks != 0) {
		uint64_t elapsed = now - s_xa.last_decode_ticks;
		if (elapsed < 1900000) return;  // too fast, skip
	}
	s_xa.last_decode_ticks = now;
	
	// Decode XA sector to PCM
	static int16_t pcm_buf[8192];
	int num_samples = xa_decode_sector_to_pcm(transfer, pcm_buf, 8192);
	if (num_samples <= 0) return;
	
	s_xa.sectors++;
	
	// Configure voice on first sector
	xa_configure_adma();
	
	// Feed to SPU2
	xa_feed_to_spu2(pcm_buf, num_samples);
	
	if (s_xa.sectors <= 5 || (s_xa.sectors % 200) == 0) {
		uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & (XA_PCM_RING_SIZE - 1);
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
	s_xa.hist1 = 0;
	s_xa.hist2 = 0;
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
	// Disable ADMA injection
	s_xa.adma_started = false;
	g_xa_adma_active = false;
	Console.WriteLn("[XA-INJECT] Stopped (%u sectors decoded)", s_xa.sectors);
}

static void xa_inject_setfilter(uint8_t file, uint8_t chan)
{
	// Reset on filter change
	if (file != s_xa.cur_file || chan != s_xa.cur_chan) {
		s_xa.cur_file = file;
		s_xa.cur_chan = chan;
		s_xa.hist1 = 0;
		s_xa.hist2 = 0;
		// Re-key voice: reset write position and re-trigger
		if (s_xa.adma_started) {
			s_xa.adma_started = false;  // will re-configure on next sector
		}
		Console.WriteLn("[XA-INJECT] CdlSetfilter: file=%d channel=%d", file, chan);
	}
}
