// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//#define mVUlogProg // Dumps MicroPrograms to \logs\*.html
//#define mVUprofileProg // Shows opcode statistics in console

#include <deque>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>
#include <arm_neon.h>
#include "Common.h"
#include "VU.h"
#include "MTVU.h"
#include "GS.h"
#include "Gif_Unit.h"
#include "iR5900-arm64.h"
#include "R5900OpcodeTables.h"
#include "common/Perf.h"

#include "microVU_Misc-arm64.h"
#include "MvuObservedEntries.h"
#include "microVU_Persist-arm64.h"

#ifndef XXH_versionNumber
#define XXH_STATIC_LINKING_ONLY 1
#define XXH_INLINE_ALL 1
#include "xxhash.h"
#endif

// Bump on any change to the arm64 mVU emitter that invalidates previously-cached
// (in-process or on-disk) program identity. Mixed into every contentHash so
// stale artifacts can't be reused after a codegen shape change.
//
// History:
//   1 — initial options sentinel (codegen constexprs + clamp modes
//       + speedhacks + FPCR).
//   2 — mixed in helper-table layout hash so the on-disk cache
//       invalidates atomically when the helper ABI shape changes.
//   3 — dropped helperTableLayoutHash from the options sentinel
//       (sentinel layout shrank; every contentHash changes).
//   5 — mVUclamp2 sign-preserving clamp selects a 2-row bounds table
//       (single-lane clamps only lane 0, x86 parity); sign-mode
//       emissions change shape (AX-02).
//   6 — lane-indexed FMUL broadcast fold made unconditional (AX-14):
//       broadcast-multiply emission changed shape (scratch Dup + 4-lane
//       FMUL -> by-element FMUL on the raw source reg); the experiment's
//       sentinel byte was retired in the same change.
//   7 — condEvilBranch ported (conditional branch in a branch delay slot
//       now emits the badBranch/evilBranch target-select sequence that
//       the arm64 port had dropped — MGS2 VU0 solver hang); pre-fix
//       payloads compiled the broken shape and must not be rehydrated.
//   8 — hot microVU scalars (divFlag/branch/cycles/…) moved adjacent to
//       the flag block and addressed as [gprMVUFlag, #imm] via
//       mVUfieldMem instead of per-site absolute materialization; every
//       block that touches them changes shape, and payloads bake the
//       field offsets, which the move changed.
//   9 — IBcc condition carry (doBranchCondCarry): the condition computes
//       into a pool temp and condBranch's tail Cmps it directly instead
//       of reloading mVU.branch with Ldrsh; conditional-branch blocks
//       change shape.
//  10 — inline jump-cache probe in normJumpCompile: JR/JALR tails probe
//       jc.prog vs prog.quick[].prog inline and Br to the cached
//       hostEntry on a hit, only falling into mVUbackupRegs + the
//       mVUcompileJIT C call on a miss; indirect-jump blocks change
//       shape.
static constexpr u32 kMvuCompilerAbiVersion = 10;

// Hash/equality functors for XXH128_hash_t — let std::unordered_map<XXH128_hash_t, …>
// work without a wrapping struct. low64 already carries the well-mixed half of
// the xxh3 output, so XORing in high64 is plenty for the bucket index; the map
// resolves collisions by ::operator== on the full 128 bits.
struct MvuContentHashHash
{
	size_t operator()(const XXH128_hash_t& h) const noexcept
	{
		return static_cast<size_t>(h.low64 ^ h.high64);
	}
};

struct MvuContentHashEq
{
	bool operator()(const XXH128_hash_t& a, const XXH128_hash_t& b) const noexcept
	{
		return a.low64 == b.low64 && a.high64 == b.high64;
	}
};

// Forward declarations
class microBlockManager;

//------------------------------------------------------------------
// IR structures — platform-independent, kept in sync with x86/microVU_IR.h.
//------------------------------------------------------------------

struct regCycleInfo
{
	u8 x : 4;
	u8 y : 4;
	u8 z : 4;
	u8 w : 4;
};

