// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <time.h>
#include <cmath>

#include "Common.h"
#include "R3000A.h"
#include "Counters.h"
#include "IopCounters.h"

#include "GS.h"
#include "GS/GS.h"
#include "MTGS.h"
#include "PerformanceMetrics.h"
#include "Patch.h"
#include "ps2/HwInternal.h"
#include "SIO/Sio.h"
#include "SPU2/spu2.h"
#include "Recording/InputRecording.h"
#include "VMManager.h"
#include "VUmicro.h"

static const uint EECNT_FUTURE_TARGET = 0x10000000;

uint g_FrameCount = 0;

// Counter 4 takes care of scanlines - hSync/hBlanks
// Counter 5 takes care of vSync/vBlanks
Counter counters[4];
SyncCounter hsyncCounter;
SyncCounter vsyncCounter;

u32 nextStartCounter;	// records the cpuRegs.cycle value of the last call to rcntUpdate()
s32 nextDeltaCounter;	// delta from nextsCounter, in cycles, until the next rcntUpdate()

// Forward declarations needed because C/C++ both are wimpy single-pass compilers.

static void rcntStartGate(bool mode, u32 sCycle);
static void rcntEndGate(bool mode, u32 sCycle);
static void rcntWcount(int index, u32 value);
static void rcntWmode(int index, u32 value);
static void rcntWtarget(int index, u32 value);
static void rcntWhold(int index, u32 value);

// For Analog/Double Strike and Interlace modes
static bool IsInterlacedVideoMode()
{
	return (gsVideoMode == GS_VideoMode::PAL || gsVideoMode == GS_VideoMode::NTSC || gsVideoMode == GS_VideoMode::DVD_NTSC || gsVideoMode == GS_VideoMode::DVD_PAL || gsVideoMode == GS_VideoMode::HDTV_1080I);
}

static bool IsProgressiveVideoMode()
{
	// The FIELD register only flips if the CMOD field in SMODE1 is set to anything but 0 and Front Porch bottom bit in SYNCV is set.
	// Also see "isReallyInterlaced()" in GSState.cpp
	return !(*(u32*)PS2GS_BASE(GS_SYNCV) & 0x1) || !(*(u32*)PS2GS_BASE(GS_SMODE1) & 0x6000);
}

void rcntReset(int index)
{
	counters[index].count = 0;
	counters[index].startCycle = cpuRegs.cycle;
}

// Updates the state of the nextCounter value (if needed) to serve
// any pending events for the given counter.
// Call this method after any modifications to the state of a counter.
static __fi void _rcntSet(int cntidx)
{
	s32 c;
	pxAssume(cntidx <= 4); // rcntSet isn't valid for h/vsync counters.

	const Counter& counter = counters[cntidx];

	// Stopped or special hsync gate?
	if (!rcntCanCount(cntidx) || (counter.mode.ClockSource == 0x3))
		return;

	if (!counter.mode.TargetInterrupt && !counter.mode.OverflowInterrupt && !counter.mode.ZeroReturn)
		return;
	// check for special cases where the overflow or target has just passed
	// (we probably missed it because we're doing/checking other things)
	if (counter.count > 0x10000 || counter.count > counter.target)
	{
		nextDeltaCounter = 4;
		return;
	}

	// nextCounter is relative to the cpuRegs.cycle when rcntUpdate() was last called.
	// However, the current _rcntSet could be called at any cycle count, so we need to take
	// that into account.  Adding the difference from that cycle count to the current one
	// will do the trick!

	c = ((0x10000 - counter.count) * counter.rate) - (cpuRegs.cycle - counter.startCycle);
	c += cpuRegs.cycle - nextStartCounter; // adjust for time passed since last rcntUpdate();

	if (c < nextDeltaCounter)
	{
		nextDeltaCounter = c;

		cpuSetNextEvent(nextStartCounter, nextDeltaCounter); // Need to update on counter resets/target changes
	}

	// Ignore target diff if target is currently disabled.
	// (the overflow is all we care about since it goes first, and then the
	// target will be turned on afterward, and handled in the next event test).

	if (counter.target & EECNT_FUTURE_TARGET)
	{
		return;
	}
	else
	{

		c = ((counter.target - counter.count) * counter.rate) - (cpuRegs.cycle - counter.startCycle);
		c += cpuRegs.cycle - nextStartCounter; // adjust for time passed since last rcntUpdate();

		if (c < nextDeltaCounter)
		{
			nextDeltaCounter = c;
			cpuSetNextEvent(nextStartCounter, nextDeltaCounter); // Need to update on counter resets/target changes
		}
	}
}


static __fi void cpuRcntSet()
{
	int i;

	// Default to next VBlank
	nextStartCounter = cpuRegs.cycle;
	nextDeltaCounter = vsyncCounter.deltaCycles - (cpuRegs.cycle - vsyncCounter.startCycle);

	// Also check next HSync
	s32 nextHsync = hsyncCounter.deltaCycles - (cpuRegs.cycle - hsyncCounter.startCycle);
	if (nextHsync < nextDeltaCounter)
		nextDeltaCounter = nextHsync;

	for (i = 0; i < 4; i++)
		_rcntSet(i);

	// sanity check!
	if (nextDeltaCounter < 0)
		nextDeltaCounter = 0;

	cpuSetNextEvent(nextStartCounter, nextDeltaCounter); // Need to update on counter resets/target changes
}


