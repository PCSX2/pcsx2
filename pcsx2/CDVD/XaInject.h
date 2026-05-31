// SPDX-FileCopyrightText: 2026 Frente C / DKWDRV Hacking Project
// SPDX-License-Identifier: GPL-3.0+
//
// XA-ADPCM PoC Inject for PCSX2 Ps1CD.cpp
// Decodes XA audio sectors and writes PCM to SPU2 RAM + configures voice playback.
// This is a proof-of-concept: decode happens on the PCSX2 host side, bypassing
// the IOP entirely. It proves that XA music CAN work through DKWDRV if the
// decode+DMA path were implemented properly.
//
// To use: #include this from Ps1CD.cpp, call xa_inject_process() from
// cdrReadInterrupt(), call xa_inject_init() on CdlReadS.

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "SPU2/regs.h"

extern void SPU2_FastWrite(u32 rmem, u16 value);
extern s16 _spu2mem[];

// ─── XA ADPCM Constants ────────────────────────────────────────────────────

static const int32_t XA_K0[5] = { 0, 60, 115, 98, 122 };
static const int32_t XA_K1[5] = { 0,  0, -52, -55, -60 };

// ─── Decoder State ──────────────────────────────────────────────────────────

struct XAInjectState {
	bool     active;
	bool     voice_configured;
	int16_t  hist[2][2];          // [ch][0=prev1, 1=prev2]
	uint32_t write_pos;           // SPU2 word address (interleaved L/R)
	uint32_t read_pos;            // Where SPU2 voice is reading (for ring tracking)
	uint32_t sectors;
};

static XAInjectState s_xa = {};

// Buffer in SPU2 RAM: Core 0 upper memory, well above PS1 SFX region.
// SPU2 has 2MB (0x100000 words of 16-bit). We use 0x70000-0x80000 (64K words).
// This is in the PS1 mapped region (map_spu1to2 puts PS1 >=0x200 at 0xC0000+).
// To avoid conflict with game SFX, use SPU2 0x70000 (below 0xC0000 PS1 remap area)
// = within Core 0's normal address space but above what most PS1 games use.
static constexpr uint32_t XA_BUF_BASE = 0x70000;   // word address in SPU2 RAM
static constexpr uint32_t XA_BUF_SIZE = 0x10000;   // 64K words = 32K stereo pairs = ~430ms@37.8kHz

// ─── Core Decode ────────────────────────────────────────────────────────────

static inline int16_t xa_decode_sample(int8_t nibble, uint8_t range, uint8_t filter, int16_t hist[2])
{
	int32_t s = (int32_t)nibble << (12 - range);
	s += (XA_K0[filter] * (int32_t)hist[0] + XA_K1[filter] * (int32_t)hist[1] + 32) >> 6;
	s = std::clamp(s, -32768, 32767);
	hist[1] = hist[0];
	hist[0] = (int16_t)s;
	return (int16_t)s;
}

// Decode one sector to interleaved L/R output. Returns total words written.
static int xa_decode_sector(const uint8_t* sector, int16_t* out, int max_words)
{
	const uint8_t* audio = sector + 24;  // skip 12 sync + 4 header + 8 subheader
	int out_idx = 0;

	for (int g = 0; g < 18 && out_idx + 224 <= max_words; g++) {
		const uint8_t* grp = audio + g * 128;

		// Decode all 8 units, collecting L and R separately
		int16_t left[112], right[112];
		int li = 0, ri = 0;

		for (int u = 0; u < 8; u++) {
			// Param bytes: units 0-3 at grp[0..3], units 4-7 at grp[8..11]
			int p_idx = (u < 4) ? u : (u + 4);
			uint8_t p = grp[p_idx];
			uint8_t range = p & 0x0F;
			uint8_t filter = (p >> 4) & 0x03;
			if (range > 12) range = 9;

			int ch = u & 1;  // even=L, odd=R
			int16_t* hist = s_xa.hist[ch];

			for (int s = 0; s < 28; s++) {
				uint8_t byte = grp[16 + s * 4 + (u >> 1)];
				int8_t nibble = (u & 1) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
				if (nibble >= 8) nibble -= 16;

				int16_t sample = xa_decode_sample(nibble, range, filter, hist);
				if (ch == 0) left[li++] = sample;
				else         right[ri++] = sample;
			}
		}

		// Interleave L/R
		int count = std::min(li, ri);
		for (int i = 0; i < count; i++) {
			out[out_idx++] = left[i];
			out[out_idx++] = right[i];
		}
	}
	return out_idx;
}

// ─── Voice Setup ────────────────────────────────────────────────────────────

