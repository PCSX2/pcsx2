// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Implementation of microVU_Persist-arm64.h. Included at the bottom of
// microVU-arm64.cpp (same pattern as microVU_ProgCache-arm64.inl) so it sees
// the full microVU/microProgram/microBlock types plus the TU-local helpers
// (mVUopenCodeCache / mVUcloseCodeCache / mVUcreateProg / mVUcomputeProgramHash).

#include "arm64/microVU_Persist-arm64.h"
#include "Memory.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace mVUPersist
{
	enum : u8
	{
		kFixSelfBlockAbs = 0, // movz+movk×3 → &log.blocks[blockIndex] + fieldOffset
		kFixBlockEntryRel26,  // B/BL → entry of log.blocks[blockIndex]
		kFixStubRel26,        // B/BL → per-VU dispatcher stub `stubId`
		kFixAdrpPage21,       // ADRP → run-invariant absolute `target`
	};

	enum : u8
	{
		kStubStartFunct = 0,
		kStubExitFunct,
		kStubStartFunctXG,
		kStubExitFunctXG,
		kStubWaitMTVU,
		kStubCopyPLState,
		kStubEndFlagsA,
		kStubEndFlagsB,
		kStubResumeXG,
		kStubCount,
	};

	struct PersistFixup
	{
		u32 codeOffset; // chunk-relative offset of the first patched insn
		u8 kind;
		u8 stubId;
		u16 fieldOffset; // kFixSelfBlockAbs: byte offset within microBlock
		u32 blockIndex; // kFixSelfBlockAbs / kFixBlockEntryRel26
		u32 _pad;
		u64 target; // kFixAdrpPage21: absolute target; debug aid otherwise
	};
	static_assert(sizeof(PersistFixup) == 24);

	struct PersistChunk
	{
		std::vector<u8> code;
		std::vector<PersistFixup> fixups;
	};

	struct PersistBlockRec
	{
		u32 chunkIndex;
		u32 entryOffset; // within chunk
		u32 startPC; // microMem byte offset
		u32 hasJumpCache;
		microRegInfo pState;
		microRegInfo pStateEnd;
		microBlock* live; // manager's copy; not serialized
	};
} // namespace mVUPersist

// Global-scope definition (fwd-declared in the header so microProgram can
// hold a pointer without pulling the implementation types).
struct MvuPersistLog
{
	std::vector<mVUPersist::PersistChunk> chunks;
	std::vector<mVUPersist::PersistBlockRec> blocks;
	// hostEntry → blocks[] index, for cross-chunk branch resolution. Keyed on
	// the manager's hostEntry (what armEmitJmp call sites actually target).
	std::unordered_map<const void*, u32> blockByEntry;
};

namespace mVUPersist
{
	static bool s_recordingEnabled = false;
	// When true, mVUinit/mVUreset's SyncRecordingFromConfig is a no-op so the
	// unit-test harness keeps manual control of recording (ABI-digest pins,
	// in-process round-trips, disk round-trips). Default false = production,
	// where recording follows EnableVUProgramCache at every reset.
	static bool s_manualRecording = false;
	static u64 s_blockCompiles[2] = {};
	static Stats s_stats[2] = {};

	// Process-wide determinism kill switch for the offline tools (pcsx2-vurunner
	// --no-progcache). When set, both the on-disk program cache and emit-time
	// recording are forced off for the whole process, so a JIT-vs-interp diff run
	// is byte-reproducible. Replaces the former PCSX2_VU_PROGCACHE_DISABLE env var
	// — no env gate in production code. Default false = no effect.
	static bool s_processDisabled = false;

	//------------------------------------------------------------------
	// Address classification
	//------------------------------------------------------------------

	// Run-invariant static ranges under the deterministic layout:
	// the executable image (incl. bss) and the fixed-base data arena. The
	// code arena is deliberately NOT here — slab addresses are exactly the
	// relocatable class and must resolve to stubs/blocks or fail.
	struct InvariantRanges
	{
		uptr imgBegin = 0, imgEnd = 0;
		uptr dataBegin = 0, dataEnd = 0;
		bool valid = false;
	};

	static bool ResolveImageRange(uptr& out_begin, uptr& out_end)
	{
#ifdef __linux__
		char exe_path[512];
		const ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if (exe_len <= 0)
			return false;
		exe_path[exe_len] = 0;

		std::FILE* maps = std::fopen("/proc/self/maps", "r");
		if (!maps)
			return false;

		uptr begin = 0, end = 0;
		bool extended = false;
		char line[1024];
		while (std::fgets(line, sizeof(line), maps))
		{
			unsigned long long m_begin = 0, m_end = 0;
			char perms[8] = {};
			unsigned long long offset = 0;
			unsigned dev_major = 0, dev_minor = 0;
			unsigned long long inode = 0;
			int path_pos = -1;
			if (std::sscanf(line, "%llx-%llx %7s %llx %x:%x %llu %n",
					&m_begin, &m_end, perms, &offset, &dev_major, &dev_minor, &inode, &path_pos) < 7)
				continue;
			const char* path = (path_pos >= 0) ? line + path_pos : "";
			// Trim trailing newline for the comparison.
			std::string_view pv(path);
			while (!pv.empty() && (pv.back() == '\n' || pv.back() == ' '))
				pv.remove_suffix(1);

			if (pv == exe_path)
			{
				if (!begin)
					begin = static_cast<uptr>(m_begin);
				end = static_cast<uptr>(m_end);
				// A fresh file segment: the bss following the *final* file
				// segment is the only anonymous region we want to absorb, so
				// re-arm the one-shot extension on every file mapping.
				extended = false;
			}
			else if (!extended && end && static_cast<uptr>(m_begin) == end && pv.empty())
			{
				// Anonymous mapping contiguous with the image's last file
				// mapping: the bss. Extend exactly once — a run of contiguous
				// unnamed mappings (glibc arena, ld scratch) must NOT chain the
				// range past true bss. ([heap]/named regions also stop here.)
				end = static_cast<uptr>(m_end);
				extended = true;
			}
		}
		std::fclose(maps);

		if (!begin || end <= begin)
			return false;
		out_begin = begin;
		out_end = end;
		return true;
#else
		return false;
#endif
	}

