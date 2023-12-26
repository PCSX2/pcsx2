// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "pcsx2/GS/GSBlock.h"
#include "pcsx2/GS/GSClut.h"
#include "pcsx2/GS/MultiISA.h"
#include <gtest/gtest.h>
#include <string.h>

#include "cpuinfo.h"

#ifdef MULTI_ISA_UNSHARED_COMPILATION

enum class TestISA
{
	isa_sse4,
	isa_avx,
	isa_avx2,
	isa_native,
};

static bool CheckCapabilities(TestISA required_caps)
{
	cpuinfo_initialize();
	if (required_caps == TestISA::isa_avx && !cpuinfo_has_x86_avx())
		return false;
	if (required_caps == TestISA::isa_avx2 && !cpuinfo_has_x86_avx2())
		return false;

	return true;
}

#define MULTI_ISA_STRINGIZE_(x) #x
#define MULTI_ISA_STRINGIZE(x) MULTI_ISA_STRINGIZE_(x)

#define MULTI_ISA_CONCAT_(a, b) a##b
#define MULTI_ISA_CONCAT(a, b) MULTI_ISA_CONCAT_(a, b)

#define MULTI_ISA_TEST(group, name) TEST(MULTI_ISA_CONCAT(MULTI_ISA_CONCAT(MULTI_ISA_UNSHARED_COMPILATION, _), group), name)
#define SKIP_IF_UNSUPPORTED() \
	if (!CheckCapabilities(TestISA::MULTI_ISA_UNSHARED_COMPILATION)) { \
		GTEST_SKIP() << "Host CPU does not support " MULTI_ISA_STRINGIZE(MULTI_ISA_UNSHARED_COMPILATION); \
	}

#else

#define MULTI_ISA_TEST(group, name) TEST(group, name)
#define SKIP_IF_UNSUPPORTED()

#endif

MULTI_ISA_UNSHARED_START

static void swizzle(const u8* table, u8* dst, const u8* src, int bpp, bool deswizzle)
{
	int pxbytes = bpp / 8;
	for (int i = 0; i < (256 / pxbytes); i++)
	{
		int soff = (deswizzle ? table[i] : i) * pxbytes;
		int doff = (deswizzle ? i : table[i]) * pxbytes;
		memcpy(&dst[doff], &src[soff], pxbytes);
	}
}

static void swizzle4(const u16* table, u8* dst, const u8* src, bool deswizzle)
{
	for (int i = 0; i < 512; i++)
	{
		int soff = (deswizzle ? table[i] : i);
		int doff = (deswizzle ? i : table[i]);
		int spx = src[soff >> 1] >> ((soff & 1) * 4) & 0xF;
		u8* dpx = &dst[doff >> 1];
		int dshift = (doff & 1) * 4;
		*dpx &= (0xF0 >> dshift);
		*dpx |= (spx << dshift);
	}
}

static void swizzleH(const u8* table, u32* dst, const u8* src, int bpp, int shift)
{
	for (int i = 0; i < 64; i++)
	{
		int spx;
		if (bpp == 8)
			spx = src[i];
		else
			spx = (src[i >> 1] >> ((i & 1) * 4)) & 0xF;
		spx <<= shift;
		dst[table[i]] = spx;
	}
}

static void expand16(u32* dst, const u16* src, const GIFRegTEXA& texa)
{
	for (int i = 0; i < 128; i++)
	{
		int r = (src[i] << 3) & 0x0000F8;
		int g = (src[i] << 6) & 0x00F800;
		int b = (src[i] << 9) & 0xF80000;
		dst[i] = r | g | b;
		if (src[i] & 0x8000)
		{
			dst[i] |= texa.TA1 << 24;
		}
		else if (!texa.AEM || src[i])
		{
			dst[i] |= texa.TA0 << 24;
		}
	}
}

static void expand8(u32* dst, const u8* src, const u32* palette)
{
	for (int i = 0; i < 256; i++)
	{
		dst[i] = palette[src[i]];
	}
}

static void expand4(u32* dst, const u8* src, const u32* palette)
{
	for (int i = 0; i < 512; i++)
	{
		dst[i] = palette[(src[i >> 1] >> ((i & 1) * 4)) & 0xF];
	}
}

static void expand4P(u8* dst, const u8* src)
{
	for (int i = 0; i < 512; i++)
	{
		dst[i] = (src[i >> 1] >> ((i & 1) * 4)) & 0xF;
	}
}

static void expandH(u32* dst, const u32* src, const u32* palette, int shift, int mask)
{
	for (int i = 0; i < 64; i++)
	{
		dst[i] = palette[(src[i] >> shift) & mask];
	}
}

static void expandHP(u8* dst, const u32* src, int shift, int mask)
{
	for (int i = 0; i < 64; i++)
	{
		dst[i] = (src[i] >> shift) & mask;
	}
}

