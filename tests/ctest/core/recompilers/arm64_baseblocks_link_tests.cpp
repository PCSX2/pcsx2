// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Direct unit tests for Arm64BaseBlocks link-site patching: B-form vs
// BL-form (call-ret stack call sites carry bit 0 in the linkmap so every
// re-patch preserves the BL opcode). Patching happens on an ordinary RW
// buffer — no execution, encoding checks only.

#include "arm64/BaseblockEx-arm64.h"

#include <gtest/gtest.h>

namespace
{
// Decode helpers for B/BL imm26.
constexpr u32 kOpcB = 0x05;  // bits[31:26]
constexpr u32 kOpcBL = 0x25; // bits[31:26]

u32 OpcodeBits(u32 insn)
{
	return insn >> 26;
}

intptr_t DecodeImm26Bytes(u32 insn)
{
	s32 imm26 = static_cast<s32>(insn << 6) >> 6; // sign-extend bits[25:0]
	return static_cast<intptr_t>(imm26) * 4;
}
} // namespace

TEST(Arm64BaseBlocksLink, BlFormSurvivesLinkAndRepatch)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	u32* const site_b = &code[8];
	u32* const site_bl = &code[16];
	constexpr u32 kPc = 0x1000;

	// Unlinked: both forms initially patch toward JITCompile, each keeping
	// its own opcode.
	bb.Link(kPc, site_b);
	bb.Link(kPc, site_bl, /*call=*/true);
	EXPECT_EQ(OpcodeBits(*site_b), kOpcB);
	EXPECT_EQ(OpcodeBits(*site_bl), kOpcBL);
	EXPECT_EQ(DecodeImm26Bytes(*site_b),
		reinterpret_cast<intptr_t>(&code[0]) - reinterpret_cast<intptr_t>(site_b));
	EXPECT_EQ(DecodeImm26Bytes(*site_bl),
		reinterpret_cast<intptr_t>(&code[0]) - reinterpret_cast<intptr_t>(site_bl));

	// New() re-patches both pending sites to the fresh block, forms intact.
	u32* const block_entry = &code[32];
	bb.New(kPc, reinterpret_cast<uptr>(block_entry));
	EXPECT_EQ(OpcodeBits(*site_b), kOpcB);
	EXPECT_EQ(OpcodeBits(*site_bl), kOpcBL);
	EXPECT_EQ(DecodeImm26Bytes(*site_b),
		reinterpret_cast<intptr_t>(block_entry) - reinterpret_cast<intptr_t>(site_b));
	EXPECT_EQ(DecodeImm26Bytes(*site_bl),
		reinterpret_cast<intptr_t>(block_entry) - reinterpret_cast<intptr_t>(site_bl));
}

TEST(Arm64BaseBlocksLink, RemoveRedirectStubIsPlainBAndRelinkKeepsBl)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kPc = 0x2000;
	u32* const site_bl = &code[8];
	u32* const block_entry = &code[32];

	bb.Link(kPc, site_bl, /*call=*/true);
	bb.New(kPc, reinterpret_cast<uptr>(block_entry));
	ASSERT_EQ(OpcodeBits(*site_bl), kOpcBL);

	// Remove() writes the redirect stub at the DEAD BLOCK'S ENTRY — that is
	// always a plain B (it is a jump into JITCompile, not a call site). The
	// BL at the call site is untouched by Remove().
	const int idx = bb.Index(kPc);
	ASSERT_GE(idx, 0);
	bb.Remove(idx, idx);
	EXPECT_EQ(OpcodeBits(*block_entry), kOpcB);
	EXPECT_EQ(DecodeImm26Bytes(*block_entry),
		reinterpret_cast<intptr_t>(&code[0]) - reinterpret_cast<intptr_t>(block_entry));
	EXPECT_EQ(OpcodeBits(*site_bl), kOpcBL);

	// Recompile at the same PC: the stale linkmap entry re-patches the call
	// site to the new block, still BL.
	u32* const new_entry = &code[48];
	bb.New(kPc, reinterpret_cast<uptr>(new_entry));
	EXPECT_EQ(OpcodeBits(*site_bl), kOpcBL);
	EXPECT_EQ(DecodeImm26Bytes(*site_bl),
		reinterpret_cast<intptr_t>(new_entry) - reinterpret_cast<intptr_t>(site_bl));
}
