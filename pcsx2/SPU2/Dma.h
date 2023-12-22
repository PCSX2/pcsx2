// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#define MADR (Index == 0 ? HW_DMA4_MADR : HW_DMA7_MADR)
#define TADR (Index == 0 ? HW_DMA4_TADR : HW_DMA7_TADR)

#ifdef PCSX2_DEVBUILD

extern void DMALogOpen();
extern void ADMAOutLogWrite(void* lpData, u32 ulSize);
extern void DMA4LogWrite(void* lpData, u32 ulSize);
extern void DMA7LogWrite(void* lpData, u32 ulSize);
extern void DMALogClose();

#endif