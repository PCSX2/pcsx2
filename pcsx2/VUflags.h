// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "VU.h"

extern u32 VU_MACx_UPDATE(VURegs* VU, u32 x);
extern u32 VU_MACy_UPDATE(VURegs* VU, u32 y);
extern u32 VU_MACz_UPDATE(VURegs* VU, u32 z);
extern u32 VU_MACw_UPDATE(VURegs* VU, u32 w);
extern void VU_MACx_CLEAR(VURegs* VU);
extern void VU_MACy_CLEAR(VURegs* VU);
extern void VU_MACz_CLEAR(VURegs* VU);
extern void VU_MACw_CLEAR(VURegs* VU);
extern void VU_STAT_UPDATE(VURegs* VU);