struct vSyncTimingInfo
{
	double Framerate;       // frames per second (8 bit fixed)
	GS_VideoMode VideoMode; // used to detect change (interlaced/progressive)
	u32 Render;             // time from vblank end to vblank start (cycles)
	u32 Blank;              // time from vblank start to vblank end (cycles)

	u32 GSBlank;            // GS CSR is swapped roughly 3.5 hblank's after vblank start

	u32 hSyncError;         // rounding error after the duration of a rendered frame (cycles)
	u32 hRender;            // time from hblank end to hblank start (cycles)
	u32 hBlank;             // time from hblank start to hblank end (cycles)
	u32 hScanlinesPerFrame; // number of scanlines per frame (525/625 for NTSC/PAL)
};

static vSyncTimingInfo vSyncInfo;

void rcntInit()
{
	int i;

	g_FrameCount = 0;

	std::memset(counters, 0, sizeof(counters));

	for (i = 0; i < 4; i++)
	{
		counters[i].rate = 2;
		counters[i].target = 0xffff;
	}
	counters[0].interrupt = 9;
	counters[1].interrupt = 10;
	counters[2].interrupt = 11;
	counters[3].interrupt = 12;

	std::memset(&vSyncInfo, 0, sizeof(vSyncInfo));

	gsVideoMode = GS_VideoMode::Uninitialized;
	gsIsInterlaced = VMManager::Internal::IsFastBootInProgress();

	hsyncCounter.Mode = MODE_HRENDER;
	hsyncCounter.startCycle = cpuRegs.cycle;
	hsyncCounter.deltaCycles = vSyncInfo.hRender;
	vsyncCounter.Mode = MODE_VRENDER;
	vsyncCounter.deltaCycles = vSyncInfo.Render;
	vsyncCounter.startCycle = cpuRegs.cycle;

	for (i = 0; i < 4; i++)
		rcntReset(i);
	cpuRcntSet();
}

static void vSyncInfoCalc(vSyncTimingInfo* info, double framesPerSecond, u32 scansPerFrame)
{
	constexpr double clock = static_cast<double>(PS2CLK);

	const u64 Frame = clock * 10000ULL / framesPerSecond;
	const u64 Scanline = Frame / scansPerFrame;

	// There are two renders and blanks per frame. This matches the PS2 test results.
	// The PAL and NTSC VBlank periods respectively lasts for approximately 22 and 26 scanlines.
	// An older test suggests that these periods are actually the periods that VBlank is off, but
	// Legendz Gekitou! Saga Battle runs very slowly if the VBlank period is inverted.
	// Some of the more timing sensitive games and their symptoms when things aren't right:
	// Dynasty Warriors 3 Xtreme Legends - fake save corruption when loading save
	// Jak II - random speedups
	// Shadow of Rome - FMV audio issues
	const bool ntsc_hblank = gsVideoMode != GS_VideoMode::PAL && gsVideoMode != GS_VideoMode::DVD_PAL;
	const u64 HalfFrame = Frame / 2;
	const float extra_scanlines = static_cast<float>(IsProgressiveVideoMode()) * (ntsc_hblank ? 0.5f : 1.5f);
	const u64 Blank = Scanline * ((ntsc_hblank ? 22.5f : 24.5f) + extra_scanlines);
	const u64 Render = HalfFrame - Blank;
	const u64 GSBlank = Scanline * ((ntsc_hblank ? 3.5 : 3) + extra_scanlines); // GS VBlank/CSR Swap happens roughly 3.5(NTSC) and 3(PAL) Scanlines after VBlank Start

	// Important!  The hRender/hBlank timer ratio below is set according to PS2 tests.
	// in EE Cycles taken from PAL system:
	// 18876 cycles for hsync
	// 15796 cycles for hsync are low (render)
	// Ratio: 83.68298368298368
	u64 hRender = Scanline * 0.8368298368298368f;
	u64 hBlank = Scanline - hRender;

	if (!IsInterlacedVideoMode())
	{
		hBlank /= 2;
		hRender /= 2;
 	}

	//TODO: Carry fixed-point math all the way through the entire vsync and hsync counting processes, and continually apply rounding
	//as needed for each scheduled v/hsync related event. Much better to handle than this messed state.
	info->Framerate = framesPerSecond;
	info->GSBlank = (u32)(GSBlank / 10000);
	info->Render = (u32)(Render / 10000);
	info->Blank = (u32)(Blank / 10000);
	const u64 accumilated_vrender = (Render % 10000) + (Blank % 10000);
	info->Render += (u32)(accumilated_vrender / 10000);

	info->hRender = (u32)(hRender / 10000);
	info->hBlank = (u32)(hBlank / 10000);
	info->hScanlinesPerFrame = scansPerFrame;

	const u64 accumilatedHRenderError = (hRender % 10000) + (hBlank % 10000);
	const u64 accumilatedHFractional = accumilatedHRenderError % 10000;
	info->hRender += (u32)(accumilatedHRenderError / 10000);
	info->hSyncError = (accumilatedHFractional * (scansPerFrame / (IsInterlacedVideoMode() ? 2 : 1))) / 10000;

	// Note: In NTSC modes there is some small rounding error in the vsync too,
	// however it would take thousands of frames for it to amount to anything and
	// is thus not worth the effort at this time.
}

