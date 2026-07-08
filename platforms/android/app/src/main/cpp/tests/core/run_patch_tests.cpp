// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// Patch::ApplyPatches logcat test runner.
//
// Mirrors the test cases in tests/ctest/core/patch_tests.cpp without gmock.
// Uses a sequential-expectation MemoryInterface that records and validates
// each Read/Write call in order.
//
// Results are printed to logcat with tag "PatchTests".

#include "run_patch_tests.h"
#include "../test_bridge.h"

#include "pcsx2/Patch.h"
#include "common/MemoryInterface.h"

#include <android/log.h>
#include <cstdint>
#include <vector>

#define TAG  "PatchTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace Patch;

// ---------------------------------------------------------------------------
// PatchCommand has a deleted copy constructor (owns data_ptr).
// Use a variadic helper to move-build a vector instead of initializer_list.
// ---------------------------------------------------------------------------

template<typename... Ts>
static std::vector<PatchCommand> makeCmds(Ts&&... args)
{
	std::vector<PatchCommand> v;
	(v.push_back(std::forward<Ts>(args)), ...);
	return v;
}

static PatchCommand Cmd(patch_place_type place, patch_cpu_type cpu, u32 addr, patch_data_type type, u64 data)
{
	PatchCommand c;
	c.placetopatch = place;
	c.cpu          = cpu;
	c.addr         = addr;
	c.type         = type;
	c.data         = data;
	return c;
}

// ---------------------------------------------------------------------------
// Sequential memory operation expectation
// ---------------------------------------------------------------------------

struct Op
{
	enum class Kind : uint8_t { R8, R16, R32, R64, W8, W16, W32, W64 };
	Kind kind;
	u32  addr;
	u64  val; // return value for reads; expected value for writes
};

static const char* kindStr(Op::Kind k)
{
	switch (k)
	{
		case Op::Kind::R8:  return "Read8";
		case Op::Kind::R16: return "Read16";
		case Op::Kind::R32: return "Read32";
		case Op::Kind::R64: return "Read64";
		case Op::Kind::W8:  return "Write8";
		case Op::Kind::W16: return "Write16";
		case Op::Kind::W32: return "Write32";
		case Op::Kind::W64: return "Write64";
	}
	return "?";
}

static Op r8(u32 a, u64 v)  { return {Op::Kind::R8,  a, v}; }
static Op r16(u32 a, u64 v) { return {Op::Kind::R16, a, v}; }
static Op r32(u32 a, u64 v) { return {Op::Kind::R32, a, v}; }
static Op r64(u32 a, u64 v) { return {Op::Kind::R64, a, v}; }
static Op w8(u32 a, u64 v)  { return {Op::Kind::W8,  a, v}; }
static Op w16(u32 a, u64 v) { return {Op::Kind::W16, a, v}; }
static Op w32(u32 a, u64 v) { return {Op::Kind::W32, a, v}; }
static Op w64(u32 a, u64 v) { return {Op::Kind::W64, a, v}; }

// ---------------------------------------------------------------------------
// SeqMem — validates calls against an ordered list
// ---------------------------------------------------------------------------

class SeqMem : public MemoryInterface
{
	const char*      m_name;
	std::vector<Op>  m_ops;
	size_t           m_idx = 0;
	bool             m_ok  = true;

	const Op* next(Op::Kind kind, u32 addr)
	{
		if (m_idx >= m_ops.size())
		{
			LOGE("  [%s] Unexpected %s(0x%08x) — no more expected ops", m_name, kindStr(kind), addr);
			m_ok = false;
			return nullptr;
		}
		const Op* e = &m_ops[m_idx];
		if (e->kind != kind || e->addr != addr)
		{
			LOGE("  [%s] Got %s(0x%08x) but expected %s(0x%08x)",
				m_name, kindStr(kind), addr, kindStr(e->kind), e->addr);
			m_ok = false;
			return nullptr;
		}
		++m_idx;
		return e;
	}

public:
	SeqMem(const char* name, std::vector<Op> ops)
		: m_name(name), m_ops(std::move(ops)) {}

	bool isOk() const
	{
		if (!m_ok)
			return false;
		if (m_idx != m_ops.size())
		{
			LOGE("  [%s] %zu ops expected but only %zu were called", m_name, m_ops.size(), m_idx);
			return false;
		}
		return true;
	}

