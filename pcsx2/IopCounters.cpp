// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

// Note on INTC usage: All counters code is always called from inside the context of an
// event test, so instead of using the iopTestIntc we just set the 0x1070 flags directly.
// The EventText function will pick it up.

#include "IopCounters.h"
#include "R3000A.h"
#include "Common.h"
#include "SPU2/spu2.h"
#include "DEV9/DEV9.h"
#include "USB/USB.h"
#include "IopHw.h"
#include "IopDma.h"
#include "CDVD/CDVD.h"

#include <math.h>

/* Config.PsxType == 1: PAL:
	 VBlank interlaced		50.00 Hz
	 VBlank non-interlaced	49.76 Hz
	 HBlank					15.625 KHz
   Config.PsxType == 0: NSTC
	 VBlank interlaced		59.94 Hz
	 VBlank non-interlaced	59.82 Hz
	 HBlank					15.73426573 KHz */

// Misc IOP Clocks
// FIXME: this divider is actually 2.73 (36864000 / 13500000), but not sure what uses it, so this'll do, we should maybe change things to float.
#define PSXPIXEL 3
#define PSXSOUNDCLK ((int)(48000))

psxCounter psxCounters[NUM_COUNTERS];
s32 psxNextCounter;
u32 psxNextsCounter;

bool hBlanking = false;
bool vBlanking = false;

// flags when the gate is off or counter disabled. (do not count)
#define IOPCNT_STOPPED (0x10000000ul)

// used to disable targets until after an overflow
#define IOPCNT_FUTURE_TARGET (0x1000000000ULL)
#define IOPCNT_MODE_WRITE_MSK 0x63FF
#define IOPCNT_MODE_FLAG_MSK 0x1800

#define IOPCNT_ENABLE_GATE (1 << 0)    // enables gate-based counters
#define IOPCNT_MODE_GATE (3 << 1)      // 0x6  Gate mode (dependant on counter)
#define IOPCNT_MODE_RESET_CNT (1 << 3) // 0x8  resets the counter on target (if interrupt only?)
#define IOPCNT_INT_TARGET (1 << 4)     // 0x10  triggers an interrupt on targets
#define IOPCNT_INT_OVERFLOW (1 << 5)   // 0x20  triggers an interrupt on overflows
#define IOPCNT_INT_REPEAT (1 << 6)     // 0x40  0=One shot (ignore TOGGLE bit 7) 1=Repeat Fire (Check TOGGLE bit 7)
#define IOPCNT_INT_TOGGLE (1 << 7)     // 0x80  0=Pulse (reset on read), 1=toggle each interrupt condition (in 1 shot not reset after fired)
#define IOPCNT_ALT_SOURCE (1 << 8)     // 0x100 uses hblank on counters 1 and 3, and PSXCLOCK on counter 0
#define IOPCNT_INT_REQ (1 << 10)       // 0x400 1=Can fire interrupt, 0=Interrupt Fired (reset on read if not 1 shot)
#define IOPCNT_INT_CMPFLAG  (1 << 11)  // 0x800 1=Target interrupt raised
#define IOPCNT_INT_OFLWFLAG (1 << 12)  // 0x1000 1=Overflow interrupt raised

// Use an arbitrary value to flag HBLANK counters.
// These counters will be counted by the hblank gates coming from the EE,
// which ensures they stay 100% in sync with the EE's hblank counters.
#define PSXHBLANK 0x2001

#if 0
// Unused
static void psxRcntReset(int index)
{
	psxCounters[index].count = 0;
	psxCounters[index].mode &= ~0x18301C00;
	psxCounters[index].sCycleT = psxRegs.cycle;
}
#endif

static bool psxRcntCanCount(int cntidx)
{
	if (psxCounters[cntidx].mode & IOPCNT_STOPPED)
		return false;

	if (!(psxCounters[cntidx].mode & IOPCNT_ENABLE_GATE))
		return true;

	const u32 gateMode = (psxCounters[cntidx].mode & IOPCNT_MODE_GATE) >> 1;

	if (cntidx == 2 || cntidx == 4 || cntidx == 5)
	{
		// Gates being enabled on these counters forces it to disable the counter if being on or off depends on a gate being on or off.
		return (gateMode & 1);
	}

	const bool blanking = cntidx == 0 ? hBlanking : vBlanking;

	// Stop counting if Gate mode 0 (only count when rendering) and blanking or Gate mode 2 (only count when blanking) when not blanking
	if ((gateMode == 0 && blanking == true) || (gateMode == 2 && blanking == false))
		return false;

	// All other cases allow counting.
	return true;
}