const char* ReportVideoMode()
{
	switch (gsVideoMode)
	{
	case GS_VideoMode::PAL:          return "PAL";
	case GS_VideoMode::NTSC:         return "NTSC";
	case GS_VideoMode::DVD_NTSC:     return "DVD NTSC";
	case GS_VideoMode::DVD_PAL:      return "DVD PAL";
	case GS_VideoMode::VESA:         return "VESA";
	case GS_VideoMode::SDTV_480P:    return "SDTV 480p";
	case GS_VideoMode::SDTV_576P:    return "SDTV 576p";
	case GS_VideoMode::HDTV_720P:    return "HDTV 720p";
	case GS_VideoMode::HDTV_1080I:   return "HDTV 1080i";
	case GS_VideoMode::HDTV_1080P:   return "HDTV 1080p";
	default:                         return "Unknown";
	}
}

const char* ReportInterlaceMode()
{
	const u64& smode2 = *(u64*)PS2GS_BASE(GS_SMODE2);
	return !IsProgressiveVideoMode() ? ((smode2 & 2) ? "Interlaced (Frame)" : "Interlaced (Field)") : "Progressive";
}

double GetVerticalFrequency()
{
	// Note about NTSC/PAL "double strike" modes:
	// NTSC and PAL can be configured in such a way to produce a non-interlaced signal.
	// This involves modifying the signal slightly by either adding or subtracting a line (526/524 instead of 525)
	// which has the function of causing the odd and even fields to strike the same lines.
	// Doing this modifies the vertical refresh rate slightly. Beatmania is sensitive to this and
	// not accounting for it will cause the audio and video to become desynced.
	//
	// In the case of the GS, I believe it adds a halfline to the vertical back porch but more research is needed.
	// For now I'm just going to subtract off the config setting.
	//
	// According to the GS:
	// NTSC (interlaced): 59.94
	// NTSC (non-interlaced): 59.82
	// PAL (interlaced): 50.00
	// PAL (non-interlaced): 49.76
	//
	// More Information:
	// https://web.archive.org/web/20201031235528/https://wiki.nesdev.com/w/index.php/NTSC_video
	// https://web.archive.org/web/20201102100937/http://forums.nesdev.com/viewtopic.php?t=7909
	// https://web.archive.org/web/20120629231826fw_/http://ntsc-tv.com/index.html
	// https://web.archive.org/web/20200831051302/https://www.hdretrovision.com/240p/

	switch (gsVideoMode)
	{
		case GS_VideoMode::Uninitialized: // SetGsCrt hasn't executed yet, give some temporary values.
			return 60.00;
		case GS_VideoMode::PAL:
		case GS_VideoMode::DVD_PAL:
			return (IsProgressiveVideoMode() == false) ? EmuConfig.GS.FrameratePAL : EmuConfig.GS.FrameratePAL - 0.24f;
		case GS_VideoMode::NTSC:
		case GS_VideoMode::DVD_NTSC:
			return (IsProgressiveVideoMode() == false) ? EmuConfig.GS.FramerateNTSC : EmuConfig.GS.FramerateNTSC - 0.11f;
		case GS_VideoMode::SDTV_480P:
			return 59.94;
		case GS_VideoMode::HDTV_1080P:
		case GS_VideoMode::HDTV_1080I:
		case GS_VideoMode::HDTV_720P:
		case GS_VideoMode::SDTV_576P:
		case GS_VideoMode::VESA:
			return 60.00;
		default:
			// Pass NTSC vertical frequency value when unknown video mode is detected.
			return FRAMERATE_NTSC * 2;
	}
}

