// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Patch.h"

#include "MockMemoryInterface.h"

#include <gtest/gtest.h>

// Create a test that makes sure applying a given list of patch commands results
// in a certain sequence of memory reads/writes.
#define PATCH_TEST(name, ...) \
	static void patch_test_setup_expected_calls_##name(MockMemoryInterface& ee, MockMemoryInterface& iop); \
	TEST(Patch, name) \
	{ \
		testing::StrictMock<MockMemoryInterface> ee; \
		testing::StrictMock<MockMemoryInterface> iop; \
		{ \
			testing::InSequence seq; \
			patch_test_setup_expected_calls_##name(ee, iop); \
		} \
		Patch::PatchCommand commands[]{__VA_ARGS__}; \
		std::vector<const Patch::PatchCommand*> pointers; \
		pointers.reserve(std::size(commands)); \
		for (Patch::PatchCommand& command : commands) \
			pointers.push_back(&command); \
		Patch::ApplyPatches(pointers, Patch::PPT_ONCE_ON_LOAD, ee, iop); \
		Patch::ApplyPatches(pointers, Patch::PPT_CONTINUOUSLY, ee, iop); \
		Patch::ApplyPatches(pointers, Patch::PPT_COMBINED_0_1, ee, iop); \
		Patch::ApplyPatches(pointers, Patch::PPT_ON_LOAD_OR_WHEN_ENABLED, ee, iop); \
	} \
	static void patch_test_setup_expected_calls_##name(MockMemoryInterface& ee, MockMemoryInterface& iop)

static Patch::PatchCommand BuildPatchCommand(
	Patch::patch_place_type place,
	Patch::patch_cpu_type cpu,
	u32 address,
	Patch::patch_data_type type,
	u64 data)
{
	Patch::PatchCommand command;
	command.placetopatch = place;
	command.cpu = cpu;
	command.addr = address;
	command.type = type;
	command.data = data;
	return command;
}

// *****************************************************************************
// Writes
// *****************************************************************************

PATCH_TEST(Byte,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::BYTE_T, 0x12))
{
	ee.ExpectIdempotentWrite8(0x00100000, 0, 0x12);
}

PATCH_TEST(Short,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::SHORT_T, 0x1234))
{
	ee.ExpectIdempotentWrite16(0x00100000, 0, 0x1234);
}

PATCH_TEST(Word,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::WORD_T, 0x12345678))
{
	ee.ExpectIdempotentWrite32(0x00100000, 0, 0x12345678);
}

PATCH_TEST(Double,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::DOUBLE_T, 0x123456789acdef12))
{
	ee.ExpectIdempotentWrite64(0x00100000, 0, 0x123456789acdef12);
}

PATCH_TEST(BigEndianShort,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::SHORT_BE_T, 0x1234))
{
	ee.ExpectIdempotentWrite16(0x00100000, 0, 0x3412);
}

PATCH_TEST(BigEndianWord,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::WORD_BE_T, 0x12345678))
{
	ee.ExpectIdempotentWrite32(0x00100000, 0, 0x78563412);
}

PATCH_TEST(BigEndianDouble,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::DOUBLE_BE_T, 0xabcdef0123456789))
{
	ee.ExpectIdempotentWrite64(0x00100000, 0, 0x8967452301efcdab);
}

PATCH_TEST(IOPByte,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_IOP, 0x00100000, Patch::BYTE_T, 0x12))
{
	iop.ExpectIdempotentWrite8(0x00100000, 0, 0x12);
}

PATCH_TEST(IOPShort,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_IOP, 0x00100000, Patch::SHORT_T, 0x1234))
{
	iop.ExpectIdempotentWrite16(0x00100000, 0, 0x1234);
}

PATCH_TEST(IOPWord,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_IOP, 0x00100000, Patch::WORD_T, 0x12345678))
{
	iop.ExpectIdempotentWrite32(0x00100000, 0, 0x12345678);
}

