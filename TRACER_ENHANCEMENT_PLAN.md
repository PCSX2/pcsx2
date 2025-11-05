# Enhanced InstructionTracer: Subsystem & Plugin Visibility

## User Request

> "Is it possible for the function trace to highlight plugin interactions so we can get a clearer picture of the emulation flow? eg if we want to extract assets"

**Answer: Yes! Here's the enhancement plan.**

---

## Current State

The InstructionTracer captures:
```cpp
struct TraceEvent {
    BreakPointCpu cpu;          // EE or IOP
    u64 pc;                     // Program counter
    u32 opcode;                 // Raw instruction
    std::string disasm;         // Disassembly
    u64 cycles;                 // CPU cycles
    u64 timestamp_ns;           // Timestamp
    std::vector<std::pair<u64,u8>> mem_r;  // Memory reads
    std::vector<std::pair<u64,u8>> mem_w;  // Memory writes
};
```

This is **good for low-level debugging** but doesn't show **high-level subsystem interactions**.

---

## Proposed Enhancement

### Add Subsystem Context to TraceEvent

```cpp
/// Subsystem categories for high-level flow tracking
enum class Subsystem : u8 {
    None = 0,           // Regular user code
    GS,                 // Graphics Synthesizer (textures, rendering)
    GIF,                // Graphics Interface (GS data transfer)
    VIF,                // VU Interface (VU program/data upload)
    DMA,                // Direct Memory Access
    SPU2,               // Sound processor
    CDVD,               // Disk/ISO access
    USB,                // USB peripherals
    DEV9,               // Network/HDD
    PAD,                // Controller input
    BIOS,               // BIOS syscalls
    VU0,                // Vector Unit 0
    VU1,                // Vector Unit 1
};

struct TraceEvent {
    // ... existing fields ...

    // NEW: Subsystem context
    Subsystem subsystem = Subsystem::None;
    std::string subsystem_detail;  // e.g., "GSgifTransfer1", "DMA CH2", "BIOS: LoadExecPS2"
};
```

### Detection Logic

#### 1. Memory Region Detection (Automatic)

**PS2 Memory Map:**
```
0x00000000 - 0x01FFFFFF : EE Main RAM
0x10000000 - 0x1000FFFF : EE Hardware registers
0x11000000 - 0x11003FFF : VU0 memory
0x11004000 - 0x1100FFFF : VU1 memory
0x12000000 - 0x12001FFF : GS registers
0x1F800000 - 0x1F9FFFFF : IOP RAM
0x1F900000 - 0x1F9FFFFF : SPU2 registers
```

When memory access (mem_r/mem_w) hits these regions, auto-tag:
- `0x10003000` → **GIF** (GIF_CTRL, GIF_TAG, etc.)
- `0x12000000` → **GS** (GS_CSR, etc.)
- `0x10008000` → **DMA** (DMA channel registers)
- `0x1F900000` → **SPU2** (IOP sound registers)

#### 2. Instruction Pattern Detection

**BIOS Syscalls:**
```cpp
// EE syscall: opcode 0x0000000c
if (opcode == 0x0000000c) {
    ev.subsystem = Subsystem::BIOS;
    u32 syscall_num = cpuRegs.GPR.r[3].UL[0];  // v1 register
    ev.subsystem_detail = GetBIOSCallName(syscall_num);  // e.g., "LoadExecPS2"
}
```

**IOP Module Calls:**
```cpp
// IOP uses jump tables for module calls
// Can detect based on PC ranges or known entry points
```

#### 3. Known Function Entry Points (Symbol-Based)

If PCSX2 has symbols loaded (ELF files), detect:
- `GSgifTransfer` entry point → **GIF**
- `sceCdRead` → **CDVD**
- Texture upload functions → **GS** + "Texture Upload"

---

## Implementation Plan

### Phase 1: Memory Region Detection (Minimal, High Value)

**Effort**: ~2 hours
**Value**: Immediate visibility into hardware access