void UpdateVSyncRate(bool force)
{
	// Notice:  (and I probably repeat this elsewhere, but it's worth repeating)
	//  The PS2's vsync timer is an *independent* crystal that is fixed to either 59.94 (NTSC)
	//  or 50.0 (PAL) Hz.  It has *nothing* to do with real TV timings or the real vsync of
	//  the GS's output circuit.  It is the same regardless if the GS is outputting interlace
	//  or progressive scan content.

	const double vertical_frequency = GetVerticalFrequency();

	const double frames_per_second = vertical_frequency / 2.0;

	if (vSyncInfo.Framerate != frames_per_second || vSyncInfo.VideoMode != gsVideoMode || force)
	{
		u32 total_scanlines = 0;
		bool custom = false;

		switch (gsVideoMode)
		{
			case GS_VideoMode::Uninitialized: // SYSCALL instruction hasn't executed yet, give some temporary values.
				if (gsIsInterlaced)
					total_scanlines = SCANLINES_TOTAL_NTSC_I;
				else
					total_scanlines = SCANLINES_TOTAL_NTSC_NI;
				break;
			case GS_VideoMode::PAL:
			case GS_VideoMode::DVD_PAL:
				custom = (EmuConfig.GS.FrameratePAL != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_PAL);
				if (gsIsInterlaced)
					total_scanlines = SCANLINES_TOTAL_PAL_I;
				else
					total_scanlines = SCANLINES_TOTAL_PAL_NI;
				break;
			case GS_VideoMode::NTSC:
			case GS_VideoMode::DVD_NTSC:
				custom = (EmuConfig.GS.FramerateNTSC != Pcsx2Config::GSOptions::DEFAULT_FRAME_RATE_NTSC);
				if (gsIsInterlaced)
					total_scanlines = SCANLINES_TOTAL_NTSC_I;
				else
					total_scanlines = SCANLINES_TOTAL_NTSC_NI;
				break;
			case GS_VideoMode::SDTV_480P:
			case GS_VideoMode::SDTV_576P:
			case GS_VideoMode::HDTV_720P:
			case GS_VideoMode::VESA:
				total_scanlines = SCANLINES_TOTAL_NTSC_I;
				break;
			case GS_VideoMode::HDTV_1080P:
			case GS_VideoMode::HDTV_1080I:
				total_scanlines = SCANLINES_TOTAL_1080;
				break;
			case GS_VideoMode::Unknown:
			default:
				if (gsIsInterlaced)
					total_scanlines = SCANLINES_TOTAL_NTSC_I;
				else
					total_scanlines = SCANLINES_TOTAL_NTSC_NI;
				Console.Error("PCSX2-Counters: Unknown video mode detected");
				pxAssertMsg(false, "Unknown video mode detected via SetGsCrt");
		}

		const bool video_mode_initialized = gsVideoMode != GS_VideoMode::Uninitialized;

		// NBA Jam 2004 PAL will fail to display 3D on the menu if this value isn't correct on reset.
		if (video_mode_initialized && vSyncInfo.VideoMode != gsVideoMode)
			CSRreg.FIELD = 1;

		vSyncInfo.VideoMode = gsVideoMode;

		vSyncInfoCalc(&vSyncInfo, frames_per_second, total_scanlines);

		if (video_mode_initialized)
			Console.WriteLn(Color_Green, "(UpdateVSyncRate) Mode Changed to %s.", ReportVideoMode());

		if (custom && video_mode_initialized)
			Console.WriteLn(Color_StrongGreen, "  ... with user configured refresh rate: %.02f Hz", vertical_frequency);

		s32 hdiff = hsyncCounter.deltaCycles;
		s32 vdiff = vsyncCounter.deltaCycles;
		hsyncCounter.deltaCycles = (hsyncCounter.Mode == MODE_HBLANK) ? vSyncInfo.hBlank : vSyncInfo.hRender;
		vsyncCounter.deltaCycles = (vsyncCounter.Mode == MODE_GSBLANK) ?
								  vSyncInfo.GSBlank :
								  ((vsyncCounter.Mode == MODE_VBLANK) ? vSyncInfo.Blank : vSyncInfo.Render);

		hsyncCounter.startCycle += hdiff - hsyncCounter.deltaCycles;
		vsyncCounter.startCycle += vdiff - vsyncCounter.deltaCycles;

		cpuRcntSet();

		VMManager::Internal::FrameRateChanged();
	}
}

// FMV switch stuff
extern uint eecount_on_last_vdec;
extern bool FMVstarted;
extern bool EnableFMV;

static bool s_last_fmv_state = false;

static __fi void DoFMVSwitch()
{
	bool new_fmv_state = s_last_fmv_state;
	if (EnableFMV)
	{
		DevCon.WriteLn("FMV started");
		new_fmv_state = true;
		EnableFMV = false;
	}
	else if (FMVstarted)
	{
		const int diff = cpuRegs.cycle - eecount_on_last_vdec;
		if (diff > 60000000)
		{
			DevCon.WriteLn("FMV ended");
			new_fmv_state = false;
			FMVstarted = false;
		}
	}

	if (new_fmv_state == s_last_fmv_state)
		return;

	s_last_fmv_state = new_fmv_state;

	switch (EmuConfig.GS.FMVAspectRatioSwitch)
	{
		case FMVAspectRatioSwitchType::Off:
			break;
		case FMVAspectRatioSwitchType::RAuto4_3_3_2:
			EmuConfig.CurrentAspectRatio = new_fmv_state ? AspectRatioType::RAuto4_3_3_2 : EmuConfig.GS.AspectRatio;
			break;
		case FMVAspectRatioSwitchType::R4_3:
			EmuConfig.CurrentAspectRatio = new_fmv_state ? AspectRatioType::R4_3 : EmuConfig.GS.AspectRatio;
			break;
		case FMVAspectRatioSwitchType::R16_9:
			EmuConfig.CurrentAspectRatio = new_fmv_state ? AspectRatioType::R16_9 : EmuConfig.GS.AspectRatio;
			break;
		default:
			break;
	}

	if (EmuConfig.Gamefixes.SoftwareRendererFMVHack && EmuConfig.GS.UseHardwareRenderer())
	{
		DevCon.Warning("FMV Switch");
		// we don't use the sw toggle here, because it'll change back to auto if set to sw
		MTGS::SetSoftwareRendering(new_fmv_state, new_fmv_state ? GSInterlaceMode::AdaptiveTFF : EmuConfig.GS.InterlaceMode, false);
	}
}