union alignas(16) microRegInfo
{
	struct
	{
		union
		{
			struct
			{
				u8 needExactMatch;
				u8 flagInfo;
				u8 q;
				u8 p;
				u8 xgkick;
				u8 viBackUp;
				u8 blockType;
				u8 r;
			};
			u64 quick64[1];
			u32 quick32[2];
		};

		u32 xgkickcycles;
		u8 unused;
		u8 vi15v;
		u16 vi15;

		struct
		{
			u8 VI[16];
			regCycleInfo VF[32];
		};
	};

	u128 full128[96 / sizeof(u128)];
	u64  full64[96 / sizeof(u64)];
	u32  full32[96 / sizeof(u32)];
};

static_assert(sizeof(microRegInfo) == 96, "microRegInfo was not 96 bytes");

struct microProgram;
struct MvuPersistLog;
struct microJumpCache
{
	microJumpCache() : prog(NULL), x86ptrStart(NULL), hostEntry(NULL) {}
	microProgram* prog;
	void* x86ptrStart; // Code entry point (name kept for struct compatibility)
	// Generalised "host entry" pointer. For JIT-emitted blocks this
	// mirrors x86ptrStart (the rec-cache slot the block was compiled
	// into). The indirection is retained for the persisted-JIT program
	// cache: hydrated blocks point hostEntry at the reloaded code
	// without disturbing the rec-cache bookkeeping. The dispatcher and
	// block-linking paths BR through hostEntry; x86ptrStart stays a
	// debugging/perf-attribution hint.
	void* hostEntry;
};

struct alignas(16) microBlock
{
	microRegInfo    pState;
	microRegInfo    pStateEnd;
	u8*             x86ptrStart; // Code entry point (name kept for struct compatibility)
	void*           hostEntry;   // see microJumpCache::hostEntry
	microJumpCache* jumpCache;
};

struct microTempRegInfo
{
	regCycleInfo VF[2];
	u8 VFreg[2];
	u8 VI;
	u8 VIreg;
	u8 q;
	u8 p;
	u8 r;
	u8 xgkick;
};

struct microVFreg
{
	u8 reg;
	u8 x;
	u8 y;
	u8 z;
	u8 w;
};

struct microVIreg
{
	u8 reg;
	u8 used;
};

struct microConstInfo
{
	u8  isValid;
	u32 regValue;
};

struct microUpperOp
{
	bool eBit;
	bool iBit;
	bool mBit;
	bool tBit;
	bool dBit;
	microVFreg VF_write;
	microVFreg VF_read[2];
};

struct microLowerOp
{
	microVFreg VF_write;
	microVFreg VF_read[2];
	microVIreg VI_write;
	microVIreg VI_read[2];
	microConstInfo constJump;
	u32  branch;
	u32  kickcycles;
	bool badBranch;
	bool evilBranch;
	bool isNOP;
	bool isFSSET;
	bool noWriteVF;
	bool backupVI;
	bool memReadIs;
	bool memReadIt;
	bool readFlags;
	bool isMemWrite;
	bool isKick;
};

struct microFlagInst
{
	bool doFlag;
	bool doNonSticky;
	u8   write;
	u8   lastWrite;
	u8   read;
};

struct microFlagCycles
{
	int xStatus[4];
	int xMac[4];
	int xClip[4];
	int cycles;
};

struct microOp
{
	u8   stall;
	bool isBadOp;
	bool isEOB;
	bool isBdelay;
	bool swapOps;
	bool backupVF;
	bool doXGKICK;
	u32  XGKICKPC;
	bool doDivFlag;
	int  readQ;
	int  writeQ;
	int  readP;
	int  writeP;
	microFlagInst sFlag;
	microFlagInst mFlag;
	microFlagInst cFlag;
	microUpperOp  uOp;
	microLowerOp  lOp;
};

