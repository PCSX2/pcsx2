// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryInterface.h"

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
	else
		return Value(0);
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
	else
		return false;
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
#undef EXPLICITLY_INSTANTIATE_TEMPLATES