static void _rcntSet(int cntidx)
{
	u64 overflowCap = (cntidx >= 3) ? 0x100000000ULL : 0x10000;
	u64 c;

	const psxCounter& counter = psxCounters[cntidx];

	// psxNextCounter is relative to the psxRegs.cycle when rcntUpdate() was last called.
	// However, the current _rcntSet could be called at any cycle count, so we need to take
	// that into account.  Adding the difference from that cycle count to the current one
	// will do the trick!

	if (counter.rate == PSXHBLANK || !psxRcntCanCount(cntidx))
		return;

	if (!(counter.mode & (IOPCNT_INT_TARGET | IOPCNT_INT_OVERFLOW)))
		return;
	// check for special cases where the overflow or target has just passed
	// (we probably missed it because we're doing/checking other things)
	if (counter.count > overflowCap || counter.count > counter.target)
	{
		psxNextCounter = 4;
		return;
	}

	c = (u64)((overflowCap - counter.count) * counter.rate) - (psxRegs.cycle - counter.sCycleT);
	c += psxRegs.cycle - psxNextsCounter; // adjust for time passed since last rcntUpdate();

	if (c < (u64)psxNextCounter)
	{
		psxNextCounter = (u32)c;
		psxSetNextBranch(psxNextsCounter, psxNextCounter); //Need to update on counter resets/target changes
	}

	//if((counter.mode & 0x10) == 0 || psxCounters[i].target > 0xffff) continue;
	if (counter.target & IOPCNT_FUTURE_TARGET)
		return;

	c = (s64)((counter.target - counter.count) * counter.rate) - (psxRegs.cycle - counter.sCycleT);
	c += psxRegs.cycle - psxNextsCounter; // adjust for time passed since last rcntUpdate();

	if (c < (u64)psxNextCounter)
	{
		psxNextCounter = (u32)c;
		psxSetNextBranch(psxNextsCounter, psxNextCounter); //Need to update on counter resets/target changes
	}
}


void psxRcntInit()
{
	int i;

	std::memset(psxCounters, 0, sizeof(psxCounters));

	for (i = 0; i < 3; i++)
	{
		psxCounters[i].rate = 1;
		psxCounters[i].mode |= IOPCNT_INT_REQ;
		psxCounters[i].target = IOPCNT_FUTURE_TARGET;
	}
	for (i = 3; i < 6; i++)
	{
		psxCounters[i].rate = 1;
		psxCounters[i].mode |= IOPCNT_INT_REQ;
		psxCounters[i].target = IOPCNT_FUTURE_TARGET;
	}

	psxCounters[0].interrupt = 0x10;
	psxCounters[1].interrupt = 0x20;
	psxCounters[2].interrupt = 0x40;

	psxCounters[3].interrupt = 0x04000;
	psxCounters[4].interrupt = 0x08000;
	psxCounters[5].interrupt = 0x10000;

	psxCounters[6].rate = 768;
	psxCounters[6].CycleT = psxCounters[6].rate;
	psxCounters[6].mode = 0x8;

	psxCounters[7].rate = PSXCLK / 1000;
	psxCounters[7].CycleT = psxCounters[7].rate;
	psxCounters[7].mode = 0x8;

	for (i = 0; i < 8; i++)
		psxCounters[i].sCycleT = psxRegs.cycle;

	// Tell the IOP to branch ASAP, so that timers can get
	// configured properly.
	psxNextCounter = 1;
	psxNextsCounter = psxRegs.cycle;
}