template <u32 pSize>
struct microIR
{
	microBlock       block;
	microBlock*      pBlock;
	microTempRegInfo regsTemp;
	microOp          info[pSize / 2];
	microConstInfo   constReg[16];
	u8  branch;
	u32 cycles;
	u32 count;
	u32 curPC;
	u32 startPC;
	u32 sFlagHack;
};

//------------------------------------------------------------------
// Program/Block Management Structures (from x86/microVU.h)
//------------------------------------------------------------------

struct microBlockLink
{
	microBlock block;
	microBlockLink* next;
};

struct microBlockLinkRef
{
	microBlock* pBlock;
	u64 quick;
};

struct microRange
{
	s32 start;
	s32 end;
};

#define mProgSize (0x4000 / 4)
struct microProgram
{
	u32                data [mProgSize];
	microBlockManager* block[mProgSize / 2];
	std::deque<microRange>* ranges;
	u32 startPC;
	int idx;
	// Content-keyed identity for in-process dedup and on-disk cache lookup.
	// Computed in mVUcreateProg over the live microMem snapshot the
	// program was created from, mixed with kMvuCompilerAbiVersion + the per-VU
	// options sentinel + VU index. Stable across processes for a given VU
	// program + config combo, so on-disk cache artifacts can be keyed by it. Set
	// once at create and not updated thereafter — re-caches via mVUsetupRange
	// don't shift identity (contentMap relies on a stable key).
	XXH128_hash_t contentHash;
	bool          contentHashValid;

	// mVU.microMemWriteGen at the moment contentHash was anchored (create /
	// hydrate). While equal, no micro-mem write has happened since anchoring,
	// so the live image still matches the hash and the drift check in
	// mVUcacheProg can be skipped.
	u64           writeGenAtAnchor;

	// Count of mVU.prog.prog[] deque slots holding a (non-owning) pointer
	// to this microProgram. Bumped on each push_front, decremented at eviction.
	// The single owner is mVU.prog.contentMap[contentHash]; per-PC deques are
	// non-owning references. Single-threaded per VU, so plain s32.
	s32           refcount;

	// Set of microMem byte offsets the dispatcher has handed off into this
	// program. Initialized at mVUcreateProg with the creator's startPC;
	// mVUsearchProg accumulates additional entries seen during the program's
	// lifetime. `version` lets callers detect "new entry since last compile"
	// cheaply (compare vs the version snapshotted by the most recent
	// SaveProgram).
	MvuObservedEntries observed;

	// Recorded block-graph snapshot (chunks + fixups) for this program, built
	// by the emit-time recorder in microVU_Persist-arm64.inl. Null when
	// recording is disabled or nothing was recorded. Owned here; freed via
	// mVUPersist::OnProgramDeleted from mVUdeleteProg. (microProgram is
	// memset-constructed in mVUcreateProg, so a raw pointer is the right shape.)
	MvuPersistLog* persist;
};

typedef std::deque<microProgram*> microProgramList;

struct microProgramQuick
{
	microBlockManager* block;
	microProgram*      prog;
};

// VE-02: last-dispatch resolution cache, probed INLINE by the dispatcher
// stub before the mVUlookupProg BL. A hit requires hostEntry != null AND
// key_quick64 == live lpState.quick64[0] (modulo the sFlagHack 0x0C00
// flagInfo bits — NOT bit 2, which is part of needExactMatch and must
// compare exactly so exact-match states always miss to the C walk) AND
// key_pcs == (maskedPC << 32 | regs().start_pc). Seeded only by
// mVUlookupProg's quick-path resolutions (needExactMatch == 0), so a
// seeded key always describes a non-exact state. hostEntry == null is
// the invalid sentinel; mVUclear (any micro-mem write) and mVUreset
// clear it. Single-threaded per VU (EE thread for VU0, the VU thread
// for MTVU VU1), so no atomics.
struct MvuLastHit
{
	u64           key_quick64;
	u64           key_pcs;
	void*         hostEntry;
	microProgram* prog;
};

