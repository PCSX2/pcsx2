# AGENT.md — PCSX2 Debug/Tracing + Code-Agent Integration

**Purpose**

* Safely extend the existing debugger with:

  * **InstructionTracer** — capture PC, opcode, disassembly, cycles, and optional memory access summaries.
  * **MemoryScanner** — snapshot/scan/rescan and **“dump when there is a change.”**
* Keep all changes **minimal**, **opt-in**, **performant**, and **cross-platform**.
* Provide **clear tool/function contracts** so a code model (e.g., OpenAI function-calling tools/Codex) can drive the features deterministically.

---

## Repository Map (areas relevant to this work)

* **Core (headless)**: `pcsx2/DebugTools/*`

  * `DebugInterface.{h,cpp}` — CPU-agnostic debug API (EE=R5900, IOP=R3000).
  * `DisassemblyManager.{h,cpp}`, `DisR5900asm.cpp`, `DisR3000A.cpp` — disassembly + metadata.
  * `Breakpoints.{h,cpp}` — execute breakpoints, memory checks, conditions.
  * `Memory.{h,cpp}` — host/guest memory access.

* **Debugger UI (Qt)**: `pcsx2-qt/Debugger/*`

  * `DisassemblyView.*`, `MemoryView.*`, `MemorySearchView.*`, `Breakpoints/*`,
    `Docking/*`, `DebuggerView.*`, `DebuggerWindow.*`.

* **Build**: `pcsx2/CMakeLists.txt`, `pcsx2-qt/CMakeLists.txt`.

---

## Golden Rules

**Never**

* Modify `3rdparty/`, generated Qt `moc`/`ui` artifacts, or vendor code.
* Block the emulation/CPU thread with UI work or disk I/O.
* Touch dynarec/JIT hot paths without explicit maintainer review.

**Always**

* Gate tracing/scanning behind explicit settings; **disabled by default**.
* Access emulator state on the **CPU thread**; update UI on the **UI thread**.
* Keep PRs small and reversible; add SPDX headers to new files.
* Prefer new modules over refactors of hot paths.

**Threading primitives**

* **CPU thread**: `Host::RunOnCPUThread(...)` for reading PC, memory, disasm, cycles.
* **UI thread**: `QtHost::RunOnUIThread(...)` for view/model updates.
* **Background**: Use `QtConcurrent` only on *snapshotted data* (no live core reads).

---

## What to Build

### 1) InstructionTracer (core, headless)

**Files:** `pcsx2/DebugTools/InstructionTracer.{h,cpp}`

* Per-CPU ring buffers (EE/IOP) of lightweight events:

```cpp
struct TraceEvent {
  BreakPointCpu cpu;    // EE or IOP
  u64 pc;               // guest PC
  u32 opcode;           // raw
  std::string disasm;   // pretty text (optional symbol lookup)
  u64 cycles;           // from DebugInterface
  u64 timestamp_ns;     // host monotonic
  // Optional summaries to keep overhead bounded:
  std::vector<std::pair<u64,u8>> mem_r; // (addr, size)
  std::vector<std::pair<u64,u8>> mem_w; // (addr, size)
};
```

* API sketch (zero/near-zero overhead when disabled):

```cpp
namespace Tracer {
  void   Enable(BreakPointCpu cpu, bool on);
  bool   IsEnabled(BreakPointCpu cpu);
  void   Record(BreakPointCpu cpu, const TraceEvent& ev); // non-blocking, lock-free ring
  size_t Drain(BreakPointCpu cpu, size_t n, OutputIt it); // consumer
  bool   DumpToFile(BreakPointCpu cpu, const fs::path& path,
                    const DumpBounds& bounds);            // async writer
}
```

* **Hooks (safe first):**

  * Capture on **step**, **breakpoint**, and **memcheck hit**.
  * Optional sampling on pause/resume boundaries.
  * Avoid interpreter/JIT emission changes in v1.

---

### 2) MemoryScanner (core, headless)

**Files:** `pcsx2/DebugTools/MemoryScanner.{h,cpp}`

* Features:

  * **Snapshot → scan → rescan** across range using `DebugInterface::read*` (paused).
  * Typed compares: `u8/u16/u32/u64/f32/f64`, exact/relative/epsilon.
  * **Watch & dump**: integrate with `CBreakPoints::AddMemCheck` to dump when a watched address changes.
  * Async-friendly submit/cancel API; enforce *paused* state for full scans.

