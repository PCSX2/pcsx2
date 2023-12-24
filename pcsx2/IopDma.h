// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

extern void psxDma2(u32 madr, u32 bcr, u32 chcr);
extern void psxDma3(u32 madr, u32 bcr, u32 chcr);
extern void psxDma6(u32 madr, u32 bcr, u32 chcr);
extern void psxDma4(u32 madr, u32 bcr, u32 chcr);
extern void psxDma7(u32 madr, u32 bcr, u32 chcr);
extern void psxDma8(u32 madr, u32 bcr, u32 chcr);
extern void psxDma9(u32 madr, u32 bcr, u32 chcr);
extern void psxDma10(u32 madr, u32 bcr, u32 chcr);
extern void psxDma11(u32 madr, u32 bcr, u32 chcr);
extern void psxDma12(u32 madr, u32 bcr, u32 chcr);

extern int  psxDma4Interrupt();
extern int  psxDma7Interrupt();
extern void psxDMA11Interrupt();
extern void psxDMA12Interrupt();
extern void dev9Interrupt();
extern void dev9Irq(int cycles);
extern void usbInterrupt();
extern void usbIrq(int cycles);
extern void fwIrq();
extern void spu2Irq();
extern void spu2DMA4Irq();
extern void spu2DMA7Irq();

extern void iopIntcIrq( uint irqType );
extern void iopTestIntc();