struct microProgManager
{
	microIR<mProgSize> IRinfo;
	microProgramList*  prog [mProgSize/2];
	microProgramQuick  quick[mProgSize/2];
	microProgram*      cur;
	int                total;
	int                isSame;
	int                cleared;
	u32                curFrame;
	u8*                x86ptr;   // Code cache write cursor (name kept for compat)
	u8*                x86start; // Start of rec-cache
	u8*                x86end;   // Reset threshold — mVUcacheSafeZone MB below the physical rec-region end
	microRegInfo       lpState;
	MvuLastHit         lastHit;  // VE-02 inline-probe cache — keep adjacent to lpState (one probe base reaches both)
};

// Safe-Zone (megabytes): the gap between prog.x86end (mVUcleanUp's
// "Program cache limit reached" reset threshold) and the physical end of
// the rec region. Emission is only bounds-checked at dispatcher exit, so
// the zone must absorb the worst-case burst between two checks — the
// initial compile session plus every mid-execution JR/JALR mVUcompileJIT
// session. x86 reserves 3 MB for its raw emitter; arm64 emission is fatter
// (fixed-width instructions, literal pools, veneers), so reserve more.
// Unlike x86, the vixl buffer physically ends at the region end, giving a
// hard backstop if a pathological burst exceeds even this.
static const uint mVUcacheSafeZone = 8;

//------------------------------------------------------------------
// Profiler (shared, platform-independent)
//------------------------------------------------------------------
#include "x86/microVU_Profiler.h"

//------------------------------------------------------------------
// Forward declarations for helpers used by regalloc
//------------------------------------------------------------------
void mVUloadReg(const a64::VRegister& reg, const void* ptr, int xyzw);
void mVUloadReg(const a64::VRegister& reg, const a64::Register& base, int64_t off, int xyzw);
void mVUloadMem(const a64::VRegister& reg, const a64::Register& base, int xyzw);
void mVUsaveReg(const a64::VRegister& reg, const void* ptr, int xyzw, bool modXYZW);
void mVUsaveReg(const a64::VRegister& reg, const a64::Register& base, int64_t off, int xyzw, bool modXYZW);
void mVUmergeRegs(const a64::VRegister& dest, const a64::VRegister& src, int xyzw, bool modXYZW = false);
void mVUunpack_xyzw(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int xyzw);

//------------------------------------------------------------------
// ARM64 Register Allocator
//------------------------------------------------------------------
#include "microVU_IR-arm64.h"

//------------------------------------------------------------------
// microVU Main Structure
//------------------------------------------------------------------

struct microVU
{
	alignas(16) u32 statFlag[4];
	alignas(16) u32 macFlag [4];
	alignas(16) u32 clipFlag[4];
	alignas(16) u32 neonCTemp[4];      // Backup used in mVUclamp2()
	alignas(16) u32 neonBackup[32][4]; // Backup for q0~q31

	// Hot JIT-runtime scalars. Kept directly after the flag arrays so
	// emitted code reaches them with a single [gprMVUFlag, #imm] Ldr/Str
	// (see mVUfieldMem below); anything placed after `prog` sits several
	// hundred KB from the x24 pin and falls out of immediate range.
	u32 code;
	u32 divFlag;
	u32 VIbackup;
	u32 VIxgkick;
	u32 branch;
	u32 badBranch;
	u32 evilBranch;
	u32 evilevilBranch;
	u32 p;
	u32 q;
	u32 totalCycles;
	s32 cycles;

	u32 index;
	u32 cop2;
	u32 vuMemSize;
	u32 microMemSize;
	u32 progSize;
	u32 progMemMask;
	u32 cacheSize;

	// Cached hash of every codegen-affecting build-time constexpr + runtime
	// option (clamp modes, FPCRs, speedhacks, gamefixes). Rebuilt at mVUinit /
	// mVUreset and mixed into every program's contentHash so configs that
	// change emit (e.g. flipping a clamp bit) produce a distinct content key.
	XXH128_hash_t optionsSentinel;
	bool          optionsSentinelValid;