static bool _rcntFireInterrupt(int i, bool isOverflow)
{
	bool ret = false;
#
	if (psxCounters[i].mode & IOPCNT_INT_REQ)
	{
		// IRQ fired
		PSXCNT_LOG("Counter %d %s IRQ Fired count %x", i, isOverflow ? "Overflow" : "Target", psxCounters[i].count);
		psxHu32(0x1070) |= psxCounters[i].interrupt;
		iopTestIntc();
		ret = true;
	}
	else
	{
		//DevCon.Warning("Counter %d IRQ not fired count %x", i, psxCounters[i].count);
		if (!(psxCounters[i].mode & IOPCNT_INT_REPEAT)) // One shot
		{
			PSXCNT_LOG("Counter %x ignoring %s interrupt (One Shot)", i, isOverflow ? "Overflow" : "Target");
			return false;
		}
	}

	if (psxCounters[i].mode & IOPCNT_INT_TOGGLE)
	{
		// Toggle mode
		psxCounters[i].mode ^= IOPCNT_INT_REQ; // Interrupt flag inverted
	}
	else
	{
		psxCounters[i].mode &= ~IOPCNT_INT_REQ; // Interrupt flag set low
	}

	return ret;
}
static void _rcntTestTarget(int i)
{
	if (psxCounters[i].count < psxCounters[i].target)
		return;

	PSXCNT_LOG("IOP Counter[%d] target 0x%I64x >= 0x%I64x (mode: %x)",
			   i, psxCounters[i].count, psxCounters[i].target, psxCounters[i].mode);

	if (psxCounters[i].mode & IOPCNT_INT_TARGET)
	{
		// Target interrupt
		if (_rcntFireInterrupt(i, false))
			psxCounters[i].mode |= IOPCNT_INT_CMPFLAG;
	}

	if (psxCounters[i].mode & IOPCNT_MODE_RESET_CNT)
	{
		// Reset on target
		psxCounters[i].count -= psxCounters[i].target;
	}
	else
		psxCounters[i].target |= IOPCNT_FUTURE_TARGET;
}


static __fi void _rcntTestOverflow(int i)
{
	u64 maxTarget = (i < 3) ? 0xffff : 0xfffffffful;
	if (psxCounters[i].count <= maxTarget)
		return;

	PSXCNT_LOG("IOP Counter[%d] overflow 0x%I64x >= 0x%I64x (mode: %x)",
			   i, psxCounters[i].count, maxTarget, psxCounters[i].mode);

	if ((psxCounters[i].mode & IOPCNT_INT_OVERFLOW))
	{
		// Overflow interrupt
		if (_rcntFireInterrupt(i, true))
			psxCounters[i].mode |= IOPCNT_INT_OFLWFLAG; // Overflow flag
	}

	// Update count.
	// Count wraps around back to zero, while the target is restored (if not in one shot mode).
	// (high bit of the target gets set by rcntWtarget when the target is behind
	// the counter value, and thus should not be flagged until after an overflow)

	psxCounters[i].count -= maxTarget + 1;
	psxCounters[i].target &= maxTarget;
}

/*
Gate:
   TM_NO_GATE                   000
   TM_GATE_ON_Count             001
   TM_GATE_ON_ClearStart        011
   TM_GATE_ON_Clear_OFF_Start   101
   TM_GATE_ON_Start             111

   = means counting
   - means not counting

   V-blank  ----+    +----------------------------+    +------
                |    |                            |    |
                |    |                            |    |
                +----+                            +----+
 TM_NO_GATE:

                0================================>============

 TM_GATE_ON_Count:

                <---->===========================><---->======

 TM_GATE_ON_ClearStart:

                =====>0================================>0=====

 TM_GATE_ON_Clear_OFF_Start:

                0====>0-------------------------->0====>0-----

 TM_GATE_ON_Start:

                <---->===========================>============
*/

static void _psxCheckStartGate(int i)
{
	if (!(psxCounters[i].mode & IOPCNT_ENABLE_GATE))
		return; // Ignore Gate

	switch ((psxCounters[i].mode & 0x6) >> 1)
	{
		case 0x0: // GATE_ON_count - count while gate signal is low (RENDER)

			// get the current count at the time of stoppage:
			psxCounters[i].count = (i < 3) ?
									   psxRcntRcount16(i) :
									   psxRcntRcount32(i);

			// Not strictly necessary.
			psxCounters[i].sCycleT = psxRegs.cycle;
			break;

		case 0x1: // GATE_ON_ClearStart - Counts constantly, clears on Blank END
			// do nothing!
			break;

		case 0x2: // GATE_ON_Clear_OFF_Start - Counts only when Blanking, clears on both ends, starts counting on next Blank Start.
			psxCounters[i].mode &= ~IOPCNT_STOPPED;
			psxCounters[i].count = 0;
			psxCounters[i].target &= ~IOPCNT_FUTURE_TARGET;
			psxCounters[i].sCycleT = psxRegs.cycle;
			break;

		case 0x3: //GATE_ON_Start - Starts counting when the next Blank Ends, no clear.
			// do nothing!
			break;
	}
}

