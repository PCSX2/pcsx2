# AGENT.md — Rules & Context for Code Agents

Purpose
- Safely extend the existing debugger with:
  - Instruction tracer: capture PC, opcode, disasm, and (optionally) memory accesses.
  - Memory scanner upgrades: snapshot/scan/rescan + “dump when there is a change”.
- Keep changes minimal, opt-in, performant, and cross-platform.

Repository Overview
- Stack: C++17+, Qt Widgets for UI, CMake build, custom PS2 emulator subsystems.
- Entry points (see README for full details):
  - Build: follow /README.md “Building” (CMake + presets/toolchains per OS).
  - Run: launch PCSX2, open Debugger window (DebuggerWindow).
- Code layout relevant to this work:
  - Core debugger (headless): pcsx2/DebugTools/*
    - DebugInterface.{h,cpp} — CPU-agnostic debug API (EE=R5900, IOP=R3000).
    - Breakpoints.{h,cpp} — execute breakpoints, memory checks, conditions.
    - DisassemblyManager.{h,cpp}, MIPSAnalyst.*, DisR5900asm.cpp, DisR3000A.cpp — disassembly and metadata.
  - Memory system: pcsx2/Memory.{h,cpp}, HostMemoryMap in Memory.h
  - Debugger UI (Qt): pcsx2-qt/Debugger/*
    - DisassemblyView.*, MemoryView.*, MemorySearchView.*, Breakpoints/*, Docking/*, DebuggerView.*, DebuggerWindow.*
- CMake:
  - Add new DebugTools sources/headers to pcsx2/CMakeLists.txt blocks pcsx2DebugToolsSources/Headers.
  - Add new Qt debugger views to pcsx2-qt/CMakeLists.txt (Debugger/* lists).

Golden Rules
- Never:
  - Modify 3rdparty/, generated Qt moc/ui artifacts, or vendor code.
  - Instrument hot dynarec paths (EE/IOP recompiler) without a maintainer review.
  - Block the emulation thread with UI or disk IO.
- Always:
  - Gate tracing/scanning behind explicit settings; disabled by default.
  - Use Host::RunOnCPUThread for CPU state/memory interaction; UI updates on UI thread.
  - Keep PRs small (<300 LOC per PR when possible), with clear test/rollback plans.
  - Add SPDX headers to new files to match repository convention.

What to Build (Scope)
- InstructionTracer (core, headless)
  - New files: pcsx2/DebugTools/InstructionTracer.h/.cpp
  - Provide per-CPU ring buffers (EE/IOP) of TraceEvent { cpu, pc, opcode, text, cycles, timestamp, optional mem_r/mem_w entries }.
  - Public API (examples):
    - Tracer::Enable(BreakPointCpu, bool), Tracer::IsEnabled(BreakPointCpu)
    - Tracer::Record(BreakPointCpu, const TraceEvent&) — fast, non-blocking
    - Tracer::Drain(BreakPointCpu, N, output_iter) — non-blocking consumer
    - Tracer::DumpToFile(BreakPointCpu, path, format, bounds)
  - Keep zero/near-zero overhead when disabled.
- MemoryScanner (core, headless)
  - New files: pcsx2/DebugTools/MemoryScanner.h/.cpp
  - Snapshot/scan/rescan across a range using DebugInterface read* APIs with typed comparisons.
  - Support “watch-and-dump” modes triggered via MemChecks or paused rescans.
  - Provide async-friendly APIs (submit/cancel) for UI use; safe on paused core only (enforce).
- UI Surfaces (Qt)
  - InstructionTraceView (new): pcsx2-qt/Debugger/Trace/InstructionTraceView.{h,cpp,ui}
    - Controls: Start/Stop (per-CPU), buffer size, filters (CPU, address range, symbol, opcode), Dump to file.
    - Read-only when core is running; allow capture on step/break.
  - MemorySearchView (extend):
    - Add “Rescan” and “Dump when value changes” toggles.
    - Use MemoryScanner engine; scans run only when paused or as change-driven via MemChecks.

Safe Integration Points
- Disassembly: use DisassemblyManager + DebugInterface::disasm(address, simplify).
- CPU and memory access:
  - DebugInterface has read8/16/32/64/128 and getPC()/getCycles() per CPU (R5900/R3000 implementations).
  - Only read memory on CPU thread or when paused. If running, either pause or rely on MemCheck for change events.
- Breakpoints and MemChecks:
  - Use CBreakPoints::AddMemCheck/RemoveMemCheck/Change* to manage change triggers.
  - Map MemCheck hits to MemoryScanner “dump on change” behavior.
- Threading:
  - CPU thread: Host::RunOnCPUThread(...) for emulator interactions.
  - UI thread: QtHost::RunOnUIThread(...) for model/view updates.
  - Background: QtConcurrent only for pure computations over already-snapshotted data; don’t read emulator memory off CPU thread.

Project-Specific Conventions
- License headers with SPDX on new files.
- Naming:
  - Core (DebugTools): PascalCase classes, camelCase methods; files match class names.
  - UI (Qt): <Name>View.{h,cpp,ui}, signals/slots Qt conventions, reuse DebuggerView base.
- Logging: Reuse existing logging categories; keep tracer logging out of global logs unless explicitly requested by user to dump.

Change Patterns
- Add new modules (preferred):
  - Introduce new files under pcsx2/DebugTools and a new UI folder pcsx2-qt/Debugger/Trace.
  - Wire into CMake lists (pcsx2/CMakeLists.txt; pcsx2-qt/CMakeLists.txt).
- Avoid refactors of dynarec/interpreter paths; focus on stepping/breakpoint-driven tracing first.
- Feature flags:
  - Settings under Debugger/UserInterface or dedicated Debugger/Tracer settings (see DebuggerWindow & DockManager patterns).
  - Default OFF; ensure persisted per layout if applicable.

Don’t-Break Areas (Human Review Required)
- Dynarec/JIT emission code and hot CPU loops (e.g., pcsx2/x86/*, opcode tables).
- Memory mapping (pcsx2/Memory.{h,cpp}, HostMemoryMap).
- Anything that changes emulator timing or thread synchronization.

How to Run, Test, and Validate
- Build: follow /README.md (CMake). Verify both pcsx2 and pcsx2-qt targets build across platforms.
- Manual test plan:
  - Open Debugger window, load a game or ELF, pause, and step through code.
  - Enable InstructionTraceView capture; verify PC/opcode/disasm entries update on step; Dump to file produces expected lines (symbol lookup optional).
  - Use MemorySearchView to run initial scan; filter and rescan; enable “dump on change” and create a MemCheck — verify file write on hits.
- Performance checks:
  - With tracing disabled: no measurable slowdown.
  - With tracing enabled: bounded memory use, no UI stalls (async dump), no core-thread blocking.

Exact Files to Focus On
- Core debug APIs and data flow:
  - pcsx2/DebugTools/DebugInterface.{h,cpp}
  - pcsx2/DebugTools/DisassemblyManager.{h,cpp}
  - pcsx2/DebugTools/Breakpoints.{h,cpp}
  - pcsx2/DebugTools/DisR5900asm.cpp, pcsx2/DebugTools/DisR3000A.cpp
  - pcsx2/Memory.{h,cpp}
- UI integration:
  - pcsx2-qt/Debugger/DebuggerView.*, DebuggerWindow.*, DockManager.*, DockLayout.*
  - pcsx2-qt/Debugger/DisassemblyView.*, MemoryView.*, MemorySearchView.*, Breakpoints/*

Implementation Checklist (Agent)
- Core
  - Create InstructionTracer.{h,cpp}; add to pcsx2/CMakeLists.txt (pcsx2DebugToolsSources/Headers).
  - Create MemoryScanner.{h,cpp}; add to CMake.
  - Add minimal unit-like checks if feasible (formatting/parsing helpers).
- UI
  - Create InstructionTraceView.{h,cpp,ui}; register as a DebuggerView in DockTables and CMake.
  - Extend MemorySearchView for rescan and “dump on change”.
- Wiring & Settings
  - Add per-CPU toggles and settings under Debugger namespaces (match DebuggerSettingsManager patterns).
  - Ensure all emulator-state access via Host::RunOnCPUThread, UI via QtHost::RunOnUIThread.
- Docs
  - Update this file if new constraints emerge; avoid duplicating README.

Quick Reference
- CPU accessor: DebugInterface::get(BreakPointCpu), .read{8,16,32,64,128}(), .disasm(), .getPC(), .getCycles()
- Breakpoints/MemChecks: CBreakPoints::Add/Remove/Change*, GetMemChecks(), Update()
- Threads: Host::RunOnCPUThread(...), QtHost::RunOnUIThread(...)
- CMake lists to edit:
  - pcsx2/CMakeLists.txt (pcsx2DebugToolsSources/Headers)
  - pcsx2-qt/CMakeLists.txt (Debugger/* file lists)