static __fi void VSyncStart(u32 sCycle)
{
	// End-of-frame tasks.
	DoFMVSwitch();
	VMManager::Internal::VSyncOnCPUThread();

	// Don't bother throttling if we're going to pause.
	if (!VMManager::Internal::IsExecutionInterrupted())
		VMManager::Internal::Throttle();

	gsPostVsyncStart(); // MUST be after framelimit; doing so before causes funk with frame times!

	// Poll input after MTGS frame push, just in case it has to stall to catch up.
	VMManager::Internal::PollInputOnCPUThread();

	if (EmuConfig.Trace.Enabled && EmuConfig.Trace.EE.m_EnableAll)
		SysTrace.EE.Counters.Write("    ================  EE COUNTER VSYNC START (frame: %d)  ================", g_FrameCount);

	// Memcard auto ejection - Uses a tick system timed off of real time, decrementing one tick per frame.
	AutoEject::CountDownTicks();
	// Memcard IO detection - Uses a tick system to determine when memcards are no longer being written.
	MemcardBusy::Decrement();

	if (!GSSMODE1reg.SINT)
	{
		hwIntcIrq(INTC_VBLANK_S);
		rcntStartGate(true, sCycle); // Counters Start Gate code
		psxVBlankStart();
	}

	// INTC - VB Blank Start Hack --
	// Hack fix!  This corrects a freezeup in Granda 2 where it decides to spin
	// on the INTC_STAT register after the exception handler has already cleared
	// it.  But be warned!  Set the value to larger than 4 and it breaks Dark
	// Cloud and other games. -_-

	// How it works: Normally the INTC raises exceptions immediately at the end of the
	// current branch test.  But in the case of Grandia 2, the game's code is spinning
	// on the INTC status, and the exception handler (for some reason?) clears the INTC
	// before returning *and* returns to a location other than EPC.  So the game never
	// gets to the point where it sees the INTC Irq set true.

	// (I haven't investigated why Dark Cloud freezes on larger values)
	// (all testing done using the recompiler -- dunno how the ints respond yet)

	//cpuRegs.eCycle[30] = 2;

	// Update 08/2021: The only game I know to require this kind of thing as of 1.7.0 is Penny Racers/Gadget Racers (which has a patch to avoid the problem and others)
	// These games have a tight loop checking INTC_STAT waiting for the VBLANK Start, however the game also has a VBLANK Hander which clears it.
	// Therefore, there needs to be some delay in order for it to see the interrupt flag before the interrupt is acknowledged, likely helped on real hardware by the pipelines.
	// Without the patch and fixing this, the games have other issues, so I'm not going to rush to fix it.
	// Refraction

	// Bail out before the next frame starts if we're paused, or the CPU has changed.
	// Need to re-check this, because we might've paused during the sleep time.
	if (VMManager::Internal::IsExecutionInterrupted())
		Cpu->ExitExecution();
}

static __fi void GSVSync()
{
	// CSR is swapped and GS vBlank IRQ is triggered roughly 3.5 hblanks after VSync Start
	if (GSSMODE1reg.SINT)
		return;

	if (IsProgressiveVideoMode())
		CSRreg.SetField();
	else
		CSRreg.SwapField();

	if (!CSRreg.VSINT)
	{
		CSRreg.VSINT = true;
		if (!GSIMR.VSMSK)
			gsIrq();
	}
}

static __fi void VSyncEnd(u32 sCycle)
{
	if (EmuConfig.Trace.Enabled && EmuConfig.Trace.EE.m_EnableAll)
		SysTrace.EE.Counters.Write("    ================  EE COUNTER VSYNC END (frame: %d)  ================", g_FrameCount);

	g_FrameCount++;
	if (!GSSMODE1reg.SINT)
	{
		hwIntcIrq(INTC_VBLANK_E); // HW Irq
		psxVBlankEnd(); // psxCounters vBlank End
		rcntEndGate(true, sCycle); // Counters End Gate Code
	}

	// This doesn't seem to be needed here.  Games only seem to break with regard to the
	// vsyncstart irq.
	//cpuRegs.eCycle[30] = 2;
}

//#define VSYNC_DEBUG		// Uncomment this to enable some vSync Timer debugging features.
#ifdef VSYNC_DEBUG
static u32 hsc = 0;
static int vblankinc = 0;
#endif

__fi void rcntUpdate_vSync()
{
	if (!cpuTestCycle(vsyncCounter.startCycle, vsyncCounter.deltaCycles))
		return;

	if (vsyncCounter.Mode == MODE_VBLANK)
	{
		vsyncCounter.startCycle += vSyncInfo.Blank;
		vsyncCounter.deltaCycles = vSyncInfo.Render;

		VSyncEnd(vsyncCounter.startCycle);

		vsyncCounter.Mode = MODE_VRENDER; // VSYNC END - Render begin
	}
	else if (vsyncCounter.Mode == MODE_GSBLANK) // GS CSR Swap and interrupt
	{
		GSVSync();

		vsyncCounter.Mode = MODE_VBLANK;
		// Don't set the start cycle, makes it easier to calculate the correct Vsync End time
		vsyncCounter.deltaCycles = vSyncInfo.Blank;
	}
	else // VSYNC Start
	{
		vsyncCounter.startCycle += vSyncInfo.Render;
		vsyncCounter.deltaCycles = vSyncInfo.GSBlank;

		VSyncStart(vsyncCounter.startCycle);

		vsyncCounter.Mode = MODE_GSBLANK;

		// Accumulate hsync rounding errors:
		hsyncCounter.deltaCycles += vSyncInfo.hSyncError;

#ifdef VSYNC_DEBUG
		vblankinc++;
		if (vblankinc > 1)
		{
			if (hsc != vSyncInfo.hScanlinesPerFrame)
				Console.WriteLn(" ** vSync > Abnormal Scanline Count: %d", hsc);
			hsc = 0;
			vblankinc = 0;
		}
#endif
	}
}