	u8 Read8(u32 addr, bool* valid) override
	{
		if (valid) *valid = true;
		const Op* e = next(Op::Kind::R8, addr);
		return e ? static_cast<u8>(e->val) : 0;
	}
	u16 Read16(u32 addr, bool* valid) override
	{
		if (valid) *valid = true;
		const Op* e = next(Op::Kind::R16, addr);
		return e ? static_cast<u16>(e->val) : 0;
	}
	u32 Read32(u32 addr, bool* valid) override
	{
		if (valid) *valid = true;
		const Op* e = next(Op::Kind::R32, addr);
		return e ? static_cast<u32>(e->val) : 0;
	}
	u64 Read64(u32 addr, bool* valid) override
	{
		if (valid) *valid = true;
		const Op* e = next(Op::Kind::R64, addr);
		return e ? e->val : 0;
	}
	u128 Read128(u32 addr, bool* valid) override { if (valid) *valid = true; return {}; }
	bool ReadBytes(u32, void*, u32) override { return false; }

	bool Write8(u32 addr, u8 val) override
	{
		const Op* e = next(Op::Kind::W8, addr);
		if (e && static_cast<u8>(e->val) != val)
		{
			LOGE("  [%s] Write8(0x%08x): got 0x%02x expected 0x%02x",
				m_name, addr, val, static_cast<u8>(e->val));
			m_ok = false;
		}
		return true;
	}
	bool Write16(u32 addr, u16 val) override
	{
		const Op* e = next(Op::Kind::W16, addr);
		if (e && static_cast<u16>(e->val) != val)
		{
			LOGE("  [%s] Write16(0x%08x): got 0x%04x expected 0x%04x",
				m_name, addr, val, static_cast<u16>(e->val));
			m_ok = false;
		}
		return true;
	}
	bool Write32(u32 addr, u32 val) override
	{
		const Op* e = next(Op::Kind::W32, addr);
		if (e && static_cast<u32>(e->val) != val)
		{
			LOGE("  [%s] Write32(0x%08x): got 0x%08x expected 0x%08x",
				m_name, addr, val, static_cast<u32>(e->val));
			m_ok = false;
		}
		return true;
	}
	bool Write64(u32 addr, u64 val) override
	{
		const Op* e = next(Op::Kind::W64, addr);
		if (e && e->val != val)
		{
			LOGE("  [%s] Write64(0x%08x): got 0x%016llx expected 0x%016llx",
				m_name, addr, static_cast<unsigned long long>(val),
				static_cast<unsigned long long>(e->val));
			m_ok = false;
		}
		return true;
	}
	bool Write128(u32, u128) override { return true; }
	bool WriteBytes(u32, void*, u32) override { return false; }
	bool CompareBytes(u32, void*, u32) override { return false; }
};

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

static int s_pass;
static int s_fail;

static void runTest(
	const char* name,
	std::vector<PatchCommand> cmds,
	std::vector<Op> ee_ops,
	std::vector<Op> iop_ops = {})
{
	SeqMem ee("EE", std::move(ee_ops));
	SeqMem iop("IOP", std::move(iop_ops));

	std::vector<const PatchCommand*> ptrs;
	for (const auto& c : cmds)
		ptrs.push_back(&c);

	// Mirror the PATCH_TEST macro: call with all four place values
	ApplyPatches(ptrs, PPT_ONCE_ON_LOAD,            ee, iop);
	ApplyPatches(ptrs, PPT_CONTINUOUSLY,             ee, iop);
	ApplyPatches(ptrs, PPT_COMBINED_0_1,             ee, iop);
	ApplyPatches(ptrs, PPT_ON_LOAD_OR_WHEN_ENABLED,  ee, iop);

	if (ee.isOk() && iop.isOk())
	{
		++s_pass;
		LOGI("PASS  %s", name);
	}
	else
	{
		++s_fail;
		LOGE("FAIL  %s", name);
	}
}

// ---------------------------------------------------------------------------
// Writes
// ---------------------------------------------------------------------------

static void testByte()
{
	runTest("Byte",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, BYTE_T, 0x12)),
		{ r8(0x00100000, 0), w8(0x00100000, 0x12) });
}

static void testShort()
{
	runTest("Short",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, SHORT_T, 0x1234)),
		{ r16(0x00100000, 0), w16(0x00100000, 0x1234) });
}

static void testWord()
{
	runTest("Word",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, WORD_T, 0x12345678)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678) });
}