```cpp
// In InstructionTracer.cpp or new SubsystemDetector.h

Subsystem DetectSubsystemFromMemory(u64 addr) {
    if (addr >= 0x12000000 && addr < 0x12002000)
        return Subsystem::GS;
    if (addr >= 0x10003000 && addr < 0x10004000)
        return Subsystem::GIF;
    if (addr >= 0x10008000 && addr < 0x10009000)
        return Subsystem::DMA;
    if (addr >= 0x11000000 && addr < 0x11004000)
        return Subsystem::VU0;
    if (addr >= 0x11004000 && addr < 0x11010000)
        return Subsystem::VU1;
    // IOP-side checks
    if (addr >= 0x1F900000 && addr < 0x1FA00000)
        return Subsystem::SPU2;

    return Subsystem::None;
}

// In tracer hook (iR5900.cpp, iR3000A.cpp):
Tracer::TraceEvent ev;
// ... existing code ...

// NEW: Auto-detect subsystem from memory accesses
for (const auto& [addr, size] : ev.mem_w) {
    Subsystem sub = DetectSubsystemFromMemory(addr);
    if (sub != Subsystem::None) {
        ev.subsystem = sub;
        break;  // First subsystem wins
    }
}
```

### Phase 2: BIOS Syscall Detection (Easy, Very Useful)

**Effort**: ~1 hour
**Value**: Understand game initialization, file loading, etc.

```cpp
// In tracer hook, after disassembly:
if (opcode == 0x0000000c) {  // syscall instruction
    ev.subsystem = Subsystem::BIOS;
    u32 syscall_num = cpuRegs.GPR.r[3].UL[0];  // v1 register
    ev.subsystem_detail = FormatString("BIOS syscall %d", syscall_num);

    // Optional: Map common syscalls
    switch (syscall_num) {
        case 6: ev.subsystem_detail = "LoadExecPS2"; break;
        case 7: ev.subsystem_detail = "ExecPS2"; break;
        // ... etc
    }
}
```

### Phase 3: DMA Channel Detection (Asset Extraction Gold)

**Effort**: ~2 hours
**Value**: See texture/model/sound uploads

DMA is **critical for asset extraction** - games use DMA to upload:
- Textures to GS
- VU programs to VU0/VU1
- Sound samples to SPU2

```cpp
// DMA channel register map (0x10008000 + channel*0x10)
// Channel 0 = VIF0, Channel 1 = VIF1, Channel 2 = GIF, etc.

std::string DetectDMAChannel(u64 addr) {
    if (addr < 0x10008000 || addr >= 0x10009000)
        return "";

    u32 channel = (addr - 0x10008000) / 0x10;
    static const char* dma_names[] = {
        "VIF0", "VIF1", "GIF", "IPU_FROM", "IPU_TO",
        "SIF0", "SIF1", "SIF2", "SPR_FROM", "SPR_TO"
    };

    if (channel < 10)
        return FormatString("DMA CH%d (%s)", channel, dma_names[channel]);
    return FormatString("DMA CH%d", channel);
}
```

### Phase 4: Symbol Resolution (Optional, Best Experience)

**Effort**: ~4 hours
**Value**: Function-level tracing like "sceCdRead", "sceGsLoadImage"

Requires integration with ELF symbol tables (if game has symbols).

---

## Usage Examples

### Example 1: Asset Extraction - Finding Texture Uploads

**Without enhancement:**
```
PC: 0x001234A0  addiu sp,sp,-32   [cycles: 12450000]
PC: 0x001234A4  sw    ra,28(sp)   [cycles: 12450004]
PC: 0x001234A8  jal   0x00200000  [cycles: 12450008]
```

**With enhancement:**
```
PC: 0x001234A0  addiu sp,sp,-32   [cycles: 12450000]
PC: 0x001234A4  sw    ra,28(sp)   [cycles: 12450004]  [GIF] Memory write to GIF_TAG
PC: 0x001234A8  jal   0x00200000  [cycles: 12450008]  [DMA] DMA CH2 (GIF) transfer
PC: 0x00200000  lui   v0,0x1200   [cycles: 12450012]  [GS] GS register access
```

You can now **filter traces** for GS/GIF activity to see exactly when textures are uploaded!

### Example 2: Game Loading Flow

**Trace filtered to BIOS + CDVD:**
```
PC: 0x00000A00  syscall           [BIOS] LoadExecPS2
PC: 0x00000B20  syscall           [BIOS] sceCdRead
PC: 0x1F402000  lw    v0,0(a0)    [CDVD] Reading from CDVD buffer
```

### Example 3: Audio Streaming

**Trace filtered to SPU2:**
```
PC: 0x80045600  sw    t0,0x1000(s0)  [SPU2] SPU2 voice 0 address
PC: 0x80045604  sw    t1,0x1002(s0)  [SPU2] SPU2 voice 0 pitch
```

---

## UI Enhancements

### InstructionTraceView Filters

Add filter dropdowns:
- **Subsystem**: [All, GS, GIF, DMA, SPU2, CDVD, BIOS, ...]
- **Show only subsystem events**: Checkbox