__fi void rcntUpdate_hScanline()
{
	if (!cpuTestCycle(hsyncCounter.startCycle, hsyncCounter.deltaCycles))
		return;

	//iopEventAction = 1;
	if (hsyncCounter.Mode == MODE_HBLANK)
	{ //HBLANK End / HRENDER Begin

		// Setup the hRender's start and end cycle information:
		hsyncCounter.startCycle += vSyncInfo.hBlank; // start  (absolute cycle value)
		hsyncCounter.deltaCycles = vSyncInfo.hRender; // endpoint (delta from start value)
		if (!GSSMODE1reg.SINT)
		{
			rcntEndGate(false, hsyncCounter.startCycle);
			psxHBlankEnd();
		}

		hsyncCounter.Mode = MODE_HRENDER;
	}
	else
	{ //HBLANK START / HRENDER End
		
		// set up the hblank's start and end cycle information:
		hsyncCounter.startCycle += vSyncInfo.hRender; // start (absolute cycle value)
		hsyncCounter.deltaCycles = vSyncInfo.hBlank;   // endpoint (delta from start value)
		if (!GSSMODE1reg.SINT)
		{
			if (!CSRreg.HSINT)
			{
				CSRreg.HSINT = true;
				if (!GSIMR.HSMSK)
					gsIrq();
			}

			rcntStartGate(false, hsyncCounter.startCycle);
			psxHBlankStart();
		}

		hsyncCounter.Mode = MODE_HBLANK;

#ifdef VSYNC_DEBUG
		hsc++;
#endif
	}
}

static __fi void _cpuTestTarget(int i)
{
	if (counters[i].count < counters[i].target)
		return;

	if (counters[i].mode.TargetInterrupt)
	{
		EECNT_LOG("EE Counter[%d] TARGET reached - mode=%x, count=%x, target=%x", i, counters[i].mode, counters[i].count, counters[i].target);
		if (!counters[i].mode.TargetReached)
		{
			counters[i].mode.TargetReached = 1;
			hwIntcIrq(counters[i].interrupt);
		}
	}

	if (counters[i].mode.ZeroReturn)
		counters[i].count -= counters[i].target; // Reset on target
	else
		counters[i].target |= EECNT_FUTURE_TARGET; // OR with future target to prevent a retrigger
}

static __fi void _cpuTestOverflow(int i)
{
	if (counters[i].count <= 0xffff)
		return;

	if (counters[i].mode.OverflowInterrupt)
	{
		EECNT_LOG("EE Counter[%d] OVERFLOW - mode=%x, count=%x", i, counters[i].mode, counters[i].count);
		if (!counters[i].mode.OverflowReached)
		{
			counters[i].mode.OverflowReached = 1;
			hwIntcIrq(counters[i].interrupt);
		}
	}

	// wrap counter back around zero, and enable the future target:
	counters[i].count -= 0x10000;
	counters[i].target &= 0xffff;
}


__fi bool rcntCanCount(int i)
{
	if (!counters[i].mode.IsCounting)
		return false;

	if (!counters[i].mode.EnableGate)
		return true;

	// If we're in gate mode, we can only count if it's not both gated and counting on HBLANK or GateMode is not 0 (Count only when low) or the signal is low.
	return ((counters[i].mode.GateSource == 0 && counters[i].mode.ClockSource != 3 && (hsyncCounter.Mode == MODE_HRENDER || counters[i].mode.GateMode != 0)) ||
			(counters[i].mode.GateSource == 1 && (vsyncCounter.Mode == MODE_VRENDER || counters[i].mode.GateMode != 0)));
}

__fi void rcntSyncCounter(int i)
{
	if (counters[i].mode.ClockSource != 0x3) // don't count hblank sources
	{
		const u32 change = (cpuRegs.cycle - counters[i].startCycle) / counters[i].rate;
		counters[i].startCycle += change * counters[i].rate;

		counters[i].startCycle &= ~(counters[i].rate - 1);

		if (rcntCanCount(i))
			counters[i].count += change;
	}
	else
		counters[i].startCycle = cpuRegs.cycle;
}

// forceinline note: this method is called from two locations, but one
// of them is the interpreter, which doesn't count. ;)  So might as
// well forceinline it!
__fi void rcntUpdate()
{
	rcntUpdate_vSync();
	// HBlank after as VSync can do error compensation
	rcntUpdate_hScanline();

	// Update counters so that we can perform overflow and target tests.

	for (int i = 0; i <= 3; i++)
	{
		rcntSyncCounter(i);

		if (counters[i].mode.ClockSource == 0x3 || !rcntCanCount(i)) // don't count hblank sources
				continue;

		_cpuTestOverflow(i);
		_cpuTestTarget(i);
	}

	cpuRcntSet();
}

