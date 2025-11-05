// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "gtest/gtest.h"

#include "DebugTools/MemoryScanner.h"
#include "common/Pcsx2Types.h"

#include <thread>
#include <chrono>

namespace
{
	using namespace MemoryScanner;

	// Test fixture for MemoryScanner tests
	class MemoryScannerTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			// Get a fresh scanner for each test
			scanner = &GetGlobalScanner();
			scanner->ClearAll();
		}

		void TearDown() override
		{
			scanner->ClearAll();
		}

		Scanner* scanner;
	};

	// Test Query construction
	TEST_F(MemoryScannerTest, QueryConstruction)
	{
		Query query;
		EXPECT_EQ(query.cpu, BREAKPOINT_EE);
		EXPECT_EQ(query.begin, 0u);
		EXPECT_EQ(query.end, 0u);
		EXPECT_EQ(query.type, ValueType::U32);
		EXPECT_EQ(query.cmp, Comparison::Exact);
	}

	TEST_F(MemoryScannerTest, QueryConstructionWithParameters)
	{
		Query query(BREAKPOINT_IOP, 0x1000, 0x2000, ValueType::U16, Comparison::GreaterThan, u16(42));

		EXPECT_EQ(query.cpu, BREAKPOINT_IOP);
		EXPECT_EQ(query.begin, 0x1000u);
		EXPECT_EQ(query.end, 0x2000u);
		EXPECT_EQ(query.type, ValueType::U16);
		EXPECT_EQ(query.cmp, Comparison::GreaterThan);
		EXPECT_EQ(std::get<u16>(query.value), 42);
	}

	// Test DumpSpec construction
	TEST_F(MemoryScannerTest, DumpSpecConstruction)
	{
		DumpSpec spec;
		EXPECT_EQ(spec.type, ValueType::U32);
		EXPECT_FALSE(spec.includeContext);
		EXPECT_EQ(spec.contextSize, 0u);
		EXPECT_TRUE(spec.appendTimestamp);
	}

	// Test Result construction
	TEST_F(MemoryScannerTest, ResultConstruction)
	{
		Result result(0x12345678, u32(0xDEADBEEF), ValueType::U32);

		EXPECT_EQ(result.address, 0x12345678u);
		EXPECT_EQ(std::get<u32>(result.value), 0xDEADBEEFu);
		EXPECT_EQ(result.type, ValueType::U32);
	}

	// Test invalid scan submissions
	TEST_F(MemoryScannerTest, InvalidScanReturnsInvalidId)
	{
		// Invalid range (begin >= end)
		Query query(BREAKPOINT_EE, 0x2000, 0x1000, ValueType::U32, Comparison::Exact, u32(0));
		ScanId scanId = scanner->SubmitInitial(query);

		// Should return invalid scan ID for invalid query
		EXPECT_EQ(scanId, INVALID_SCAN_ID);
	}

	TEST_F(MemoryScannerTest, ScanStatusNotFound)
	{
		// Query status for non-existent scan
		ScanId invalidId = 999999;
		auto status = scanner->GetStatus(invalidId);

		EXPECT_EQ(status, Scanner::ScanStatus::NotFound);
	}

	TEST_F(MemoryScannerTest, ResultsForInvalidScanReturnsEmpty)
	{
		// Query results for non-existent scan
		ScanId invalidId = 999999;
		auto results = scanner->Results(invalidId);

		EXPECT_TRUE(results.empty());
	}

	// Test scan cancellation
	TEST_F(MemoryScannerTest, CancelNonExistentScan)
	{
		// Should not crash when cancelling non-existent scan
		ScanId invalidId = 999999;
		EXPECT_NO_THROW(scanner->Cancel(invalidId));
	}

	// Test active scan count
	TEST_F(MemoryScannerTest, ActiveScanCountStartsAtZero)
	{
		EXPECT_EQ(scanner->GetActiveScanCount(), 0u);
	}

	// Test ClearAll
	TEST_F(MemoryScannerTest, ClearAllRemovesScans)
	{
		scanner->ClearAll();
		EXPECT_EQ(scanner->GetActiveScanCount(), 0u);
	}

	// Test Value variant storage
	TEST_F(MemoryScannerTest, ValueStorageU8)
	{
		Value val = u8(0xFF);
		EXPECT_EQ(std::get<u8>(val), 0xFF);
	}

	TEST_F(MemoryScannerTest, ValueStorageU16)
	{
		Value val = u16(0xABCD);
		EXPECT_EQ(std::get<u16>(val), 0xABCD);
	}

	TEST_F(MemoryScannerTest, ValueStorageU32)
	{
		Value val = u32(0xDEADBEEF);
		EXPECT_EQ(std::get<u32>(val), 0xDEADBEEF);
	}

	TEST_F(MemoryScannerTest, ValueStorageU64)
	{
		Value val = u64(0x123456789ABCDEF0);
		EXPECT_EQ(std::get<u64>(val), 0x123456789ABCDEF0ULL);
	}

	TEST_F(MemoryScannerTest, ValueStorageF32)
	{
		Value val = 3.14159f;
		EXPECT_FLOAT_EQ(std::get<float>(val), 3.14159f);
	}

	TEST_F(MemoryScannerTest, ValueStorageF64)
	{
		Value val = 2.718281828;
		EXPECT_DOUBLE_EQ(std::get<double>(val), 2.718281828);
	}

	// Test different value types
	TEST_F(MemoryScannerTest, ValueTypeSizes)
	{
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::U8), 1u);
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::U16), 2u);
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::U32), 4u);
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::U64), 8u);
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::F32), 4u);
		EXPECT_EQ(Scanner::GetTypeSize(ValueType::F64), 8u);
	}

	// Test comparison types (enum values)
	TEST_F(MemoryScannerTest, ComparisonEnumValues)
	{
		// Just verify the enum values exist
		Comparison cmp = Comparison::Exact;
		EXPECT_EQ(cmp, Comparison::Exact);
		cmp = Comparison::NotEqual;
		EXPECT_EQ(cmp, Comparison::NotEqual);
		cmp = Comparison::GreaterThan;
		EXPECT_EQ(cmp, Comparison::GreaterThan);
		cmp = Comparison::LessThan;
		EXPECT_EQ(cmp, Comparison::LessThan);
		cmp = Comparison::Changed;
		EXPECT_EQ(cmp, Comparison::Changed);
		cmp = Comparison::Unchanged;
		EXPECT_EQ(cmp, Comparison::Unchanged);
		cmp = Comparison::Increased;
		EXPECT_EQ(cmp, Comparison::Increased);
		cmp = Comparison::Decreased;
		EXPECT_EQ(cmp, Comparison::Decreased);
		cmp = Comparison::Relative;
		EXPECT_EQ(cmp, Comparison::Relative);
		cmp = Comparison::Epsilon;
		EXPECT_EQ(cmp, Comparison::Epsilon);
	}

	// Test ValueToString
	TEST_F(MemoryScannerTest, ValueToStringU8)
	{
		Value val = u8(0xFF);
		std::string str = Scanner::ValueToString(val, ValueType::U8);
		EXPECT_EQ(str, "0xff");
	}

	TEST_F(MemoryScannerTest, ValueToStringU16)
	{
		Value val = u16(0xABCD);
		std::string str = Scanner::ValueToString(val, ValueType::U16);
		EXPECT_EQ(str, "0xabcd");
	}

	TEST_F(MemoryScannerTest, ValueToStringU32)
	{
		Value val = u32(0xDEADBEEF);
		std::string str = Scanner::ValueToString(val, ValueType::U32);
		EXPECT_EQ(str, "0xdeadbeef");
	}

	TEST_F(MemoryScannerTest, ValueToStringU64)
	{
		Value val = u64(0x123456789ABCDEF0);
		std::string str = Scanner::ValueToString(val, ValueType::U64);
		EXPECT_EQ(str, "0x123456789abcdef0");
	}

	TEST_F(MemoryScannerTest, ValueToStringF32)
	{
		Value val = 3.14159f;
		std::string str = Scanner::ValueToString(val, ValueType::F32);
		// Should be in scientific notation
		EXPECT_NE(str.find('e'), std::string::npos);
	}

	TEST_F(MemoryScannerTest, ValueToStringF64)
	{
		Value val = 2.718281828;
		std::string str = Scanner::ValueToString(val, ValueType::F64);
		// Should be in scientific notation
		EXPECT_NE(str.find('e'), std::string::npos);
	}

	// Test RemoveDumpWatch on non-existent watch
	TEST_F(MemoryScannerTest, RemoveNonExistentWatch)
	{
		bool result = scanner->RemoveDumpWatch(BREAKPOINT_EE, 0x12345678);
		EXPECT_FALSE(result);
	}

	// Test thread safety - multiple ClearAll calls
	TEST_F(MemoryScannerTest, ThreadSafetyClearAll)
	{
		std::vector<std::thread> threads;
		for (int i = 0; i < 10; ++i)
		{
			threads.emplace_back([this]() {
				scanner->ClearAll();
			});
		}

		for (auto& thread : threads)
		{
			thread.join();
		}

		EXPECT_EQ(scanner->GetActiveScanCount(), 0u);
	}

	// Test that multiple Results calls don't crash
	TEST_F(MemoryScannerTest, MultipleResultsCalls)
	{
		ScanId invalidId = 999999;

		for (int i = 0; i < 10; ++i)
		{
			auto results = scanner->Results(invalidId);
			EXPECT_TRUE(results.empty());
		}
	}

	// Test global scanner singleton
	TEST_F(MemoryScannerTest, GlobalScannerSingleton)
	{
		Scanner& scanner1 = GetGlobalScanner();
		Scanner& scanner2 = GetGlobalScanner();

		// Should be the same instance
		EXPECT_EQ(&scanner1, &scanner2);
	}

} // namespace