static void testDouble()
{
	runTest("Double",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, DOUBLE_T, 0x123456789acdef12ULL)),
		{ r64(0x00100000, 0), w64(0x00100000, 0x123456789acdef12ULL) });
}

static void testBigEndianShort()
{
	runTest("BigEndianShort",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, SHORT_BE_T, 0x1234)),
		{ r16(0x00100000, 0), w16(0x00100000, 0x3412) });
}

static void testBigEndianWord()
{
	runTest("BigEndianWord",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, WORD_BE_T, 0x12345678)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x78563412) });
}

static void testBigEndianDouble()
{
	runTest("BigEndianDouble",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, DOUBLE_BE_T, 0xabcdef0123456789ULL)),
		{ r64(0x00100000, 0), w64(0x00100000, 0x8967452301efcdabULL) });
}

static void testIOPByte()
{
	runTest("IOPByte",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_IOP, 0x00100000, BYTE_T, 0x12)),
		{},
		{ r8(0x00100000, 0), w8(0x00100000, 0x12) });
}

static void testIOPShort()
{
	runTest("IOPShort",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_IOP, 0x00100000, SHORT_T, 0x1234)),
		{},
		{ r16(0x00100000, 0), w16(0x00100000, 0x1234) });
}

static void testIOPWord()
{
	runTest("IOPWord",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_IOP, 0x00100000, WORD_T, 0x12345678)),
		{},
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678) });
}

// ---------------------------------------------------------------------------
// Extended writes
// ---------------------------------------------------------------------------

static void testExtended8BitWrite()
{
	runTest("Extended8BitWrite",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00100000, EXTENDED_T, 0x00000012)),
		{ r8(0x00100000, 0), w8(0x00100000, 0x12) });
}

static void testExtended16BitWrite()
{
	runTest("Extended16BitWrite",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x10100000, EXTENDED_T, 0x00001234)),
		{ r16(0x00100000, 0), w16(0x00100000, 0x1234) });
}

static void testExtended32BitWrite()
{
	runTest("Extended32BitWrite",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x20100000, EXTENDED_T, 0x12345678)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678) });
}

// ---------------------------------------------------------------------------
// Extended increments / decrements
// ---------------------------------------------------------------------------

static void testExtended8BitIncrement()
{
	runTest("Extended8BitIncrement",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30000012, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30000012, EXTENDED_T, 0x00100000)),
		{ r8(0x00100000, 0x00), w8(0x00100000, 0x12),
		  r8(0x00100000, 0x12), w8(0x00100000, 0x24) });
}

static void testExtended8BitIncrementWrapping()
{
	runTest("Extended8BitIncrementWrapping",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30000012, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30000012, EXTENDED_T, 0x00100000)),
		{ r8(0x00100000, 0xee), w8(0x00100000, 0x00),
		  r8(0x00100000, 0x00), w8(0x00100000, 0x12) });
}

static void testExtended8BitDecrement()
{
	runTest("Extended8BitDecrement",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30100012, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30100012, EXTENDED_T, 0x00100000)),
		{ r8(0x00100000, 0x24), w8(0x00100000, 0x12),
		  r8(0x00100000, 0x12), w8(0x00100000, 0x00) });
}

static void testExtended16BitIncrement()
{
	runTest("Extended16BitIncrement",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30201234, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30201234, EXTENDED_T, 0x00100000)),
		{ r16(0x00100000, 0x0000), w16(0x00100000, 0x1234),
		  r16(0x00100000, 0x1234), w16(0x00100000, 0x2468) });
}

static void testExtended16BitDecrement()
{
	runTest("Extended16BitDecrement",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30301234, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30301234, EXTENDED_T, 0x00100000)),
		{ r16(0x00100000, 0x2468), w16(0x00100000, 0x1234),
		  r16(0x00100000, 0x1234), w16(0x00100000, 0x0000) });
}

static void testExtended32BitIncrement()
{
	runTest("Extended32BitIncrement",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30400000, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x12345678, EXTENDED_T, 0x00000000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x30400000, EXTENDED_T, 0x00100000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x12345678, EXTENDED_T, 0x00000000)),
		{ r32(0x00100000, 0x00000000), w32(0x00100000, 0x12345678),
		  r32(0x00100000, 0x12345678), w32(0x00100000, 0x2468acf0) });
}

// ---------------------------------------------------------------------------
// Serial write (Extended)
// ---------------------------------------------------------------------------

