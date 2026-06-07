// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/MemoryInterface.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class MockMemoryInterface : public MemoryInterface
{
public:
	MOCK_METHOD(u8, Read8, (u32 address, bool* valid), (override));
	MOCK_METHOD(u16, Read16, (u32 address, bool* valid), (override));
	MOCK_METHOD(u32, Read32, (u32 address, bool* valid), (override));
	MOCK_METHOD(u64, Read64, (u32 address, bool* valid), (override));
	MOCK_METHOD(u128, Read128, (u32 address, bool* valid), (override));
	MOCK_METHOD(bool, ReadBytes, (u32 address, void* dest, u32 size), (override));

	MOCK_METHOD(bool, Write8, (u32 address, u8 value), (override));
	MOCK_METHOD(bool, Write16, (u32 address, u16 value), (override));
	MOCK_METHOD(bool, Write32, (u32 address, u32 value), (override));
	MOCK_METHOD(bool, Write64, (u32 address, u64 value), (override));
	MOCK_METHOD(bool, Write128, (u32 address, u128 value), (override));
	MOCK_METHOD(bool, WriteBytes, (u32 address, void* src, u32 size), (override));

	MOCK_METHOD(bool, CompareBytes, (u32 address, void* src, u32 size), (override));

	struct SetValidOutParameterAction
	{
		template <typename... Args>
		void operator()(const Args&... args) const
		{
			if (std::get<1>(std::tie(args...)))
				*std::get<1>(std::tie(args...)) = true;
		}
	};

#define DEFINE_EXPECT_CALL_FUNCTIONS(size) \
	void ExpectRead##size(u32 address, u##size return_value) \
	{ \
		auto actions = testing::DoAll(SetValidOutParameterAction{}, testing::Return(return_value)); \
		EXPECT_CALL(*this, Read##size(address, testing::_)).Times(1).WillOnce(std::move(actions)); \
	} \
	void ExpectWrite##size(u32 address, u##size expected_value) \
	{ \
		EXPECT_CALL(*this, Write##size(address, expected_value)).Times(1).WillOnce(testing::Return(true)); \
	} \
	void ExpectIdempotentWrite##size(u32 address, u##size old_value, u##size new_value) \
	{ \
		ExpectRead##size(address, old_value); \
		if (old_value != new_value) \
			ExpectWrite##size(address, new_value); \
	}
	DEFINE_EXPECT_CALL_FUNCTIONS(8)
	DEFINE_EXPECT_CALL_FUNCTIONS(16)
	DEFINE_EXPECT_CALL_FUNCTIONS(32)
	DEFINE_EXPECT_CALL_FUNCTIONS(64)
	DEFINE_EXPECT_CALL_FUNCTIONS(128)
#undef DEFINE_EXPECT_CALL_FUNCTIONS
};
