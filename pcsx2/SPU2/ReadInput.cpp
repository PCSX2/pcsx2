// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Dma.h"
#include "IopDma.h"
#include "IopHw.h"
#include "SPU2/Debug.h"
#include "SPU2/defs.h"
#include "SPU2/spu2.h"
#include "common/Console.h"

// XA ADMA globals (defined in Ps1CD.cpp)
extern bool g_xa_adma_active;
extern bool g_xa_adma_stereo;
extern int16_t g_xa_pcm_ring[];
extern uint32_t g_xa_pcm_write;
extern uint32_t g_xa_pcm_read;
extern int g_xa_last_half_filled;
extern int g_xa_freq;
static constexpr uint32_t XA_RING_MASK = 524287;
static constexpr uint32_t XA_PCM_RING_SIZE = 524288;

// Sector FIFO globals (defined in Ps1CD.cpp)
extern uint8_t g_xa_sector_fifo[];
extern uint32_t g_xa_sector_fifo_write;
extern uint32_t g_xa_sector_fifo_read;

// Fractional resampler state: consume XA ring at g_xa_freq, output at 44100 Hz
// Uses PS1 CD-XA zigzag polyphase FIR (29-tap, 7 phases, 6:7 ratio for 37800→44100)
static uint32_t s_xa_phase_acc = 0;
static int16_t  s_xa_ringbuf_L[32] = {};
static int16_t  s_xa_ringbuf_R[32] = {};
static uint32_t s_xa_ringbuf_p = 0;
static uint32_t s_xa_sixstep = 6;
static uint32_t s_xa_underrun_count = 0;

// Output buffer: zigzag produces 7 samples per 6 inputs, we buffer and drain to ADMA
static int16_t  s_xa_outbuf_L[1024] = {};
static int16_t  s_xa_outbuf_R[1024] = {};
static uint32_t s_xa_outbuf_write = 0;
static uint32_t s_xa_outbuf_read = 0;

// PS1 CD-XA zigzag interpolation tables (from Mednafen/Duckstation, hardware-accurate)
// 7 phases x 29 taps. For 37800Hz: every 6 input samples → 7 output samples (44100Hz)
static const int16_t s_xa_zigzag[7][29] = {
	{0,      0x0,     0x0,     0x0,    0x0,     -0x0002, 0x000A,  -0x0022, 0x0041, -0x0054,
	 0x0034, 0x0009,  -0x010A, 0x0400, -0x0A78, 0x234C,  0x6794,  -0x1780, 0x0BCD, -0x0623,
	 0x0350, -0x016D, 0x006B,  0x000A, -0x0010, 0x0011,  -0x0008, 0x0003,  -0x0001},
	{0,       0x0,    0x0,     -0x0002, 0x0,    0x0003,  -0x0013, 0x003C,  -0x004B, 0x00A2,
	 -0x00E3, 0x0132, -0x0043, -0x0267, 0x0C9D, 0x74BB,  -0x11B4, 0x09B8,  -0x05BF, 0x0372,
	 -0x01A8, 0x00A6, -0x001B, 0x0005,  0x0006, -0x0008, 0x0003,  -0x0001, 0x0},
	{0,      0x0,     -0x0001, 0x0003,  -0x0002, -0x0005, 0x001F,  -0x004A, 0x00B3, -0x0192,
	 0x02B1, -0x039E, 0x04F8,  -0x05A6, 0x7939,  -0x05A6, 0x04F8,  -0x039E, 0x02B1, -0x0192,
	 0x00B3, -0x004A, 0x001F,  -0x0005, -0x0002, 0x0003,  -0x0001, 0x0,     0x0},
	{0,       -0x0001, 0x0003,  -0x0008, 0x0006, 0x0005,  -0x001B, 0x00A6, -0x01A8, 0x0372,
	 -0x05BF, 0x09B8,  -0x11B4, 0x74BB,  0x0C9D, -0x0267, -0x0043, 0x0132, -0x00E3, 0x00A2,
	 -0x004B, 0x003C,  -0x0013, 0x0003,  0x0,    -0x0002, 0x0,     0x0,    0x0},
	{-0x0001, 0x0003,  -0x0008, 0x0011,  -0x0010, 0x000A, 0x006B,  -0x016D, 0x0350, -0x0623,
	 0x0BCD,  -0x1780, 0x6794,  0x234C,  -0x0A78, 0x0400, -0x010A, 0x0009,  0x0034, -0x0054,
	 0x0041,  -0x0022, 0x000A,  -0x0001, 0x0,     0x0001, 0x0,     0x0,     0x0},
	{0x0002,  -0x0008, 0x0010,  -0x0023, 0x002B, 0x001A,  -0x00EB, 0x027B,  -0x0548, 0x0AFA,
	 -0x16FA, 0x53E0,  0x3C07,  -0x1249, 0x080E, -0x0347, 0x015B,  -0x0044, -0x0017, 0x0046,
	 -0x0023, 0x0011,  -0x0005, 0x0,     0x0,    0x0,     0x0,     0x0,     0x0},
	{-0x0005, 0x0011,  -0x0023, 0x0046, -0x0017, -0x0044, 0x015B,  -0x0347, 0x080E, -0x1249,
	 0x3C07,  0x53E0,  -0x16FA, 0x0AFA, -0x0548, 0x027B,  -0x00EB, 0x001A,  0x002B, -0x0023,
	 0x0010,  -0x0008, 0x0002,  0x0,    0x0,     0x0,     0x0,     0x0,     0x0},
};