// *****************************************************************************
// Writes (Extended)
// *****************************************************************************

PATCH_TEST(Extended8BitWrite,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00100000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectWrite8(0x00100000, 0x12);
}

PATCH_TEST(Extended16BitWrite,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x10100000, Patch::EXTENDED_T, 0x00001234))
{
	ee.ExpectWrite16(0x00100000, 0x1234);
}

PATCH_TEST(Extended32BitWrite,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x20100000, Patch::EXTENDED_T, 0x12345678))
{
	ee.ExpectWrite32(0x00100000, 0x12345678);
}

// *****************************************************************************
// Increments/Decrements (Extended)
// *****************************************************************************

PATCH_TEST(Extended8BitIncrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30000012, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30000012, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead8(0x00100000, 0x00);
	ee.ExpectWrite8(0x00100000, 0x12);
	ee.ExpectRead8(0x00100000, 0x12);
	ee.ExpectWrite8(0x00100000, 0x24);
}

PATCH_TEST(Extended8BitIncrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30000012, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30000012, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead8(0x00100000, 0xee);
	ee.ExpectWrite8(0x00100000, 0x00);
	ee.ExpectRead8(0x00100000, 0x00);
	ee.ExpectWrite8(0x00100000, 0x12);
}

PATCH_TEST(Extended8BitDecrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30100012, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30100012, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead8(0x00100000, 0x24);
	ee.ExpectWrite8(0x00100000, 0x12);
	ee.ExpectRead8(0x00100000, 0x12);
	ee.ExpectWrite8(0x00100000, 0x00);
}

PATCH_TEST(Extended8BitDecrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30100012, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30100012, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead8(0x00100000, 0x12);
	ee.ExpectWrite8(0x00100000, 0x00);
	ee.ExpectRead8(0x00100000, 0x00);
	ee.ExpectWrite8(0x00100000, 0xee);
}

PATCH_TEST(Extended16BitIncrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30201234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30201234, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead16(0x00100000, 0x0000);
	ee.ExpectWrite16(0x00100000, 0x1234);
	ee.ExpectRead16(0x00100000, 0x1234);
	ee.ExpectWrite16(0x00100000, 0x2468);
}

PATCH_TEST(Extended16BitIncrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30201234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30201234, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead16(0x00100000, 0xedcc);
	ee.ExpectWrite16(0x00100000, 0x0000);
	ee.ExpectRead16(0x00100000, 0x0000);
	ee.ExpectWrite16(0x00100000, 0x1234);
}

PATCH_TEST(Extended16BitDecrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30301234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30301234, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead16(0x00100000, 0x2468);
	ee.ExpectWrite16(0x00100000, 0x1234);
	ee.ExpectRead16(0x00100000, 0x1234);
	ee.ExpectWrite16(0x00100000, 0x0000);
}

PATCH_TEST(Extended16BitDecrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30301234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30301234, Patch::EXTENDED_T, 0x00100000))
{
	ee.ExpectRead16(0x00100000, 0x1234);
	ee.ExpectWrite16(0x00100000, 0x0000);
	ee.ExpectRead16(0x00100000, 0x0000);
	ee.ExpectWrite16(0x00100000, 0xedcc);
}

PATCH_TEST(Extended32BitIncrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30400000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30400000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead32(0x00100000, 0x00000000);
	ee.ExpectWrite32(0x00100000, 0x12345678);
	ee.ExpectRead32(0x00100000, 0x12345678);
	ee.ExpectWrite32(0x00100000, 0x2468acf0);
}

PATCH_TEST(Extended32BitIncrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30400000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30400000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead32(0x00100000, 0xedcba988);
	ee.ExpectWrite32(0x00100000, 0x00000000);
	ee.ExpectRead32(0x00100000, 0x00000000);
	ee.ExpectWrite32(0x00100000, 0x12345678);
}