static std::string image2hex(const u8* bin, int rows, int columns, int bpp)
{
	std::string out;
	const char* hex = "0123456789ABCDEF";

	for (int y = 0; y < rows; y++)
	{
		if (y != 0)
			out.push_back('\n');
		for (int x = 0; x < columns; x++)
		{
			if (x != 0)
				out.push_back(' ');
			if (bpp == 4)
			{
				if (x & 1)
				{
					out.push_back(hex[*bin >> 4]);
					bin++;
				}
				else
				{
					out.push_back(hex[*bin & 0xF]);
				}
			}
			else
			{
				for (int z = 0; z < (bpp / 8); z++)
				{
					out.push_back(hex[*bin >> 4]);
					out.push_back(hex[*bin & 0xF]);
					bin++;
				}
			}
		}
	}

	return out;
}

struct TestData
{
	alignas(64) u8 block[256];
	alignas(64) u8 output[256 * (32 / 4)];
	alignas(64) u32 clut32[256];
	alignas(64) u64 clut64[256];

	/// Get some input data with pixel values counting up from 0
	static TestData Linear()
	{
		TestData output;
		memset(output.output, 0, sizeof(output.output));
		for (int i = 0; i < 256; i++)
		{
			output.block[i] = i;
			output.clut32[i] = i | (i << 16);
		}
		GSClut::ExpandCLUT64_T32_I8(output.clut32, output.clut64);
		return output;
	}

	/// Get some input data with random-ish (but consistent across runs) pixel values
	static TestData Random()
	{
		srand(0);
		TestData output;
		memset(output.output, 0, sizeof(output.output));
		for (int i = 0; i < 256; i++)
		{
			output.block[i] = rand();
			output.clut32[i] = rand();
		}
		GSClut::ExpandCLUT64_T32_I8(output.clut32, output.clut64);
		return output;
	}

	/// Move data from output back to block to run an expand
	TestData prepareExpand()
	{
		TestData output = *this;
		memcpy(output.block, output.output, sizeof(output.block));
		return output;
	}
};

static TestData swizzle(const u8* table, TestData data, int bpp, bool deswizzle)
{
	swizzle(table, data.output, data.block, bpp, deswizzle);
	return data;
}

static TestData swizzle4(const u16* table, TestData data, bool deswizzle)
{
	swizzle4(table, data.output, data.block, deswizzle);
	return data;
}

static TestData swizzleH(const u8* table, TestData data, int bpp, int shift)
{
	swizzleH(table, reinterpret_cast<u32*>(data.output), data.block, bpp, shift);
	return data;
}

static TestData expand16(TestData data, const GIFRegTEXA& texa)
{
	expand16(reinterpret_cast<u32*>(data.output), reinterpret_cast<const u16*>(data.block), texa);
	return data;
}

static TestData expand8(TestData data)
{
	expand8(reinterpret_cast<u32*>(data.output), data.block, data.clut32);
	return data;
}

static TestData expand4(TestData data)
{
	expand4(reinterpret_cast<u32*>(data.output), data.block, data.clut32);
	return data;
}

static TestData expand4P(TestData data)
{
	expand4P(data.output, data.block);
	return data;
}

static TestData expandH(TestData data, int shift, int mask)
{
	expandH(reinterpret_cast<u32*>(data.output), reinterpret_cast<const u32*>(data.block), data.clut32, shift, mask);
	return data;
}

static TestData expandHP(TestData data, int shift, int mask)
{
	expandHP(data.output, reinterpret_cast<u32*>(data.block), shift, mask);
	return data;
}

static void runTest(void (*fn)(TestData))
{
	fn(TestData::Linear());
	fn(TestData::Random());
}

static void assertEqual(const TestData& expected, const TestData& actual, const char* name, int rows, int columns, int bpp)
{
	std::string estr = image2hex(expected.output, rows, columns, bpp);
	std::string astr = image2hex(actual.output,   rows, columns, bpp);
	EXPECT_STREQ(estr.c_str(), astr.c_str()) << "Unexpected " << name;
}

MULTI_ISA_TEST(ReadTest, Read32)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		GSBlock::ReadBlock32(data.block, data.output, 32);
		assertEqual(expected, data, "Read32", 8, 8, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write32)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, false);
		GSBlock::WriteBlock32<32, 0xFFFFFFFF>(data.output, data.block, 32);
		assertEqual(expected, data, "Write32", 8, 8, 32);
	});
}

MULTI_ISA_TEST(ReadTest, Read16)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable16[0][0], data, 16, true);
		GSBlock::ReadBlock16(data.block, data.output, 32);
		assertEqual(expected, data, "Read16", 8, 16, 16);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read16)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		GIFRegTEXA texa = {0};
		texa.TA0 = 1;
		texa.TA1 = 2;
		TestData expected = swizzle(&columnTable16[0][0], data, 16, true);
		expected = expand16(expected.prepareExpand(), texa);
		GSBlock::ReadAndExpandBlock16<false>(data.block, data.output, 64, texa);
		assertEqual(expected, data, "ReadAndExpand16", 8, 16, 32);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read16AEM)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		// Actually test AEM
		u8 idx = data.block[0] >> 1;
		data.block[idx * 2 + 0] = 0;
		data.block[idx * 2 + 1] = 0;
		GIFRegTEXA texa = {0};
		texa.TA0 = 1;
		texa.TA1 = 2;
		texa.AEM = 1;
		TestData expected = swizzle(&columnTable16[0][0], data, 16, true);
		expected = expand16(expected.prepareExpand(), texa);
		GSBlock::ReadAndExpandBlock16<true>(data.block, data.output, 64, texa);
		assertEqual(expected, data, "ReadAndExpand16AEM", 8, 16, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write16)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable16[0][0], data, 16, false);
		GSBlock::WriteBlock16<32>(data.output, data.block, 32);
		assertEqual(expected, data, "Read16", 8, 16, 16);
	});
}