static inline int16_t xa_zigzag_interpolate(const int16_t* ringbuf, uint32_t table_index, uint32_t p) {
	const int16_t* table = s_xa_zigzag[table_index];
	int32_t sum = 0;
	for (uint32_t i = 0; i < 29; i++)
		sum += ((int32_t)ringbuf[(p - i) & 0x1F] * (int32_t)table[i]) >> 15;
	return (int16_t)std::clamp<int32_t>(sum, -0x8000, 0x7FFF);
}

// Pump sectors from FIFO to ring — declared in XaInject.h, implemented in Ps1CD.cpp
extern void xa_pump_fifo_to_ring_impl(uint32_t target_ring_avail);

// Core 0 Input is "SPDIF mode" - Source audio is AC3 compressed.

// Core 1 Input is "CDDA mode" - Source audio data is 32 bits.
// PS2 note:  Very! few PS2 games use this mode.  Some PSX games used it, however no
// *known* PS2 game does since it was likely only available if the game was recorded to CD
// media (ie, not available in DVD mode, which almost all PS2 games use).  Plus PS2 games
// generally prefer to use ADPCM streaming audio since they need as much storage space as
// possible for FMVs and high-def textures.
//

/* ── CD-DA read mode ──
 * Use SPU2 OutPos directly for sample fetch (strict 1:1 ring sync).
 * Helps avoid phase drift/robotic artifacts from custom source stepping.
 */