PATCH_TEST(Extended32BitDecrement,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30500000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30500000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead32(0x00100000, 0x2468acf0);
	ee.ExpectWrite32(0x00100000, 0x12345678);
	ee.ExpectRead32(0x00100000, 0x12345678);
	ee.ExpectWrite32(0x00100000, 0x00000000);
}

PATCH_TEST(Extended32BitDecrementWrapping,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30500000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x30500000, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead32(0x00100000, 0x12345678);
	ee.ExpectWrite32(0x00100000, 0x00000000);
	ee.ExpectRead32(0x00100000, 0x00000000);
	ee.ExpectWrite32(0x00100000, 0xedcba988);
}

// *****************************************************************************
// Serial Write (Extended)
// *****************************************************************************

PATCH_TEST(ExtendedSerialWriteZero,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x40100000, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00000000, Patch::EXTENDED_T, 0x00000000))
{
}

PATCH_TEST(ExtendedSerialWriteOnce,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x40100000, Patch::EXTENDED_T, 0x00010000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x11111111))
{
	ee.ExpectWrite32(0x00100000, 0x12345678);
}

PATCH_TEST(ExtendedSerialWriteContiguous,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x40100000, Patch::EXTENDED_T, 0x00020001),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x11111111))
{
	ee.ExpectWrite32(0x00100000, 0x12345678);
	ee.ExpectWrite32(0x00100004, 0x23456789);
}

PATCH_TEST(ExtendedSerialWriteStrided,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x40100000, Patch::EXTENDED_T, 0x00020002),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x12345678, Patch::EXTENDED_T, 0x11111111))
{
	ee.ExpectWrite32(0x00100000, 0x12345678);
	ee.ExpectWrite32(0x00100008, 0x23456789);
}

// *****************************************************************************
// Copy bytes (Extended)
// *****************************************************************************

PATCH_TEST(ExtendedCopyBytes0,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x50100000, Patch::EXTENDED_T, 0x00000000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000000))
{
}

PATCH_TEST(ExtendedCopyBytes2,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x50100000, Patch::EXTENDED_T, 0x00000002),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead8(0x00100000, 0x12);
	ee.ExpectWrite8(0x00200000, 0x12);
	ee.ExpectRead8(0x00100001, 0x12);
	ee.ExpectWrite8(0x00200001, 0x12);
}

// *****************************************************************************
// Pointer write (Extended)
// *****************************************************************************

PATCH_TEST(ExtendedPointerWrite8,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00000001, Patch::EXTENDED_T, 0x00000004))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectWrite8(0x00200004, 0x12);
}

PATCH_TEST(ExtendedPointerWrite16,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x00001234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00010001, Patch::EXTENDED_T, 0x00000004))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectWrite16(0x00200004, 0x1234);
}

PATCH_TEST(ExtendedPointerWrite32,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x12345678),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00020001, Patch::EXTENDED_T, 0x00000004))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectWrite32(0x00200004, 0x12345678);
}

PATCH_TEST(ExtendedPointerWriteMultiEven,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x12345678),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00020002, Patch::EXTENDED_T, 0x00000004),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00000008, Patch::EXTENDED_T, 0x00000000))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectRead32(0x00200004, 0x00300000);
	ee.ExpectWrite32(0x00300008, 0x12345678);
}

PATCH_TEST(ExtendedPointerWriteMultiOdd,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x12345678),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00020003, Patch::EXTENDED_T, 0x00000004),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00000008, Patch::EXTENDED_T, 0x0000000c))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectRead32(0x00200004, 0x00300000);
	ee.ExpectRead32(0x00300008, 0x00400000);
	ee.ExpectWrite32(0x0040000c, 0x12345678);
}

PATCH_TEST(ExtendedPointerWriteFirstNull,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x12345678),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00020000, Patch::EXTENDED_T, 0x00000004))
{
	ee.ExpectRead32(0x00100000, 0x00000000);
}

