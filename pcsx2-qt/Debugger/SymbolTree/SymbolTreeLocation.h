// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QString>

#include "common/Pcsx2Types.h"
#include "DebugTools/DebugInterface.h"

class DebugInterface;

// A memory location, either a register or an address.
struct SymbolTreeLocation
{
	enum Type
	{
		REGISTER,
		MEMORY,
		NONE // Put NONE last so nodes of this type sort to the bottom.
	};
	
	Type type = NONE;
	u32 address = 0;

	SymbolTreeLocation();
	SymbolTreeLocation(Type type_arg, u32 address_arg);

	QString toString(DebugInterface& cpu) const;

	SymbolTreeLocation addOffset(u32 offset) const;

	u8 read8(DebugInterface& cpu) const;
	u16 read16(DebugInterface& cpu) const;
	u32 read32(DebugInterface& cpu) const;
	u64 read64(DebugInterface& cpu) const;
	u128 read128(DebugInterface& cpu) const;

	void write8(u8 value, DebugInterface& cpu) const;
	void write16(u16 value, DebugInterface& cpu) const;
	void write32(u32 value, DebugInterface& cpu) const;
	void write64(u64 value, DebugInterface& cpu) const;
	void write128(u128 value, DebugInterface& cpu) const;

	friend auto operator<=>(const SymbolTreeLocation& lhs, const SymbolTreeLocation& rhs) = default;
};