* API sketch:

```cpp
class MemoryScanner {
 public:
  struct Query { BreakPointCpu cpu; u64 begin; u64 end; ValueType type; Comparison cmp; Value val; };
  ScanId SubmitInitial(const Query&);
  ScanId SubmitRescan(ScanId, const Query& delta);
  void   Cancel(ScanId);
  std::vector<Result> Results(ScanId) const;
  bool   DumpOnChange(BreakPointCpu cpu, u64 addr, const fs::path& outPath, DumpSpec spec);
};
```

---

### 3) UI Surfaces (Qt)

* **InstructionTraceView** → `pcsx2-qt/Debugger/Trace/InstructionTraceView.{h,cpp,ui}`

  * Controls: Start/Stop per-CPU, buffer size, filters (CPU, addr range, symbol, opcode), **Dump to file**.
  * Read-only display while running; allow capture during step/break.
* **MemorySearchView (extend)**

  * Add **Rescan** and **Dump when value changes** toggles.
  * Reuse existing background worker pipeline (`QtConcurrent`) for *post-snapshot* filtering only.

---

### 4) Wiring & CMake

* Add to `pcsx2/CMakeLists.txt`:

  * `pcsx2DebugToolsSources/Headers += InstructionTracer, MemoryScanner`
* Add to `pcsx2-qt/CMakeLists.txt`:

  * Include new `Debugger/Trace/InstructionTraceView` sources.
* Persist toggles in existing Debugger settings patterns.

---

## Safe Integration Points

* **Disassembly**: `DisassemblyManager + DebugInterface::disasm(addr, simplify)`.
* **CPU / Memory**: `DebugInterface::get(BreakPointCpu)`, `.read{8,16,32,64,128}()`, `.getPC()`, `.getCycles()`.
* **MemChecks**: `CBreakPoints::AddMemCheck/RemoveMemCheck/Change*` for on-change triggers → connect to MemoryScanner dump.
* **Threading discipline**:

  * No UI updates on CPU thread.
  * No emulator memory reads on UI/background threads unless paused or marshaled via CPU thread.

---

## Tool/Function Contracts (for code models)

Expose deterministic JSON-schema tools so an agent can drive the debugger. You can surface these via your app’s backend (HTTP/stdio). **Validate all inputs and keep writes opt-in.**

**Emulator control**

```json
{
  "name": "emulator_control",
  "description": "Pause/resume/step or load/save state.",
  "parameters": {
    "type": "object",
    "properties": {
      "action": { "type": "string", "enum": ["pause","resume","step","save_state","load_state"] },
      "path":   { "type": "string", "description": "Required for save/load." }
    },
    "required": ["action"]
  }
}
```

**Memory**

```json
{
  "name": "mem_read",
  "description": "Read guest memory; returns hex.",
  "parameters": {
    "type": "object",
    "properties": {
      "space": { "type": "string", "enum": ["EE","IOP","VU0","VU1","GS"] },
      "addr":  { "type": "integer" },
      "size":  { "type": "integer", "minimum": 1, "maximum": 1048576 }
    },
    "required": ["space","addr","size"]
  }
}
```

```json
{
  "name": "mem_write",
  "description": "Write guest memory with hex bytes (requires writes enabled).",
  "parameters": {
    "type": "object",
    "properties": {
      "space":    { "type": "string", "enum": ["EE","IOP","VU0","VU1","GS"] },
      "addr":     { "type": "integer" },
      "hex_bytes":{ "type": "string", "pattern": "^[0-9a-fA-F]*$" }
    },
    "required": ["space","addr","hex_bytes"]
  }
}
```

**Registers**

```json
{
  "name": "regs_get",
  "description": "Return snapshot of CPU/VU registers.",
  "parameters": {
    "type": "object",
    "properties": { "target": { "type": "string", "enum": ["EE","IOP","VU0","VU1"] } },
    "required": ["target"]
  }
}
```

**Scanning / Tracing / Dumps**

```json
{
  "name": "scan_memory",
  "description": "Snapshot and search for a value/pattern; returns matches.",
  "parameters": {
    "type": "object",
    "properties": {
      "space": { "type": "string", "enum": ["EE","IOP"] },
      "type":  { "type": "string", "enum": ["u8","u16","u32","u64","f32","f64"] },
      "query": { "type": "string", "description": "Literal or hex pattern" }
    },
    "required": ["space","type","query"]
  }
}
```