PATCH_TEST(ExtendedPointerWriteLastNull,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x60100000, Patch::EXTENDED_T, 0x12345678),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00020003, Patch::EXTENDED_T, 0x00000004),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00000008, Patch::EXTENDED_T, 0x0000000c))
{
	ee.ExpectRead32(0x00100000, 0x00200000);
	ee.ExpectRead32(0x00200004, 0x00300000);
	ee.ExpectRead32(0x00300008, 0x00000000);
}

// *****************************************************************************
// Boolean operation (Extended)
// *****************************************************************************

PATCH_TEST(ExtendedBooleanOr8,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead8(0x00100000, 0x78);
	ee.ExpectWrite8(0x00100000, 0x7a);
}

PATCH_TEST(ExtendedBooleanOr16,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00101234))
{
	ee.ExpectRead16(0x00100000, 0x89ab);
	ee.ExpectWrite16(0x00100000, 0x9bbf);
}

PATCH_TEST(ExtendedBooleanAnd8,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00200012))
{
	ee.ExpectRead8(0x00100000, 0x34);
	ee.ExpectWrite8(0x00100000, 0x10);
}

PATCH_TEST(ExtendedBooleanAnd16,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00301234))
{
	ee.ExpectRead16(0x00100000, 0x5678);
	ee.ExpectWrite16(0x00100000, 0x1230);
}

PATCH_TEST(ExtendedBooleanXor8,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00400012))
{
	ee.ExpectRead8(0x00100000, 0x89);
	ee.ExpectWrite8(0x00100000, 0x9b);
}

PATCH_TEST(ExtendedBooleanXor16,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x70100000, Patch::EXTENDED_T, 0x00501234))
{
	ee.ExpectRead16(0x00100000, 0x89ab);
	ee.ExpectWrite16(0x00100000, 0x9b9f);
}

// *****************************************************************************
// Do multi-lines if conditional (Extended)
// *****************************************************************************

PATCH_TEST(ExtendedConditional8BitEqualTrue,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01010012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead8(0x00100000, 0x12);
	ee.ExpectWrite8(0x00200000, 0x12);
	ee.ExpectWrite8(0x00300000, 0x12);
}

PATCH_TEST(ExtendedConditional8BitEqualFalse,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01010012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead8(0x00100000, 0x21);
	ee.ExpectWrite8(0x00300000, 0x12);
}

PATCH_TEST(ExtendedConditionalEqualTrue,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01001234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x1234);
	ee.ExpectWrite8(0x00200000, 0x12);
	ee.ExpectWrite8(0x00300000, 0x12);
}

PATCH_TEST(ExtendedConditionalEqualFalse,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01001234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x4321);
	ee.ExpectWrite8(0x00300000, 0x12);
}

PATCH_TEST(ExtendedConditionalNotEqualTrue,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01101234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x4321);
	ee.ExpectWrite8(0x00200000, 0x12);
}

PATCH_TEST(ExtendedConditionalNotEqualFalse,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01101234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x1234);
}

PATCH_TEST(ExtendedConditionalLessThanTrue,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01101234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x4321);
	ee.ExpectWrite8(0x00200000, 0x12);
}

PATCH_TEST(ExtendedConditionalLessThanFalse,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xd0100000, Patch::EXTENDED_T, 0x01101234),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x1234);
}

PATCH_TEST(ExtendedConditionalECodeEqualTrue,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xe0011234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x1234);
	ee.ExpectWrite8(0x00200000, 0x12);
	ee.ExpectWrite8(0x00300000, 0x12);
}

PATCH_TEST(ExtendedConditionalECodeEqualFalse,
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0xe0011234, Patch::EXTENDED_T, 0x00100000),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00200000, Patch::EXTENDED_T, 0x00000012),
	BuildPatchCommand(Patch::PPT_ONCE_ON_LOAD, Patch::CPU_EE, 0x00300000, Patch::EXTENDED_T, 0x00000012))
{
	ee.ExpectRead16(0x00100000, 0x4321);
	ee.ExpectWrite8(0x00300000, 0x12);
}