static void _psxCheckEndGate(int i)
{
	if (!(psxCounters[i].mode & IOPCNT_ENABLE_GATE))
		return; // Ignore Gate

	// NOTE: Starting and stopping of modes 0 and 2 are checked in psxRcntCanCount(), only need to update the start cycle and counts.
	switch ((psxCounters[i].mode & 0x6) >> 1)
	{
		case 0x0: // GATE_ON_count - count while gate signal is low (RENDER)
			psxCounters[i].sCycleT = psxRegs.cycle;
			break;

		case 0x1: // GATE_ON_ClearStart - Counts constantly, clears on Blank END
			psxCounters[i].count = 0;
			psxCounters[i].target &= ~IOPCNT_FUTURE_TARGET;
			break;

		case 0x2: // GATE_ON_Clear_OFF_Start - Counts only when Blanking, clears on both ends, starts counting on next Blank Start.
			// No point in updating the count, since we're gonna clear it.
			psxCounters[i].count = 0;
			psxCounters[i].target &= ~IOPCNT_FUTURE_TARGET;
			psxCounters[i].sCycleT = psxRegs.cycle;
			break; // do not set the counter

		case 0x3: // GATE_ON_Start - Starts counting when the next Blank Ends, no clear.
			if (psxCounters[i].mode & IOPCNT_STOPPED)
			{
				psxCounters[i].sCycleT = psxRegs.cycle;
				psxCounters[i].mode &= ~IOPCNT_STOPPED;
			}
			break;
	}
}

void psxHBlankStart()
{
	// AlternateSource/scanline counters for Gates 1 and 3.
	// We count them here so that they stay nicely synced with the EE's hsync.
	if ((psxCounters[1].rate == PSXHBLANK) && psxRcntCanCount(1))
	{
		psxCounters[1].count++;
		_rcntTestOverflow(1);
		_rcntTestTarget(1);
	}

	if ((psxCounters[3].rate == PSXHBLANK) && psxRcntCanCount(3))
	{
		psxCounters[3].count++;
		_rcntTestOverflow(3);
		_rcntTestTarget(3);
	}

	_psxCheckStartGate(0);

	hBlanking = true;

	_rcntSet(0);
}

void psxHBlankEnd()
{
	_psxCheckEndGate(0);

	hBlanking = false;

	_rcntSet(0);
}

void psxVBlankStart()
{
	cdvdVsync();
	iopIntcIrq(0);
	
	_psxCheckStartGate(1);
	_psxCheckStartGate(3);

	vBlanking = true;

	_rcntSet(1);
	_rcntSet(3);
}

void psxVBlankEnd()
{
	iopIntcIrq(11);
	
	_psxCheckEndGate(1);
	_psxCheckEndGate(3);

	vBlanking = false;

	_rcntSet(1);
	_rcntSet(3);
}

