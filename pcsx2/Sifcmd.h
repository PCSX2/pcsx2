// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

struct t_sif_dma_transfer
{
	void *src;
	void *dest;
	s32 size;
	s32 attr;
};