static void testExtendedSerialWriteZero()
{
	runTest("ExtendedSerialWriteZero",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x40100000, EXTENDED_T, 0x00000000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00000000, EXTENDED_T, 0x00000000)),
		{});
}

static void testExtendedSerialWriteOnce()
{
	runTest("ExtendedSerialWriteOnce",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x40100000, EXTENDED_T, 0x00010000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x12345678, EXTENDED_T, 0x11111111)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678) });
}

static void testExtendedSerialWriteContiguous()
{
	runTest("ExtendedSerialWriteContiguous",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x40100000, EXTENDED_T, 0x00020001),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x12345678, EXTENDED_T, 0x11111111)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678),
		  r32(0x00100004, 0), w32(0x00100004, 0x23456789) });
}

static void testExtendedSerialWriteStrided()
{
	runTest("ExtendedSerialWriteStrided",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x40100000, EXTENDED_T, 0x00020002),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x12345678, EXTENDED_T, 0x11111111)),
		{ r32(0x00100000, 0), w32(0x00100000, 0x12345678),
		  r32(0x00100008, 0), w32(0x00100008, 0x23456789) });
}

// ---------------------------------------------------------------------------
// Copy bytes (Extended)
// ---------------------------------------------------------------------------

static void testExtendedCopyBytes0()
{
	runTest("ExtendedCopyBytes0",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x50100000, EXTENDED_T, 0x00000000),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000000)),
		{});
}

static void testExtendedCopyBytes2()
{
	runTest("ExtendedCopyBytes2",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x50100000, EXTENDED_T, 0x00000002),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000000)),
		{ r8(0x00100000, 0x12), r8(0x00200000, 0), w8(0x00200000, 0x12),
		  r8(0x00100001, 0x12), r8(0x00200001, 0), w8(0x00200001, 0x12) });
}

// ---------------------------------------------------------------------------
// Pointer write (Extended)
// ---------------------------------------------------------------------------

static void testExtendedPointerWrite8()
{
	runTest("ExtendedPointerWrite8",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x60100000, EXTENDED_T, 0x00000012),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00000001, EXTENDED_T, 0x00000004)),
		{ r32(0x00100000, 0x00200000), r8(0x00200004, 0), w8(0x00200004, 0x12) });
}

static void testExtendedPointerWrite32()
{
	runTest("ExtendedPointerWrite32",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x60100000, EXTENDED_T, 0x12345678),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00020001, EXTENDED_T, 0x00000004)),
		{ r32(0x00100000, 0x00200000), r32(0x00200004, 0), w32(0x00200004, 0x12345678) });
}

static void testExtendedPointerWriteSkipsNullSingle()
{
	runTest("ExtendedPointerWriteSkipsNullSingle",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x60100000, EXTENDED_T, 0x0fffffff),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00020001, EXTENDED_T, 0x0fffffff),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x20200000, EXTENDED_T, 0x12345678),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x20300000, EXTENDED_T, 0x12345678)),
		{ r32(0x00100000, 0),                              // null pointer → skip write
		  r32(0x00200000, 0), w32(0x00200000, 0x12345678), // IdempotentWrite32
		  r32(0x00300000, 0), w32(0x00300000, 0x12345678) });
}

// ---------------------------------------------------------------------------
// Boolean operations (Extended)
// ---------------------------------------------------------------------------

static void testExtendedBooleanOr8()
{
	runTest("ExtendedBooleanOr8",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x70100000, EXTENDED_T, 0x00000012)),
		{ r8(0x00100000, 0x78), w8(0x00100000, 0x7a) });
}

static void testExtendedBooleanAnd8()
{
	runTest("ExtendedBooleanAnd8",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x70100000, EXTENDED_T, 0x00200012)),
		{ r8(0x00100000, 0x34), w8(0x00100000, 0x10) });
}

static void testExtendedBooleanXor8()
{
	runTest("ExtendedBooleanXor8",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x70100000, EXTENDED_T, 0x00400012)),
		{ r8(0x00100000, 0x89), w8(0x00100000, 0x9b) });
}

static void testExtendedBooleanOr16()
{
	runTest("ExtendedBooleanOr16",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x70100000, EXTENDED_T, 0x00101234)),
		{ r16(0x00100000, 0x89ab), w16(0x00100000, 0x9bbf) });
}

static void testExtendedBooleanAnd16()
{
	runTest("ExtendedBooleanAnd16",
		makeCmds(Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x70100000, EXTENDED_T, 0x00301234)),
		{ r16(0x00100000, 0x5678), w16(0x00100000, 0x1230) });
}

