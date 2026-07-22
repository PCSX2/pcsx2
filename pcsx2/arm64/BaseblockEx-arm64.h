// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
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
//
// Link-map liveness (2026-07-20). Entries used to accumulate for the whole
// lifetime of the map: Link() inserts unconditionally, and nothing pruned.
// Every recompile of a caller emits its branch at a *fresh* code address and
// appended another entry, so New(pc) re-patched every site the map had ever
// seen for that pc — each with its own 4-byte icache flush. Measured on
// Dirge of Cerberus: 13,372 stale sites on one PC, and 93.5% of EE-thread
// cycles inside __aarch64_sync_cache_range. Each entry now carries the
// identity of the block that emitted it (owner startpc + that block's code
// address), and both Link() and New() erase entries whose owner is gone or has
// since been recompiled. Cost becomes O(live callers) instead of O(all callers
// ever), and the map stops growing without bound.
//
// Liveness rests on two invariants: block code is bump-allocated and never
// reclaimed until a full Reset() (which also clears the map), so an fnptr
// value never identifies two different compiles; and BASEBLOCKEX::fnptr is
// authoritative for the block's *current* code, which is why New() updates
// it when a startpc is recompiled in place rather than leaving it stale.

#pragma once

#include <algorithm>
#include <map>

#include "common/HostSys.h"
#include "arm64/AsmHelpers.h" // armGetWritableCodePtr (iOS dual-map W^X)
#include "x86/BaseblockEx.h"  // BASEBLOCK, BASEBLOCKEX, BaseBlockArray, recLUT_SetPage

class Arm64BaseBlocks
{
protected:
	// A registered branch site, tagged with the block that emitted it so
	// New() can tell live sites from ones left behind by a superseded
	// compile. See the liveness note in the file header.
	struct LinkSite
	{
		// Patch site address, with kLinkSiteCallBit in bit 0.
		uptr site;
		// Block that emitted this site. owner_fnptr == 0 means "untracked":
		// the site was registered outside a compile (only the direct unit
		// tests do this), and is treated as permanently live.
		u32 owner_startpc;
		uptr owner_fnptr;
	};

	using linkmap_t = std::multimap<u32, LinkSite>;

	BaseBlockArray blocks;
	linkmap_t links;
	uptr jitcompile = 0;

	// The block currently being compiled, published by New(). Link() stamps
	// it onto every site it records.
	u32 emitting_startpc = 0;
	uptr emitting_fnptr = 0;

	// Link sites are 4-byte-aligned instruction addresses, so bit 0 of the
	// stored uptr is free: it tags the site's branch form. Untagged = B,
	// tagged = BL (call-ret stack call sites — the BL pushes the hardware
	// return-address stack so the callee's paired RET predicts).
	static constexpr uptr kLinkSiteCallBit = 1;

	// Encode a B/BL imm26 from `site` to `target`. ARM64 encoding:
	//   B : bits[31:26] = 000101, BL: bits[31:26] = 100101
	//   bits[25:0] = sign-extended ((target - site) >> 2)
	static u32 EncodeB(uptr site, uptr target, bool call = false)
	{
		const intptr_t off = static_cast<intptr_t>(target) - static_cast<intptr_t>(site);
		pxAssertRel((off & 3) == 0, "Branch offset not 4-byte aligned");
		const intptr_t imm26 = off >> 2;
		pxAssertRel(imm26 >= -(1 << 25) && imm26 < (1 << 25), "Branch offset out of B imm26 range");
		return (call ? 0x94000000u : 0x14000000u) | (static_cast<u32>(imm26) & 0x03FFFFFFu);
	}

	// Store only, no cache maintenance. Valid ONLY for a site inside the
	// block currently being emitted: that buffer has not been executed yet,
	// and armEndBlock() issues one whole-range flush over it before it can
	// be. (AetherSX2 does the same — its Link() performs no flush at all,
	// leaving it to the single range flush in armEndBlock.)
	// Never use this on code that is already live; use PatchAtomic.
	static void PatchWord(uptr site, u32 instr)
	{
		// 4-byte aligned word stores are atomic on AArch64. `site` is the RX
		// address; under iOS dual-map W^X the store goes through the RW alias
		// (identity elsewhere). Callers hold an open Begin/EndCodeWrite scope
		// for the toggle modes; on Darwin the "signal handler" caller is
		// really the Mach exception-handler thread, so that scope is safe.
		*reinterpret_cast<volatile u32*>(armGetWritableCodePtr(reinterpret_cast<u8*>(site))) = instr;
	}