### Color Coding

```cpp
switch (event.subsystem) {
    case Subsystem::GS:
    case Subsystem::GIF:
        row_color = QColor(100, 200, 100);  // Green - Graphics
        break;
    case Subsystem::SPU2:
        row_color = QColor(200, 100, 200);  // Purple - Audio
        break;
    case Subsystem::CDVD:
        row_color = QColor(100, 100, 200);  // Blue - I/O
        break;
    case Subsystem::BIOS:
        row_color = QColor(200, 200, 100);  // Yellow - System
        break;
}
```

### Export Format (NDJSON)

```json
{
  "ts": 1727472000.123,
  "cpu": "EE",
  "pc": "0x001234A4",
  "op": "sw ra,28(sp)",
  "cycles": 12450004,
  "subsystem": "GIF",
  "subsystem_detail": "GIF_TAG write",
  "mem": {"w": [["0x10003000", 4]]}
}
```

AI agents can now query: *"Show me all GIF transfers in the last 1000 cycles"*

---

## Asset Extraction Workflow

With these enhancements, extracting game assets becomes much easier:

### 1. Start Tracing with Subsystem Filter
```json
// Via MCP tool
{"method": "trace_start", "params": {"cpu": "EE", "filter_subsystem": ["GS", "GIF"]}}
```

### 2. Identify Texture Upload Patterns
- Look for DMA CH2 (GIF) transfers
- Check destination: GS VRAM regions
- Capture memory contents at those addresses

### 3. Export Memory Dumps
```json
// When you see texture upload
{"method": "mem_read", "params": {"space": "EE", "addr": "0x50000000", "size": 65536}}
```

### 4. Reconstruct Asset Format
- Trace shows: width, height, format (from GS registers)
- Memory dump gives: raw pixel data
- Combine to reconstruct PNG/TGA

---

## MCP Protocol Extension

Add optional parameter to `trace_start`:

```json
{
  "name": "trace_start",
  "parameters": {
    "cpu": {"type": "string", "enum": ["EE", "IOP"]},
    "filter_subsystems": {
      "type": "array",
      "items": {"type": "string", "enum": ["GS", "GIF", "DMA", "SPU2", "CDVD", "BIOS", "VU0", "VU1"]},
      "description": "Only trace events related to these subsystems"
    }
  }
}
```

---

## Implementation Effort

| Phase | Effort | Value | Priority |
|-------|--------|-------|----------|
| Memory region detection | 2 hours | High | **P0** |
| BIOS syscall detection | 1 hour | High | **P0** |
| DMA channel detection | 2 hours | Very High | **P1** |
| UI filters/colors | 2 hours | Medium | P2 |
| Symbol resolution | 4 hours | High | P2 |
| **Total** | **11 hours** | | |

**Recommended**: Implement P0 (3 hours) for immediate value.

---

## Compatibility & Performance

### Performance Impact

- Memory region checks: **~5 ns per event** (simple range checks)
- Syscall detection: **~2 ns per event** (single opcode compare)
- **Total overhead**: <10 ns per traced event
- Still **zero overhead when tracing disabled**

### Backward Compatibility

- TraceEvent grows by ~16 bytes (enum + string)
- Old traces can default to `Subsystem::None`
- UI gracefully ignores subsystem field if not present

---

## Summary

**Yes, this is absolutely possible and highly valuable!**

### What You Get:

✅ **Clear visibility** into GS/GIF/DMA/SPU2/CDVD interactions
✅ **Asset extraction workflow** - see exactly when textures upload
✅ **BIOS call tracking** - understand game initialization
✅ **Filterable traces** - focus on what matters
✅ **Color-coded UI** - visual subsystem separation
✅ **AI agent queries** - "show me all texture uploads"

### Minimal Implementation (3 hours):

1. Add `Subsystem` enum + fields to `TraceEvent`
2. Memory region detection in tracer hooks
3. BIOS syscall detection
4. Update UI to display subsystem column

### Result:

Traces go from **low-level assembly** to **high-level flow visualization** perfect for:
- 🎨 Texture/model extraction
- 🎵 Audio ripping
- 📁 File system analysis
- 🔍 Reverse engineering game engines

---

## Next Steps

**Option A**: Implement immediately (I can do Phase 1 + 2 in ~30 minutes)
**Option B**: Add to backlog as separate feature request
**Option C**: Document and let user/maintainers decide

**Recommendation**: Implement Phase 1 (memory region detection) - it's minimal code with huge value for your use case.

Would you like me to implement this enhancement now?
