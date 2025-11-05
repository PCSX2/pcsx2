// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "gtest/gtest.h"

#include "DebugTools/InstructionTracer.h"

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

namespace
{

// Helper to create a simple test event
Tracer::TraceEvent CreateTestEvent(BreakPointCpu cpu, u64 pc, u32 opcode, const std::string& disasm)
{
	Tracer::TraceEvent ev;
	ev.cpu = cpu;
	ev.pc = pc;
	ev.opcode = opcode;
	ev.disasm = disasm;
	ev.cycles = 1000;
	ev.timestamp_ns = std::chrono::steady_clock::now().time_since_epoch().count();
	return ev;
}

class InstructionTracerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Disable tracing for both CPUs to start fresh
		Tracer::Enable(BREAKPOINT_EE, false);
		Tracer::Enable(BREAKPOINT_IOP, false);

		// Drain any leftover events
		std::vector<Tracer::TraceEvent> dummy;
		Tracer::Drain(BREAKPOINT_EE, 100000, std::back_inserter(dummy));
		Tracer::Drain(BREAKPOINT_IOP, 100000, std::back_inserter(dummy));
	}

	void TearDown() override
	{
		// Clean up - disable tracing
		Tracer::Enable(BREAKPOINT_EE, false);
		Tracer::Enable(BREAKPOINT_IOP, false);
	}
};

TEST_F(InstructionTracerTest, InitiallyDisabled)
{
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_EE));
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_IOP));
}

TEST_F(InstructionTracerTest, EnableDisable)
{
	Tracer::Enable(BREAKPOINT_EE, true);
	EXPECT_TRUE(Tracer::IsEnabled(BREAKPOINT_EE));
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_IOP));

	Tracer::Enable(BREAKPOINT_EE, false);
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_EE));

	Tracer::Enable(BREAKPOINT_IOP, true);
	EXPECT_TRUE(Tracer::IsEnabled(BREAKPOINT_IOP));
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_EE));

	Tracer::Enable(BREAKPOINT_IOP, false);
	EXPECT_FALSE(Tracer::IsEnabled(BREAKPOINT_IOP));
}

TEST_F(InstructionTracerTest, RecordWhenDisabledIsNoop)
{
	// Tracing is disabled, so recording should be a no-op
	auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000, 0x24020001, "addiu v0,zero,0x1");
	Tracer::Record(BREAKPOINT_EE, ev);

	std::vector<Tracer::TraceEvent> events;
	size_t drained = Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(events));

	EXPECT_EQ(drained, 0u);
	EXPECT_TRUE(events.empty());
}

TEST_F(InstructionTracerTest, RecordAndDrainSingleEvent)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000, 0x24020001, "addiu v0,zero,0x1");
	Tracer::Record(BREAKPOINT_EE, ev);

	std::vector<Tracer::TraceEvent> events;
	size_t drained = Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(events));

	EXPECT_EQ(drained, 1u);
	ASSERT_EQ(events.size(), 1u);

	EXPECT_EQ(events[0].cpu, BREAKPOINT_EE);
	EXPECT_EQ(events[0].pc, 0x80001000u);
	EXPECT_EQ(events[0].opcode, 0x24020001u);
	EXPECT_EQ(events[0].disasm, "addiu v0,zero,0x1");
	EXPECT_EQ(events[0].cycles, 1000u);
}

TEST_F(InstructionTracerTest, RecordMultipleEvents)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	for (int i = 0; i < 5; ++i)
	{
		auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000 + i * 4, i, "test" + std::to_string(i));
		Tracer::Record(BREAKPOINT_EE, ev);
	}

	std::vector<Tracer::TraceEvent> events;
	size_t drained = Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(events));

	EXPECT_EQ(drained, 5u);
	ASSERT_EQ(events.size(), 5u);

	for (int i = 0; i < 5; ++i)
	{
		EXPECT_EQ(events[i].pc, 0x80001000u + i * 4);
		EXPECT_EQ(events[i].opcode, static_cast<u32>(i));
		EXPECT_EQ(events[i].disasm, "test" + std::to_string(i));
	}
}

TEST_F(InstructionTracerTest, DrainPartialEvents)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	// Record 10 events
	for (int i = 0; i < 10; ++i)
	{
		auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000 + i * 4, i, "test" + std::to_string(i));
		Tracer::Record(BREAKPOINT_EE, ev);
	}

	// Drain only 5
	std::vector<Tracer::TraceEvent> events1;
	size_t drained1 = Tracer::Drain(BREAKPOINT_EE, 5, std::back_inserter(events1));
	EXPECT_EQ(drained1, 5u);
	ASSERT_EQ(events1.size(), 5u);

	// Drain remaining 5
	std::vector<Tracer::TraceEvent> events2;
	size_t drained2 = Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(events2));
	EXPECT_EQ(drained2, 5u);
	ASSERT_EQ(events2.size(), 5u);

	// Verify events are in order
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_EQ(events1[i].opcode, static_cast<u32>(i));
		EXPECT_EQ(events2[i].opcode, static_cast<u32>(i + 5));
	}
}