static __fi void _rcntSetGate(int index)
{
	if (counters[index].mode.EnableGate)
	{
		// If the Gate Source is hblank and the clock selection is also hblank
		// the timer completely turns off (HW Tested).
		if (!(counters[index].mode.GateSource == 0 && counters[index].mode.ClockSource == 3))
			EECNT_LOG("EE Counter[%d] Using Gate!  Source=%s, Mode=%d.",
				index, counters[index].mode.GateSource ? "vblank" : "hblank", counters[index].mode.GateMode);
		else
			EECNT_LOG("EE Counter[%d] GATE DISABLED because of hblank source.", index);
	}
}

// mode - 0 means hblank source, 8 means vblank source.
static __fi void rcntStartGate(bool isVblank, u32 sCycle)
{
	for (int i = 0; i < 4; i++)
	{
		if (!isVblank && (counters[i].mode.ClockSource == 3) && rcntCanCount(i))
		{
			// Update counters using the hblank as the clock.  This keeps the hblank source
			// nicely in sync with the counters and serves as an optimization also, since these
			// counter won't receive special rcntUpdate scheduling.
			// Note: Target and overflow tests must be done here since they won't be done
			// currectly by rcntUpdate (since it's not being scheduled for these counters)
			counters[i].count += HBLANK_COUNTER_SPEED;
			_cpuTestOverflow(i);
			_cpuTestTarget(i);
		}

		if (!counters[i].mode.EnableGate)
			continue;

		if ((!!counters[i].mode.GateSource) != isVblank)
			continue;

		switch (counters[i].mode.GateMode)
		{
			case 0x0:  //Count When Signal is low (V_RENDER ONLY)

				// Just set the start cycle -- counting will be done as needed
				// for events (overflows, targets, mode changes, and the gate off below)
				rcntSyncCounter(i);
				counters[i].startCycle = sCycle & ~(counters[i].rate - 1);
				EECNT_LOG("EE Counter[%d] %s StartGate Type0, count = %x", i,
					isVblank ? "vblank" : "hblank", counters[i].count);
				break;
			case 0x2: // Reset on Vsync end
				// This is the vsync start so do nothing.
				break;
			case 0x1: // Reset on Vsync start
			case 0x3: // Reset on Vsync start and end
				rcntSyncCounter(i);
				counters[i].count = 0;
				counters[i].target &= 0xffff;
				counters[i].startCycle = sCycle & ~(counters[i].rate - 1);
				EECNT_LOG("EE Counter[%d] %s StartGate Type%d, count = %x", i,
					isVblank ? "vblank" : "hblank", counters[i].mode.GateMode, counters[i].count);
				break;
		}
	}

	// No need to update actual counts here.  Counts are calculated as needed by reads to
	// rcntRcount().  And so long as sCycleT is set properly, any targets or overflows
	// will be scheduled and handled.

	// Note: No need to set counters here.  They'll get set when control returns to
	// rcntUpdate, since we're being called from there anyway.
}

// mode - 0 means hblank signal, 8 means vblank signal.
static __fi void rcntEndGate(bool isVblank, u32 sCycle)
{
	for (int i = 0; i < 4; i++)
	{
		if (!counters[i].mode.EnableGate)
			continue;

		if ((!!counters[i].mode.GateSource) != isVblank)
			continue;

		switch (counters[i].mode.GateMode)
		{
			case 0x0: //Count When Signal is low (V_RENDER ONLY)
				counters[i].startCycle = sCycle & ~(counters[i].rate - 1);

				EECNT_LOG("EE Counter[%d] %s EndGate Type0, count = %x", i,
					isVblank ? "vblank" : "hblank", counters[i].count);
				break;

			case 0x1: // Reset on Vsync start
				// This is the vsync end so do nothing
				break;

			case 0x2: // Reset on Vsync end
			case 0x3: // Reset on Vsync start and end
				rcntSyncCounter(i);
				EECNT_LOG("EE Counter[%d]  %s EndGate Type%d, count = %x", i,
					isVblank ? "vblank" : "hblank", counters[i].mode.GateMode, counters[i].count);
				counters[i].count = 0;
				counters[i].target &= 0xffff;
				counters[i].startCycle = sCycle & ~(counters[i].rate - 1);
				break;
		}
	}
	// Note: No need to set counters here.  They'll get set when control returns to
	// rcntUpdate, since we're being called from there anyway.
}

static __fi void rcntWmode(int index, u32 value)
{
	rcntSyncCounter(index);

	// Clear OverflowReached and TargetReached flags (0xc00 mask), but *only* if they are set to 1 in the
	// given value.  (yes, the bits are cleared when written with '1's).

	counters[index].modeval &= ~(value & 0xc00);
	counters[index].modeval = (counters[index].modeval & 0xc00) | (value & 0x3ff);
	EECNT_LOG("EE Counter[%d] writeMode = %x passed value=%x", index, counters[index].modeval, value);

	switch (counters[index].mode.ClockSource) { //Clock rate divisers *2, they use BUSCLK speed not PS2CLK
		case 0: counters[index].rate = 2; break;
		case 1: counters[index].rate = 32; break;
		case 2: counters[index].rate = 512; break;
		case 3: counters[index].rate = vSyncInfo.hBlank+vSyncInfo.hRender; break;
	}

	// In case the rate has changed we need to set the start cycle to the previous tick.
	counters[index].startCycle = cpuRegs.cycle & ~(counters[index].rate - 1);
	_rcntSetGate(index);
	_rcntSet(index);
}

