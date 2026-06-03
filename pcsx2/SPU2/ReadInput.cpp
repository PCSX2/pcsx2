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
static uint32_t s_xa_phase_acc = 0;
static int16_t  s_xa_hist_L[4] = {0, 0, 0, 0};  // [0]=oldest(n-3), [3]=newest(n)
static int16_t  s_xa_hist_R[4] = {0, 0, 0, 0};
static uint32_t s_xa_underrun_count = 0;

// PS1 SPU Gaussian interpolation table (hardware-accurate, 512 entries)
// Same table used by Duckstation and Ares for SPU voice interpolation
static const int16_t s_gauss_table[512] = {
	-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,
	-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,-0x001,
	0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0001,
	0x0001,0x0001,0x0001,0x0002,0x0002,0x0002,0x0003,0x0003,
	0x0003,0x0004,0x0004,0x0005,0x0005,0x0006,0x0007,0x0007,
	0x0008,0x0009,0x0009,0x000A,0x000B,0x000C,0x000D,0x000E,
	0x000F,0x0010,0x0011,0x0012,0x0013,0x0015,0x0016,0x0018,
	0x0019,0x001B,0x001C,0x001E,0x0020,0x0021,0x0023,0x0025,
	0x0027,0x0029,0x002C,0x002E,0x0030,0x0033,0x0035,0x0038,
	0x003A,0x003D,0x0040,0x0043,0x0046,0x0049,0x004D,0x0050,
	0x0054,0x0057,0x005B,0x005F,0x0063,0x0067,0x006B,0x006F,
	0x0074,0x0078,0x007D,0x0082,0x0087,0x008C,0x0091,0x0096,
	0x009C,0x00A1,0x00A7,0x00AD,0x00B3,0x00BA,0x00C0,0x00C7,
	0x00CD,0x00D4,0x00DB,0x00E3,0x00EA,0x00F2,0x00FA,0x0101,
	0x010A,0x0112,0x011B,0x0123,0x012C,0x0135,0x013F,0x0148,
	0x0152,0x015C,0x0166,0x0171,0x017B,0x0186,0x0191,0x019C,
	0x01A8,0x01B4,0x01C0,0x01CC,0x01D9,0x01E5,0x01F2,0x0200,
	0x020D,0x021B,0x0229,0x0237,0x0246,0x0255,0x0264,0x0273,
	0x0283,0x0293,0x02A3,0x02B4,0x02C4,0x02D6,0x02E7,0x02F9,
	0x030B,0x031D,0x0330,0x0343,0x0356,0x036A,0x037E,0x0392,
	0x03A7,0x03BC,0x03D1,0x03E7,0x03FC,0x0413,0x042A,0x0441,
	0x0458,0x0470,0x0488,0x04A0,0x04B9,0x04D2,0x04EC,0x0506,
	0x0520,0x053B,0x0556,0x0572,0x058E,0x05AA,0x05C7,0x05E4,
	0x0601,0x061F,0x063E,0x065C,0x067C,0x069B,0x06BB,0x06DC,
	0x06FD,0x071E,0x0740,0x0762,0x0784,0x07A7,0x07CB,0x07EF,
	0x0813,0x0838,0x085D,0x0883,0x08A9,0x08D0,0x08F7,0x091E,
	0x0946,0x096F,0x0998,0x09C1,0x09EB,0x0A16,0x0A40,0x0A6C,
	0x0A98,0x0AC4,0x0AF1,0x0B1E,0x0B4C,0x0B7A,0x0BA9,0x0BD8,
	0x0C07,0x0C38,0x0C68,0x0C99,0x0CCB,0x0CFD,0x0D30,0x0D63,
	0x0D97,0x0DCB,0x0E00,0x0E35,0x0E6B,0x0EA1,0x0ED7,0x0F0F,
	0x0F46,0x0F7F,0x0FB7,0x0FF1,0x102A,0x1065,0x109F,0x10DB,
	0x1116,0x1153,0x118F,0x11CD,0x120B,0x1249,0x1288,0x12C7,
	0x1307,0x1347,0x1388,0x13C9,0x140B,0x144D,0x1490,0x14D4,
	0x1517,0x155C,0x15A0,0x15E6,0x162C,0x1672,0x16B9,0x1700,
	0x1747,0x1790,0x17D8,0x1821,0x186B,0x18B5,0x1900,0x194B,
	0x1996,0x19E2,0x1A2E,0x1A7B,0x1AC8,0x1B16,0x1B64,0x1BB3,
	0x1C02,0x1C51,0x1CA1,0x1CF1,0x1D42,0x1D93,0x1DE5,0x1E37,
	0x1E89,0x1EDC,0x1F2F,0x1F82,0x1FD5,0x2029,0x207D,0x20D1,
	0x2125,0x2179,0x21CE,0x2223,0x2277,0x22CC,0x2321,0x2376,
	0x23CB,0x2420,0x2475,0x24CA,0x251F,0x2574,0x25C9,0x261F,
	0x2674,0x26C9,0x271E,0x2773,0x27C8,0x281D,0x2872,0x28C7,
	0x291C,0x2970,0x29C5,0x2A1A,0x2A6F,0x2AC3,0x2B18,0x2B6C,
	0x2BC0,0x2C15,0x2C69,0x2CBD,0x2D11,0x2D65,0x2DB9,0x2E0D,
	0x2E60,0x2EB4,0x2F07,0x2F5B,0x2FAE,0x3001,0x3054,0x30A7,
	0x30FA,0x314D,0x319F,0x31F1,0x3244,0x3296,0x32E8,0x333A,
	0x338C,0x33DD,0x342F,0x3480,0x34D1,0x3522,0x3573,0x35C4,
	0x3614,0x3665,0x36B5,0x3705,0x3755,0x37A4,0x37F4,0x3843,
	0x3892,0x38E1,0x3930,0x397F,0x39CE,0x3A1C,0x3A6B,0x3AB9,
	0x3B07,0x3B55,0x3BA2,0x3BF0,0x3C3D,0x3C8B,0x3CD8,0x3D25,
	0x3D71,0x3DBE,0x3E0A,0x3E56,0x3EA2,0x3EEE,0x3F3A,0x3F85,
	0x3FD1,0x401C,0x4067,0x40B2,0x40FC,0x4147,0x4191,0x41DB,
	0x4225,0x426F,0x42B8,0x4302,0x434B,0x4394,0x43DD,0x4425,
	0x446E,0x44B6,0x44FF,0x4547,0x458F,0x45D7,0x461E,0x4666,
	0x46AD,0x46F4,0x473B,0x4782,0x47C9,0x480F,0x4855,0x489C,
	0x48E2,0x4927,0x496D,0x49B2,0x49F7,0x4A3D,0x4A82,0x4AC6,
	0x4B0B,0x4B4F,0x4B94,0x4BD8,0x4C1B,0x4C5F,0x4CA2,0x4CE6,
	0x4D29,0x4D6C,0x4DAF,0x4DF2,0x4E34,0x4E76,0x4EB8,0x4EFA,
	0x4F3C,0x4F7E,0x4FBF,0x5000,0x5041,0x5082,0x50C3,0x5103,
	0x5144,0x5184,0x51C4,0x5203,0x5243,0x5282,0x52C1,0x5300,
	0x533F,0x537E,0x53BC,0x53FA,0x5438,0x5476,0x54B3,0x54F1,
	0x552E,0x556B,0x55A7,0x55E4,0x5620,0x565C,0x5698,0x56D3,
	0x570F,0x574A,0x5785,0x57C0,0x57FA,0x5834,0x586E,0x58A8,
	0x58E1,0x591A,0x5953,0x598C,0x59C4,0x59FC,0x5A34,0x5A6C,
	0x5AA3,0x5ADA,0x5B11,0x5B48,0x5B7E,0x5BB4,0x5BEA,0x5C1F,
};

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

		// XA inject: fill the half with resampled data (37800/18900 Hz → 48000 Hz)
		// Uses fractional phase accumulator so playback rate is correct.
		if (Index == 0 && g_xa_adma_active) {
			int half_to_fill = (ReadIndex == 0x100) ? 0 : (ReadIndex == 0) ? 1 : -1;
			if (half_to_fill >= 0 && half_to_fill != g_xa_last_half_filled) {
				// Pump sectors from FIFO to keep ring fed (target: ~4 sectors ahead)
				uint32_t ring_avail = (g_xa_pcm_write - g_xa_pcm_read) & XA_RING_MASK;
				if (ring_avail < 32256) {  // < 4 sectors stereo (4 * 4032 * 2)
					xa_pump_fifo_to_ring_impl(32256);
				}

				uint32_t base_l = 0x2000 + half_to_fill * 0x100;
				uint32_t base_r = 0x2200 + half_to_fill * 0x100;
				int underrun_this = 0;

				// Phase increment: (input_freq / output_rate) in 16.16 fixed point
				// PSX mode runs SPU2 at 44100Hz, not 48000Hz
				uint32_t phase_inc = ((uint32_t)g_xa_freq << 16) / 44100;

				for (int i = 0; i < 0x100; i++) {
					s_xa_phase_acc += phase_inc;

					while (s_xa_phase_acc >= 0x10000) {
						s_xa_phase_acc -= 0x10000;
						// Shift history: [0]=[1], [1]=[2], [2]=[3], [3]=new
						s_xa_hist_L[0] = s_xa_hist_L[1]; s_xa_hist_L[1] = s_xa_hist_L[2]; s_xa_hist_L[2] = s_xa_hist_L[3];
						s_xa_hist_R[0] = s_xa_hist_R[1]; s_xa_hist_R[1] = s_xa_hist_R[2]; s_xa_hist_R[2] = s_xa_hist_R[3];
						if (g_xa_pcm_read != g_xa_pcm_write) {
							s_xa_hist_L[3] = g_xa_pcm_ring[g_xa_pcm_read];
							g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
							// Ring is always stereo-interleaved (mono duplicated in feed)
							if (g_xa_pcm_read != g_xa_pcm_write) {
								s_xa_hist_R[3] = g_xa_pcm_ring[g_xa_pcm_read];
								g_xa_pcm_read = (g_xa_pcm_read + 1) & XA_RING_MASK;
							} else { s_xa_hist_R[3] = s_xa_hist_L[3]; }
							underrun_this = 0;
						} else {
							// Underrun: fade toward zero
							s_xa_hist_L[3] = s_xa_hist_L[3] * 3 / 4;
							s_xa_hist_R[3] = s_xa_hist_R[3] * 3 / 4;
							underrun_this++;
						}
					}

					// PS1 Gaussian interpolation (4-tap)
					// Map 16.16 fractional phase to 8-bit gauss index (0-255)
					uint8_t gi = (uint8_t)(s_xa_phase_acc >> 8);
					int32_t outL = (int32_t)s_gauss_table[0x0FF - gi] * (int32_t)s_xa_hist_L[0];
					outL += (int32_t)s_gauss_table[0x1FF - gi] * (int32_t)s_xa_hist_L[1];
					outL += (int32_t)s_gauss_table[0x100 + gi] * (int32_t)s_xa_hist_L[2];
					outL += (int32_t)s_gauss_table[0x000 + gi] * (int32_t)s_xa_hist_L[3];
					int32_t outR = (int32_t)s_gauss_table[0x0FF - gi] * (int32_t)s_xa_hist_R[0];
					outR += (int32_t)s_gauss_table[0x1FF - gi] * (int32_t)s_xa_hist_R[1];
					outR += (int32_t)s_gauss_table[0x100 + gi] * (int32_t)s_xa_hist_R[2];
					outR += (int32_t)s_gauss_table[0x000 + gi] * (int32_t)s_xa_hist_R[3];
					_spu2mem[base_l + i] = (int16_t)(outL >> 15);
					_spu2mem[base_r + i] = (int16_t)(outR >> 15);
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
						s_xa_hist_L[0] = s_xa_hist_L[1] = s_xa_hist_L[2] = s_xa_hist_L[3] = 0;
						s_xa_hist_R[0] = s_xa_hist_R[1] = s_xa_hist_R[2] = s_xa_hist_R[3] = 0;
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