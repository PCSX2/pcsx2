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
#include "R3000A.h"

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
	uint64_t last_decode_ticks;   // EE cycle count at last decode (for rate limiting)
	uint8_t  cur_file;            // current filter file
	uint8_t  cur_chan;            // current filter channel
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
	const uint8_t* audio = sector + 12;  // skip 4 header + 8 subheader (sync already stripped)
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

// (SPU2writeDMA4Mem not used in this version — direct SPU2 RAM write instead)

// Write decoded PCM directly into SPU2 RAM as ADPCM blocks with shift=0,filter=0
// (each nibble → sample<<12). This is lossy (16bit→4bit) but avoids DMA crashes.
// For the PoC we just prove audio comes out.
//
// SPU ADPCM block = 16 bytes: flags/filter/shift | flags2 | 14 data bytes (28 nibbles = 28 samples)
//
// ACTUALLY: Let's write PCM directly as raw 16-bit into SPU2 RAM and configure
// the voice at shift=0 filter=0 — NO. SPU2 always decodes ADPCM.
//
// SAFEST PoC: Write directly via _spu2mem[] and configure a voice.
// SPU2 ADPCM: 16 bytes = 28 4-bit samples. shift=0 filter=0 → sample = nibble << 12.
// We lose precision but prove the path works.

static void xa_encode_adpcm_block(const int16_t* pcm_in, int count, uint8_t* adpcm_out)
{
	// Header: shift=0, filter=0 → byte = 0x00
	// Loop flags: 0 (no loop)
	adpcm_out[0] = 0x00;  // shift=0, filter=0
	adpcm_out[1] = 0x00;  // no loop/end flags
	for (int i = 0; i < 14; i++) {
		uint8_t lo = 0, hi = 0;
		int si = i * 2;
		if (si < count) {
			// Quantize 16-bit to 4-bit: sample >> 12
			int s = pcm_in[si] >> 12;
			lo = (uint8_t)(s & 0x0F);
		}
		if (si + 1 < count) {
			int s = pcm_in[si + 1] >> 12;
			hi = (uint8_t)(s & 0x0F);
		}
		adpcm_out[2 + i] = lo | (hi << 4);
	}
}

