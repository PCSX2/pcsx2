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

// Fractional resampler state: consume XA ring at g_xa_freq, output at 48000 Hz
static uint32_t s_xa_phase_acc = 0;
static int16_t  s_xa_prev_L = 0;
static int16_t  s_xa_prev_R = 0;
static int16_t  s_xa_cur_L = 0;
static int16_t  s_xa_cur_R = 0;
static uint32_t s_xa_underrun_count = 0;

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

		// XA inject: fill the half with resampled data (37800/18900 Hz → 48000 Hz)
		// Uses fractional phase accumulator so playback rate is correct.
		if (Index == 0 && g_xa_adma_active) {
			int half_to_fill = (ReadIndex == 0x100) ? 0 : (ReadIndex == 0) ? 1 : -1;
			if (half_to_fill >= 0 && half_to_fill != g_xa_last_half_filled) {
				uint32_t base_l = 0x2000 + half_to_fill * 0x100;
				uint32_t base_r = 0x2200 + half_to_fill * 0x100;
				int underrun_this = 0;

				// Phase increment: (input_freq / 48000) in 16.16 fixed point
				uint32_t phase_inc = ((uint32_t)g_xa_freq << 16) / 48000;

				for (int i = 0; i < 0x100; i++) {
					s_xa_phase_acc += phase_inc;

					while (s_xa_phase_acc >= 0x10000) {
						s_xa_phase_acc -= 0x10000;
						s_xa_prev_L = s_xa_cur_L;
						s_xa_prev_R = s_xa_cur_R;
						if (g_xa_pcm_read != g_xa_pcm_write) {
							s_xa_cur_L = g_xa_pcm_ring[g_xa_pcm_read];
							g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
							// Ring is always stereo-interleaved (mono duplicated in feed)
							if (g_xa_pcm_read != g_xa_pcm_write) {
								s_xa_cur_R = g_xa_pcm_ring[g_xa_pcm_read];
								g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
							} else { s_xa_cur_R = s_xa_cur_L; }
							underrun_this = 0;
						} else {
							// Underrun: fade toward zero
							s_xa_cur_L = s_xa_cur_L * 3 / 4;
							s_xa_cur_R = s_xa_cur_R * 3 / 4;
							underrun_this++;
						}
					}

					// Linear interpolation
					uint32_t frac = s_xa_phase_acc;
					int16_t outL = (int16_t)(((int32_t)s_xa_prev_L * (0x10000 - frac) + (int32_t)s_xa_cur_L * frac) >> 16);
					int16_t outR = (int16_t)(((int32_t)s_xa_prev_R * (0x10000 - frac) + (int32_t)s_xa_cur_R * frac) >> 16);
					_spu2mem[base_l + i] = outL;
					_spu2mem[base_r + i] = outR;
				}

				g_xa_last_half_filled = half_to_fill;
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
						s_xa_cur_L = 0; s_xa_cur_R = 0;
						s_xa_prev_L = 0; s_xa_prev_R = 0;
					}
				} else {
					s_xa_underrun_count = 0;
				}

				static uint32_t fill_log = 0;
				fill_log++;
				if (fill_log <= 10 || fill_log % 500 == 0) {
					uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
					Console.WriteLn("[XA-ADMA-FILL] #%u half=%d underrun=%d ring_avail=%u mem[L]=%d %d %d %d phase_inc=0x%04X",
						fill_log, half_to_fill, underrun_this, ring_avail,
						(int)_spu2mem[base_l], (int)_spu2mem[base_l+1],
						(int)_spu2mem[base_l+2], (int)_spu2mem[base_l+3],
						phase_inc);
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