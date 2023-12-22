// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DEV9/DEV9.h"
#include "USB/USB.h"
#include "IopHw.h"
#include "IopDma.h"
#include "Common.h"
#include "R3000A.h"

using namespace R3000A;

void dev9Interrupt()
{
	if (DEV9irqHandler() != 1) return;

	iopIntcIrq(13);
}

void dev9Irq(int cycles)
{
	PSX_INT(IopEvt_DEV9, cycles);
}

void usbInterrupt()
{
	iopIntcIrq(22);
}

void usbIrq(int cycles)
{
	PSX_INT(IopEvt_USB, cycles);
}

void fwIrq()
{
	iopIntcIrq(24);
}

void spu2Irq()
{
	#ifdef SPU2IRQTEST
		Console.Warning("spu2Irq");
	#endif
	iopIntcIrq(9);
}

void iopIntcIrq(uint irqType)
{
	psxHu32(0x1070) |= 1 << irqType;
	iopTestIntc();
}