	// (HostSys::FlushInstructionCache, not the raw builtin: on Darwin the
	// builtin lowers to a compiler-rt ___clear_cache call that the iOS link
	// doesn't provide; the wrapper uses sys_icache_invalidate there.)
	static void FlushRange(uptr lo, uptr hi)
	{
		HostSys::FlushInstructionCache(reinterpret_cast<void*>(lo), static_cast<u32>(hi - lo));
	}

	static void PatchAtomic(uptr site, u32 instr)
	{
		PatchWord(site, instr);
		// Then make sure cores fetching instructions see the new word.
		FlushRange(site, site + 4);
		// Signal-safe (counter-only) — Remove() can call this from the
		// SIGSEGV fastmem handler.
	}

	// Drop every entry for `dest_pc` whose owner is gone or has been
	// recompiled since. Called from both Link() and New(): New() alone is not
	// enough, because a destination that is never recompiled again would never
	// have its list revisited, and the entries its callers keep re-registering
	// would accumulate for the life of the map. Erasing a multimap element
	// invalidates only that element's iterator, so the end of the range stays
	// valid across the walk.
	void PruneDeadLinks(u32 dest_pc)
	{
		const auto range = links.equal_range(dest_pc);
		for (auto it = range.first; it != range.second;)
		{
			if (IsOwnerLive(it->second))
				++it;
			else
				it = links.erase(it);
		}
	}

	// True when `ls` still lives inside a block that is both present and has
	// not been recompiled since the site was registered.
	bool IsOwnerLive(const LinkSite& ls)
	{
		if (ls.owner_fnptr == 0)
			return true; // untracked registration (direct unit tests)

		const BASEBLOCKEX* owner = Get(ls.owner_startpc);
		return owner && owner->startpc == ls.owner_startpc && owner->fnptr == ls.owner_fnptr;
	}