	static const InvariantRanges& GetInvariantRanges()
	{
		static InvariantRanges r = []() {
			InvariantRanges out;
			if (!ResolveImageRange(out.imgBegin, out.imgEnd))
			{
				DevCon.Warning("mVUPersist: could not resolve executable image range — recording will mark all programs non-persistable");
				return out;
			}
			// Sanity: a known bss global must land inside the resolved image
			// range; if it doesn't, the maps parse is wrong and trusting it
			// would misclassify heap pointers as invariant (silent corruption).
			const uptr probe = reinterpret_cast<uptr>(&microVU0);
			if (probe < out.imgBegin || probe >= out.imgEnd)
			{
				DevCon.Warning("mVUPersist: image-range sanity probe failed (&microVU0=%p not in [%p,%p)) — recording disabled",
					&microVU0, (void*)out.imgBegin, (void*)out.imgEnd);
				return out;
			}
			out.dataBegin = reinterpret_cast<uptr>(SysMemory::GetDataPtr(0));
			out.dataEnd = out.dataBegin + HostMemoryMap::MainSize;
			out.valid = true;
			return out;
		}();
		return r;
	}

	static bool IsInvariantAddress(const void* addr)
	{
		const InvariantRanges& r = GetInvariantRanges();
		if (!r.valid)
			return false;
		const uptr a = reinterpret_cast<uptr>(addr);
		return (a >= r.imgBegin && a < r.imgEnd) || (a >= r.dataBegin && a < r.dataEnd);
	}

	//------------------------------------------------------------------
	// Dispatcher stub table
	//------------------------------------------------------------------

	static const void* StubAddress(microVU& mVU, u8 id)
	{
		switch (id)
		{
			case kStubStartFunct:   return mVU.startFunct;
			case kStubExitFunct:    return mVU.exitFunct;
			case kStubStartFunctXG: return mVU.startFunctXG;
			case kStubExitFunctXG:  return mVU.exitFunctXG;
			case kStubWaitMTVU:     return mVU.waitMTVU;
			case kStubCopyPLState:  return mVU.copyPLState;
			case kStubEndFlagsA:    return mVU.endProgramFlagsA;
			case kStubEndFlagsB:    return mVU.endProgramFlagsB;
			case kStubResumeXG:     return mVU.resumePtrXG;
			default:                return nullptr;
		}
	}

