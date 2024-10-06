// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

struct t_sif_dma_transfer
{
	void *src;
	void *dest;
	s32 size;
	s32 attr;
};