TEST_F(InstructionTracerTest, SeparateCpuBuffers)
{
	Tracer::Enable(BREAKPOINT_EE, true);
	Tracer::Enable(BREAKPOINT_IOP, true);

	// Record to EE
	auto ev_ee = CreateTestEvent(BREAKPOINT_EE, 0x80001000, 0x1234, "ee_test");
	Tracer::Record(BREAKPOINT_EE, ev_ee);

	// Record to IOP
	auto ev_iop = CreateTestEvent(BREAKPOINT_IOP, 0x00001000, 0x5678, "iop_test");
	Tracer::Record(BREAKPOINT_IOP, ev_iop);

	// Drain EE
	std::vector<Tracer::TraceEvent> ee_events;
	Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(ee_events));
	ASSERT_EQ(ee_events.size(), 1u);
	EXPECT_EQ(ee_events[0].cpu, BREAKPOINT_EE);
	EXPECT_EQ(ee_events[0].disasm, "ee_test");

	// Drain IOP
	std::vector<Tracer::TraceEvent> iop_events;
	Tracer::Drain(BREAKPOINT_IOP, 10, std::back_inserter(iop_events));
	ASSERT_EQ(iop_events.size(), 1u);
	EXPECT_EQ(iop_events[0].cpu, BREAKPOINT_IOP);
	EXPECT_EQ(iop_events[0].disasm, "iop_test");
}

TEST_F(InstructionTracerTest, MemoryAccessTracking)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000, 0x8C420000, "lw v0,0(v0)");
	ev.mem_r.push_back({0x80002000, 4});
	ev.mem_w.push_back({0x80003000, 2});

	Tracer::Record(BREAKPOINT_EE, ev);

	std::vector<Tracer::TraceEvent> events;
	Tracer::Drain(BREAKPOINT_EE, 10, std::back_inserter(events));

	ASSERT_EQ(events.size(), 1u);
	ASSERT_EQ(events[0].mem_r.size(), 1u);
	ASSERT_EQ(events[0].mem_w.size(), 1u);

	EXPECT_EQ(events[0].mem_r[0].first, 0x80002000u);
	EXPECT_EQ(events[0].mem_r[0].second, 4u);
	EXPECT_EQ(events[0].mem_w[0].first, 0x80003000u);
	EXPECT_EQ(events[0].mem_w[0].second, 2u);
}

TEST_F(InstructionTracerTest, DumpToFileBasic)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	// Record a few events
	for (int i = 0; i < 3; ++i)
	{
		auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000 + i * 4, i, "test" + std::to_string(i));
		Tracer::Record(BREAKPOINT_EE, ev);
	}

	// Dump to file
	const std::string test_file = "/tmp/test_trace.ndjson";
	Tracer::DumpBounds bounds;
	bounds.max_events = 0; // All events
	bounds.oldest_first = true;

	bool success = Tracer::DumpToFile(BREAKPOINT_EE, test_file, bounds);
	EXPECT_TRUE(success);

	// Verify file exists and has content
	std::ifstream file(test_file);
	ASSERT_TRUE(file.is_open());

	std::string line;
	int line_count = 0;
	while (std::getline(file, line))
	{
		line_count++;
		// Basic validation - should be valid JSON with expected fields
		EXPECT_TRUE(line.find("\"ts\":") != std::string::npos);
		EXPECT_TRUE(line.find("\"cpu\":\"EE\"") != std::string::npos);
		EXPECT_TRUE(line.find("\"pc\":") != std::string::npos);
		EXPECT_TRUE(line.find("\"op\":") != std::string::npos);
		EXPECT_TRUE(line.find("\"cycles\":") != std::string::npos);
	}

	EXPECT_EQ(line_count, 3);
	file.close();

	// Clean up
	std::remove(test_file.c_str());
}

TEST_F(InstructionTracerTest, DumpToFileWithBounds)
{
	Tracer::Enable(BREAKPOINT_EE, true);

	// Record 10 events
	for (int i = 0; i < 10; ++i)
	{
		auto ev = CreateTestEvent(BREAKPOINT_EE, 0x80001000 + i * 4, i, "test" + std::to_string(i));
		Tracer::Record(BREAKPOINT_EE, ev);
	}

	// Dump only 5 events
	const std::string test_file = "/tmp/test_trace_bounded.ndjson";
	Tracer::DumpBounds bounds;
	bounds.max_events = 5;
	bounds.oldest_first = true;

	bool success = Tracer::DumpToFile(BREAKPOINT_EE, test_file, bounds);
	EXPECT_TRUE(success);

	// Verify file has exactly 5 lines
	std::ifstream file(test_file);
	ASSERT_TRUE(file.is_open());

	std::string line;
	int line_count = 0;
	while (std::getline(file, line))
	{
		line_count++;
	}

	EXPECT_EQ(line_count, 5);
	file.close();

	// Clean up
	std::remove(test_file.c_str());
}

} // namespace
