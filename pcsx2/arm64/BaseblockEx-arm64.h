// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64-specific BaseBlocks with block linking that stays signal-safe.
//
// x86 BaseBlocks (pcsx2/x86/BaseblockEx.h) uses a std::multimap to track
// pending link sites and rewrites them whenever a block is added or
// removed. The multimap is not used here because Remove() runs from the
// SIGSEGV fastmem handler, where mutating an STL container is unsafe.
//
// This implementation supports block linking while keeping Remove() signal-safe:
//   - Link()/New() touch the multimap, but they only run from the
//     compile path (single-threaded, never from a signal).
//   - Remove() does NOT walk the link map. Instead it overwrites the
//     first 4 bytes of each removed block with `B JITCompile`, so any
//     stale link still resolves correctly via the dispatcher (which
//     can re-patch the link to the freshly compiled target on its next
//     dispatch). Block memory isn't reclaimed until a full reset, so
//     this 4-byte rewrite always lands on memory the recompiler still
//     owns.
//
// The patch site for each link is the address of a single B instruction
// emitted by SetBranchImm (see iR5900-arm64.cpp). Aligned 32-bit stores
// are atomic on AArch64, and `cacheflush(2)` on the patch site is
// async-signal-safe, so the redirect-stub Remove() write is signal-safe.
//
// Range: B imm26 covers ±128 MB. The EE recompiler region is 64 MB
// (HostMemoryMap::EErecSize), and JITCompile lives in the same region,
// so all link sites are reachable with a single B.

#pragma once

#include <map>

#include "arm64/JitTelemetry.h"
#include "x86/BaseblockEx.h"  // BASEBLOCK, BASEBLOCKEX, BaseBlockArray, recLUT_SetPage

class Arm64BaseBlocks
{
protected:
	using linkmap_t = std::multimap<u32, uptr>;

	BaseBlockArray blocks;
	linkmap_t links;
	uptr jitcompile = 0;

	// Encode a B imm26 from `site` to `target`. ARM64 B encoding:
	//   bits[31:26] = 000101
	//   bits[25:0]  = sign-extended ((target - site) >> 2)
	static u32 EncodeB(uptr site, uptr target)
	{
		const intptr_t off = static_cast<intptr_t>(target) - static_cast<intptr_t>(site);
		pxAssertRel((off & 3) == 0, "Branch offset not 4-byte aligned");
		const intptr_t imm26 = off >> 2;
		pxAssertRel(imm26 >= -(1 << 25) && imm26 < (1 << 25), "Branch offset out of B imm26 range");
		return 0x14000000u | (static_cast<u32>(imm26) & 0x03FFFFFFu);
	}

	static void PatchAtomic(uptr site, u32 instr)
	{
		// 4-byte aligned word stores are atomic on AArch64.
		*reinterpret_cast<volatile u32*>(site) = instr;
		// Then make sure cores fetching instructions see the new word.
		__builtin___clear_cache(reinterpret_cast<char*>(site),
			reinterpret_cast<char*>(site) + 4);
		// Signal-safe (counter-only) — Remove() can call this from the
		// SIGSEGV fastmem handler.
		ArmJitTelemetry::AddLinkPatch();
	}

public:
	Arm64BaseBlocks()
		: blocks(0x4000)
	{
	}

	void SetJITCompile(const void* recompiler_)
	{
		jitcompile = reinterpret_cast<uptr>(recompiler_);
	}

	// Register a link site that wants to branch directly to the block at
	// `pc`. Patches immediately if a block already exists; otherwise
	// records the site so New(pc, ...) can patch it later.
	void Link(u32 pc, void* patch_site)
	{
		pxAssertRel(jitcompile, "SetJITCompile() must be called before Link()");

		BASEBLOCKEX* target = Get(pc);
		const uptr target_addr = (target && target->startpc == pc)
			? target->fnptr : jitcompile;
		PatchAtomic(reinterpret_cast<uptr>(patch_site),
			EncodeB(reinterpret_cast<uptr>(patch_site), target_addr));

		links.insert({pc, reinterpret_cast<uptr>(patch_site)});
	}

	BASEBLOCKEX* New(u32 startpc, uptr fnptr)
	{
		// Patch any pending links waiting for a block at this PC. After
		// patching they go directly to fnptr instead of routing through
		// JITCompile.
		const auto range = links.equal_range(startpc);
		for (auto it = range.first; it != range.second; ++it)
			PatchAtomic(it->second, EncodeB(it->second, fnptr));

		return blocks.insert(startpc, fnptr);
	}

	int LastIndex(u32 startpc) const
	{
		if (blocks.size() == 0)
			return -1;

		int imin = 0, imax = (int)blocks.size() - 1, imid;

		while (imin != imax)
		{
			imid = (imin + imax + 1) >> 1;

			if (blocks[imid].startpc > startpc)
				imax = imid - 1;
			else
				imin = imid;
		}

		if (IsDevBuild)
		{
			if (imin != 0)
				pxAssert(blocks[imin].startpc <= startpc);
			if (imin < (int)blocks.size() - 1)
				pxAssert(blocks[imin + 1].startpc > startpc);
		}

		return imin;
	}

	__fi int Index(u32 startpc) const
	{
		int idx = LastIndex(startpc);

		if ((idx == -1) || (startpc < blocks[idx].startpc) ||
			((blocks[idx].size) && (startpc >= blocks[idx].startpc + blocks[idx].size * 4)))
			return -1;
		else
			return idx;
	}

	__fi BASEBLOCKEX* operator[](int idx)
	{
		if (idx < 0 || idx >= (int)blocks.size())
			return 0;

		return &blocks[idx];
	}

	__fi BASEBLOCKEX* Get(u32 startpc)
	{
		return (*this)[Index(startpc)];
	}

	// Signal-safe: writes a redirect stub at each removed block's entry
	// point so any stale link still resolves through JITCompile, then
	// erases from the flat sorted array. Does NOT touch the link map —
	// stale entries there are harmless (they just trigger a re-patch on
	// the next compile cycle for the same PC).
	__fi void Remove(int first, int last)
	{
		pxAssert(first <= last);

		if (jitcompile)
		{
			for (int i = first; i <= last; ++i)
			{
				const uptr site = blocks[i].fnptr;
				PatchAtomic(site, EncodeB(site, jitcompile));
			}
		}

		blocks.erase(first, last + 1);
	}

	__fi void Reset()
	{
		blocks.clear();
		links.clear();
	}

#ifdef PCSX2_RECOMPILER_TESTS
	// Test-only introspection. Returns true iff a link patch site within the
	// block containing src_pc targets a block at dst_pc. The link multimap
	// is keyed by destination PC, so the entries for dst_pc are walked to check
	// whether the patch site lies inside [block.fnptr, block.fnptr + x86size),
	// where x86size is BASEBLOCKEX's (legacy-named) host machine-code byte size.
	// O(L_d + log B) where L_d is the number of links to dst_pc and B is the
	// block count.
	bool IsLinked(u32 src_pc, u32 dst_pc) const
	{
		const int idx = LastIndex(src_pc);
		if (idx < 0)
			return false;
		const BASEBLOCKEX& b = blocks[idx];
		if (src_pc < b.startpc || src_pc >= b.startpc + b.size * 4)
			return false;
		const uptr lo = b.fnptr;
		const uptr hi = b.fnptr + b.x86size;
		const auto range = links.equal_range(dst_pc);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second >= lo && it->second < hi)
				return true;
		}
		return false;
	}
#endif
};
