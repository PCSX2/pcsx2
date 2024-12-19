// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "SaveState.h"
#include "vtlb.h"

#include "common/Assertions.h"

void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr, u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register, u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
  pxFailRel("Not implemented.");
}

bool SaveStateBase::vuJITFreeze()
{
  pxFailRel("Not implemented.");
	return false;
}