StereoOut32 V_Core::ReadInput_HiFi()
{
	if (SPU2::IsRunningPSXMode() && SPU2::MsgToConsole())
		SPU2::ConLog("ReadInput_HiFi!!!!!\n");

	/* Read one sample pair at current SPU2 output position (strict ring sync) */
	const u16 idx = (OutPos * 2) & 0x1FF;

	static int debug_count = 0;
	if (debug_count == 0)
	{
		Console.WriteLn("[DKWDRV HACK] CDDA SPDIF path active: PlayMode=8 ReadInput_HiFi");
		Console.WriteLn("[DKWDRV HACK] CDDA step=1.000000000 (forced 1:1 user test)");
	}
	if (debug_count == 4800)
	{
		Console.WriteLn("[DKWDRV HACK] CDDA timing check: OutPos ring-sync active (0.1s marker)");
	}
	debug_count++;

	s32 L = (s32)((s32&)(*GetMemPtr(0x2000 + (Index << 10) + idx)));
	s32 R = (s32)((s32&)(*GetMemPtr(0x2200 + (Index << 10) + idx)));

	if (Index == 1)
	{
		L >>= 16;
		R >>= 16;
	}

	StereoOut32 retval(L, R);

	/* ── Original DMA refill logic (unchanged) ── */
	const u16 ReadIndex = (OutPos * 2) & 0x1FF;
	const bool cdda_spdif = (Index == 1) && ((PlayMode & 8) != 0);
	const bool cdda_early_refill = cdda_spdif && (ReadIndex == 0x40 || ReadIndex == 0x140);
	// Simulate MADR increase, GTA VC tracks the MADR address for calculating a certain point in the buffer
	if (InputDataTransferred)
	{
		u32 amount = std::min(InputDataTransferred, (u32)0x180);

		InputDataTransferred -= amount;
		MADR += amount;
		// Because some games watch the MADR to see when it reaches the end we need to end the DMA here
		// Tom & Jerry War of the Whiskers is one such game, the music will skip
		if (!InputDataTransferred && !InputDataLeft)
		{
			if (Index == 0)
				spu2DMA4Irq();
			else
				spu2DMA7Irq();
		}
	}

	if (ReadIndex == 0x100 || ReadIndex == 0x0 || ReadIndex == 0x80 || ReadIndex == 0x180 || cdda_early_refill)
	{
		if (ReadIndex == 0x100)
			InputPosWrite = 0;
		else if (ReadIndex == 0)
			InputPosWrite = 0x100;

		static bool cdda_refill_logged = false;
		if (cdda_early_refill && !cdda_refill_logged)
		{
			Console.WriteLn("[DKWDRV HACK] CDDA anti-starve: enabling early ADMA refill checkpoints (0x40/0x140)");
			cdda_refill_logged = true;
		}

		// [DKWDRV HACK] Only block normal ADMA when XA ring actually has data
		if (InputDataLeft >= 0x100 && !(Index == 0 && g_xa_adma_active && ((g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK) > 0))
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;
			if (InputDataLeft < 0x100)
			{
				if (IsDevBuild)
				{
					SPU2::FileLog("[%10d] AutoDMA%c block end.\n", Cycles, GetDmaIndexChar());
					if (InputDataLeft > 0)
					{
						if (SPU2::MsgAutoDMA())
							SPU2::ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}

				InputDataLeft = 0;
			}
		}
		else if ((AutoDMACtrl & (Index + 1)))
			AutoDMACtrl |= ~3;
	}
	return retval;
}

StereoOut32 V_Core::ReadInput()
{
	StereoOut32 retval;
	u16 ReadIndex = OutPos;

	// XA instrumentation: log first few ReadInput calls for Core 0
	if (Index == 0 && g_xa_adma_active) {
		static uint32_t ri_calls = 0;
		ri_calls++;
		if (ri_calls <= 3 || ri_calls == 100 || ri_calls == 1000) {
			Console.WriteLn("[XA-READINPUT] Core0 call #%u: OutPos=0x%03X PlayMode=%d AutoDMACtrl=%d InputDataLeft=%d AdmaInProgress=%d",
				ri_calls, (unsigned)ReadIndex, (int)PlayMode, (int)AutoDMACtrl, (int)InputDataLeft, (int)AdmaInProgress);
		}
	}

	for (int i = 0; i < 2; i++)
		if (Cores[i].IRQEnable && (0x2000 + (Index << 10) + ReadIndex) == (Cores[i].IRQA & 0xfffffdff))
			SetIrqCall(i);

	// PlayMode & 2 is Bypass Mode, so it doesn't go through the SPU
	if ((Index == 1) || !(Index == 0 && (PlayMode & 2) != 0))
	{
		retval = StereoOut32(
			(s32)(*GetMemPtr(0x2000 + (Index << 10) + ReadIndex)),
			(s32)(*GetMemPtr(0x2200 + (Index << 10) + ReadIndex)));
	}

	// XA instrumentation: log what ReadInput returns for Core 0
	if (Index == 0 && g_xa_adma_active) {
		static uint32_t ret_log = 0;
		ret_log++;
		if (ret_log <= 5 || ret_log == 512 || ret_log == 1024) {
			Console.WriteLn("[XA-READINPUT-RET] #%u OutPos=0x%03X retval L=%d R=%d PlayMode=%d cond=%d",
				ret_log, (unsigned)ReadIndex, retval.Left, retval.Right, (int)PlayMode,
				(int)((Index == 1) || !(Index == 0 && (PlayMode & 2) != 0)));
		}
	}

#ifdef PCSX2_DEVBUILD
	DebugCores[Index].admaWaveformL[OutPos % 0x100] = retval.Left;
	DebugCores[Index].admaWaveformR[OutPos % 0x100] = retval.Right;
#endif

	// Simulate MADR increase, GTA VC tracks the MADR address for calculating a certain point in the buffer
	if (InputDataTransferred)
	{
		u32 amount = std::min(InputDataTransferred, (u32)0x180);

		InputDataTransferred -= amount;
		MADR += amount;
		// Because some games watch the MADR to see when it reaches the end we need to end the DMA here
		// Tom & Jerry War of the Whiskers is one such game, the music will skip
		if (!InputDataTransferred && !InputDataLeft)
		{
			if (Index == 0)
				spu2DMA4Irq();
			else
				spu2DMA7Irq();
		}
	}

	if (PlayMode == 2 && Index == 0) //Bitstream bypass refills twice as quickly (GTA VC)
		ReadIndex = (ReadIndex * 2) & 0x1FF;

	if (ReadIndex == 0x100 || ReadIndex == 0x0 || ReadIndex == 0x80 || ReadIndex == 0x180)
	{
		if (ReadIndex == 0x100)
			InputPosWrite = 0;
		else if (ReadIndex == 0)
			InputPosWrite = 0x100;

		// XA inject: fill the half with resampled data using PS1 zigzag FIR
		// 37800Hz: 6 input → 7 output (44100Hz). 18900Hz: 6 input → 14 output (44100Hz)
		if (Index == 0 && g_xa_adma_active) {
			int half_to_fill = (ReadIndex == 0x100) ? 0 : (ReadIndex == 0) ? 1 : -1;
			if (half_to_fill >= 0 && half_to_fill != g_xa_last_half_filled) {
				// Pump FIFO → ring, capped at ~1s
				uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
				if (ring_avail < 75600) {
					xa_pump_fifo_to_ring_impl(75600);
				}

				uint32_t base_l = 0x2000 + half_to_fill * 0x100;
				uint32_t base_r = 0x2200 + half_to_fill * 0x100;
				int underrun_this = 0;

				// Feed input samples into zigzag ring buffer, collect output
				// We need 0x100 (256) output samples to fill one ADMA half
				// For 37800Hz: ratio is 7/6, so we need ~220 input samples for 256 outputs
				// For 18900Hz: ratio is 14/6 ≈ 2.333, so we need ~110 input samples
				int output_per_input = (g_xa_freq == 18900) ? 14 : 7;
				int input_per_cycle = 6;  // always 6 inputs per cycle

				int out_pos = 0;
				while (out_pos < 0x100) {
					// Try to get one input sample pair from ring
					if (g_xa_pcm_read != g_xa_pcm_write) {
						s_xa_ringbuf_L[s_xa_ringbuf_p] = g_xa_pcm_ring[g_xa_pcm_read];
						g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
						if (g_xa_pcm_read != g_xa_pcm_write) {
							s_xa_ringbuf_R[s_xa_ringbuf_p] = g_xa_pcm_ring[g_xa_pcm_read];
							g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
						} else {
							s_xa_ringbuf_R[s_xa_ringbuf_p] = s_xa_ringbuf_L[s_xa_ringbuf_p];
						}
						s_xa_ringbuf_p = (s_xa_ringbuf_p + 1) & 0x1F;
						underrun_this = 0;
					} else {
						// Underrun: push silence
						s_xa_ringbuf_L[s_xa_ringbuf_p] = 0;
						s_xa_ringbuf_R[s_xa_ringbuf_p] = 0;
						s_xa_ringbuf_p = (s_xa_ringbuf_p + 1) & 0x1F;
						underrun_this++;
					}

					s_xa_sixstep--;
					if (s_xa_sixstep == 0) {
						s_xa_sixstep = input_per_cycle;
						// Produce output_per_input output samples
						for (int j = 0; j < output_per_input && out_pos < 0x100; j++) {
							int table_idx = j % 7;
							_spu2mem[base_l + out_pos] = xa_zigzag_interpolate(s_xa_ringbuf_L, table_idx, s_xa_ringbuf_p);
							_spu2mem[base_r + out_pos] = xa_zigzag_interpolate(s_xa_ringbuf_R, table_idx, s_xa_ringbuf_p);
							out_pos++;
						}
					}
				}

				g_xa_last_half_filled = half_to_fill;
				// Signal ADMA in-progress so SPU2 keeps calling ReadInput
				InputDataLeft = 0x200;
				AdmaInProgress = 1;

				// Track consecutive underrun fills for mute detection
				if (underrun_this >= 128) {
					s_xa_underrun_count++;
					if (s_xa_underrun_count >= 8) {
						for (int i = 0; i < 0x100; i++) {
							_spu2mem[base_l + i] = 0;
							_spu2mem[base_r + i] = 0;
						}
						memset(s_xa_ringbuf_L, 0, sizeof(s_xa_ringbuf_L));
						memset(s_xa_ringbuf_R, 0, sizeof(s_xa_ringbuf_R));
					}
				} else {
					s_xa_underrun_count = 0;
				}

				static uint32_t fill_log = 0;
				fill_log++;
				if (fill_log <= 10 || fill_log % 500 == 0) {
					uint32_t ring_avail_now = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
					Console.WriteLn("[XA-ADMA-FILL] #%u half=%d underrun=%d ring_avail=%u mem[L]=%d %d %d %d zigzag",
						fill_log, half_to_fill, underrun_this, ring_avail_now,
						(int)_spu2mem[base_l], (int)_spu2mem[base_l+1],
						(int)_spu2mem[base_l+2], (int)_spu2mem[base_l+3]);
				}
			}
		}

		// [DKWDRV HACK] Only block normal ADMA when XA ring actually has data
		if (InputDataLeft >= 0x100 && !(Index == 0 && g_xa_adma_active && ((g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK) > 0))
		{
			AutoDMAReadBuffer(0);
			AdmaInProgress = 1;
			if (InputDataLeft < 0x100)
			{
				if (IsDevBuild)
				{
					SPU2::FileLog("[%10d] AutoDMA%c block end.\n", Cycles, GetDmaIndexChar());
					if (InputDataLeft > 0)
					{
						if (SPU2::MsgAutoDMA())
							SPU2::ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
					}
				}

				InputDataLeft = 0;
			}
		}
		else if ((AutoDMACtrl & (Index + 1)))
			AutoDMACtrl |= ~3;
	}
	return retval;
}