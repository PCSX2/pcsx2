# CLAUDE.md — ARM64 Recompiler Port (Apple Silicon)

> This file is auto-loaded by Claude Code at the start of every session.
> It is the single entry point for the native Apple Silicon (ARM64) PCSX2 effort.

---

## ⏯ RESUME PROTOCOL — read this first if you are a fresh session

You are continuing a long-running, multi-session project: porting PCSX2's JIT
recompilers (EE / IOP / VU0 / VU1 + vtlb fastmem) from x86-64 to ARM64 so that
PCSX2 runs at playable speed natively on Apple Silicon.

**To pick up where the last session left off, read these in order:**

1. **`arm64-port/PROGRESS.md`** — the living roadmap. The top "▶ CURRENT FOCUS"
   block tells you exactly what to work on next. Phase/task checkboxes show what
   is done (`[x]`), in progress (`[~]`), and not started (`[ ]`).
2. **`arm64-port/JOURNAL.md`** — append-only session log. Read the **most recent
   1–2 entries** for fresh context: what was just done, decisions made, open
   blockers, and the explicit "Next step".
3. **`arm64-port/CONVENTIONS.md`** — the technical contract: ARM64 register
   allocation map, VIXL emission patterns, the build/test loop, debugging tools,
   and git hygiene. Follow it; do not invent parallel conventions.

Deep background (read on demand, not every session):
- `arm64-port/reference/apple-silicon-analysis.md` — what works / what's missing.
- `arm64-port/reference/ARM64_RECOMPILER_PLAN.md` — the full phased plan.

**After reading those three files you should be able to state, in one sentence,
what the next concrete coding task is.** If you cannot, re-read PROGRESS.md.

---

## 🎯 Mission

Make the core PS2 processor recompilers work on ARM64. Today the native ARM64
build runs but falls back to interpreters (orders of magnitude too slow). The
build system, GS (Metal/Vulkan/SW-JIT), NEON math, and VIF dynarec are already
done. The remaining work is the EE, IOP, and VU recompilers + vtlb fastmem.

Order of attack (see PROGRESS.md for detail): vtlb/skeleton → EE → IOP → VU.

---

## 🔧 Build / Run / Test (verified on this machine)

Paths are real and current as of the last journal entry.

```bash
# Deps are already built here (do NOT rebuild unless they're gone):
#   /Users/isztld/Documents/projects/pcsx2/pcsx2-deps
# Build dir already configured for arm64 Release:
#   build/

# --- Incremental rebuild (the common case) ---
cmake --build build --target pcsx2-qt -j18

# --- Re-configure from scratch (only if build/ is broken or deleted) ---
cmake -DCMAKE_PREFIX_PATH="/Users/isztld/Documents/projects/pcsx2/pcsx2-deps" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64" \
      -DDISABLE_ADVANCE_SIMD=ON \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
      -DUSE_LINKED_FFMPEG=ON \
      -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
      -B build .

# --- Verify the binary is actually arm64 ---
file build/pcsx2-qt/PCSX2.app/Contents/MacOS/PCSX2   # must say: arm64

# --- Run (macOS: ALWAYS postprocess the bundle first — see below) ---
cmake --build build --target pcsx2-postprocess-bundle   # macdeployqt: bundle Qt + fix install names
codesign --force --deep --sign - build/pcsx2-qt/PCSX2.app
open build/pcsx2-qt/PCSX2.app
# or for logs in the terminal:
build/pcsx2-qt/PCSX2.app/Contents/MacOS/PCSX2

# --- Unit tests ---
cmake --build build --target unittests -j18 && ctest --test-dir build/tests/ctest
```

**macOS bundle rule (avoids the duplicate-Qt / "Could not load the Qt platform
plugin cocoa" crash):** `cmake --build build --target pcsx2-qt` only relinks the
binary — it does **not** run the bundle postprocess, so launching `PCSX2.app`
straight from a `pcsx2-qt` build half-deploys it (main binary loads Qt from
`pcsx2-deps/lib` while the bundled Cocoa plugin loads Qt from the app's
`Frameworks/`). Before launching, run `cmake --build build --target
pcsx2-postprocess-bundle` (then re-sign), which has `macdeployqt` copy the Qt
plugins/frameworks and rewrite install names to `@executable_path/../Frameworks/...`.
Verify with `otool -L .../MacOS/PCSX2 | rg 'Qt6'` — healthy paths are
`@executable_path/...`, not absolute `pcsx2-deps/lib/...`. (A plain
`make -C build` builds the `all` target, which already includes the postprocess
step unless configured with `SKIP_POSTPROCESS_BUNDLE`.) Full detail in
`arm64-port/CONVENTIONS.md` §3.

If a fresh codesign is needed after a build (rare, for distribution):
```bash
codesign --force --sign - build/pcsx2-qt/PCSX2.app/Contents/Frameworks/libshaderc_shared.1.dylib
codesign --force --deep --sign - build/pcsx2-qt/PCSX2.app
```

---

## 🚧 Hard rules (do not violate)

1. **Never break the x86-64 build.** All ARM64 code goes behind `#ifdef _M_ARM64`
   (or `#ifndef _M_X86`) guards or in `pcsx2/arm64/` files. The x86 recompiler in
   `pcsx2/x86/` is the reference — read it, never break it.
2. **Work on the `armjit` branch.** Make atomic commits (one opcode family / one
   subtask per commit). Commit messages: `ARM64: <what>` e.g.
   `ARM64: Add recLB/recSB load-store generators`.
3. **Tight build/test loop.** Change 1–2 functions → `cmake --build build` →
   fix errors → test → commit. Do not write hundreds of lines before compiling.
4. **The interpreter is the ground truth.** When ARM64 JIT output is wrong,
   compare against `Interpreter.cpp` / `R3000AInterpreter.cpp` semantics.
   Interpreter fallback (`recCall(...)`) is an acceptable first pass for rare ops.
5. **End every working session by updating the trackers** (see below).

---

## ✅ Session-end checklist (every session must do this before stopping)

1. Update **`arm64-port/PROGRESS.md`**: flip checkboxes, move the "▶ CURRENT
   FOCUS" pointer to the next task.
2. Append a new entry to **`arm64-port/JOURNAL.md`** using the template at the
   top of that file (date, goal, what changed + commit hashes, decisions,
   blockers, **Next step**).
3. Commit the doc updates together with (or right after) the code:
   `git add -A && git commit`.

The next session's ability to resume depends entirely on these two files being
current. Treat updating them as part of "done", not optional cleanup.