	// Owning index keyed by content hash. Every live microProgram is
	// referenced exactly once from here; per-startPC deques (prog.prog[]) and
	// quick slots (prog.quick[]) hold non-owning pointers tracked via
	// microProgram::refcount. Cross-startPC dedup happens by hash lookup:
	// searches that hit this map short-circuit the per-PC deque walk and reuse
	// the existing object across all startPCs whose microMem hashes to the
	// same key. Lives on microVU (not microProgManager) because mVUinit memsets
	// the prog manager — std::unordered_map can't survive that.
	std::unordered_map<XXH128_hash_t, microProgram*, MvuContentHashHash, MvuContentHashEq> mvuContentMap;

	// Ownership parking lot for programs evicted from mvuContentMap after
	// their content drifted from the anchor image (see the drift check in
	// mVUcacheProg). Still referenced (non-owning) by per-PC deques / quick
	// slots until freed alongside the map at mVUreset / mVUclose. Lives on
	// microVU for the same memset reason as the map above.
	std::vector<microProgram*> mvuOrphanedProgs;

	// Monotonic count of micro-memory writes (mVUclear calls). Compared
	// against microProgram::writeGenAtAnchor so mVUcacheProg only pays the
	// whole-image drift hash when a write can actually have occurred since
	// the program's identity was anchored.
	u64 microMemWriteGen;

	microProgManager               prog;
	microProfiler                  profiler;
	std::unique_ptr<microRegAlloc> regAlloc;
	// Persistent vixl MacroAssembler for the per-VU code cache. Constructed
	// once per mVUreset over [prog.x86start, physical rec end); cursor advances
	// across all subsequent block compiles. Avoids per-dispatch MacroAssembler
	// construction/teardown, which is measurable on in-order cores.
	std::unique_ptr<vixl::aarch64::MacroAssembler> jitAsm;
	std::FILE*                     logFile;

	u8* cache;
	u8* startFunct;
	u8* exitFunct;
	u8* startFunctXG;
	u8* exitFunctXG;
	u8* waitMTVU;
	u8* copyPLState;
	// Per-VU SFLAGc + micro_flag tail helpers BL'd by mVUendProgram /
	// mVUsetupBranch emit. See mVUGenerateEndProgramFlagsHelper in
	// microVU-arm64.cpp — each exit thunk's inline shrinks from ~20 insns
	// to one mov+bl pair.
	u8* endProgramFlagsA; // non-Ebit exits (isEbit == 0 || isEbit == 3)
	u8* endProgramFlagsB; // Ebit exits (isEbit && isEbit != 3)
	u8* resumePtrXG;

	// Compile-time only (never read by emitted code): pool GPR index holding
	// the live IBcc condition value between the branch op and condBranch's
	// tail Cmp, or -1. The mVU.branch memory store stays authoritative
	// (bad/evil-branch continuations read it cross-block); the carry just
	// skips the tail's Ldrsh reload. Cleared by anything emitted in between
	// that can clobber a caller-saved pool reg at runtime or recycle the
	// slot at compile time (XGKICK's C call, end-program emission, the
	// divtrace per-op hook).
	int branchCondCarryGpr;

	VURegs& regs() const { return ::vuRegs[index]; }

	__fi REG_VI& getVI(uint reg) const { return regs().VI[reg]; }
	__fi VECTOR& getVF(uint reg) const { return regs().VF[reg]; }
	__fi VIFregisters& getVifRegs() const
	{
		return (index && THREAD_VU1) ? vu1Thread.vifRegs : regs().GetVifRegs();
	}

