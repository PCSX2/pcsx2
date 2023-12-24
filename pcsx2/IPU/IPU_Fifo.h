// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include <string>

// Important!  All FIFO containers in this header should be 'struct' type, not class type.
// They are saved into the savestate as-is, and keeping them as struct ensures that the
// layout of their contents is reliable.

struct IPU_Fifo_Input
{
	alignas(16) u32 data[32];
	int readpos, writepos;

	int write(const u32* pMem, int size);
	int read(void *value);
	void clear();
	std::string desc() const;
};

struct IPU_Fifo_Output
{
	alignas(16) u32 data[32];
	int readpos, writepos;

	// returns number of qw read
	int write(const u32 * value, uint size);
	void read(void *value, uint size);
	void clear();
	std::string desc() const;
};

struct IPU_Fifo
{
	alignas(16) IPU_Fifo_Input in;
	alignas(16) IPU_Fifo_Output out;

	void init();
	void clear();
};

alignas(16) extern IPU_Fifo ipu_fifo;