void psxRcntUpdate()
{
	int i;

	psxNextCounter = 0x7fffffff;
	psxNextsCounter = psxRegs.cycle;

	for (i = 0; i < 6; i++)
	{
		// don't count disabled or hblank counters...
		// We can't check the ALTSOURCE flag because the PSXCLOCK source *should*
		// be counted here.

		if (!psxRcntCanCount(i))
			continue;

		if ((psxCounters[i].mode & IOPCNT_INT_REPEAT) && !(psxCounters[i].mode & IOPCNT_INT_TOGGLE))
		{ //Repeat IRQ mode Pulsed, resets a few cycles after the interrupt, this should do.
			psxCounters[i].mode |= IOPCNT_INT_REQ;
		}

		if (psxCounters[i].rate == PSXHBLANK)
			continue;

		if (psxCounters[i].rate != 1)
		{
			const u32 change = (psxRegs.cycle - psxCounters[i].sCycleT) / psxCounters[i].rate;

			if (change > 0)
			{
				psxCounters[i].count += change;
				psxCounters[i].sCycleT += change * psxCounters[i].rate;
			}
		}
		else
		{
			psxCounters[i].count += psxRegs.cycle - psxCounters[i].sCycleT;
			psxCounters[i].sCycleT = psxRegs.cycle;
		}

		_rcntTestOverflow(i);
		_rcntTestTarget(i);
	}

	const u32 spu2_delta = (psxRegs.cycle - lClocks) % 768;
	psxCounters[6].sCycleT = psxRegs.cycle;
	psxCounters[6].CycleT = psxCounters[6].rate - spu2_delta;
	SPU2async();
	psxNextCounter = psxCounters[6].CycleT;

	DEV9async(1);
	const s32 diffusb = psxRegs.cycle - psxCounters[7].sCycleT;
	s32 cusb = psxCounters[7].CycleT;

	if (diffusb >= psxCounters[7].CycleT)
	{
		USBasync(diffusb);
		psxCounters[7].sCycleT += psxCounters[7].rate * (diffusb / psxCounters[7].rate);
		psxCounters[7].CycleT = psxCounters[7].rate;
	}
	else
		cusb -= diffusb;

	if (cusb < psxNextCounter)
		psxNextCounter = cusb;

	for (i = 0; i < 6; i++)
		_rcntSet(i);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void psxRcntWcount16(int index, u16 value)
{
	pxAssert(index < 3);
	//DevCon.Warning("16bit IOP Counter[%d] writeCount16 = %x", index, value);

	if (psxCounters[index].rate != PSXHBLANK)
	{
		const u32 change = (psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate;
		psxCounters[index].sCycleT += change * psxCounters[index].rate;
	}

	psxCounters[index].count = value & 0xffff;

	psxCounters[index].target &= 0xffff;

	if (psxCounters[index].count > psxCounters[index].target)
	{
		// Count already higher than Target
		//DevCon.Warning("32bit Count already higher than target");
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;
	}

	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void psxRcntWcount32(int index, u32 value)
{
	pxAssert(index >= 3 && index < 6);
	PSXCNT_LOG("32bit IOP Counter[%d] writeCount32 = %x", index, value);

	if (psxCounters[index].rate != PSXHBLANK)
	{
		// Re-adjust the sCycleT to match where the counter is currently
		// (remainder of the rate divided into the time passed will do the trick)

		const u32 change = (psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate;
		psxCounters[index].sCycleT += change * psxCounters[index].rate;
	}

	psxCounters[index].count = value;

	psxCounters[index].target &= 0xffffffff;

	if (psxCounters[index].count > psxCounters[index].target)
	{
		// Count already higher than Target
		//DevCon.Warning("32bit Count already higher than target");
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;
	}

	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
__fi void psxRcntWmode16(int index, u32 value)
{
	int irqmode = 0;
	PSXCNT_LOG("16bit IOP Counter[%d] writeMode = 0x%04X", index, value);

	pxAssume(index >= 0 && index < 3);
	psxCounter& counter = psxCounters[index];

	counter.mode = (value & IOPCNT_MODE_WRITE_MSK) | (counter.mode & IOPCNT_MODE_FLAG_MSK); // Write new value, preserve flags
	counter.mode |= IOPCNT_INT_REQ; // IRQ Enable

	if (value & (1 << 4))
	{
		irqmode += 1;
	}
	if (value & (1 << 5))
	{
		irqmode += 2;
	}
	if (value & (1 << 7))
	{
		PSXCNT_LOG("16 Counter %d Toggle IRQ on %s", index, (irqmode & 3) == 1 ? "Target" : ((irqmode & 3) == 2 ? "Overflow" : "Target and Overflow"));
	}
	else
	{
		PSXCNT_LOG("16 Counter %d Pulsed IRQ on %s", index, (irqmode & 3) == 1 ? "Target" : ((irqmode & 3) == 2 ? "Overflow" : "Target and Overflow"));
	}
	if (!(value & (1 << 6)))
	{
		PSXCNT_LOG("16 Counter %d One Shot", index);
	}
	else
	{
		PSXCNT_LOG("16 Counter %d Repeat", index);
	}
	if (index == 2)
	{
		switch (value & 0x200)
		{
			case 0x000:
				psxCounters[2].rate = 1;
				break;
			case 0x200:
				psxCounters[2].rate = 8;
				break;
				jNO_DEFAULT;
		}
	}
	else
	{
		// Counters 0 and 1 can select PIXEL or HSYNC as an alternate source:
		counter.rate = 1;

		if (value & IOPCNT_ALT_SOURCE)
			counter.rate = (index == 0) ? PSXPIXEL : PSXHBLANK;

		if (counter.rate == PSXPIXEL)
			Console.Warning("PSX Pixel clock set to time 0, sync may be incorrect");

		if (counter.mode & IOPCNT_ENABLE_GATE)
		{
			// If set to gate mode 3, the counting starts at the end of the next blank depending on which counter.
			if ((counter.mode & IOPCNT_MODE_GATE) == 0x4 && !psxRcntCanCount(index))
				counter.mode |= IOPCNT_STOPPED;
			else if ((counter.mode & IOPCNT_MODE_GATE) == 0x6)
				counter.mode |= IOPCNT_STOPPED;

			PSXCNT_LOG("IOP Counter[%d] Gate Check set, value = 0x%04X", index, value);
		}
	}

	// Current counter *always* resets on mode write.
	counter.count = 0;
	counter.sCycleT = psxRegs.cycle;

	counter.target &= 0xffff;

	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
__fi void psxRcntWmode32(int index, u32 value)
{
	PSXCNT_LOG("32bit IOP Counter[%d] writeMode = 0x%04x", index, value);
	int irqmode = 0;
	pxAssume(index >= 3 && index < 6);
	psxCounter& counter = psxCounters[index];

	counter.mode = (value & IOPCNT_MODE_WRITE_MSK) | (counter.mode & IOPCNT_MODE_FLAG_MSK); // Write new value, preserve flags
	counter.mode |= IOPCNT_INT_REQ; // IRQ Enable

	if (value & (1 << 4))
	{
		irqmode += 1;
	}
	if (value & (1 << 5))
	{
		irqmode += 2;
	}
	if (value & (1 << 7))
	{
		PSXCNT_LOG("32 Counter %d Toggle IRQ on %s", index, (irqmode & 3) == 1 ? "Target" : ((irqmode & 3) == 2 ? "Overflow" : "Target and Overflow"));
	}
	else
	{
		PSXCNT_LOG("32 Counter %d Pulsed IRQ on %s", index, (irqmode & 3) == 1 ? "Target" : ((irqmode & 3) == 2 ? "Overflow" : "Target and Overflow"));
	}
	if (!(value & (1 << 6)))
	{
		PSXCNT_LOG("32 Counter %d One Shot", index);
	}
	else
	{
		PSXCNT_LOG("32 Counter %d Repeat", index);
	}
	if (index == 3)
	{
		// Counter 3 has the HBlank as an alternate source.
		counter.rate = 1;
		if (value & IOPCNT_ALT_SOURCE)
			counter.rate = PSXHBLANK;

		if (counter.mode & IOPCNT_ENABLE_GATE)
		{
			PSXCNT_LOG("IOP Counter[3] Gate Check set, value = %x", value);
			// If set to gate mode 2 or 3, the counting starts at the start and end of the next blank respectively depending on which counter.
			if ((counter.mode & IOPCNT_MODE_GATE) > 0x2)
				counter.mode |= IOPCNT_STOPPED;
		}
	}
	else
	{
		switch (value & 0x6000)
		{
			case 0x0000:
				counter.rate = 1;
				break;
			case 0x2000:
				counter.rate = 8;
				break;
			case 0x4000:
				counter.rate = 16;
				break;
			case 0x6000:
				counter.rate = 256;
				break;
		}
	}

	// Current counter *always* resets on mode write.
	counter.count = 0;
	counter.sCycleT = psxRegs.cycle;
	counter.target &= 0xffffffff;
	_rcntSet(index);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void psxRcntWtarget16(int index, u32 value)
{
	pxAssert(index < 3);
	PSXCNT_LOG("IOP Counter[%d] writeTarget16 = %lx", index, value);
	psxCounters[index].target = value & 0xffff;

	if (!(psxCounters[index].mode & IOPCNT_INT_TOGGLE))
	{
		// Pulse mode reset
		psxCounters[index].mode |= IOPCNT_INT_REQ; // Interrupt flag reset to high
	}

	if (psxRcntCanCount(index) &&
		(psxCounters[index].rate != PSXHBLANK))
	{
		// Re-adjust the sCycleT to match where the counter is currently
		// (remainder of the rate divided into the time passed will do the trick)

		const u32 change = (psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate;
		psxCounters[index].count += change;
		psxCounters[index].sCycleT += change * psxCounters[index].rate;
	}

	// protect the target from an early arrival.
	// if the target is behind the current count, then set the target overflow
	// flag, so that the target won't be active until after the next overflow.

	if (psxCounters[index].target <= psxCounters[index].count)
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;

	_rcntSet(index);
}

void psxRcntWtarget32(int index, u32 value)
{
	pxAssert(index >= 3 && index < 6);
	PSXCNT_LOG("IOP Counter[%d] writeTarget32 = %lx mode %x", index, value, psxCounters[index].mode);

	psxCounters[index].target = value;

	if (!(psxCounters[index].mode & IOPCNT_INT_TOGGLE))
	{
		// Pulse mode reset
		psxCounters[index].mode |= IOPCNT_INT_REQ; // Interrupt flag reset to high
	}

	if (psxRcntCanCount(index) &&
		(psxCounters[index].rate != PSXHBLANK))
	{
		// Re-adjust the sCycleT to match where the counter is currently
		// (remainder of the rate divided into the time passed will do the trick)

		const u32 change = (psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate;
		psxCounters[index].count += change;
		psxCounters[index].sCycleT += change * psxCounters[index].rate;
	}
	// protect the target from an early arrival.
	// if the target is behind the current count, then set the target overflow
	// flag, so that the target won't be active until after the next overflow.

	if (psxCounters[index].target <= psxCounters[index].count)
		psxCounters[index].target |= IOPCNT_FUTURE_TARGET;

	_rcntSet(index);
}

u16 psxRcntRcount16(int index)
{
	u32 retval = (u32)psxCounters[index].count;

	pxAssert(index < 3);

	PSXCNT_LOG("IOP Counter[%d] readCount16 = %lx", index, (u16)retval);

	// Don't count HBLANK timers
	// Don't count stopped gates either.
	const bool canCount = psxRcntCanCount(index);
	if ((psxCounters[index].rate != PSXHBLANK) && canCount)
	{
		u32 delta = (u32)((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate);
		retval += delta;
		PSXCNT_LOG("              (delta = %lx)", delta);
	}
	else if (!canCount && (psxCounters[index].mode & IOPCNT_ENABLE_GATE) && (psxCounters[index].mode & IOPCNT_MODE_GATE) == 4)
		retval = 0;

	return (u16)retval;
}

u32 psxRcntRcount32(int index)
{
	u32 retval = (u32)psxCounters[index].count;

	pxAssert(index >= 3 && index < 6);

	PSXCNT_LOG("IOP Counter[%d] readCount32 = %lx", index, retval);

	const bool canCount = psxRcntCanCount(index);
	if ((psxCounters[index].rate != PSXHBLANK) && canCount)
	{
		u32 delta = (u32)((psxRegs.cycle - psxCounters[index].sCycleT) / psxCounters[index].rate);
		retval += delta;
		PSXCNT_LOG("               (delta = %lx)", delta);
	}
	else if (!canCount && (psxCounters[index].mode & IOPCNT_ENABLE_GATE) && (psxCounters[index].mode & IOPCNT_MODE_GATE) == 4)
		retval = 0;

	return retval;
}

bool SaveStateBase::psxRcntFreeze()
{
	if (!FreezeTag("iopCounters"))
		return false;

	Freeze(psxCounters);
	Freeze(psxNextCounter);
	Freeze(psxNextsCounter);
	Freeze(hBlanking);
	Freeze(vBlanking);

	if (!IsOkay())
		return false;

	if (IsLoading())
		psxRcntUpdate();

	return true;
}