// Configure SPU2 Core 0 Voice 23 to play PCM from our buffer.
// We write SPU2 raw ADPCM data that is actually raw PCM packed as "silent ADPCM"
// WAIT — SPU2 voices decode ADPCM, not raw PCM. We can't just write PCM and expect
// voices to play it correctly.
//
// SOLUTION: Write a "raw PCM" header format that SPU2 interprets as identity decode:
//   - filter=0, shift=0 means: sample = nibble << 12 (no prediction, no shift)
//   - That's NOT raw PCM — it's 4-bit!
//
// REAL SOLUTION: Use ADMA (AutoDMA) pathway. Write decoded PCM to the ADMA input
// buffer area, which IS raw 16-bit PCM. The ADMA path feeds into the mixer directly.
//
// ADMA for Core 0 uses SPU2 RAM at 0x2000 (input area, 0x200 words = 512 samples).
// This is the same path that native PS1DRV uses. Let's do that instead!

// REVISED: Write to ADMA input area (0x2000 for Core 0) and trigger ADMA playback.
// But ADMA requires DMA timing synchronization...
//
// SIMPLEST PoC: Convert PCM → SPU-ADPCM format in SPU2 RAM, then use voice playback.
// SPU ADPCM block = 16 bytes: 2 header + 14 data = 28 4-bit samples.
// With filter=0, shift=0: each nibble becomes sample << 12. Max = 7<<12 = 28672.
// That's lossy! 16-bit → 4-bit is terrible quality.
//
// BETTER PoC: Abuse the SPU2 ADPCM format with shift=0, filter=0 to pack 28 samples
// per 16-byte block. Not great but enough to prove the pipeline works.
//
// ACTUALLY BEST: Use the IOP DMA → SPU2 Core 0 AutoDMA path, which accepts raw PCM.
// We write to the IOP-side DMA buffer and trigger DMA4. This is exactly what PS1DRV does.
// From PCSX2 host: call SPU2writeDMA4Mem() with our decoded PCM buffer.

extern void SPU2writeDMA4Mem(u16* pMem, u32 size);

static void xa_setup_adma_playback(int16_t* pcm, int num_words)
{
	// Feed decoded PCM through DMA4 (Core 0 ADMA) — exactly like PS1DRV would
	// The SPU2 AutoDMACtrl must have bit 0 set for Core 0 ADMA.
	// We poke it directly:
	SPU2_FastWrite(SPU2_CORE0 + REG_S_ADMAS, 0x01);  // Enable ADMA for Core 0

	// Write PCM in chunks to avoid buffer overrun
	// ADMA buffer is 0x200 words (512) — we write in 512-word chunks
	const int CHUNK = 512;
	for (int offset = 0; offset < num_words; offset += CHUNK) {
		int count = std::min(CHUNK, num_words - offset);
		SPU2writeDMA4Mem((u16*)(pcm + offset), count);
	}
}

// ─── Public API ─────────────────────────────────────────────────────────────

static void xa_inject_init()
{
	s_xa.active = true;
	s_xa.voice_configured = false;
	s_xa.hist[0][0] = s_xa.hist[0][1] = 0;
	s_xa.hist[1][0] = s_xa.hist[1][1] = 0;
	s_xa.write_pos = XA_BUF_BASE;
	s_xa.read_pos = XA_BUF_BASE;
	s_xa.sectors = 0;
	Console.WriteLn("[XA-INJECT] Started XA decode, ADMA output to Core 0");
}

static void xa_inject_stop()
{
	s_xa.active = false;
	Console.WriteLn("[XA-INJECT] Stopped after %u sectors", s_xa.sectors);
}

static void xa_inject_process(const uint8_t* transfer, int mode, int file_filter, int chan_filter)
{
	if (!s_xa.active)
		return;

	// MODE_STRSND must be set (bit 6 = 0x40)
	if (!(mode & 0x40))
		return;

	// Subheader
	uint8_t sub_file = transfer[16];
	uint8_t sub_chan = transfer[17];
	uint8_t sub_mode = transfer[18];

	// Must be audio (submode bit 2)
	if (!(sub_mode & 0x04))
		return;

	// Filter check (MODE_SF = bit 3 = 0x08)
	if (mode & 0x08) {
		if (sub_file != (uint8_t)file_filter || sub_chan != (uint8_t)chan_filter)
			return;
	}

	// Decode
	static int16_t pcm[18 * 112 * 2 + 64];  // 4032 + margin
	int total_words = xa_decode_sector(transfer, pcm, sizeof(pcm)/sizeof(pcm[0]));
	if (total_words <= 0)
		return;

	// Feed through ADMA (the real PS1DRV pathway)
	xa_setup_adma_playback(pcm, total_words);

	s_xa.sectors++;
	if (s_xa.sectors <= 5 || (s_xa.sectors % 200) == 0) {
		Console.WriteLn("[XA-INJECT] Sector %u: %d words via ADMA4 (file=%d ch=%d)",
			s_xa.sectors, total_words, sub_file, sub_chan);
	}
}