static __fi void rcntWcount(int index, u32 value)
{
	EECNT_LOG("EE Counter[%d] writeCount = %x,   oldcount=%x, target=%x", index, value, counters[index].count, counters[index].target);

	// re-calculate the start cycle of the counter based on elapsed time since the last counter update:
	rcntSyncCounter(index);

	counters[index].count = value & 0xffff;

	// reset the target, and make sure we don't get a premature target.
	counters[index].target &= 0xffff;

	if (counters[index].count >= counters[index].target)
		counters[index].target |= EECNT_FUTURE_TARGET;

	_rcntSet(index);
}

static __fi void rcntWtarget(int index, u32 value)
{
	EECNT_LOG("EE Counter[%d] writeTarget = %x", index, value);

	counters[index].target = value & 0xffff;

	// guard against premature (instant) targeting.
	// If the target is behind the current count, set it up so that the counter must
	// overflow first before the target fires:

	rcntSyncCounter(index);

	if (counters[index].target <= counters[index].count)
		counters[index].target |= EECNT_FUTURE_TARGET;

	_rcntSet(index);
}

static __fi void rcntWhold(int index, u32 value)
{
	EECNT_LOG("EE Counter[%d] Hold Write = %x", index, value);
	counters[index].hold = value;
}

__fi u32 rcntRcount(int index)
{
	u32 ret;

	rcntSyncCounter(index);
	
	ret = counters[index].count;

	// Spams the Console.
	EECNT_LOG("EE Counter[%d] readCount32 = %x", index, ret);
	return (u16)ret;
}

template <uint page>
__fi u16 rcntRead32(u32 mem)
{
	// Important DevNote:
	// Yes this uses a u16 return value on purpose!  The upper bits 16 of the counter registers
	// are all fixed to 0, so we always truncate everything in these two pages using a u16
	// return value! --air

	switch( mem )
	{
		case(RCNT0_COUNT):	return (u16)rcntRcount(0);
		case(RCNT0_MODE):	return (u16)counters[0].modeval;
		case(RCNT0_TARGET):	return (u16)counters[0].target;
		case(RCNT0_HOLD):	return (u16)counters[0].hold;

		case(RCNT1_COUNT):	return (u16)rcntRcount(1);
		case(RCNT1_MODE):	return (u16)counters[1].modeval;
		case(RCNT1_TARGET):	return (u16)counters[1].target;
		case(RCNT1_HOLD):	return (u16)counters[1].hold;

		case(RCNT2_COUNT):	return (u16)rcntRcount(2);
		case(RCNT2_MODE):	return (u16)counters[2].modeval;
		case(RCNT2_TARGET):	return (u16)counters[2].target;

		case(RCNT3_COUNT):	return (u16)rcntRcount(3);
		case(RCNT3_MODE):	return (u16)counters[3].modeval;
		case(RCNT3_TARGET):	return (u16)counters[3].target;
	}

	return psHu16(mem);
}

template <uint page>
__fi bool rcntWrite32(u32 mem, mem32_t& value)
{
	pxAssume(mem >= RCNT0_COUNT && mem < 0x10002000);

	// [TODO] : counters should actually just use the EE's hw register space for storing
	// count, mode, target, and hold. This will allow for a simplified handler for register
	// reads.

	switch( mem )
	{
		case(RCNT0_COUNT):	return rcntWcount(0, value),	false;
		case(RCNT0_MODE):	return rcntWmode(0, value),		false;
		case(RCNT0_TARGET):	return rcntWtarget(0, value),	false;
		case(RCNT0_HOLD):	return rcntWhold(0, value),		false;

		case(RCNT1_COUNT):	return rcntWcount(1, value),	false;
		case(RCNT1_MODE):	return rcntWmode(1, value),		false;
		case(RCNT1_TARGET):	return rcntWtarget(1, value),	false;
		case(RCNT1_HOLD):	return rcntWhold(1, value),		false;

		case(RCNT2_COUNT):	return rcntWcount(2, value),	false;
		case(RCNT2_MODE):	return rcntWmode(2, value),		false;
		case(RCNT2_TARGET):	return rcntWtarget(2, value),	false;

		case(RCNT3_COUNT):	return rcntWcount(3, value),	false;
		case(RCNT3_MODE):	return rcntWmode(3, value),		false;
		case(RCNT3_TARGET):	return rcntWtarget(3, value),	false;
	}

	// unhandled .. do memory writeback.
	return true;
}

template u16 rcntRead32<0x00>(u32 mem);
template u16 rcntRead32<0x01>(u32 mem);

template bool rcntWrite32<0x00>(u32 mem, mem32_t& value);
template bool rcntWrite32<0x01>(u32 mem, mem32_t& value);

bool SaveStateBase::rcntFreeze()
{
	Freeze(counters);
	Freeze(hsyncCounter);
	Freeze(vsyncCounter);
	Freeze(nextDeltaCounter);
	Freeze(nextStartCounter);
	Freeze(vSyncInfo);
	Freeze(gsVideoMode);
	Freeze(gsIsInterlaced);

	if (IsLoading())
		cpuRcntSet();

	return IsOkay();
}