static void xa_setup_adma_playback(int16_t* pcm, int num_words)
{
	// Write ADPCM-encoded data directly into SPU2 RAM at our buffer region
	// num_words is stereo interleaved (L R L R...), so num_words/2 = stereo pairs
	// For simplicity: mono mixdown, write into voice 23 buffer

	// Mono mixdown
	int num_samples = num_words / 2;  // stereo pairs
	static int16_t mono[4096];
	for (int i = 0; i < num_samples && i < 4096; i++) {
		int32_t m = ((int32_t)pcm[i*2] + (int32_t)pcm[i*2+1]) / 2;
		mono[i] = (int16_t)m;
	}

	// Encode as ADPCM blocks (28 samples per 16 bytes = 8 words)
	uint32_t wp = s_xa.write_pos;
	int samp_idx = 0;
	while (samp_idx < num_samples) {
		uint8_t block[16];
		int remaining = num_samples - samp_idx;
		int block_samps = (remaining > 28) ? 28 : remaining;
		xa_encode_adpcm_block(mono + samp_idx, block_samps, block);

		// Wrap write pointer
		if (wp + 8 >= XA_BUF_BASE + XA_BUF_SIZE) {
			wp = XA_BUF_BASE;
		}

		// Write 16 bytes = 8 words into SPU2 RAM
		for (int w = 0; w < 8; w++) {
			_spu2mem[wp + w] = (int16_t)((uint16_t)block[w*2] | ((uint16_t)block[w*2+1] << 8));
		}
		wp += 8;
		samp_idx += 28;
	}

	s_xa.write_pos = wp;

	// Configure voice 23 on first sector
	if (!s_xa.voice_configured) {
		s_xa.voice_configured = true;
		uint32_t ssa = XA_BUF_BASE;

		// Pre-fill entire ring buffer with silent ADPCM blocks (shift=0, no flags)
		// so voice can loop freely without hitting garbage
		for (uint32_t addr = XA_BUF_BASE; addr < XA_BUF_BASE + XA_BUF_SIZE; addr += 8) {
			_spu2mem[addr] = 0;  // shift=0, filter=0
			_spu2mem[addr+1] = 0; // flags=0 (nothing special)
			for (int w = 2; w < 8; w++) _spu2mem[addr+w] = 0;
		}
		// Last block: set LOOP_END + LOOP (0x03) so voice jumps to loop addr
		uint32_t last_block = XA_BUF_BASE + XA_BUF_SIZE - 8;
		_spu2mem[last_block + 1] = 0x03;  // LOOP_END | LOOP
		// First block: set LOOP_START (0x04) — marks loop target
		_spu2mem[XA_BUF_BASE + 1] = 0x04;

		// Volume L/R = max
		SPU2_FastWrite(SPU2_CORE0 + (23 * 16) + 0, 0x3FFF);  // VOLL
		SPU2_FastWrite(SPU2_CORE0 + (23 * 16) + 2, 0x3FFF);  // VOLR
		// Pitch = 37800/48000 * 4096 = 0x0C9A (SPU2 runs at 48kHz)
		SPU2_FastWrite(SPU2_CORE0 + (23 * 16) + 4, 0x0C9A);  // Pitch
		// ADSR: instant attack, no decay, full sustain, no release
		SPU2_FastWrite(SPU2_CORE0 + (23 * 16) + 6, 0x000F);  // ADSR1
		SPU2_FastWrite(SPU2_CORE0 + (23 * 16) + 8, 0x1FC0);  // ADSR2

		// Start address = buffer start
		uint32_t reg_base = 0x1C0 + 23 * 12;
		SPU2_FastWrite(SPU2_CORE0 + reg_base + 0, (u16)((ssa >> 16) & 0x3F)); // SSAH
		SPU2_FastWrite(SPU2_CORE0 + reg_base + 2, (u16)(ssa & 0xFFFF));       // SSAL
		// Loop address = buffer start (voice loops entire ring)
		SPU2_FastWrite(SPU2_CORE0 + reg_base + 4, (u16)((ssa >> 16) & 0x3F)); // LSAH
		SPU2_FastWrite(SPU2_CORE0 + reg_base + 6, (u16)(ssa & 0xFFFF));       // LSAL

		// Ensure voice 23 is in dry mix (VMIXL/VMIXR)
		SPU2_FastWrite(SPU2_CORE0 + REG_S_VMIXL + 2, (u16)(1 << (23 - 16)));
		SPU2_FastWrite(SPU2_CORE0 + REG_S_VMIXR + 2, (u16)(1 << (23 - 16)));

		// Key On voice 23
		SPU2_FastWrite(SPU2_CORE0 + 0x1A0 + 2, (u16)(1 << (23 - 16)));

		Console.WriteLn("[XA-INJECT] Voice 23 configured: SSA=0x%05X, ring buffer %u words, looping", ssa, XA_BUF_SIZE);
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
	s_xa.last_decode_ticks = 0;
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

	// Subheader — Transfer has sync stripped: [0..3]=header, [4..7]=subheader
	uint8_t sub_file = transfer[4];
	uint8_t sub_chan = transfer[5];
	uint8_t sub_mode = transfer[6];

	// Must be audio (submode bit 2)
	if (!(sub_mode & 0x04))
		return;

	// Filter check (MODE_SF = bit 3 = 0x08)
	if (mode & 0x08) {
		if (sub_file != (uint8_t)file_filter || sub_chan != (uint8_t)chan_filter)
			return;
	}

	// Rate limiter: XA at 37.8kHz produces 2016 samples/sector = 53.3ms per sector.
	// IOP clock = 36.864 MHz. 53.3ms = 1,964,544 IOP cycles.
	// We use psxRegs.cycle for timing.
	constexpr uint32_t XA_SECTOR_INTERVAL = 1900000;  // ~51.5ms in IOP cycles (slightly tight)
	uint64_t now = (uint64_t)psxRegs.cycle;
	if (s_xa.last_decode_ticks != 0) {
		uint32_t elapsed = (uint32_t)(now - s_xa.last_decode_ticks);
		if (elapsed < XA_SECTOR_INTERVAL) {
			return;  // Too soon — skip this sector
		}
	}
	s_xa.last_decode_ticks = now;

	// Detect filter change — reset voice to avoid playing stale buffer
	if (s_xa.cur_file != sub_file || s_xa.cur_chan != sub_chan) {
		s_xa.cur_file = sub_file;
		s_xa.cur_chan = sub_chan;
		s_xa.write_pos = XA_BUF_BASE;
		s_xa.voice_configured = false;  // Re-key voice from new position
		s_xa.hist[0][0] = s_xa.hist[0][1] = 0;
		s_xa.hist[1][0] = s_xa.hist[1][1] = 0;
	}

	// Decode
	static int16_t pcm[18 * 112 * 2 + 64];  // 4032 + margin
	int total_words = xa_decode_sector(transfer, pcm, sizeof(pcm)/sizeof(pcm[0]));
	if (total_words <= 0)
		return;

	// Feed through direct SPU2 RAM write
	xa_setup_adma_playback(pcm, total_words);

	s_xa.sectors++;
	if (s_xa.sectors <= 5 || (s_xa.sectors % 200) == 0) {
		Console.WriteLn("[XA-INJECT] Sector %u: %d words via ADMA4 (file=%d ch=%d)",
			s_xa.sectors, total_words, sub_file, sub_chan);
	}
}
