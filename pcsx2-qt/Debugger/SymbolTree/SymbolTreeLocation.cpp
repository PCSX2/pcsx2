// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeLocation.h"

#include "DebugTools/DebugInterface.h"

SymbolTreeLocation::SymbolTreeLocation() = default;

SymbolTreeLocation::SymbolTreeLocation(Type type_arg, u32 address_arg)
	: type(type_arg)
	, address(address_arg)
{
}

QString SymbolTreeLocation::toString(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegisterName(0, address);
			else
				return QString("Reg %1").arg(address);
		case MEMORY:
			return QString::number(address, 16);
		default:
		{
		}
	}
	return QString();
}

SymbolTreeLocation SymbolTreeLocation::addOffset(u32 offset) const
{
	SymbolTreeLocation location;
	switch (type)
	{
		case REGISTER:
			if (offset == 0)
				location = *this;
			break;
		case MEMORY:
			location.type = type;
			location.address = address + offset;
			break;
		default:
		{
		}
	}
	return location;
}

u8 SymbolTreeLocation::read8(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegister(EECAT_GPR, address)._u8[0];
			break;
		case MEMORY:
			return (u8)cpu.read8(address);
		default:
		{
		}
	}
	return 0;
}

u16 SymbolTreeLocation::read16(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegister(EECAT_GPR, address)._u16[0];
			break;
		case MEMORY:
			return (u16)cpu.read16(address);
		default:
		{
		}
	}
	return 0;
}

u32 SymbolTreeLocation::read32(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegister(EECAT_GPR, address)._u32[0];
			break;
		case MEMORY:
			return cpu.read32(address);
		default:
		{
		}
	}
	return 0;
}

u64 SymbolTreeLocation::read64(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegister(EECAT_GPR, address)._u64[0];
			break;
		case MEMORY:
			return cpu.read64(address);
		default:
		{
		}
	}
	return 0;
}

u128 SymbolTreeLocation::read128(DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				return cpu.getRegister(EECAT_GPR, address);
			break;
		case MEMORY:
			return cpu.read128(address);
		default:
		{
		}
	}
	return u128::From32(0);
}

void SymbolTreeLocation::write8(u8 value, DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				cpu.setRegister(0, address, u128::From32(value));
			break;
		case MEMORY:
			cpu.write8(address, value);
			break;
		default:
		{
		}
	}
}

void SymbolTreeLocation::write16(u16 value, DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				cpu.setRegister(0, address, u128::From32(value));
			break;
		case MEMORY:
			cpu.write16(address, value);
			break;
		default:
		{
		}
	}
}

void SymbolTreeLocation::write32(u32 value, DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				cpu.setRegister(0, address, u128::From32(value));
			break;
		case MEMORY:
			cpu.write32(address, value);
			break;
		default:
		{
		}
	}
}

void SymbolTreeLocation::write64(u64 value, DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				cpu.setRegister(0, address, u128::From64(value));
			break;
		case MEMORY:
			cpu.write64(address, value);
			break;
		default:
		{
		}
	}
}

void SymbolTreeLocation::write128(u128 value, DebugInterface& cpu) const
{
	switch (type)
	{
		case REGISTER:
			if (address < 32)
				cpu.setRegister(0, address, value);
			break;
		case MEMORY:
			cpu.write128(address, value);
			break;
		default:
		{
		}
	}
}