	// Inline static-NEON 96-byte equality compare. Returns 0 if equal, non-zero
	// otherwise. Layout-locked to microRegInfo (96 B / 16-byte aligned); see
	// static_assert below.
	__fi u32 compareState(microRegInfo* lhs, microRegInfo* rhs) const {
		const u32* a = reinterpret_cast<const u32*>(lhs);
		const u32* b = reinterpret_cast<const u32*>(rhs);
		uint32x4_t c0 = vceqq_u32(vld1q_u32(a +  0), vld1q_u32(b +  0));
		uint32x4_t c1 = vceqq_u32(vld1q_u32(a +  4), vld1q_u32(b +  4));
		if (vminvq_u32(vandq_u32(c0, c1)) == 0)
			return 1;
		uint32x4_t c2 = vceqq_u32(vld1q_u32(a +  8), vld1q_u32(b +  8));
		uint32x4_t c3 = vceqq_u32(vld1q_u32(a + 12), vld1q_u32(b + 12));
		uint32x4_t c4 = vceqq_u32(vld1q_u32(a + 16), vld1q_u32(b + 16));
		uint32x4_t c5 = vceqq_u32(vld1q_u32(a + 20), vld1q_u32(b + 20));
		uint32x4_t a23 = vandq_u32(c2, c3);
		uint32x4_t a45 = vandq_u32(c4, c5);
		return (vminvq_u32(vandq_u32(a23, a45)) == 0) ? 1 : 0;
	}
};

// MemOperand for one of the hot microVU scalars (the code..cycles block
// right after the flag arrays), addressed off the gprMVUFlag (x24) pin the
// same way mVUglobMem rides gprMVUglob. Micro mode only: cop2 macro ops
// emit inline in EE blocks, where x24 holds the EE recompiler's &VU0 pin.
__fi static a64::MemOperand mVUfieldMem(mV, const void* addr)
{
	pxAssert(!mVU.cop2);
	const ptrdiff_t off = reinterpret_cast<const u8*>(addr) - reinterpret_cast<const u8*>(&mVU.macFlag[0]);
	pxAssert(off > 0 && off <= 16380 && (off & 3) == 0);
	return a64::MemOperand(gprMVUFlag, off);
}

// Ldr/Str a hot microVU scalar. In cop2 macro mode x24 is not ours (see
// mVUfieldMem), so shared emit bodies (DIV/SQRT/RSQRT divFlag traffic) fall
// back to the absolute-address path there.
__fi static void mVUldrField(mV, const a64::CPURegister& reg, const void* addr)
{
	if (mVU.cop2)
		armLoadPtr(reg, addr);
	else
		armAsm->Ldr(reg, mVUfieldMem(mVU, addr));
}

__fi static void mVUstrField(mV, const a64::CPURegister& reg, const void* addr)
{
	if (mVU.cop2)
		armStorePtr(reg, addr);
	else
		armAsm->Str(reg, mVUfieldMem(mVU, addr));
}

// Drop the IBcc condition carry (see microVU::branchCondCarryGpr). Called
// from every emission site between the branch op and condBranch's tail that
// either emits a C call (XGKICK, end-program helpers) or hands control to a
// trap handler (divtrace) — after those, the pool temp's value can't be
// trusted, so the tail falls back to the Ldrsh reload.
__fi static void mVUclearBranchCondCarry(mV)
{
	mVU.branchCondCarryGpr = -1;
}

//------------------------------------------------------------------
// Block Manager (from x86/microVU.h — platform-independent)
//------------------------------------------------------------------