	// Coalesce the just-patched sites into as few cache-maintenance ranges as
	// possible. The per-call cost is dominated by the `dsb ish` / `isb` pair,
	// not by the DC/IC ops, so N adjacent 4-byte flushes cost ~N barrier pairs
	// where one range flush costs one. Sites emitted by the same block sit a
	// few instructions apart, so this usually collapses to a single range.
	static void FlushPatchedSites(uptr* sites, u32 count)
	{
		if (count == 0)
			return;

		std::sort(sites, sites + count);

		constexpr uptr kMaxGap = 256;
		uptr lo = sites[0], hi = sites[0] + 4;
		for (u32 i = 1; i < count; i++)
		{
			if (sites[i] - hi <= kMaxGap)
			{
				hi = sites[i] + 4;
				continue;
			}
			FlushRange(lo, hi);
			lo = sites[i];
			hi = sites[i] + 4;
		}
		FlushRange(lo, hi);
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
	// records the site so New(pc, ...) can patch it later. `call` sites
	// get BL instead of B (and keep it across every re-patch).
	void Link(u32 pc, void* patch_site, bool call = false)
	{
		pxAssertRel(jitcompile, "SetJITCompile() must be called before Link()");
		pxAssertRel((reinterpret_cast<uptr>(patch_site) & kLinkSiteCallBit) == 0,
			"Link patch site must be 4-byte aligned");

		BASEBLOCKEX* target = Get(pc);
		const uptr target_addr = (target && target->startpc == pc)
			? target->fnptr : jitcompile;
		// No flush: the site is in the block being emitted right now, and
		// armEndBlock() range-flushes that buffer. See PatchWord.
		PatchWord(reinterpret_cast<uptr>(patch_site),
			EncodeB(reinterpret_cast<uptr>(patch_site), target_addr, call));

		// Reap this destination's dead entries before adding ours, so its list
		// tracks live callers rather than every caller it has ever had.
		PruneDeadLinks(pc);

		links.insert({pc,
			LinkSite{reinterpret_cast<uptr>(patch_site) | (call ? kLinkSiteCallBit : 0),
				emitting_startpc, emitting_fnptr}});
	}

	// Begin compiling the block at `startpc`, whose code starts at `fnptr`.
	// Returns its BASEBLOCKEX, creating one if this startpc has no live block.
	//
	// A startpc can be recompiled while its BASEBLOCKEX is still in the array
	// (recClear resets BLOCK->fnptr across a straddled extent but deliberately
	// spares the in-progress block's entry). BaseBlockArray::insert() has no
	// dedup, so that case must reuse the existing entry — and it must retarget
	// it at the new code, otherwise fnptr keeps pointing at the superseded
	// compile and every consumer of it is wrong: x86size is computed from a
	// dead base, Remove() writes its redirect stub over dead code instead of
	// the live entry, and Link() sends new callers into stale code.
	BASEBLOCKEX* New(u32 startpc, uptr fnptr)
	{
		// Published for Link() to stamp onto the sites this block emits.
		emitting_startpc = startpc;
		emitting_fnptr = fnptr;

		const int idx = Index(startpc);
		BASEBLOCKEX* block = (idx >= 0 && blocks[idx].startpc == startpc)
			? &blocks[idx] : nullptr;
		if (block)
			block->fnptr = fnptr;
		else
			block = blocks.insert(startpc, fnptr);

		// Repoint the links waiting on this PC at the new code, dropping any
		// left behind by a block that has since been removed or recompiled.
		// Those sites are unreachable code; patching them is pure waste, and
		// it is that waste which used to dominate the EE thread.
		uptr patched[64];
		u32 npatched = 0;

		const auto range = links.equal_range(startpc);
		for (auto it = range.first; it != range.second;)
		{
			if (!IsOwnerLive(it->second))
			{
				it = links.erase(it);
				continue;
			}

			const uptr site = it->second.site & ~kLinkSiteCallBit;
			PatchWord(site, EncodeB(site, fnptr, (it->second.site & kLinkSiteCallBit) != 0));
			if (npatched < std::size(patched))
				patched[npatched++] = site;
			else
				FlushRange(site, site + 4); // overflow: flush as we go
			++it;
		}

		FlushPatchedSites(patched, npatched);
		return block;
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
	// mutating an STL container here is not signal-safe. The entries this
	// strands are reaped by the next New() for their destination PC, which
	// sees the owner is gone; until then the redirect stub keeps any site
	// still pointing at this block correct.
	//
	// SL-1: a resident self-loop's back-edge is an internal B to the loop-top
	// (past the entry redirect), so it gets its own atomic repoint — to the
	// block's cold spill stub, which writes back the pinned set, publishes pc,
	// and exits via DispatcherEvent. Flat-array field reads + PatchAtomic
	// only, so the signal-safety contract holds.
	__fi void Remove(int first, int last)
	{
		pxAssert(first <= last);

		if (jitcompile)
		{
			for (int i = first; i <= last; ++i)
			{
				const uptr site = blocks[i].fnptr;
				PatchAtomic(site, EncodeB(site, jitcompile));

				if (blocks[i].backedge_site)
					PatchAtomic(blocks[i].backedge_site,
						EncodeB(blocks[i].backedge_site, blocks[i].backedge_stub));
			}
		}

		blocks.erase(first, last + 1);
	}

	__fi void Reset()
	{
		blocks.clear();
		links.clear();
		emitting_startpc = 0;
		emitting_fnptr = 0;
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
			const uptr site = it->second.site & ~kLinkSiteCallBit;
			if (site >= lo && site < hi)
				return true;
		}
		return false;
	}

	// Test-only: number of registered link sites targeting dst_pc, and the
	// total across all destinations. Used to pin that the map stays bounded
	// by *live* sites rather than growing with every recompile.
	size_t LinkCount(u32 dst_pc) const
	{
		const auto range = links.equal_range(dst_pc);
		return static_cast<size_t>(std::distance(range.first, range.second));
	}

	size_t TotalLinkCount() const { return links.size(); }
#endif
};