	static bool ResolveStub(microVU& mVU, const void* target, u8& out_id)
	{
		for (u8 i = 0; i < kStubCount; i++)
		{
			if (StubAddress(mVU, i) == target && target)
			{
				out_id = i;
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------
	// Emit-time recorder
	//------------------------------------------------------------------

	class Recorder final : public ArmAddressRecorder
	{
	public:
		struct EpisodeBlock
		{
			microBlock* live;
			u32 entryOffset;
			u32 startPC;
		};

		microVU* mvu = nullptr;
		u8* chunkBase = nullptr;
		microProgram* prog = nullptr;
		MvuPersistLog* log = nullptr;
		std::vector<PersistFixup> fixups;
		std::vector<EpisodeBlock> episodeBlocks;
		bool active = false;
		bool failed = false;
		const char* failReason = nullptr;

		void Begin(microVU& m, u8* base)
		{
			pxAssert(!active);
			mvu = &m;
			chunkBase = base;
			prog = nullptr;
			log = nullptr;
			fixups.clear();
			episodeBlocks.clear();
			active = true;
			failed = false;
			failReason = nullptr;
		}

		void Fail(const char* reason)
		{
			if (!failed)
			{
				failed = true;
				failReason = reason;
			}
		}

		bool InSlab(const void* addr) const
		{
			const u8* a = static_cast<const u8*>(addr);
			return a >= mvu->cache && a < mvu->prog.x86end;
		}

		bool InChunk(const void* addr) const
		{
			const u8* a = static_cast<const u8*>(addr);
			return a >= chunkBase && a < armGetCurrentCodePointer();
		}

		u32 OffsetOf(u8* at) const
		{
			pxAssert(at >= chunkBase);
			return static_cast<u32>(at - chunkBase);
		}

		// Index a block of this episode will get once committed.
		u32 ProvisionalIndex(size_t episode_pos) const
		{
			return static_cast<u32>((log ? log->blocks.size() : 0) + episode_pos);
		}

		// --- ArmAddressRecorder ---

		MoveForm ClassifyMove(const void* addr) override
		{
			const u8* a = static_cast<const u8*>(addr);
			for (const EpisodeBlock& eb : episodeBlocks)
			{
				const u8* blk = reinterpret_cast<const u8*>(eb.live);
				if (a >= blk && a < blk + sizeof(microBlock))
					return MoveForm::CanonicalAbs;
			}
			if (IsInvariantAddress(addr))
				return MoveForm::Default;
			if (InSlab(addr))
				Fail("address-of-code materialized outside branch helpers");
			else
				Fail("unclassifiable pointer materialized (heap?)");
			return MoveForm::Default;
		}

		void OnCanonicalAbsMove(u8* at, const void* addr) override
		{
			const u8* a = static_cast<const u8*>(addr);
			for (size_t i = 0; i < episodeBlocks.size(); i++)
			{
				const u8* blk = reinterpret_cast<const u8*>(episodeBlocks[i].live);
				if (a >= blk && a < blk + sizeof(microBlock))
				{
					PersistFixup f = {};
					f.codeOffset = OffsetOf(at);
					f.kind = kFixSelfBlockAbs;
					f.fieldOffset = static_cast<u16>(a - blk);
					f.blockIndex = ProvisionalIndex(i);
					f.target = reinterpret_cast<u64>(addr);
					fixups.push_back(f);
					return;
				}
			}
			Fail("canonical move target lost between classify and emit");
		}

		void OnAdrp(u8* at, const void* addr) override
		{
			if (IsInvariantAddress(addr))
			{
				PersistFixup f = {};
				f.codeOffset = OffsetOf(at);
				f.kind = kFixAdrpPage21;
				f.target = reinterpret_cast<u64>(addr);
				fixups.push_back(f);
				return;
			}
			Fail("ADRP to non-invariant target");
		}

		void OnDirectBranch(u8* at, const void* target, bool is_call) override
		{
			if (InChunk(target))
				return; // relative distance preserved on relocation

			u8 stub_id = 0;
			if (ResolveStub(*mvu, target, stub_id))
			{
				PersistFixup f = {};
				f.codeOffset = OffsetOf(at);
				f.kind = kFixStubRel26;
				f.stubId = stub_id;
				f.target = reinterpret_cast<u64>(target);
				fixups.push_back(f);
				return;
			}

			if (log)
			{
				const auto it = log->blockByEntry.find(target);
				if (it != log->blockByEntry.end())
				{
					PersistFixup f = {};
					f.codeOffset = OffsetOf(at);
					f.kind = kFixBlockEntryRel26;
					f.blockIndex = it->second;
					f.target = reinterpret_cast<u64>(target);
					fixups.push_back(f);
					return;
				}
			}

			Fail(InSlab(target) ? "direct branch to unrecorded slab target"
			                    : "direct branch to non-slab target");
		}

		bool WantsLongCondBranch(const void* target) override
		{
			// Intra-chunk: short form relocates fine. Cross-chunk: needs the
			// imm26 reach of a plain B so the patcher can retarget it.
			return !InChunk(target) && InSlab(target);
		}

		void OnAbsoluteTarget(const void* target) override
		{
			if (IsInvariantAddress(target))
				return;
			Fail(InSlab(target) ? "absolute slab address baked"
			                    : "absolute unclassifiable address baked");
		}

		// --- episode lifecycle ---

		void RegisterBlock(microBlock* block, u8* entry, u32 startPC_bytes)
		{
			if (failed)
				return;

			microProgram* cur = mvu->prog.cur;
			if (!cur)
				return;
			if (!prog)
			{
				prog = cur;
				if (!prog->persist)
					prog->persist = new MvuPersistLog();
				log = prog->persist;
			}
			else if (prog != cur)
			{
				// mVUsearchProg switched programs mid-episode (JR/JALR slow
				// path). Splitting the chunk at the switch is possible but
				// not worth it — drop the episode.
				Fail("program switch mid-episode");
				return;
			}

			episodeBlocks.push_back({block, OffsetOf(entry), startPC_bytes});
		}

		void End(u8* chunkEnd)
		{
			pxAssert(active);
			active = false;

			const u32 vu = mvu->index & 1;
			if (episodeBlocks.empty() && fixups.empty())
				return; // nothing emitted (pure lookup episode / hydration)

			if (failed || episodeBlocks.empty() || !log)
			{
				s_stats[vu].chunksDropped++;
				if (failed)
					DevCon.WriteLn("mVUPersist: VU%u chunk dropped (%s)", vu, failReason);
				return;
			}

			// add() may have deduped a block against an existing variant (the
			// manager then keeps the OLD entry and this episode's copy of the
			// code is unreachable). Skipping just that record would shift the
			// provisional indices already baked into SelfBlockAbs fixups —
			// drop the whole episode instead. Rare corner, conservative.
			for (const EpisodeBlock& eb : episodeBlocks)
			{
				if (eb.live->hostEntry != chunkBase + eb.entryOffset)
				{
					s_stats[vu].chunksDropped++;
					DevCon.WriteLn("mVUPersist: VU%u chunk dropped (block manager deduped a variant)", vu);
					return;
				}
			}

			const u32 chunkIndex = static_cast<u32>(log->chunks.size());
			for (const EpisodeBlock& eb : episodeBlocks)
			{
				PersistBlockRec rec = {};
				rec.chunkIndex = chunkIndex;
				rec.entryOffset = eb.entryOffset;
				rec.startPC = eb.startPC;
				rec.hasJumpCache = (eb.live->jumpCache != nullptr) ? 1 : 0;
				std::memcpy(&rec.pState, &eb.live->pState, sizeof(microRegInfo));
				std::memcpy(&rec.pStateEnd, &eb.live->pStateEnd, sizeof(microRegInfo));
				rec.live = eb.live;
				log->blockByEntry.emplace(eb.live->hostEntry, static_cast<u32>(log->blocks.size()));
				log->blocks.push_back(rec);
				s_stats[vu].blocksRecorded++;
			}

			PersistChunk chunk;
			chunk.code.assign(chunkBase, chunkEnd);
			chunk.fixups = std::move(fixups);
			s_stats[vu].fixupsRecorded += chunk.fixups.size();
			log->chunks.push_back(std::move(chunk));
			s_stats[vu].chunksRecorded++;
		}
	};

	// VU0 episodes run on the EE thread, VU1 on the MTVU thread — same
	// affinity as armAsm itself, so per-VU recorder objects are safe.
	static Recorder s_recorder[2];

	void SetRecordingEnabled(bool enabled)
	{
		// The offline-tooling determinism gate trumps every enable request:
		// recording changes emitted code forms, and SetProcessDisable(true)
		// (pcsx2-vurunner --no-progcache) promises a byte-reproducible JIT.
		if (enabled && s_processDisabled)
			enabled = false;
		if (enabled == s_recordingEnabled)
			return;
		s_recordingEnabled = enabled;

		// Recording state is part of program identity (canonical movs /
		// forced-long cond branches change the bytes a program compiles to),
		// so flipping it invalidates both VUs' options sentinels. The next
		// consumer — mVUcomputeProgramHash at dispatch, mVUbuildOptionsSentinel
		// at init/reset, the ProgCache VERSION handshake — rebuilds with the
		// new state mixed in. Benign race: an MTVU compile between this store
		// and the recompiler reset that follows a config toggle hashes against
		// the new sentinel inside a cache dir keyed to the old one; the entry
		// is unreachable and the dir is evicted on the next handshake.
		microVU0.optionsSentinelValid = false;
		microVU1.optionsSentinelValid = false;
	}
	bool IsRecordingEnabled() { return s_recordingEnabled; }

	void SetProcessDisable(bool disable) { s_processDisabled = disable; }
	bool IsProcessDisabled() { return s_processDisabled; }

	void SyncRecordingFromConfig(bool config_enabled)
	{
		// Production: recording follows EnableVUProgramCache, called from
		// mVUinit/mVUreset before the options sentinel is rebuilt. The
		// test-manual override lets the recompiler-test harness drive
		// recording itself without these reset-time syncs clobbering it.
		if (s_manualRecording)
			return;
		const bool was = s_recordingEnabled;
		SetRecordingEnabled(config_enabled);
		if (s_recordingEnabled != was)
		{
			// One line per state change — the authoritative signal that the
			// persisted-JIT cache will (or won't) write .vuprog payloads this
			// session. Cheap to grep in emulog when diagnosing a "no payloads"
			// report. DevCon so it stays out of production logs but remains
			// greppable in Devel / raised-loglevel during bring-up.
			DevCon.WriteLn(Color_StrongBlue,
				"mVUProgCache: persisted-JIT recording %s (VuJitProgramCache=%d)",
				s_recordingEnabled ? "ENABLED" : "disabled",
				config_enabled ? 1 : 0);
		}
	}

	void SetTestManualRecording(bool manual) { s_manualRecording = manual; }

	void BeginEpisode(microVU& mVU, u8* chunkBase)
	{
		if (!s_recordingEnabled)
			return;
		Recorder& r = s_recorder[mVU.index & 1];
		r.Begin(mVU, chunkBase);
		armAddressRecorder = &r;
	}

	void EndEpisode(microVU& mVU, u8* chunkEnd)
	{
		Recorder& r = s_recorder[mVU.index & 1];
		if (!r.active)
			return;
		pxAssert(armAddressRecorder == &r);
		armAddressRecorder = nullptr;
		r.End(chunkEnd);
	}

	void OnBlockCompiled(microVU& mVU, microBlock* block, u8* entry, u32 startPC_bytes)
	{
		const u32 vu = mVU.index & 1;
		s_blockCompiles[vu]++;
		Recorder& r = s_recorder[vu];
		if (r.active && armAddressRecorder == &r)
			r.RegisterBlock(block, entry, startPC_bytes);
	}

	void OnProgramDeleted(microProgram& prog)
	{
		delete prog.persist;
		prog.persist = nullptr;
	}

	//------------------------------------------------------------------
	// Serialization
	//------------------------------------------------------------------

	static constexpr u32 kImageMagic = 0x5055564Du; // 'MVUP'
	static constexpr u32 kImageVersion = 1;

	struct ImageHeader
	{
		u32 magic;
		u32 version;
		u32 vuIndex;
		u32 flags;
		u64 hashLo;
		u64 hashHi;
		u64 imageAnchor; // &microVU0 — any bss global; fixed under non-PIE
		u64 dataArena; // SysMemory::GetDataPtr(0)
		u64 codeArena; // SysMemory::GetCodePtr(0)
		u32 progStartPC; // microMem byte offset of the creating entry
		u32 rangeCount;
		u32 blockCount;
		u32 chunkCount;
		u64 payloadHash; // XXH3-64 of every byte after this header — guards
		                 // the on-disk .vuprog against bit rot / partial
		                 // writes the structural checks can't see (a flipped
		                 // CODE byte is valid structure but wrong code)
		u32 reserved[2];
	};
	static_assert(sizeof(ImageHeader) == 88);

	struct DiskRange
	{
		s32 start;
		s32 end;
	};

	struct DiskBlock
	{
		u32 chunkIndex;
		u32 entryOffset;
		u32 startPC;
		u32 hasJumpCache;
		u8 pState[sizeof(microRegInfo)];
		u8 pStateEnd[sizeof(microRegInfo)];
	};

	struct DiskChunkHeader
	{
		u32 codeSize;
		u32 fixupCount;
	};

	template <typename T>
	static void AppendPod(std::vector<u8>& out, const T& v)
	{
		const u8* p = reinterpret_cast<const u8*>(&v);
		out.insert(out.end(), p, p + sizeof(T));
	}

	bool SerializeProgram(microVU& mVU, const microProgram& prog, std::vector<u8>& out)
	{
		const MvuPersistLog* log = prog.persist;
		if (!log || log->blocks.empty() || !prog.contentHashValid)
			return false;

		out.clear();
		ImageHeader hdr = {};
		hdr.magic = kImageMagic;
		hdr.version = kImageVersion;
		hdr.vuIndex = mVU.index;
		hdr.hashLo = prog.contentHash.low64;
		hdr.hashHi = prog.contentHash.high64;
		hdr.imageAnchor = reinterpret_cast<u64>(&microVU0);
		hdr.dataArena = reinterpret_cast<u64>(SysMemory::GetDataPtr(0));
		hdr.codeArena = reinterpret_cast<u64>(SysMemory::GetCodePtr(0));
		hdr.progStartPC = static_cast<u32>(prog.startPC) * 8u;
		hdr.rangeCount = prog.ranges ? static_cast<u32>(prog.ranges->size()) : 0;
		hdr.blockCount = static_cast<u32>(log->blocks.size());
		hdr.chunkCount = static_cast<u32>(log->chunks.size());
		AppendPod(out, hdr);

		if (prog.ranges)
		{
			for (const microRange& mr : *prog.ranges)
				AppendPod(out, DiskRange{mr.start, mr.end});
		}

		for (const PersistBlockRec& rec : log->blocks)
		{
			DiskBlock db = {};
			db.chunkIndex = rec.chunkIndex;
			db.entryOffset = rec.entryOffset;
			db.startPC = rec.startPC;
			db.hasJumpCache = rec.hasJumpCache;
			std::memcpy(db.pState, &rec.pState, sizeof(microRegInfo));
			std::memcpy(db.pStateEnd, &rec.pStateEnd, sizeof(microRegInfo));
			AppendPod(out, db);
		}

		for (const PersistChunk& chunk : log->chunks)
		{
			AppendPod(out, DiskChunkHeader{static_cast<u32>(chunk.code.size()),
				static_cast<u32>(chunk.fixups.size())});
			for (const PersistFixup& f : chunk.fixups)
				AppendPod(out, f);
			out.insert(out.end(), chunk.code.begin(), chunk.code.end());
		}

		// Stamp the payload checksum into the already-appended header.
		const u64 payload_hash = XXH3_64bits(
			out.data() + sizeof(ImageHeader), out.size() - sizeof(ImageHeader));
		std::memcpy(out.data() + offsetof(ImageHeader, payloadHash),
			&payload_hash, sizeof(payload_hash));
		return true;
	}

	//------------------------------------------------------------------
	// Instruction patchers
	//------------------------------------------------------------------

	static bool PatchAbsMov(u32* insn, u64 value)
	{
		// movz xN, #imm16          (hw=0) — 0xD2800000
		// movk xN, #imm16, lsl #s  (hw=s/16) — 0xF2800000
		static constexpr u32 kOpMask = 0xFF800000u;
		static constexpr u32 kMovz = 0xD2800000u;
		static constexpr u32 kMovk = 0xF2800000u;
		for (int i = 0; i < 4; i++)
		{
			const u32 expect_op = (i == 0) ? kMovz : kMovk;
			const u32 expect_hw = static_cast<u32>(i) << 21;
			if ((insn[i] & kOpMask) != expect_op || (insn[i] & 0x00600000u) != expect_hw)
				return false;
			const u32 imm16 = static_cast<u32>((value >> (16 * i)) & 0xFFFFu);
			insn[i] = (insn[i] & ~(0xFFFFu << 5)) | (imm16 << 5);
		}
		return true;
	}

	static bool PatchRel26(u32* insn, const u8* at, const void* target)
	{
		// B = 0x14000000, BL = 0x94000000 — bit 31 selects, imm26 in [25:0].
		if ((*insn & 0x7C000000u) != 0x14000000u)
			return false;
		const s64 disp = GetPCDisplacement(at, target);
		if (!vixl::IsInt26(disp))
			return false;
		*insn = (*insn & 0xFC000000u) | (static_cast<u32>(disp) & 0x03FFFFFFu);
		return true;
	}

	static bool PatchAdrpPage21(u32* insn, const u8* at, u64 target)
	{
		// ADRP = 0x90000000 | immlo[30:29] | immhi[23:5].
		if ((*insn & 0x9F000000u) != 0x90000000u)
			return false;
		const s64 page_disp = static_cast<s64>(target >> 12) -
		                      static_cast<s64>(reinterpret_cast<uptr>(at) >> 12);
		if (!vixl::IsInt21(page_disp))
			return false;
		const u32 immlo = static_cast<u32>(page_disp) & 3u;
		const u32 immhi = (static_cast<u32>(page_disp) >> 2) & 0x7FFFFu;
		*insn = (*insn & 0x9F00001Fu) | (immlo << 29) | (immhi << 5);
		return true;
	}

	//------------------------------------------------------------------
	// Hydration
	//------------------------------------------------------------------

#ifdef PCSX2_RECOMPILER_TESTS
	// Placement of the most recent hydration, for TestVerifyHydratedCode.
	// Test-only state — never written in a hooks-off Release build.
	static std::vector<u8*> s_lastHydratedChunkBase[2];
#endif

	template <typename T>
	static bool ReadPod(const u8*& p, const u8* end, T& out)
	{
		if (static_cast<size_t>(end - p) < sizeof(T))
			return false;
		std::memcpy(&out, p, sizeof(T));
		p += sizeof(T);
		return true;
	}

	microProgram* HydrateProgram(microVU& mVU, const u8* data, size_t size)
	{
		const u32 vu = mVU.index & 1;
		const u8* p = data;
		const u8* const end = data + size;

		ImageHeader hdr;
		if (!ReadPod(p, end, hdr) || hdr.magic != kImageMagic || hdr.version != kImageVersion ||
			hdr.vuIndex != mVU.index)
		{
			s_stats[vu].hydrationRejects++;
			return nullptr;
		}

		// Payload integrity. The structural checks below validate shape, not
		// content — a flipped code byte parses fine and runs wrong.
		if (XXH3_64bits(p, static_cast<size_t>(end - p)) != hdr.payloadHash)
		{
			DevCon.Warning("mVUPersist: VU%u hydration rejected — payload checksum mismatch", vu);
			s_stats[vu].hydrationRejects++;
			return nullptr;
		}

		// Layout-base verification: every unrecorded address class in the
		// chunks assumes these. A PIE dev build / layout drift fails here and
		// falls back to recompile (degrade, never corrupt).
		if (hdr.imageAnchor != reinterpret_cast<u64>(&microVU0) ||
			hdr.dataArena != reinterpret_cast<u64>(SysMemory::GetDataPtr(0)) ||
			hdr.codeArena != reinterpret_cast<u64>(SysMemory::GetCodePtr(0)))
		{
			DevCon.Warning("mVUPersist: VU%u hydration rejected — layout bases differ", vu);
			s_stats[vu].hydrationRejects++;
			return nullptr;
		}

		// Identity: the image must describe the program bytes currently in
		// micro memory.
		const XXH128_hash_t live_hash = mVUcomputeProgramHash(mVU);
		if (hdr.hashLo != live_hash.low64 || hdr.hashHi != live_hash.high64)
		{
			s_stats[vu].hydrationRejects++;
			return nullptr;
		}

		std::vector<DiskRange> ranges(hdr.rangeCount);
		for (DiskRange& r : ranges)
			if (!ReadPod(p, end, r))
				return nullptr;

		std::vector<DiskBlock> blocks(hdr.blockCount);
		for (DiskBlock& b : blocks)
			if (!ReadPod(p, end, b))
				return nullptr;

		struct ParsedChunk
		{
			const u8* code;
			u32 codeSize;
			std::vector<PersistFixup> fixups;
		};
		std::vector<ParsedChunk> chunks(hdr.chunkCount);
		size_t total_code = 0;
		for (ParsedChunk& c : chunks)
		{
			DiskChunkHeader ch;
			if (!ReadPod(p, end, ch))
				return nullptr;
			c.fixups.resize(ch.fixupCount);
			for (PersistFixup& f : c.fixups)
				if (!ReadPod(p, end, f))
					return nullptr;
			if (static_cast<size_t>(end - p) < ch.codeSize)
				return nullptr;
			c.code = p;
			c.codeSize = ch.codeSize;
			p += ch.codeSize;
			total_code += ch.codeSize + 16;
		}

		// Structural validation before any side effects.
		for (const DiskBlock& b : blocks)
		{
			if (b.chunkIndex >= chunks.size() || b.startPC >= mVU.microMemSize ||
				(b.startPC & 7) || b.entryOffset >= chunks[b.chunkIndex].codeSize)
				return nullptr;
		}
		for (const ParsedChunk& c : chunks)
		{
			for (const PersistFixup& f : c.fixups)
			{
				const u32 span = (f.kind == kFixSelfBlockAbs) ? 16 : 4;
				if (f.codeOffset + span > c.codeSize)
					return nullptr;
				if ((f.kind == kFixSelfBlockAbs || f.kind == kFixBlockEntryRel26) &&
					f.blockIndex >= blocks.size())
					return nullptr;
				if (f.kind == kFixStubRel26 && f.stubId >= kStubCount)
					return nullptr;
				if (f.kind > kFixAdrpPage21)
					return nullptr;
			}
		}

		// Capacity: refuse rather than trip the mid-hydration cache-full reset.
		const size_t cache_free = static_cast<size_t>(mVU.prog.x86end - mVU.prog.x86ptr);
		if (total_code + (mVUcacheSafeZone * _1mb) > cache_free)
		{
			s_stats[vu].hydrationRejects++;
			return nullptr;
		}

		pxAssert(!armAsm); // caller must not have an open emission episode
		mVUopenCodeCache(mVU);

		microProgram* prog = mVUcreateProg(mVU, static_cast<int>(hdr.progStartPC / 8));

		// Restore compiled ranges + the range-compare image (mVUcmpProg under
		// !doWholeProgCompare diffs prog.data inside these windows; the live
		// microMem is identity-verified above, so copying from it is exact).
		prog->ranges->clear();
		for (const DiskRange& r : ranges)
		{
			prog->ranges->push_back(microRange{r.start, r.end});
			if (!doWholeProgCompare && r.start >= 0 && r.end > r.start &&
				static_cast<u32>(r.end) <= mVU.microMemSize)
			{
				std::memcpy(reinterpret_cast<u8*>(prog->data) + r.start,
					mVU.regs().Micro + r.start, static_cast<size_t>(r.end - r.start));
			}
		}

		// Copy each chunk to the current cursor (16-aligned, as recorded).
		std::vector<u8*> chunk_base(chunks.size());
		for (size_t i = 0; i < chunks.size(); i++)
		{
			while (armAsm->GetCursorOffset() & 15)
				armAsm->Nop();
			chunk_base[i] = mVU.prog.x86start + armAsm->GetCursorOffset();
			{
				vixl::CodeBufferCheckScope scope(armAsm, chunks[i].codeSize,
					vixl::CodeBufferCheckScope::kReserveBufferSpace,
					vixl::CodeBufferCheckScope::kNoAssert);
				armAsm->GetBuffer()->EmitData(chunks[i].code, chunks[i].codeSize);
			}
		}

		// Register every block with the program's managers. add() dedupes by
		// pState, mirroring compile-time behavior.
		std::vector<microBlock*> final_block(blocks.size());
		for (size_t i = 0; i < blocks.size(); i++)
		{
			const DiskBlock& b = blocks[i];
			const u32 slot = b.startPC / 8;
			if (!prog->block[slot])
				prog->block[slot] = new microBlockManager();

			microBlock tmp;
			std::memcpy(&tmp.pState, b.pState, sizeof(microRegInfo));
			std::memcpy(&tmp.pStateEnd, b.pStateEnd, sizeof(microRegInfo));
			tmp.x86ptrStart = chunk_base[b.chunkIndex] + b.entryOffset;
			tmp.hostEntry = tmp.x86ptrStart;
			tmp.jumpCache = nullptr;

			microBlock* installed = prog->block[slot]->add(mVU, &tmp);
			// mVUcompileJIT dereferences jumpCache unguarded (it was allocated
			// at compile time by normJumpCompile) — restore it eagerly.
			if (b.hasJumpCache && !installed->jumpCache)
				installed->jumpCache = new microJumpCache[mProgSize / 2];
			final_block[i] = installed;
		}

		// Patch fixups directly in the slab (still inside the BeginCodeWrite
		// window; mVUcloseCodeCache flushes the icache over the whole span).
		for (size_t i = 0; i < chunks.size(); i++)
		{
			for (const PersistFixup& f : chunks[i].fixups)
			{
				u8* at = chunk_base[i] + f.codeOffset;
				u32* insn = reinterpret_cast<u32*>(at);
				bool ok = false;
				switch (f.kind)
				{
					case kFixSelfBlockAbs:
						ok = PatchAbsMov(insn,
							reinterpret_cast<u64>(final_block[f.blockIndex]) + f.fieldOffset);
						break;
					case kFixBlockEntryRel26:
						ok = PatchRel26(insn, at, final_block[f.blockIndex]->hostEntry);
						break;
					case kFixStubRel26:
						ok = PatchRel26(insn, at, StubAddress(mVU, f.stubId));
						break;
					case kFixAdrpPage21:
						ok = PatchAdrpPage21(insn, at, f.target);
						break;
				}
				if (!ok)
				{
					// Encoding mismatch — the image doesn't describe this
					// build's emitter output. Abandon: unregister nothing
					// (blocks point at fully-written code; the only unsound
					// state would be a half-patched chunk), so fail hard
					// before any block can run.
					pxFailRel("mVUPersist: fixup patch failed — serialized image inconsistent with emitter");
					mVUcloseCodeCache(mVU);
					return nullptr;
				}
			}
		}

		mVUcloseCodeCache(mVU);

		// Rebuild the persist log so the hydrated program can grow new chunks
		// and be re-serialized.
		delete prog->persist;
		prog->persist = new MvuPersistLog();
		MvuPersistLog* log = prog->persist;
		for (size_t i = 0; i < chunks.size(); i++)
		{
			PersistChunk pc;
			pc.code.assign(chunks[i].code, chunks[i].code + chunks[i].codeSize);
			pc.fixups = chunks[i].fixups;
			log->chunks.push_back(std::move(pc));
		}
		for (size_t i = 0; i < blocks.size(); i++)
		{
			PersistBlockRec rec = {};
			rec.chunkIndex = blocks[i].chunkIndex;
			rec.entryOffset = blocks[i].entryOffset;
			rec.startPC = blocks[i].startPC;
			rec.hasJumpCache = blocks[i].hasJumpCache;
			std::memcpy(&rec.pState, blocks[i].pState, sizeof(microRegInfo));
			std::memcpy(&rec.pStateEnd, blocks[i].pStateEnd, sizeof(microRegInfo));
			rec.live = final_block[i];
			log->blockByEntry.emplace(final_block[i]->hostEntry, static_cast<u32>(i));
			log->blocks.push_back(rec);
		}

#ifdef PCSX2_RECOMPILER_TESTS
		s_lastHydratedChunkBase[vu] = chunk_base;
#endif
		s_stats[vu].programsHydrated++;
		s_stats[vu].blocksHydrated += blocks.size();
		return prog;
	}

	//------------------------------------------------------------------
	// Test hooks
	//------------------------------------------------------------------

	bool TestSerializeNewestProgram(u32 vu_index, std::vector<u8>& out)
	{
		microVU& mVU = (vu_index & 1) ? microVU1 : microVU0;
		microProgram* newest = nullptr;
		for (const auto& entry : mVU.mvuContentMap)
		{
			if (!newest || entry.second->idx > newest->idx)
				newest = entry.second;
		}
		if (!newest)
			return false;
		return SerializeProgram(mVU, *newest, out);
	}

	bool TestHydrate(u32 vu_index, const u8* data, size_t size)
	{
		microVU& mVU = (vu_index & 1) ? microVU1 : microVU0;
		return HydrateProgram(mVU, data, size) != nullptr;
	}

#ifdef PCSX2_RECOMPILER_TESTS
	bool TestVerifyHydratedCode(u32 vu_index, const u8* image, size_t size)
	{
		const u32 vu = vu_index & 1;
		const std::vector<u8*>& placed = s_lastHydratedChunkBase[vu];
		if (placed.empty())
			return false;

		// Re-parse the image (cheap, and avoids holding parse state).
		const u8* p = image;
		const u8* const end = image + size;
		ImageHeader hdr;
		if (!ReadPod(p, end, hdr) || hdr.chunkCount != placed.size())
			return false;
		// Bounds-check each section advance before adding (mirror ReadPod):
		// an unchecked p += count*size can step past end, after which the
		// per-chunk `end - p` guard below underflows to a huge size_t.
		if (static_cast<size_t>(end - p) < static_cast<size_t>(hdr.rangeCount) * sizeof(DiskRange))
			return false;
		p += static_cast<size_t>(hdr.rangeCount) * sizeof(DiskRange);
		if (static_cast<size_t>(end - p) < static_cast<size_t>(hdr.blockCount) * sizeof(DiskBlock))
			return false;
		p += static_cast<size_t>(hdr.blockCount) * sizeof(DiskBlock);

		for (u32 i = 0; i < hdr.chunkCount; i++)
		{
			DiskChunkHeader ch;
			if (!ReadPod(p, end, ch))
				return false;
			std::vector<PersistFixup> fixups(ch.fixupCount);
			for (PersistFixup& f : fixups)
				if (!ReadPod(p, end, f))
					return false;
			if (static_cast<size_t>(end - p) < ch.codeSize)
				return false;
			const u8* expect = p;
			p += ch.codeSize;

			std::vector<u8> mask(ch.codeSize, 0);
			for (const PersistFixup& f : fixups)
			{
				const u32 span = (f.kind == kFixSelfBlockAbs) ? 16 : 4;
				for (u32 b = 0; b < span && f.codeOffset + b < ch.codeSize; b++)
					mask[f.codeOffset + b] = 1;
			}

			const u8* live = placed[i];
			for (u32 b = 0; b < ch.codeSize; b++)
			{
				if (!mask[b] && live[b] != expect[b])
				{
					DevCon.Error("mVUPersist: hydrated code mismatch at chunk %u offset 0x%x (live %02x != image %02x)",
						i, b, live[b], expect[b]);
					return false;
				}
			}
		}
		return true;
	}
#endif // PCSX2_RECOMPILER_TESTS

	u64 GetBlockCompileCount(u32 vu_index) { return s_blockCompiles[vu_index & 1]; }
	Stats GetStats(u32 vu_index) { return s_stats[vu_index & 1]; }

	//------------------------------------------------------------------
	// ABI-digest guard support
	//------------------------------------------------------------------

	// Zero every operand field that may legitimately differ between two
	// correct emissions of the same program (addresses + PC-relative
	// displacements to relocatable or layout-dependent targets). Everything
	// left — opcode bits, register fields, data immediates, instruction
	// order — is the emitter's shape.
	static u32 CanonicalizeInsn(u32 insn)
	{
		// 64-bit MOVZ/MOVN/MOVK (sf=1): address materialization. 32-bit
		// forms carry data constants (cycle counts etc.) and stay intact.
		const u32 mov_op = insn & 0xFF800000u;
		if (mov_op == 0xD2800000u || mov_op == 0xF2800000u || mov_op == 0x92800000u)
			return insn & ~(0xFFFFu << 5); // clear imm16, keep hw + Rd
		// B / BL: stub + cross-chunk displacements depend on placement.
		if ((insn & 0x7C000000u) == 0x14000000u)
			return insn & 0xFC000000u;
		// ADRP: page displacement is placement-dependent by construction.
		if ((insn & 0x9F000000u) == 0x90000000u)
			return insn & ~((3u << 29) | (0x7FFFFu << 5));
		return insn;
	}

	bool TestComputeEmitDigest(u32 vu_index, u64& out_digest)
	{
		out_digest = 0;
		std::vector<u8> image;
		if (!TestSerializeNewestProgram(vu_index, image))
			return false;

		const u8* p = image.data();
		const u8* const end = p + image.size();
		ImageHeader hdr;
		if (!ReadPod(p, end, hdr))
			return false;

		// Canonical buffer: structure counts + ranges + block geometry +
		// fixup records (sans address payloads) + canonicalized code.
		// Header hashes/bases and microRegInfo bytes are excluded — the
		// former are per-run, the latter may carry padding.
		std::vector<u8> canon;
		AppendPod(canon, hdr.vuIndex);
		AppendPod(canon, hdr.progStartPC);
		AppendPod(canon, hdr.rangeCount);
		AppendPod(canon, hdr.blockCount);
		AppendPod(canon, hdr.chunkCount);

		for (u32 i = 0; i < hdr.rangeCount; i++)
		{
			DiskRange r;
			if (!ReadPod(p, end, r))
				return false;
			AppendPod(canon, r);
		}
		for (u32 i = 0; i < hdr.blockCount; i++)
		{
			DiskBlock b;
			if (!ReadPod(p, end, b))
				return false;
			AppendPod(canon, b.chunkIndex);
			AppendPod(canon, b.entryOffset);
			AppendPod(canon, b.startPC);
			AppendPod(canon, b.hasJumpCache);
		}
		for (u32 i = 0; i < hdr.chunkCount; i++)
		{
			DiskChunkHeader ch;
			if (!ReadPod(p, end, ch))
				return false;
			AppendPod(canon, ch);
			for (u32 f = 0; f < ch.fixupCount; f++)
			{
				PersistFixup fx;
				if (!ReadPod(p, end, fx))
					return false;
				AppendPod(canon, fx.codeOffset);
				AppendPod(canon, fx.kind);
				AppendPod(canon, fx.stubId);
				AppendPod(canon, fx.fieldOffset);
				AppendPod(canon, fx.blockIndex);
			}
			if (static_cast<size_t>(end - p) < ch.codeSize || (ch.codeSize & 3))
				return false;
			for (u32 off = 0; off < ch.codeSize; off += 4)
			{
				u32 insn;
				std::memcpy(&insn, p + off, 4);
				const u32 c = CanonicalizeInsn(insn);
				AppendPod(canon, c);
			}
			p += ch.codeSize;
		}

		out_digest = XXH3_64bits(canon.data(), canon.size());
		return true;
	}
} // namespace mVUPersist
