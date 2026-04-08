// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string_view>

namespace SimpSkateTrace
{
bool IsEnabled();
bool IsEETracepoint(u32 pc);
void OnEETracepoint(u32 pc);

void OnIsoOpen(const std::string_view& iso_path);
void OnIsoMapBuilt(size_t file_count, bool has_assets_blt, u32 assets_blt_lsn, u32 assets_blt_size);
void OnIsoReadRun(
	u32 start_lsn,
	u32 sector_count,
	int mode,
	u32 ee_pc,
	u32 iop_pc,
	const std::string_view& owner_path,
	u32 owner_offset,
	u32 owner_size);
} // namespace SimpSkateTrace

