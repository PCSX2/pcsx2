// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "VU.h"
#include "PS2Float.h"

extern bool IsOverflowSet(VURegs* VU, s32 shift);
extern u32  VU_MACx_UPDATE(VURegs * VU, float x);
extern u32  VU_MACy_UPDATE(VURegs * VU, float y);
extern u32  VU_MACz_UPDATE(VURegs * VU, float z);
extern u32  VU_MACw_UPDATE(VURegs * VU, float w);
extern u32  VU_MACx_UPDATE(VURegs* VU, PS2Float x);
extern u32  VU_MACy_UPDATE(VURegs* VU, PS2Float y);
extern u32  VU_MACz_UPDATE(VURegs* VU, PS2Float z);
extern u32  VU_MACw_UPDATE(VURegs* VU, PS2Float w);
extern void VU_MACx_CLEAR(VURegs * VU);
extern void VU_MACy_CLEAR(VURegs * VU);
extern void VU_MACz_CLEAR(VURegs * VU);
extern void VU_MACw_CLEAR(VURegs * VU);
extern void VU_STAT_UPDATE(VURegs * VU);
