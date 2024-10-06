// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#define MADR (Index == 0 ? HW_DMA4_MADR : HW_DMA7_MADR)
#define TADR (Index == 0 ? HW_DMA4_TADR : HW_DMA7_TADR)

#ifdef PCSX2_DEVBUILD

extern void DMALogOpen();
extern void ADMAOutLogWrite(void* lpData, u32 ulSize);
extern void DMA4LogWrite(void* lpData, u32 ulSize);
extern void DMA7LogWrite(void* lpData, u32 ulSize);
extern void DMALogClose();

#endif