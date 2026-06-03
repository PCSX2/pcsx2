// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryInterface.h"

#include <bit>

template <MemoryAccessType Value>
Value MemoryInterface::Read(u32 address, bool* valid)
{
	if constexpr (std::is_same_v<Value, u8> || std::is_same_v<Value, s8>)
		return static_cast<Value>(Read8(address, valid));
	else if constexpr (std::is_same_v<Value, u16> || std::is_same_v<Value, s16>)
		return static_cast<Value>(Read16(address, valid));
	else if constexpr (std::is_same_v<Value, u32> || std::is_same_v<Value, s32>)
		return static_cast<Value>(Read32(address, valid));
	else if constexpr (std::is_same_v<Value, u64> || std::is_same_v<Value, s64>)
		return static_cast<Value>(Read64(address, valid));
	else if constexpr (std::is_same_v<Value, u128>)
		return Read128(address, valid);
	else if constexpr (std::is_same_v<Value, s128>)
	{
		u128 value = Read128(address, valid);
		return s128{
			.lo = static_cast<s64>(value.lo),
			.hi = static_cast<s64>(value.hi),
		};
	}
	else if constexpr (std::is_same_v<Value, float>)
		return std::bit_cast<float>(Read32(address, valid));
	else if constexpr (std::is_same_v<Value, double>)
		return std::bit_cast<double>(Read64(address, valid));
	else
		return Value(0);
}

std::optional<std::string> MemoryInterface::ReadString(u32 address, u32 max_size, ReadStringFlags flags)
{
	std::string string;

	for (u32 i = 0; i < max_size; i++)
	{
		bool valid;
		char c = Read8(address + i, &valid);
		if (!valid)
			return std::nullopt;

		if (c == '\0')
			return string;
		else if (!(flags & ALLOW_NON_PRINTABLE_CHARACTERS) && (c < ' ' || c > '~'))
			return std::nullopt;

		string += c;
	}

	if (flags & ALLOW_LONG_STRINGS)
	{
		string += '~';
		return string;
	}

	return std::nullopt;
}

template <MemoryAccessType Value>
bool MemoryInterface::Write(u32 address, Value value)
{
	if constexpr (std::is_same_v<Value, u8> || std::is_same_v<Value, s8>)
		return Write8(address, static_cast<u8>(value));
	else if constexpr (std::is_same_v<Value, u16> || std::is_same_v<Value, s16>)
		return Write16(address, static_cast<u16>(value));
	else if constexpr (std::is_same_v<Value, u32> || std::is_same_v<Value, s32>)
		return Write32(address, static_cast<u32>(value));
	else if constexpr (std::is_same_v<Value, u64> || std::is_same_v<Value, s64>)
		return Write64(address, static_cast<u64>(value));
	else if constexpr (std::is_same_v<Value, u128>)
		return Write128(address, value);
	else if constexpr (std::is_same_v<Value, s128>)
	{
		u128 unsigned_value;
		unsigned_value.lo = static_cast<u64>(value.lo);
		unsigned_value.hi = static_cast<u64>(value.hi);
		return Write128(address, unsigned_value);
	}
	else if constexpr (std::is_same_v<Value, float>)
		return Write32(address, std::bit_cast<u32>(value));
	else if constexpr (std::is_same_v<Value, double>)
		return Write64(address, std::bit_cast<u64>(value));
	else
		return false;
}

bool MemoryInterface::WriteString(u32 address, std::string_view string)
{
	if (!WriteBytes(address, string.data(), static_cast<u32>(string.size())))
		return false;

	if (!Write8(address + static_cast<u32>(string.size()), '\0'))
		return false;

	return true;
}

bool MemoryInterface::IdempotentWrite8(u32 address, u8 value)
{
	bool valid;
	u8 existing_value = Read8(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write8(address, value);
}

bool MemoryInterface::IdempotentWrite16(u32 address, u16 value)
{
	bool valid;
	u16 existing_value = Read16(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write16(address, value);
}

bool MemoryInterface::IdempotentWrite32(u32 address, u32 value)
{
	bool valid;
	u32 existing_value = Read32(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write32(address, value);
}

bool MemoryInterface::IdempotentWrite64(u32 address, u64 value)
{
	bool valid;
	u64 existing_value = Read64(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write64(address, value);
}

bool MemoryInterface::IdempotentWrite128(u32 address, u128 value)
{
	bool valid;
	u128 existing_value = Read128(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write128(address, value);
}

bool MemoryInterface::IdempotentWriteBytes(u32 address, void* src, u32 size)
{
	if (CompareBytes(address, src, size))
		return true;

	return WriteBytes(address, src, size);
}

template <MemoryAccessType Value>
bool MemoryInterface::IdempotentWrite(u32 address, Value value)
{
	bool valid;
	Value existing_value = Read<Value>(address, &valid);
	if (!valid || existing_value == value)
		return valid;

	return Write<Value>(address, value);
}

#define EXPLICITLY_INSTANTIATE_TEMPLATES(type) \
	template type MemoryInterface::Read<type>(u32 addres, bool* valid); \
	template bool MemoryInterface::Write<type>(u32 address, type value); \
	template bool MemoryInterface::IdempotentWrite<type>(u32 address, type value);
EXPLICITLY_INSTANTIATE_TEMPLATES(u8)
EXPLICITLY_INSTANTIATE_TEMPLATES(s8)
EXPLICITLY_INSTANTIATE_TEMPLATES(u16)
EXPLICITLY_INSTANTIATE_TEMPLATES(s16)
EXPLICITLY_INSTANTIATE_TEMPLATES(u32)
EXPLICITLY_INSTANTIATE_TEMPLATES(s32)
EXPLICITLY_INSTANTIATE_TEMPLATES(u64)
EXPLICITLY_INSTANTIATE_TEMPLATES(s64)
EXPLICITLY_INSTANTIATE_TEMPLATES(u128)
EXPLICITLY_INSTANTIATE_TEMPLATES(s128)
EXPLICITLY_INSTANTIATE_TEMPLATES(float)
EXPLICITLY_INSTANTIATE_TEMPLATES(double)
#undef EXPLICITLY_INSTANTIATE_TEMPLATES