```json
{
  "name": "trace_start",
  "description": "Start instruction tracing with optional summaries.",
  "parameters": {
    "type": "object",
    "properties": {
      "cpu":         { "type": "string", "enum": ["EE","IOP"], "default": "EE" },
      "sample_every":{ "type": "integer", "minimum": 1, "default": 1 },
      "with_regs":   { "type": "boolean", "default": false },
      "with_mem":    { "type": "boolean", "default": false },
      "limit":       { "type": "integer" },
      "out_path":    { "type": "string", "default": "trace.ndjson" }
    },
    "required": ["cpu"]
  }
}
```

```json
{
  "name": "trace_stop",
  "description": "Stop tracing and flush to disk.",
  "parameters": { "type": "object", "properties": {} }
}
```

```json
{
  "name": "dump_memory",
  "description": "Dump selected memory spaces to a file with a small JSON header.",
  "parameters": {
    "type": "object",
    "properties": {
      "spaces":  { "type": "array", "items": { "type": "string", "enum": ["EE","IOP","VU0","VU1","GS"] }, "default": ["EE","IOP"] },
      "out_path":{ "type": "string", "default": "dump.ps2mem" }
    }
  }
}
```

---

## Trace & Dump Formats

**Trace (NDJSON, one event per line)**

```json
{
  "ts": 1727472000.123,
  "cpu": "EE",
  "pc": "0x00123450",
  "op": "addiu v0,a0,0x18",
  "cycles": 12345678,
  "regs": { "v0": "0x...", "a0": "0x..." },
  "mem":  { "r": [["0x20012340",4]], "w": [["0x20012344",4]] }
}
```

**Dump (`dump.ps2mem`)**

```
{ "version":1, "endianness":"little", "segments":[{"space":"EE","base":0,"size":33554432}, ...] }
--
<raw bytes…>
```

---

## Safety Rails

* **Writes are opt-in**: require `PCSX2_ALLOW_WRITES=true` (or equivalent setting) for `mem_write` to succeed.
* **Pause before mutate**: auto-pause before `load_state` or bulk writes; block if not paused.
* **Caps**: limit `mem_read/size` and dump sizes; chunk async I/O to avoid stalls.
* **Backpressure**: bounded rings; drop-oldest policy; warn when near capacity.
* **Audit**: log tool calls; gate network exposure behind an allowlist.

---

## How to Run, Test, Validate

1. Build PCSX2 per repository README.
2. Launch with the Debugger; pause or break on entry as needed.
3. **InstructionTracer**: enable capture, step a few instructions, then **Dump** → verify NDJSON contents and symbol formatting.
4. **MemoryScanner**: run initial scan → filter → **Rescan**; enable **Dump when value changes**, create a memcheck, verify dump file is produced.
5. **Tool contracts**: call each tool with valid/invalid params; confirm schema validation and that writes are blocked unless opted-in.
6. **Concurrency**: ensure no UI updates on CPU thread; scans run only while paused (except memcheck-driven dumps).

---

## CMake & File Additions (Checklist)

* [ ] `pcsx2/DebugTools/InstructionTracer.{h,cpp}` → add to `pcsx2/CMakeLists.txt` (`pcsx2DebugToolsSources/Headers`).
* [ ] `pcsx2/DebugTools/MemoryScanner.{h,cpp}` → add to same lists.
* [ ] `pcsx2-qt/Debugger/Trace/InstructionTraceView.{h,cpp,ui}` → add to `pcsx2-qt/CMakeLists.txt`.
* [ ] Extend `pcsx2-qt/Debugger/MemorySearchView.*` with **Rescan** + **Dump on change**.
* [ ] Persist settings under existing Debugger settings namespaces.

---

## Quick Start (for integrators)

* Expose the **Tool/Function Contracts** above from your app (HTTP or stdio). Give the model the tool schemas; on each call, validate inputs and forward to `DebugInterface`, `Breakpoints`, and the new `InstructionTracer`/`MemoryScanner` APIs.
* Keep everything **opt-in**, **paused for scans**, and **bounded** for performance. Small, focused PRs make review easy.
