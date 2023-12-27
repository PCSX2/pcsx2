// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "VU.h"

extern u32  VU_MACx_UPDATE(VURegs * VU, float x);
extern u32  VU_MACy_UPDATE(VURegs * VU, float y);
extern u32  VU_MACz_UPDATE(VURegs * VU, float z);
extern u32  VU_MACw_UPDATE(VURegs * VU, float w);
extern void VU_MACx_CLEAR(VURegs * VU);
extern void VU_MACy_CLEAR(VURegs * VU);
extern void VU_MACz_CLEAR(VURegs * VU);
extern void VU_MACw_CLEAR(VURegs * VU);
extern void VU_STAT_UPDATE(VURegs * VU);