MULTI_ISA_TEST(ReadTest, Read8)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable8[0][0], data, 8, true);
		GSBlock::ReadBlock8(data.block, data.output, 16);
		assertEqual(expected, data, "Read8", 16, 16, 8);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read8)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable8[0][0], data, 8, true);
		expected = expand8(expected.prepareExpand());
		GSBlock::ReadAndExpandBlock8_32(data.block, data.output, 64, data.clut32);
		assertEqual(expected, data, "ReadAndExpand8", 16, 16, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write8)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable8[0][0], data, 8, false);
		GSBlock::WriteBlock8<32>(data.output, data.block, 16);
		assertEqual(expected, data, "Write8", 16, 16, 8);
	});
}

MULTI_ISA_TEST(ReadTest, Read8H)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandHP(expected.prepareExpand(), 24, 0xFF);
		GSBlock::ReadBlock8HP(data.block, data.output, 8);
		assertEqual(expected, data, "Read8H", 8, 8, 8);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read8H)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandH(expected.prepareExpand(), 24, 0xFF);
		GSBlock::ReadAndExpandBlock8H_32(data.block, data.output, 32, data.clut32);
		assertEqual(expected, data, "ReadAndExpand8H", 8, 8, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write8H)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzleH(&columnTable32[0][0], data, 8, 24);
		GSBlock::UnpackAndWriteBlock8H(data.block, 8, data.output);
		assertEqual(expected, data, "Write8H", 8, 8, 32);
	});
}

MULTI_ISA_TEST(ReadTest, Read4)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle4(&columnTable4[0][0], data, true);
		GSBlock::ReadBlock4(data.block, data.output, 16);
		assertEqual(expected, data, "Read4", 16, 32, 4);
	});
}

MULTI_ISA_TEST(ReadTest, Read4P)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle4(&columnTable4[0][0], data, true);
		expected = expand4P(expected.prepareExpand());
		GSBlock::ReadBlock4P(data.block, data.output, 32);
		assertEqual(expected, data, "Read4P", 16, 32, 8);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read4)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle4(&columnTable4[0][0], data, true);
		expected = expand4(expected.prepareExpand());
		GSBlock::ReadAndExpandBlock4_32(data.block, data.output, 128, data.clut32);
		assertEqual(expected, data, "ReadAndExpand4", 16, 32, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write4)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle4(&columnTable4[0][0], data, false);
		GSBlock::WriteBlock4<32>(data.output, data.block, 16);
		assertEqual(expected, data, "Write4", 16, 16, 4);
	});
}

MULTI_ISA_TEST(ReadTest, Read4HH)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandHP(expected.prepareExpand(), 28, 0xF);
		GSBlock::ReadBlock4HHP(data.block, data.output, 8);
		assertEqual(expected, data, "Read4HH", 8, 8, 8);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read4HH)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandH(expected.prepareExpand(), 28, 0xF);
		GSBlock::ReadAndExpandBlock4HH_32(data.block, data.output, 32, data.clut32);
		assertEqual(expected, data, "ReadAndExpand4HH", 8, 8, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write4HH)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzleH(&columnTable32[0][0], data, 4, 28);
		GSBlock::UnpackAndWriteBlock4HH(data.block, 4, data.output);
		assertEqual(expected, data, "Write4HH", 8, 8, 32);
	});
}

MULTI_ISA_TEST(ReadTest, Read4HL)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandHP(expected.prepareExpand(), 24, 0xF);
		GSBlock::ReadBlock4HLP(data.block, data.output, 8);
		assertEqual(expected, data, "Read4HL", 8, 8, 8);
	});
}

MULTI_ISA_TEST(ReadAndExpandTest, Read4HL)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzle(&columnTable32[0][0], data, 32, true);
		expected = expandH(expected.prepareExpand(), 24, 0xF);
		GSBlock::ReadAndExpandBlock4HL_32(data.block, data.output, 32, data.clut32);
		assertEqual(expected, data, "ReadAndExpand4HL", 8, 8, 32);
	});
}

MULTI_ISA_TEST(WriteTest, Write4HL)
{
	SKIP_IF_UNSUPPORTED();

	runTest([](TestData data)
	{
		TestData expected = swizzleH(&columnTable32[0][0], data, 4, 24);
		GSBlock::UnpackAndWriteBlock4HL(data.block, 4, data.output);
		assertEqual(expected, data, "Write4HL", 8, 8, 32);
	});
}

MULTI_ISA_UNSHARED_END