// ---------------------------------------------------------------------------
// Conditionals (Extended)
// ---------------------------------------------------------------------------

static void testExtendedConditional8BitEqualTrue()
{
	// Condition matches (0x12 == 0x12) → next 1 command executes
	runTest("ExtendedConditional8BitEqualTrue",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0xd0100000, EXTENDED_T, 0x01010012),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000012),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00300000, EXTENDED_T, 0x00000012)),
		{ r8(0x00100000, 0x12),
		  r8(0x00200000, 0), w8(0x00200000, 0x12),
		  r8(0x00300000, 0), w8(0x00300000, 0x12) });
}

static void testExtendedConditional8BitEqualFalse()
{
	// Condition does NOT match (0x21 != 0x12) → 1 command skipped
	runTest("ExtendedConditional8BitEqualFalse",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0xd0100000, EXTENDED_T, 0x01010012),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000012),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00300000, EXTENDED_T, 0x00000012)),
		{ r8(0x00100000, 0x21),
		  // 0x00200000 command is skipped
		  r8(0x00300000, 0), w8(0x00300000, 0x12) });
}

static void testExtendedConditionalNotEqualTrue()
{
	// NE condition: 0x4321 != 0x1234 → executes next command
	runTest("ExtendedConditionalNotEqualTrue",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0xd0100000, EXTENDED_T, 0x01101234),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000012)),
		{ r16(0x00100000, 0x4321),
		  r8(0x00200000, 0), w8(0x00200000, 0x12) });
}

static void testExtendedConditionalNotEqualFalse()
{
	// NE condition: 0x1234 == 0x1234 → command skipped
	runTest("ExtendedConditionalNotEqualFalse",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0xd0100000, EXTENDED_T, 0x01101234),
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0x00200000, EXTENDED_T, 0x00000012)),
		{ r16(0x00100000, 0x1234) });
}

static void testExtendedConditionalResetSkipCount()
{
	// 0xff prefix resets skip count. The ONCE_ON_LOAD conditional fails (0xab != 0x12),
	// but the CONTINUOUSLY command runs in its own fresh-state ApplyPatches call.
	runTest("ExtendedConditionalResetSkipCount",
		makeCmds(
			Cmd(PPT_ONCE_ON_LOAD, CPU_EE, 0xd0100000, EXTENDED_T, 0xff010012),
			Cmd(PPT_CONTINUOUSLY,  CPU_EE, 0x00200000, EXTENDED_T, 0x00000012)),
		{ r8(0x00100000, 0xab),
		  r8(0x00200000, 0), w8(0x00200000, 0x12) });
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void RunPatchTests()
{
	s_pass = 0;
	s_fail = 0;
	LOGI("=== Patch Tests ===");

	testByte();
	testShort();
	testWord();
	testDouble();
	testBigEndianShort();
	testBigEndianWord();
	testBigEndianDouble();
	testIOPByte();
	testIOPShort();
	testIOPWord();

	testExtended8BitWrite();
	testExtended16BitWrite();
	testExtended32BitWrite();

	testExtended8BitIncrement();
	testExtended8BitIncrementWrapping();
	testExtended8BitDecrement();
	testExtended16BitIncrement();
	testExtended16BitDecrement();
	testExtended32BitIncrement();

	testExtendedSerialWriteZero();
	testExtendedSerialWriteOnce();
	testExtendedSerialWriteContiguous();
	testExtendedSerialWriteStrided();

	testExtendedCopyBytes0();
	testExtendedCopyBytes2();

	testExtendedPointerWrite8();
	testExtendedPointerWrite32();
	testExtendedPointerWriteSkipsNullSingle();

	testExtendedBooleanOr8();
	testExtendedBooleanAnd8();
	testExtendedBooleanXor8();
	testExtendedBooleanOr16();
	testExtendedBooleanAnd16();

	testExtendedConditional8BitEqualTrue();
	testExtendedConditional8BitEqualFalse();
	testExtendedConditionalNotEqualTrue();
	testExtendedConditionalNotEqualFalse();
	testExtendedConditionalResetSkipCount();

	LOGI("=== Results: %d passed, %d failed ===", s_pass, s_fail);
	ReportTestResults("PatchTests", s_pass, s_pass + s_fail);
}

#undef LOGI
#undef LOGE
#undef TAG