class microBlockManager
{
private:
	microBlockLink *qBlockList, *qBlockEnd;
	microBlockLink *fBlockList, *fBlockEnd;
	std::vector<microBlockLinkRef> quickLookup;
	int qListI, fListI;

public:
	inline int getFullListCount() const { return fListI; }
	microBlockManager()
	{
		qListI = fListI = 0;
		qBlockEnd = qBlockList = nullptr;
		fBlockEnd = fBlockList = nullptr;
	}
	~microBlockManager() { reset(); }
	void reset()
	{
		for (microBlockLink* linkI = qBlockList; linkI != nullptr;)
		{
			microBlockLink* freeI = linkI;
			safe_delete_array(linkI->block.jumpCache);
			linkI = linkI->next;
			_aligned_free(freeI);
		}
		for (microBlockLink* linkI = fBlockList; linkI != nullptr;)
		{
			microBlockLink* freeI = linkI;
			safe_delete_array(linkI->block.jumpCache);
			linkI = linkI->next;
			_aligned_free(freeI);
		}
		qListI = fListI = 0;
		qBlockEnd = qBlockList = nullptr;
		fBlockEnd = fBlockList = nullptr;
		quickLookup.clear();
	};
	microBlock* add(microVU& mVU, microBlock* pBlock)
	{
		microBlock* thisBlock = search(mVU, &pBlock->pState);
		if (!thisBlock)
		{
			u8 fullCmp = pBlock->pState.needExactMatch;
			if (fullCmp)
				fListI++;
			else
				qListI++;

			microBlockLink*& blockList = fullCmp ? fBlockList : qBlockList;
			microBlockLink*& blockEnd  = fullCmp ? fBlockEnd  : qBlockEnd;
			microBlockLink*  newBlock  = (microBlockLink*)_aligned_malloc(sizeof(microBlockLink), 32);
			newBlock->block.jumpCache  = nullptr;
			newBlock->next             = nullptr;

			if (blockEnd)
			{
				blockEnd->next = newBlock;
				blockEnd       = newBlock;
			}
			else
			{
				blockEnd = blockList = newBlock;
			}

			std::memcpy(&newBlock->block, pBlock, sizeof(microBlock));
			thisBlock = &newBlock->block;

			quickLookup.push_back({&newBlock->block, pBlock->pState.quick64[0]});
		}
		return thisBlock;
	}
	__ri microBlock* search(microVU& mVU, microRegInfo* pState)
	{
		if (pState->needExactMatch)
		{
			microBlockLink* prevI = nullptr;
			for (microBlockLink* linkI = fBlockList; linkI != nullptr; prevI = linkI, linkI = linkI->next)
			{
				if (mVU.compareState(pState, &linkI->block.pState) == 0)
				{
					if (linkI != fBlockList)
					{
						prevI->next = linkI->next;
						linkI->next = fBlockList;
						fBlockList = linkI;
					}
					return &linkI->block;
				}
			}
		}
		else
		{
			const u64 quick64 = pState->quick64[0];
			for (const microBlockLinkRef& ref : quickLookup)
			{
				if (mVUsFlagHack)
				{
					if ((ref.quick & ~0x0C04) != (quick64 & ~0x0C04)) continue;
				}
				else if (ref.quick != quick64) continue;

				if (doConstProp && (ref.pBlock->pState.vi15 != pState->vi15))  continue;
				if (doConstProp && (ref.pBlock->pState.vi15v != pState->vi15v)) continue;
				return ref.pBlock;
			}
		}
		return nullptr;
	}
	void printInfo(int pc, bool printQuick)
	{
		int listI = printQuick ? qListI : fListI;
		if (listI < 7)
			return;
		microBlockLink* linkI = printQuick ? qBlockList : fBlockList;
		for (int i = 0; i <= listI; i++)
		{
			u32 viCRC = 0, vfCRC = 0, crc = 0, z = sizeof(microRegInfo) / 4;
			for (u32 j = 0; j < 4;  j++) viCRC -= ((u32*)linkI->block.pState.VI)[j];
			for (u32 j = 0; j < 32; j++) vfCRC -= linkI->block.pState.VF[j].x + (linkI->block.pState.VF[j].y << 8) + (linkI->block.pState.VF[j].z << 16) + (linkI->block.pState.VF[j].w << 24);
			for (u32 j = 0; j < z;  j++) crc   -= ((u32*)&linkI->block.pState)[j];
			DevCon.WriteLn(Color_Green,
				"[%04x][Block #%d][crc=%08x][q=%02d][p=%02d][xgkick=%d][vi15=%04x][vi15v=%d][viBackup=%02d]"
				"[flags=%02x][exactMatch=%x][blockType=%d][viCRC=%08x][vfCRC=%08x]",
				pc, i, crc, linkI->block.pState.q,
				linkI->block.pState.p, linkI->block.pState.xgkick, linkI->block.pState.vi15, linkI->block.pState.vi15v,
				linkI->block.pState.viBackUp, linkI->block.pState.flagInfo, linkI->block.pState.needExactMatch,
				linkI->block.pState.blockType, viCRC, vfCRC);
			linkI = linkI->next;
		}
	}
};

