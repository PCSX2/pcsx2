// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Pcsx2Types.h"

#include <type_traits>

template <typename Value>
concept MemoryAccessType = std::is_same_v<Value, u8> || std::is_same_v<Value, s8> ||
                           std::is_same_v<Value, u16> || std::is_same_v<Value, s16> ||
                           std::is_same_v<Value, u32> || std::is_same_v<Value, s32> ||
                           std::is_same_v<Value, u64> || std::is_same_v<Value, s64> ||
                           std::is_same_v<Value, u128> || std::is_same_v<Value, s128>;

/// Interface for reading/writing guest memory.
class MemoryInterface
{
public:
	virtual u8 Read8(u32 address, bool* valid = nullptr) = 0;
	virtual u16 Read16(u32 address, bool* valid = nullptr) = 0;
	virtual u32 Read32(u32 address, bool* valid = nullptr) = 0;
	virtual u64 Read64(u32 address, bool* valid = nullptr) = 0;
	virtual u128 Read128(u32 address, bool* valid = nullptr) = 0;
	virtual bool ReadBytes(u32 address, void* dest, u32 size) = 0;

	template <MemoryAccessType Value>
	Value Read(u32 address, bool* valid = nullptr);

	virtual bool Write8(u32 address, u8 value) = 0;
	virtual bool Write16(u32 address, u16 value) = 0;
	virtual bool Write32(u32 address, u32 value) = 0;
	virtual bool Write64(u32 address, u64 value) = 0;
	virtual bool Write128(u32 address, u128 value) = 0;
	virtual bool WriteBytes(u32 address, void* src, u32 size) = 0;

	template <MemoryAccessType Value>
	bool Write(u32 address, Value value);

	bool IdempotentWrite8(u32 address, u8 value);
	bool IdempotentWrite16(u32 address, u16 value);
	bool IdempotentWrite32(u32 address, u32 value);
	bool IdempotentWrite64(u32 address, u64 value);
	bool IdempotentWrite128(u32 address, u128 value);
	bool IdempotentWriteBytes(u32 address, void* src, u32 size);

	template <MemoryAccessType Value>
	bool IdempotentWrite(u32 address, Value value);

	virtual bool CompareBytes(u32 address, void* src, u32 size) = 0;
};
