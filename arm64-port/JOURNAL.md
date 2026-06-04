# JOURNAL — ARM64 Recompiler Port

> Append-only log, **newest entry at the top**. One entry per working session.
> A fresh session reads the top 1–2 entries to recover context.
> Copy the template below for each new entry. Keep entries factual and short.

---

## ENTRY TEMPLATE (copy this)

```
## YYYY-MM-DD — <session title>

**Goal:** <what this session set out to do>

**What changed:**
- <files touched / features added>
- Commits: <short hashes + one-line each>

**Decisions & rationale:**
- <design choice and *why* — this is the part future sessions need most>

**Blockers / open questions:**
- <anything stuck, or "none">

**Next step:** <the single most concrete next task — must match PROGRESS ▶ CURRENT FOCUS>
```

---

## 2026-06-04 — Agentic workflow setup + gap verification

**Goal:** Make the project resumable across fresh Claude Code sessions, and verify
precisely what recompiler code is missing before any implementation.

**What changed:**
- Added `CLAUDE.md` (root) as the auto-loaded entry point + resume protocol.
- Created `arm64-port/` workflow: `PROGRESS.md` (roadmap), `JOURNAL.md` (this log),
  `CONVENTIONS.md` (technical contract).
- Moved reference docs into `arm64-port/reference/` (`apple-silicon-analysis.md`,
  `ARM64_RECOMPILER_PLAN.md`) via `git mv` (history preserved).
- Commits: <to be filled when committed>

**Decisions & rationale:**
- **File-based tracking, not Claude's private memory.** The trackers must be in the
  repo so they're portable, committed, and visible to any fresh session/machine.
- **Three-file model** (PROGRESS = what's next, JOURNAL = recent context,
  CONVENTIONS = how) keeps each file single-purpose and small enough to read fast.
- **Verified gap (the key technical finding):** Searched all branches/history.
  `pcsx2/arm64/` has only ever received two commits — `fe9399612` (VIF dynarec) and
  `8a18403fe` (EE/VU/IOP *stubs*). There is **no dormant EE/IOP/VU recompiler** to
  revive anywhere in this repo. This is a genuine from-scratch port (~35.8k lines of
  x86 rec to reimplement; ARM64 currently 1.8k lines = VIF + AsmHelpers + stubs).
  Corrected an earlier mis-statement that upstream had substantial ARM64 rec code.
- Confirmed exact fallback/stub locations: `VMManager.cpp:2721-2731` (interpreter
  forced under `#else` of `#ifdef _M_X86`); `arm64/RecStubs.cpp:11-14`
  (`vtlb_DynBackpatchLoadStore` = `pxFailRel`); `RecStubs.cpp:16-30` (`vuJITFreeze`
  hack). Build is confirmed native arm64; deps at
  `/Users/isztld/Documents/projects/pcsx2/pcsx2-deps`; build dir configured.

**Blockers / open questions:**
- Optional, unresolved: whether an external ARM64 fork (Android / community macOS)
  exists that's worth adapting instead of pure from-scratch. Not investigated yet —
  would need a web search, not git. Park until/unless we want to revisit strategy.

**Next step:** Phase 0.5–0.6 → study VIXL MacroAssembler API and build a scratch
harness that emits + executes ARM64 code, then begin Phase 1.1 (`aR5900.h` skeleton).
