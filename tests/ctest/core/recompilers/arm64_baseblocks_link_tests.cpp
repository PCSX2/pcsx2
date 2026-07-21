// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
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

// ---------------------------------------------------------------------------
// Link-map liveness. Entries are stamped with the block that emitted them, so
// New() can drop the ones stranded in code that has been removed or superseded
// instead of re-patching them forever. Regression cover for the Dirge of
// Cerberus blowup: 13,372 dead sites on one PC, 93.5% of the EE thread spent
// flushing icache for branches nothing can reach.
// ---------------------------------------------------------------------------

namespace
{
// A link registered while `owner_pc` was being compiled at `owner_entry`.
void CompileAndLink(Arm64BaseBlocks& bb, u32 owner_pc, u32* owner_entry, u32 dest_pc,
	u32* site, bool call = false)
{
	bb.New(owner_pc, reinterpret_cast<uptr>(owner_entry));
	bb.Link(dest_pc, site, call);
}
} // namespace

TEST(Arm64BaseBlocksLink, NewRebindsAnExistingBlockToItsNewCode)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kPc = 0x1000;
	u32* const first = &code[16];
	u32* const second = &code[24];
	u32* const site = &code[8];

	bb.Link(kPc, site);
	bb.New(kPc, reinterpret_cast<uptr>(first));
	ASSERT_EQ(bb.Get(kPc)->fnptr, reinterpret_cast<uptr>(first));

	// Recompiling a startpc whose BASEBLOCKEX survived a straddled recClear
	// must retarget the entry, not leave it aimed at the superseded compile.
	bb.New(kPc, reinterpret_cast<uptr>(second));
	EXPECT_EQ(bb.Get(kPc)->fnptr, reinterpret_cast<uptr>(second));
	// And it must not have produced a duplicate array entry for the same pc.
	EXPECT_EQ(bb.Index(kPc), 0);
	// Callers follow the block to its new code instead of branching into the
	// dead first compile.
	EXPECT_EQ(DecodeImm26Bytes(*site),
		reinterpret_cast<intptr_t>(second) - reinterpret_cast<intptr_t>(site));
}

TEST(Arm64BaseBlocksLink, StaleOwnerEntriesPrunedOnNew)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kOwner = 0x1000;
	constexpr u32 kDest = 0x2000;
	u32* const site_old = &code[8];

	// Owner compiles and links to a destination that does not exist yet...
	CompileAndLink(bb, kOwner, &code[16], kDest, site_old);
	// ...then is recompiled into a shape that no longer branches there, so
	// nothing re-registers a link for kDest and only New(kDest) can notice.
	bb.New(kOwner, reinterpret_cast<uptr>(&code[24]));
	ASSERT_EQ(bb.LinkCount(kDest), 1u);

	const u32 stale_word = *site_old;

	// The destination compiles: the site left behind in the owner's dead
	// first compile is dropped, not re-patched.
	u32* const dest_entry = &code[40];
	bb.New(kDest, reinterpret_cast<uptr>(dest_entry));

	EXPECT_EQ(bb.LinkCount(kDest), 0u);
	EXPECT_EQ(*site_old, stale_word);
}

TEST(Arm64BaseBlocksLink, RemovedOwnerEntriesPruned)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kOwner = 0x1000;
	constexpr u32 kDest = 0x2000;
	u32* const site = &code[8];

	CompileAndLink(bb, kOwner, &code[16], kDest, site);
	ASSERT_EQ(bb.LinkCount(kDest), 1u);

	const int idx = bb.Index(kOwner);
	ASSERT_GE(idx, 0);
	bb.Remove(idx, idx);

	const u32 stale_word = *site;
	bb.New(kDest, reinterpret_cast<uptr>(&code[40]));

	// Owner is gone, so its site is unreachable code: dropped, never patched.
	EXPECT_EQ(bb.LinkCount(kDest), 0u);
	EXPECT_EQ(*site, stale_word);
}

TEST(Arm64BaseBlocksLink, MapStaysBoundedUnderOwnerChurn)
{
	alignas(64) static u32 code[512];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kOwner = 0x1000;
	constexpr u32 kDest = 0x2000;
	constexpr int kRecompiles = 100;

	// The pathological shape: one block recompiled over and over, each compile
	// re-registering its branch at a fresh address. Before the liveness
	// filter this grew one entry per recompile forever (13,372 on one PC in
	// Dirge of Cerberus). The list must stay at the single live caller the
	// whole way through — the destination here is never recompiled, so
	// nothing but Link() itself can do the reaping.
	for (int i = 0; i < kRecompiles; i++)
	{
		CompileAndLink(bb, kOwner, &code[256 + i], kDest, &code[i]);
		ASSERT_EQ(bb.LinkCount(kDest), 1u) << "grew at recompile " << i;
	}

	const u32 first_word = code[0];
	u32* const dest_entry = &code[400];
	bb.New(kDest, reinterpret_cast<uptr>(dest_entry));

	// Exactly one live site survives — the one in the owner's current compile.
	EXPECT_EQ(bb.LinkCount(kDest), 1u);
	EXPECT_EQ(bb.TotalLinkCount(), 1u);
	EXPECT_EQ(DecodeImm26Bytes(code[kRecompiles - 1]),
		reinterpret_cast<intptr_t>(dest_entry) - reinterpret_cast<intptr_t>(&code[kRecompiles - 1]));
	EXPECT_EQ(code[0], first_word);
}

TEST(Arm64BaseBlocksLink, BlFormSurvivesPruning)
{
	alignas(64) static u32 code[64];
	std::memset(code, 0, sizeof(code));

	Arm64BaseBlocks bb;
	bb.SetJITCompile(&code[0]);

	constexpr u32 kOwner = 0x1000;
	constexpr u32 kDest = 0x2000;
	u32* const site_old = &code[8];
	u32* const site_new = &code[9];

	CompileAndLink(bb, kOwner, &code[16], kDest, site_old, /*call=*/true);
	CompileAndLink(bb, kOwner, &code[24], kDest, site_new, /*call=*/true);

	u32* const dest_entry = &code[40];
	bb.New(kDest, reinterpret_cast<uptr>(dest_entry));

	// The call-form tag rides through the liveness filter with the rest of
	// the entry — the survivor is still a BL.
	ASSERT_EQ(bb.LinkCount(kDest), 1u);
	EXPECT_EQ(OpcodeBits(*site_new), kOpcBL);
	EXPECT_EQ(DecodeImm26Bytes(*site_new),
		reinterpret_cast<intptr_t>(dest_entry) - reinterpret_cast<intptr_t>(site_new));
}