//------------------------------------------------------------------
// Globals and Prototypes
//------------------------------------------------------------------

alignas(16) extern microVU microVU0;
alignas(16) extern microVU microVU1;

extern void DumpVUState(u32 n, u32 pc);

extern void mVUclear(mV, u32, u32);
extern void mVUreset(microVU& mVU, bool resetReserve);
extern void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState);
extern void* mVUcompile(microVU& mVU, u32 startPC, uptr pState);
_mVUt extern void* mVUcompileJIT(u32 startPC, uptr ptr);

extern void mVUcleanUpVU0();
extern void mVUcleanUpVU1();
mVUop(mVUopU);
mVUop(mVUopL);

extern void mVUcacheProg(microVU& mVU, microProgram& prog);
extern void mVUdeleteProg(microVU& mVU, microProgram*& prog);
_mVUt extern void* mVUsearchProg(u32 startPC, uptr pState);
extern void* mVUexecuteVU0(u32 startPC, u32 cycles);
extern void* mVUexecuteVU1(u32 startPC, u32 cycles);

// Non-template lookup-only entry points called from the dispatcher's inline
// fast path. Mirror mVUexecute's hot-path body (cycle field set + maskedPC
// + mVUlookupProg) but return nullptr on cache miss instead of falling
// through to compile; the dispatcher then BLs mVUexecuteVUx for the slow
// path.
extern void* mVUlookupProg_VU0(u32 startPC, u32 cycles);
extern void* mVUlookupProg_VU1(u32 startPC, u32 cycles);

// Content-hash plumbing (xxhash3-128 program identity).
//   - mVUbuildOptionsSentinel populates mVU.optionsSentinel from the current
//     codegen-affecting config snapshot. Call at init / reset.
//   - mVUcomputeProgramHash returns the 128-bit content hash for a freshly
//     cached program image. Called from mVUcacheProg.
extern void          mVUbuildOptionsSentinel(microVU& mVU);
extern XXH128_hash_t mVUcomputeProgramHash(microVU& mVU);

typedef void (*mVUrecCall)(u32, u32);
typedef void (*mVUrecCallXG)(void);

// Out-of-line definition — needs complete microVU type and globals
inline void microRegAlloc::writeVIBackup(const a64::Register& reg)
{
	microVU& mVU = (index ? microVU1 : microVU0);
	mVUstrField(mVU, reg.W(), &mVU.VIbackup);
}

template <typename T>
void makeUnique(T& v)
{
	v.erase(unique(v.begin(), v.end()), v.end());
}

template <typename T>
void sortVector(T& v)
{
	sort(v.begin(), v.end());
}

// Block entry-point lookup — if not found, compile directly (do NOT call mVUblockFetch — that causes infinite recursion)
__fi void* mVUentryGet(microVU& mVU, microBlockManager* block, u32 startPC, uptr pState)
{
	microBlock* pBlock = block->search(mVU, (microRegInfo*)pState);
	if (pBlock)
		return pBlock->hostEntry;
	return mVUcompile(mVU, startPC, pState);
}

//------------------------------------------------------------------
// ARM64 helper .inl files + shared analysis/tables
//------------------------------------------------------------------
#include "microVU_Clamp-arm64.inl"
#include "microVU_Misc-arm64.inl"
#include "microVU_Alloc-arm64.inl"
#include "microVU_Flags-arm64.inl"
#include "x86/microVU_Analyze.inl"
// Forward declarations for stubs defined in Branch .inl but referenced by Lower .inl
static void mVUdivSet(mV);
static void mVU_XGKICK_DELAY(mV);
static void mVU_XGKICK_SYNC(mV, bool);

#include "microVU_Upper-arm64.inl"
#include "microVU_Lower-arm64.inl"
#include "x86/microVU_Tables.inl"
#include "microVU_Branch-arm64.inl"
#include "microVU_Compile-arm64.inl"
