# PINE Protocol Extension — Debugger Commands

## Summary

This proposal extends the PINE IPC protocol with opcodes 0x10–0x46 that expose the
complete PCSX2 GUI debugger interface, controller input, the DEV9 network bridge,
GS stream status, batched memory access and audio muting to external tools. The additions allow a remote client to control execution, manage save
states by name, inspect all registers across both CPUs, enumerate threads, modules,
and call stacks, list and look up EE function symbols and global variables, retrieve
local variables and function parameters, disassemble instruction ranges, save game
screenshots, read and override pad input, and capture, inject, and inspect PS2
network traffic — all through the existing PINE socket.

All new opcodes are numbered above 0x0F, so no existing opcode values change and
clients that do not use the new opcodes are completely unaffected.

---

## Motivation

The existing PINE protocol covers memory read/write and basic emulator metadata.
External tools that need a full debugger experience — IDE integrations, breakpoint
managers, code coverage harnesses, automated test runners — currently have no way to
control execution or inspect CPU state without a separate, custom socket. Adding these
opcodes makes PINE a self-contained debugger transport.

---

## New Opcodes

### Execution control and breakpoints (0x10–0x16)

| Value | Name                     | Purpose                                              |
|-------|--------------------------|------------------------------------------------------|
| 0x10  | `MsgGetProgramCounter`   | Read the current EE program counter and paused flag  |
| 0x11  | `MsgPause`               | Halt EE execution at the next instruction boundary   |
| 0x12  | `MsgResume`              | Resume EE execution                                  |
| 0x13  | `MsgStep`                | Execute exactly one EE instruction while paused      |
| 0x14  | `MsgSetBreakpoint`       | Register a PC-execution breakpoint at an EE address  |
| 0x15  | `MsgClearBreakpoint`     | Remove the breakpoint at a given EE address          |
| 0x16  | `MsgClearAllBreakpoints` | Remove every PC breakpoint                           |

### Register access (0x17–0x19)

| Value | Name              | Purpose                                                     |
|-------|-------------------|-------------------------------------------------------------|
| 0x17  | `MsgGetRegisters` | Quick read: all 32 EE GPRs (lower 32 bits), PC, HI, LO     |
| 0x18  | `MsgGetRegister`  | Read one register by CPU type, category, and index → u128   |
| 0x19  | `MsgSetRegister`  | Write one register by CPU type, category, and index         |

### Process state (0x1A–0x1D)

| Value | Name               | Purpose                                |
|-------|--------------------|----------------------------------------|
| 0x1A  | `MsgGetEEThreads`  | List all EE (R5900) threads            |
| 0x1B  | `MsgGetIOPThreads` | List all IOP (R3000) threads           |
| 0x1C  | `MsgGetModules`    | List all loaded IOP modules            |
| 0x1D  | `MsgGetStack`      | Walk the EE call stack                 |

### High-level stepping and symbol lookup (0x1E–0x21)

| Value | Name            | Purpose                                              |
|-------|-----------------|------------------------------------------------------|
| 0x1E  | `MsgStepInto`   | Set temp breakpoint at next instruction and resume   |
| 0x1F  | `MsgStepOver`   | Set temp breakpoint past function calls and resume   |
| 0x20  | `MsgStepOut`    | Walk stack, set temp breakpoint at caller, resume    |
| 0x21  | `MsgGetSymbol`  | Look up EE function symbol name by address           |

### Save-state and emulator control (0x22–0x27)

| Value | Name                | Purpose                                                    |
|-------|---------------------|------------------------------------------------------------|
| 0x22  | `MsgSaveStateFile`  | Save state to a named file path (async)                    |
| 0x23  | `MsgLoadStateFile`  | Load state from a named file path                          |
| 0x24  | `MsgReset`          | Cold-boot reset                                            |
| 0x25  | `MsgFrameAdvance`   | Advance N video frames then pause                          |
| 0x26  | `MsgGetFPS`         | Current emulated framerate as `f32`                        |
| 0x27  | `MsgSetLimiterMode` | Set speed limiter (0=Nominal, 1=Turbo, 2=Slomo, 3=Unlimited) |

### Breakpoint inspection and disassembly (0x28–0x29)

| Value | Name                  | Purpose                                                    |
|-------|-----------------------|------------------------------------------------------------|
| 0x28  | `MsgListBreakpoints`  | List active EE and/or IOP PC breakpoints                   |
| 0x29  | `MsgDisassemble`      | Disassemble N instructions at an address on EE or IOP      |

### Symbol database queries (0x2A–0x2D)

| Value | Name                | Purpose                                                    |
|-------|---------------------|------------------------------------------------------------|
| 0x2A  | `MsgListFunctions`  | Paginated list of EE function symbols (address, size, name)|
| 0x2B  | `MsgGetSymbolByName`| Look up any EE symbol by name → address and size           |
| 0x2C  | `MsgListGlobals`    | Paginated list of EE global variables                      |
| 0x2D  | `MsgGetLocals`      | List locals and parameters for the function at an address  |

### Memory watchpoints (0x2E–0x31)

| Value | Name                 | Purpose                                                      |
|-------|----------------------|--------------------------------------------------------------|
| 0x2E  | `MsgAddWatch`        | Add a memory watchpoint that breaks on read, write, or both  |
| 0x2F  | `MsgRemoveWatch`     | Remove a memory watchpoint by address range                  |
| 0x30  | `MsgListWatches`     | List all active memory watchpoints                           |
| 0x31  | `MsgClearAllWatches` | Remove all memory watchpoints                                |

### Screenshot (0x32)

| Value | Name               | Purpose                                      |
|-------|--------------------|----------------------------------------------|
| 0x32  | `MsgSaveSnapshot`  | Save a screenshot of the current game frame  |

### Pad/Controller input (0x33–0x37)

| Value | Name               | Purpose                                                |
|-------|--------------------|--------------------------------------------------------|
| 0x33  | `MsgPadGetState`   | Read full pad state (buttons, analogs, pressures)      |
| 0x34  | `MsgPadSetButton`  | Set a single button by index and normalized value      |
| 0x35  | `MsgPadSetAnalog`  | Set analog stick positions directly (raw 0–255)        |
| 0x36  | `MsgPadSetState`   | Set the complete pad state in one message (bulk write) |
| 0x37  | `MsgPadGetType`    | Query the controller type of a pad slot                |

### DEV9 network bridge (0x38–0x3F)

| Value | Name                | Purpose                                                         |
|-------|---------------------|-----------------------------------------------------------------|
| 0x38  | `MsgNetGetStatus`   | Query DEV9 adapter status (enabled, type, MAC, PS2 IP)          |
| 0x39  | `MsgNetCapture`     | Enable/disable packet capture with direction filter and queue   |
| 0x3A  | `MsgNetRecvCapture` | Drain buffered packets from the capture queue                   |
| 0x3B  | `MsgNetInject`      | Inject a raw Ethernet frame into the PS2 RX FIFO               |
| 0x3C  | `MsgNetInjectTx`    | Inject a raw Ethernet frame out through the TX adapter          |
| 0x3D  | `MsgNetDisassemble` | Dissect a packet and return a human-readable text summary       |
| 0x3E  | `MsgNetSetFilter`   | Set the capture filter (protocol, port, or IP)                  |
| 0x3F  | `MsgNetGetStats`    | Return TX/RX counters and capture queue statistics              |

### GS stream control (0x40–0x42)

The frames themselves go over the separate GS stream socket (`GSStreamServer`), not PINE. These opcodes only report on it.

| Value | Name                   | Purpose                                                         |
|-------|------------------------|-----------------------------------------------------------------|
| 0x40  | `MsgGSStreamGetPort`   | Return the GS stream listener port (0 when disabled)            |
| 0x41  | `MsgGSStreamGetStatus` | Return active flag, port, subscribers, frames delivered/dropped |
| 0x42  | `MsgGSStreamPing`      | Empty reply; confirms the GS stream control plane is wired up   |

### Batched memory access (0x43–0x44)

| Value | Name               | Purpose                                                  |
|-------|--------------------|----------------------------------------------------------|
| 0x43  | `MsgReadMemBatch`  | Read fixed-size slices from many EE or IOP addresses     |
| 0x44  | `MsgWriteMemBatch` | Write fixed-size slices to many EE or IOP addresses      |

The batch wire format is described in [`pcsx2_pine_readmembatch_design.md`](https://github.com/TylerDev888/CodeDesigner/blob/master/docs/pcsx2_pine_readmembatch_design.md) in the CodeDesigner repository.

### Audio output (0x45–0x46)

| Value | Name               | Purpose                                                    |
|-------|--------------------|------------------------------------------------------------|
| 0x45  | `MsgSetAudioMuted` | Mute or unmute audio output (same as the Toggle Mute hotkey) |
| 0x46  | `MsgGetAudioState` | Query the audio output mute state                          |

---

## Packet Formats

All packets follow the existing PINE framing convention:

```
Request:  [total_size : u32 LE] [opcode : u8] [args…]
Reply:    [total_size : u32 LE] [status : u8] [data…]
```

`total_size` includes the four-byte size field itself. `status` is `IPC_OK (0x00)` on
success or `IPC_FAIL (0xFF)` on failure.

### MsgGetProgramCounter (0x10)

- **Request:** 5 bytes total — opcode only, no arguments.
- **Reply (OK):** 10 bytes — `IPC_OK`, then the EE program counter as a 32-bit
  little-endian unsigned integer, then a single byte that is `1` if emulation is
  currently paused or `0` if it is running.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgPause (0x11)

- **Request:** 5 bytes total — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`. Returns success whether or not execution was
  already paused.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgResume (0x12)

- **Request:** 5 bytes total — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`. Returns success whether or not execution was
  already running.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgStep (0x13)

- **Request:** 5 bytes total — opcode only.
- **Reply (OK):** 9 bytes — `IPC_OK`, then the new EE program counter as a 32-bit
  little-endian unsigned integer after the instruction has executed.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when emulation is not currently paused.

### MsgSetBreakpoint (0x14)

- **Request:** 9 bytes total — opcode followed by the target EE virtual address as
  a 32-bit little-endian unsigned integer.
- **Reply (OK):** 5 bytes — `IPC_OK`. Duplicate addresses are silently ignored.
- When execution reaches the registered address the emulator pauses automatically.

### MsgClearBreakpoint (0x15)

- **Request:** 9 bytes total — opcode followed by the EE address to remove.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when the address was not registered.

### MsgClearAllBreakpoints (0x16)

- **Request:** 5 bytes total — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`. Removes all PC breakpoints.

### MsgGetRegisters (0x17)

- **Request:** 5 bytes total — opcode only.
- **Reply (OK):** 145 bytes — `IPC_OK`, then the lower 32 bits of each of the 32 EE
  general-purpose registers in order (r0–r31, 128 bytes), then the program counter
  (4 bytes), then the HI register (4 bytes), then the LO register (4 bytes).
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgGetRegister (0x18)

Generic single-register read for any register on either CPU.

- **Request:** 8 bytes — opcode, `cpu` (u8: 0=EE, 1=IOP), `category` (u8), `index` (u8).
- **Reply (OK):** 21 bytes — `IPC_OK`, then the 16-byte register value as a 128-bit
  little-endian integer. Registers narrower than 128 bits are zero-extended.
- **Reply (fail):** 5 bytes — `IPC_FAIL`.

EE register categories:

| Value | Name        | Width  | Notes                         |
|-------|-------------|--------|-------------------------------|
| 0     | GPR         | 128-bit| indices 0–31; 32=PC, 33=HI, 34=LO |
| 1     | CP0         | 32-bit | indices 0–31                  |
| 2     | FPR         | 32-bit | indices 0–31                  |
| 3     | FCR         | 32-bit | FP control registers          |
| 4     | VU0F        | 128-bit| VU0 float; index 32=ACC       |
| 5     | VU0I        | 32-bit | VU0 integer                   |
| 6     | GSPRIV      | 64-bit | GS private registers          |

IOP has one category (0 = GPR), indices 0–31; index 32=PC, 33=HI, 34=LO.

### MsgSetRegister (0x19)

Generic single-register write. Only valid when emulation is paused.

- **Request:** 24 bytes — opcode, `cpu` (u8), `category` (u8), `index` (u8), then the
  new register value as a 128-bit little-endian integer (16 bytes).
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` (emulation not paused, or invalid category/index).

### MsgGetEEThreads (0x1A)

- **Request:** 5 bytes — opcode only. Emulation must be paused.
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each thread:

  ```
  TID      : u32LE
  PC       : u32LE   (current program counter of the thread)
  status   : u8      (0=bad, 1=run, 2=ready, 4=wait, 8=suspend, 0x10=dormant)
  wait     : u8      (0=none, 1=wakeup, 2=sema, 3=sleep, 4=delay, …)
  priority : u32LE
  entry    : u32LE   (initial entry point)
  stack_top: u32LE
  ```

  22 bytes per thread; up to 256 threads.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active or emulation is
  not paused.

### MsgGetIOPThreads (0x1B)

Same layout as `MsgGetEEThreads`; returns IOP (R3000) threads. Up to 1000 threads.
Emulation must be paused.

### MsgGetModules (0x1C)

- **Request:** 5 bytes — opcode only. Emulation must be paused.
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each module:

  ```
  name      : 32 bytes, NUL-padded
  version   : u16LE
  text_addr : u32LE
  entry     : u32LE
  gp        : u32LE
  text_size : u32LE
  data_size : u32LE
  bss_size  : u32LE
  ```

  58 bytes per module; up to 1000 modules.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active or emulation is
  not paused.

### MsgGetStack (0x1D)

Walks the EE call stack for the currently running thread. Emulation must be paused.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each frame:

  ```
  entry      : u32LE   (function start address)
  pc         : u32LE   (next instruction within this frame)
  sp         : u32LE   (stack pointer inside this frame)
  stack_size : u32LE   (frame size in bytes)
  ```

  16 bytes per frame; up to 1024 frames.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active or emulation is
  not paused.

### MsgStepInto (0x1E)

Sets a temporary breakpoint at the next logical instruction and resumes. The client
must poll `MsgGetProgramCounter` until `IsPaused == 1` to learn the new PC.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK` (returned immediately; execution is still running).
- **Reply (fail):** 5 bytes — `IPC_FAIL` if emulation is not currently paused.

Branch and delay-slot handling mirrors the GUI debugger:
- Unconditional branch → breakpoint at branch target.
- Conditional branch taken → breakpoint at branch target.
- Conditional branch not taken → breakpoint at `PC + 8` (skip delay slot).
- SYSCALL → breakpoint at the syscall handler target.
- Default → breakpoint at `PC + 4`.

### MsgStepOver (0x1F)

Like `MsgStepInto` but skips over function calls (`jal`/`jalr`).

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK` immediately.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if emulation is not currently paused.

For linked branches (`jal`/`jalr`) the breakpoint is placed at `PC + 8` so the entire
call is skipped. Other branches follow the same rules as `MsgStepInto`.

### MsgStepOut (0x20)

Walks the call stack to find the caller and sets a temporary breakpoint there.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK` immediately.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if emulation is not paused or the stack
  cannot be walked (fewer than 2 frames).

### MsgGetSymbol (0x21)

Looks up the EE function symbol whose range includes the given address.

- **Request:** 9 bytes — opcode followed by the query address (u32LE).
- **Reply (OK):** variable — `IPC_OK`, `start_address` (u32LE), then the NUL-terminated
  function name as a UTF-8 string.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no symbol overlaps the address or no
  valid VM is active.

### MsgSaveStateFile (0x22)

Saves the current emulator state to a file at an arbitrary path. The save is performed
asynchronously (fire-and-forget) so `IPC_OK` is returned before the file is written.

- **Request:** variable — opcode, `path_len` (u16LE, 1–512), then `path_len` bytes of
  UTF-8 path.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if `path_len` is 0 or > 512, or if no valid
  VM is active.

### MsgLoadStateFile (0x23)

Loads a state from an arbitrary file path. Blocks until the load completes.

- **Request:** variable — opcode, `path_len` (u16LE, 1–512), then `path_len` bytes of
  UTF-8 path.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` on I/O error or version mismatch.

### MsgReset (0x24)

Triggers a cold-boot reset of the emulated PS2 (equivalent to pressing the RESET
button on the console).

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgFrameAdvance (0x25)

Advances the emulation by exactly N video frames and then pauses. Useful for
frame-by-frame analysis without maintaining a dedicated step loop.

- **Request:** 6 bytes — opcode, `num_frames` (u8, 1–255).
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if `num_frames` is 0 or no valid VM.

### MsgGetFPS (0x26)

Returns the current emulated frame rate.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 9 bytes — `IPC_OK`, then current FPS as a 32-bit little-endian float.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgSetLimiterMode (0x27)

Changes the speed limiter.

- **Request:** 6 bytes — opcode, `mode` (u8): 0 = Nominal (full speed), 1 = Turbo,
  2 = Slomo, 3 = Unlimited.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` for unknown `mode` or no valid VM.

### MsgListBreakpoints (0x28)

Returns all active PC breakpoints on EE, IOP, or both.

- **Request:** 6 bytes — opcode, `cpu` (u8): 0 = EE only, 1 = IOP only, 0xFF = both.
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each breakpoint:

  ```
  addr    : u32LE
  enabled : u8   (1 = enabled, 0 = disabled)
  cpu     : u8   (0x01 = EE, 0x02 = IOP)
  ```

  6 bytes per breakpoint; up to 10 000 breakpoints.
- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgDisassemble (0x29)

Disassembles up to 1 000 instructions starting at a given address on EE or IOP.

- **Request:** 12 bytes — opcode, `cpu` (u8: 0=EE, 1=IOP), `address` (u32LE),
  `count` (u16LE, clamped to 1 000).
- **Reply (OK):** variable — `IPC_OK`, `returned` (u16LE), then for each instruction:

  ```
  addr     : u32LE
  text_len : u8      (number of bytes in the following text, max 255)
  text     : bytes   (UTF-8 disassembly string, not NUL-terminated)
  ```

- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgListFunctions (0x2A)

Returns a paginated slice of all EE function symbols loaded in the symbol database.

- **Request:** 11 bytes — opcode, `offset` (u32LE, 0-based index into the full list),
  `max_count` (u16LE, maximum entries to return in one reply).
- **Reply (OK):** variable — `IPC_OK`, `total` (u32LE, total function count),
  `returned` (u16LE), then for each function:

  ```
  address  : u32LE
  size     : u32LE   (bytes)
  name_len : u8      (max 255)
  name     : bytes   (demangled UTF-8 name, not NUL-terminated)
  ```

- **Reply (fail):** 5 bytes — `IPC_FAIL` when no valid VM is active.

### MsgGetSymbolByName (0x2B)

Looks up any symbol in the EE symbol database by its demangled name.

- **Request:** variable — opcode, `name_len` (u8, 1–255), then `name_len` bytes of
  UTF-8 symbol name.
- **Reply (OK):** 13 bytes — `IPC_OK`, `address` (u32LE), `size` (u32LE).
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no symbol with that name exists.

### MsgListGlobals (0x2C)

Returns a paginated slice of all EE global variables from the symbol database.
Same encoding as `MsgListFunctions`.

- **Request:** 11 bytes — opcode, `offset` (u32LE), `max_count` (u16LE).
- **Reply (OK):** variable — `IPC_OK`, `total` (u32LE), `returned` (u16LE), then for
  each global:

  ```
  address  : u32LE
  size     : u32LE
  name_len : u8
  name     : bytes
  ```

- **Reply (fail):** 5 bytes — `IPC_FAIL`.

### MsgGetLocals (0x2D)

Finds the EE function whose address range contains the given address and returns
all of its parameters and local variables with their storage locations.

- **Request:** 9 bytes — opcode, `address` (u32LE).
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each variable:

  ```
  storage_type : u8    (0 = global address, 1 = register number, 2 = stack offset)
  value        : s32LE (address / register index / SP-relative byte offset)
  name_len     : u8
  name         : bytes (UTF-8, not NUL-terminated)
  ```

  Parameters are listed first, then local variables.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no function contains the address, the
  function has no debug information, or no valid VM is active.

---

### MsgAddWatch (0x2E)

Registers a memory watchpoint that breaks execution when the specified address range is
accessed. Watchpoints are only effective in JIT mode; they have no effect under the
interpreter or HLE. The result mode is always `BREAK` (pause execution on hit).

- **Request:** 15 bytes — opcode, `cpu` (u8), `start` (u32LE), `end` (u32LE), `cond` (u8).
  - `cpu`: `0` = EE (R5900), `1` = IOP (R3000).
  - `start`: first byte of the watched range (inclusive).
  - `end`: first byte *past* the watched range (exclusive); must be greater than `start`.
  - `cond`: bitmask — `0x01` = break on read, `0x02` = break on write, `0x03` = break on
    read or write.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if `cpu` is out of range, `cond` is zero or
  contains undefined bits, `end <= start`, or no valid VM is active.

**Example — watch writes to 0x00203000..0x00203003 on EE:**
```
Request:  [0x0F 0x00 0x00 0x00]  [0x2E]  [0x00]  [0x00 0x30 0x20 0x00]  [0x04 0x30 0x20 0x00]  [0x02]
Reply OK: [0x05 0x00 0x00 0x00]  [0x00]
```

---

### MsgRemoveWatch (0x2F)

Removes a previously registered memory watchpoint. The address range must match the
range used when the watchpoint was added.

- **Request:** 14 bytes — opcode, `cpu` (u8), `start` (u32LE), `end` (u32LE).
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no matching watchpoint exists, `cpu` is out
  of range, `end <= start`, or no valid VM is active.

---

### MsgListWatches (0x30)

Returns all active memory watchpoints, optionally filtered by CPU.

- **Request:** 6 bytes — opcode, `cpu_sel` (u8).
  - `cpu_sel`: `0` = EE only, `1` = IOP only, `0xFF` = both.
- **Reply (OK):** variable — `IPC_OK`, `count` (u32LE), then for each watchpoint:

  ```
  start  : u32LE  — first byte of the watched range (inclusive)
  end    : u32LE  — first byte past the watched range (exclusive)
  cond   : u8     — condition bitmask (0x01=read, 0x02=write, 0x03=read+write)
  result : u8     — action bitmask (0x01=log, 0x02=break, 0x03=both)
  cpu    : u8     — 0x01=EE, 0x02=IOP (BreakPointCpu enum values)
  ```

- **Reply (fail):** 5 bytes — `IPC_FAIL`.

---

### MsgClearAllWatches (0x31)

Removes all active memory watchpoints on both CPUs.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM is active.

---

### MsgSaveSnapshot (0x32)

Queues a screenshot of the current game frame. The image is saved to the configured
snapshots directory (same as the in-game screenshot hotkey). The filename and format
(PNG, JPEG, or WebP) follow the `Screenshots` settings in the PCSX2 configuration.

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM is active.

---

### MsgPadGetState (0x33)

Read the full input state of one pad slot, including the button bitmask, analog stick
positions, and per-button pressure values.

- **Request:** 6 bytes — opcode + `pad` (u8, unified slot 0–7).
- **Reply (OK):** 29 bytes:
  - `IPC_OK` (1 byte)
  - `buttons` (u32 LE) — PS2 button bitmask; a bit that is **clear** means the
    corresponding button is **pressed** (PS2 hardware convention).
  - `lx` (u8) — left stick X axis, 0–255, 0x7F = centred.
  - `ly` (u8) — left stick Y axis, 0–255, 0x7F = centred.
  - `rx` (u8) — right stick X axis, 0–255, 0x7F = centred.
  - `ry` (u8) — right stick Y axis, 0–255, 0x7F = centred.
  - `pressures[16]` (16 × u8) — pressure values (0–255) for inputs 0–15
    in `PadDualshock2::Inputs` order: PAD_UP, PAD_RIGHT, PAD_DOWN, PAD_LEFT,
    PAD_TRIANGLE, PAD_CIRCLE, PAD_CROSS, PAD_SQUARE, PAD_SELECT, PAD_START,
    PAD_L1, PAD_L2, PAD_R1, PAD_R2, PAD_L3, PAD_R3.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM, slot is out of range, or
  the slot has no connected controller.

### MsgPadSetButton (0x34)

Set a single button (or analog axis) on a pad to a normalized value. The value goes
through the full deadzone and pressure pipeline just like a physical input.

- **Request:** 11 bytes — opcode + `pad` (u8) + `button_index` (u8) + `value` (f32 LE).
  - `pad` — unified slot 0–7.
  - `button_index` — index in `PadDualshock2::Inputs` (0–25; see table above).
  - `value` — 0.0 = fully released, 1.0 = fully pressed. Clamped to [0.0, 1.0].
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM, slot is out of range, or
  the button index is ≥ `PadDualshock2::Inputs::LENGTH` (26).

### MsgPadSetAnalog (0x35)

Set the raw analog stick positions on a pad, bypassing deadzone processing. Useful
for precise remote control where the client manages its own deadzone logic.

- **Request:** 10 bytes — opcode + `pad` (u8) + `lx` (u8) + `ly` (u8) + `rx` (u8)
  + `ry` (u8).
  - `lx`/`ly` — left stick X and Y, 0–255, 0x7F = centred.
  - `rx`/`ry` — right stick X and Y, 0–255, 0x7F = centred.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM, slot is out of range, or
  the slot has no connected controller.

### MsgPadSetState (0x36)

Atomically replace the complete input state of one pad in a single message. Suitable
for TAS playback or any use case that needs to update the entire controller state at
once.

- **Request:** 30 bytes — opcode + `pad` (u8) + payload:
  - `buttons` (u32 LE) — reserved field (not applied directly; button state is
    derived from the pressure array below).
  - `lx` (u8) + `ly` (u8) + `rx` (u8) + `ry` (u8) — analog stick positions.
  - `pressures[16]` (16 × u8) — pressure per button in `PadDualshock2::Inputs`
    order (same order as `MsgPadGetState`). A pressure of 0 means released; any
    non-zero value means pressed with that pressure.
- **Reply (OK):** 5 bytes — `IPC_OK`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM, slot is out of range, or
  the slot has no connected controller.

### MsgPadGetType (0x37)

Query the controller type installed in a pad slot.

- **Request:** 6 bytes — opcode + `pad` (u8, unified slot 0–7).
- **Reply (OK):** 6 bytes — `IPC_OK` + `controller_type` (u8).

`controller_type` values (`Pad::ControllerType` enum):

| Value | Meaning        |
|-------|----------------|
| 0     | NotConnected   |
| 1     | DualShock2     |
| 2     | Guitar         |
| 3     | Jogcon         |
| 4     | Negcon         |
| 5     | Popn           |

- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM or slot is out of range.

---

### MsgNetGetStatus (0x38)

Query the current DEV9 network adapter status without requiring a running game.

- **Request:** 5 bytes — size (u32 LE) + opcode.
- **Reply (OK):** 17 bytes — `IPC_OK` + `enabled` (u8) + `adapter_type` (u8)
  + `ps2_mac` (6 bytes) + `ps2_ip` (4 bytes).

`adapter_type` maps to `Pcsx2Config::DEV9Options::NetApi`:

| Value | Meaning       |
|-------|---------------|
| 0     | Unset         |
| 1     | PCAP_Bridged  |
| 2     | PCAP_Switched |
| 3     | TAP           |
| 4     | Sockets       |

`enabled` is 1 when `EthEnable` is set **and** the adapter has been successfully
opened.  `ps2_mac` is the 6-byte PS2 MAC address (zeroed if the adapter is not
active).  `ps2_ip` is the last IPv4 address seen in a PS2-originating packet (zeroed
until at least one packet has been processed).

- **Reply (fail):** never fails; always returns `IPC_OK`.

---

### MsgNetCapture (0x39)

Enable or disable the packet capture ring buffer.

- **Request:** 9 bytes — opcode + `enable` (u8) + `direction` (u8)
  + `max_queue` (u16 LE).
- **Reply (OK):** 5 bytes — `IPC_OK`.

`direction` controls which packets are stored:

| Value | Meaning          |
|-------|------------------|
| 0     | Both TX and RX   |
| 1     | TX only (PS2→Host) |
| 2     | RX only (Host→PS2) |

When `enable` is 0 the capture is stopped and the queue is cleared (regardless of the
`direction` and `max_queue` fields).  When `enable` is 1 the queue is started fresh
with the given direction filter and a maximum of `max_queue` entries (0 → default
1000).  Packets that arrive while the queue is full are silently dropped; the dropped
count is available via `MsgNetGetStats`.

A separate per-protocol filter may be applied with `MsgNetSetFilter`; the filter is
applied in addition to the direction filter.

---

### MsgNetRecvCapture (0x3A)

Drain captured packets from the ring buffer.

- **Request:** 7 bytes — opcode + `max_packets` (u16 LE).
- **Reply (OK):** 7 + N bytes — `IPC_OK` + `count` (u16 LE) + per-packet records.

Each per-packet record:

| Field          | Type    | Description                                      |
|----------------|---------|--------------------------------------------------|
| `direction`    | u8      | 0 = TX (PS2→Host), 1 = RX (Host→PS2)            |
| `timestamp_us` | u64 LE  | Monotonic timestamp in microseconds              |
| `length`       | u16 LE  | Length of the raw Ethernet frame in bytes        |
| `data`         | bytes   | Raw Ethernet frame bytes (`length` bytes)        |

Up to `max_packets` packets are removed from the queue atomically.  If the reply
buffer would overflow before all requested packets fit, the loop stops early and only
the packets that fit are returned; the remainder stays in the queue.

---

### MsgNetInject (0x3B)

Inject a raw Ethernet frame into the PS2 RX path as if it arrived from the network.
This is the primary mechanism for packet playback / fuzzing.

- **Request:** 7 + N bytes — opcode + `length` (u16 LE) + `data` (`length` bytes).
- **Reply (OK):** 6 bytes — `IPC_OK` + `success` (u8).

`success` is 1 if the frame was written to the SMAP RX FIFO, 0 if the FIFO was full
or the adapter was not initialised.  The FIFO holds at most 64 frames; if it is full
the caller should retry after a short delay.

The frame bypasses all adapter backends and is injected directly into the SMAP
hardware emulation, so it works with any adapter type (PCAP, TAP, Sockets) or even
when no real network device is configured.

---

### MsgNetInjectTx (0x3C)

Inject a raw Ethernet frame out through the TX adapter as if the PS2 sent it.

- **Request:** 7 + N bytes — opcode + `length` (u16 LE) + `data` (`length` bytes).
- **Reply (OK):** 6 bytes — `IPC_OK` + `success` (u8).

`success` is 1 if the adapter accepted the frame, 0 if the adapter was not
initialised.  This is useful for replaying captured PS2-originated traffic without
running the PS2 emulation.

---

### MsgNetDisassemble (0x3D)

Dissect a raw Ethernet frame and return a human-readable text summary of every
protocol layer.

- **Request:** 7 + N bytes — opcode + `length` (u16 LE) + `data` (`length` bytes).
- **Reply (OK):** 7 + T bytes — `IPC_OK` + `text_len` (u16 LE) + `text` (`text_len`
  bytes, **not** null-terminated).

The text contains one line per recognised protocol layer, for example:

```
[Ethernet] dst=00:04:1F:82:30:31 src=AA:BB:CC:DD:EE:FF type=0x0800
[IPv4] src=192.168.1.1 dst=192.0.2.100 proto=6 (TCP) ttl=64 id=0x0000 len=60
[TCP] src=80 dst=12345 seq=1000 ack=0 flags=[SYN] win=65535 payload=0B
```

Supported layers: Ethernet, ARP, IPv4, ICMP, TCP, UDP.

---

### MsgNetSetFilter (0x3E)

Set the protocol filter applied to the capture queue.  The filter is checked after the
direction filter set by `MsgNetCapture`; a packet must pass both filters to be queued.

- **Request:** 6 + extra bytes — opcode + `filter_type` (u8) + type-specific args.
- **Reply (OK):** 5 bytes — `IPC_OK`.

| `filter_type` | Extra args                   | Description                           |
|---------------|------------------------------|---------------------------------------|
| 0             | (none)                       | No filter — capture all packets       |
| 1             | `protocol` (u8)              | Filter by L3/L4 protocol (see below)  |
| 2             | `port` (u16 LE)              | Filter by TCP or UDP port number      |
| 3             | `ip` (4 bytes, network order) | Filter by source or destination IPv4  |

`protocol` values for `filter_type` = 1:

| Value | Meaning                             |
|-------|-------------------------------------|
| 0     | All (no protocol filter)            |
| 1     | TCP only                            |
| 2     | UDP only                            |
| 3     | ICMP only                           |
| 4     | ARP only                            |
| 5     | DNS only (UDP source or dest port 53) |
| 6     | DHCP only (UDP port 67 or 68)       |

---

### MsgNetGetStats (0x3F)

Return cumulative TX/RX counters and the current capture queue statistics.  Counters
are reset by `TermNet()` (when the adapter is torn down).

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 35 bytes — `IPC_OK` + fields below.

| Field                | Type    | Description                                        |
|----------------------|---------|----------------------------------------------------|
| `tx_packets`         | u32 LE  | Total packets sent by the PS2 since adapter open   |
| `rx_packets`         | u32 LE  | Total packets received by the PS2 since adapter open |
| `tx_bytes`           | u64 LE  | Total TX bytes                                     |
| `rx_bytes`           | u64 LE  | Total RX bytes                                     |
| `capture_queue_size` | u16 LE  | Number of packets currently in the capture queue   |
| `dropped_count`      | u32 LE  | Packets dropped because the capture queue was full |

---

### MsgGSStreamGetPort (0x40)

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 7 bytes — `IPC_OK` + `port` (u16 LE); 0 when the stream listener is not initialized.

---

### MsgGSStreamGetStatus (0x41)

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 20 bytes — `IPC_OK` + `active` (u8) + `port` (u16 LE) + `subscribers` (u32 LE)
  + `frames_delivered` (u32 LE) + `frames_dropped` (u32 LE).

---

### MsgGSStreamPing (0x42)

- **Request:** 5 bytes — opcode only.
- **Reply (OK):** 5 bytes — `IPC_OK`, no payload.

---

### MsgSetAudioMuted (0x45) / MsgGetAudioState (0x46)

Mute, unmute, or query PCSX2's audio output. Muting goes through
`SPU2::SetOutputMuted`, the same path as the Toggle Mute hotkey, so the saved
volume comes back on unmute.

- **Request (0x45):** 6 bytes — opcode + `muted` (u8, 0 = unmute, 1 = mute).
- **Request (0x46):** 5 bytes — opcode only.
- **Reply (OK):** 7 bytes — `IPC_OK` + `muted` (u8) + `muted_in_settings` (u8).
  `muted` is the effective state after the request. `muted_in_settings` is set
  when the user's "Mute All Sound" setting is on; that setting wins, so an
  unmute request leaves `muted = 1`.
- **Reply (fail):** 5 bytes — `IPC_FAIL` if no valid VM.

---

Send opcodes 0x10–0x46 as **single commands**, one per request buffer. Many of them
mutate or observe CPU state and are run on the CPU thread, so mixing them into a
multi-command request makes ordering hard to reason about. The server does not
currently reject them inside a multi-command request; this is a client rule.

---

## Breakpoint Integration

`MsgSetBreakpoint` wires into the existing PCSX2 `CBreakPoints` infrastructure so
the interpreter's normal breakpoint-check path triggers the pause automatically.
The high-level step opcodes (0x1E–0x20) use temporary stepping breakpoints (the same
mechanism as the built-in GUI debugger's Step Into/Over/Out buttons), including the
skip-first mechanism to avoid re-triggering on a breakpoint that is already at the
current PC.

---

## Backwards Compatibility

- All new opcodes are numbered above 0x0F and do not overlap with any existing opcode.
- Clients that never send the new opcodes see no behavioural change.
- Builds that do not include this change return `IPC_FAIL` for any unrecognised
  opcode, which is already the documented behaviour for `MsgUnimplemented (0xFF)`.

---

## Testing Checklist

- [ ] `MsgGetProgramCounter` returns a consistent PC value while a game is running
- [ ] `MsgPause` / `MsgResume` toggle the paused flag reported by `MsgGetProgramCounter`
- [ ] `MsgStep` while paused advances the PC; returns `IPC_FAIL` while running
- [ ] `MsgSetBreakpoint` + `MsgResume` pauses at the correct address
- [ ] `MsgClearBreakpoint` removes the breakpoint; `MsgClearAllBreakpoints` removes all
- [ ] `MsgGetRegisters` returns a 145-byte reply; `r0` is always zero
- [ ] `MsgGetRegister(0, 0, 0)` returns EE r0 (always zero, 16 bytes)
- [ ] `MsgGetRegister(1, 0, 0)` returns IOP r0 (16 bytes, zero-extended)
- [ ] `MsgSetRegister` while paused changes the register value; `IPC_FAIL` while running
- [ ] `MsgGetEEThreads` returns correct thread count, status, and PC fields
- [ ] `MsgGetIOPThreads` returns IOP thread list
- [ ] `MsgGetModules` lists loaded IOP modules with correct names and addresses
- [ ] `MsgGetStack` returns at least one frame while paused inside a function
- [ ] `MsgStepInto` + poll `MsgGetProgramCounter` follows branch targets correctly
- [ ] `MsgStepOver` skips function calls (`jal` → PC + 8)
- [ ] `MsgStepOut` resumes and pauses at the return site of the current function
- [ ] `MsgGetSymbol` returns name and start address for a known function address
- [ ] `MsgGetSymbol` returns `IPC_FAIL` for an address with no symbol
- [ ] `MsgSaveStateFile` + `MsgLoadStateFile` round-trip through a named `.p2s` file
- [ ] `MsgReset` restarts the game from boot
- [ ] `MsgFrameAdvance(1)` advances exactly one frame
- [ ] `MsgGetFPS` returns a plausible value (e.g., near 60 for NTSC games)
- [ ] `MsgSetLimiterMode(1)` enables turbo; `MsgSetLimiterMode(0)` restores normal speed
- [ ] `MsgListBreakpoints(0xFF)` returns all active breakpoints across both CPUs
- [ ] `MsgDisassemble(0, pc, 4)` returns 4 instruction lines starting at EE PC
- [ ] `MsgListFunctions` with `offset=0, max_count=100` returns up to 100 entries
- [ ] `MsgListFunctions` `total` matches the count returned by repeated pages
- [ ] `MsgGetSymbolByName` resolves a known function name to the correct address
- [ ] `MsgGetSymbolByName` returns `IPC_FAIL` for an unknown name
- [ ] `MsgListGlobals` returns global variable names, addresses, and sizes
- [ ] `MsgGetLocals` returns parameters and locals for a function with DWARF debug info
- [ ] `MsgGetLocals` returns `IPC_FAIL` for an address outside any function
- [ ] All opcodes return `IPC_FAIL` gracefully for invalid arguments
- [ ] `MsgAddWatch(0, start, end, 0x02)` adds a write watchpoint on EE; writing to the range pauses the emulator
- [ ] `MsgAddWatch(0, start, end, 0x01)` adds a read watchpoint; reading the range pauses the emulator
- [ ] `MsgAddWatch` returns `IPC_FAIL` for invalid `cond` (0 or bits outside 0x03)
- [ ] `MsgAddWatch` returns `IPC_FAIL` when `end <= start`
- [ ] `MsgRemoveWatch` removes the matching watchpoint; subsequent list no longer contains it
- [ ] `MsgRemoveWatch` returns `IPC_FAIL` for an address range that has no watchpoint
- [ ] `MsgListWatches(0xFF)` returns watchpoints from both EE and IOP
- [ ] `MsgListWatches(0)` returns only EE watchpoints
- [ ] `MsgClearAllWatches` removes all watchpoints; `MsgListWatches` returns count=0
- [ ] `MsgSaveSnapshot` queues a screenshot; a file appears in the snapshots directory
- [ ] `MsgSaveSnapshot` returns `IPC_FAIL` when no game is running
- [ ] `MsgGSStreamGetStatus` reports `active=1` and a non-zero port while the GS stream is running
- [ ] `MsgSetAudioMuted(1)` mutes output and replies `muted=1`; `MsgSetAudioMuted(0)` restores the previous volume
- [ ] `MsgSetAudioMuted(0)` replies `muted=1, muted_in_settings=1` while "Mute All Sound" is on
- [ ] `MsgPadGetState(0)` returns buttons, analogs, and pressures for pad 0
- [ ] `MsgPadGetState` returns `IPC_FAIL` for an out-of-range slot or disconnected pad
- [ ] `MsgPadSetButton(0, 6, 1.0)` presses Cross on pad 0; the game sees the button held
- [ ] `MsgPadSetButton(0, 6, 0.0)` releases Cross; the game sees the button released
- [ ] `MsgPadSetButton` returns `IPC_FAIL` for an invalid pad slot or button index
- [ ] `MsgPadSetAnalog(0, 0xFF, 0x7F, 0x7F, 0x7F)` pushes the left stick fully right
- [ ] `MsgPadSetAnalog` returns `IPC_FAIL` for an out-of-range pad slot
- [ ] `MsgPadSetState(0, ...)` sets buttons, analogs, and pressures atomically
- [ ] `MsgPadSetState` returns `IPC_FAIL` for an out-of-range or disconnected pad
- [ ] `MsgPadGetType(0)` returns `0x01` for a DualShock2 controller
- [ ] `MsgPadGetType(0)` returns `0x00` when no controller is connected
- [ ] `MsgPadGetType` returns `IPC_FAIL` for an out-of-range slot
- [ ] `MsgNetGetStatus` returns `enabled=0` when DEV9 is disabled; always returns `IPC_OK`
- [ ] `MsgNetGetStatus` returns the correct MAC address when DEV9 is active
- [ ] `MsgNetCapture(1, 0, 100)` enables capture for both TX and RX with queue size 100
- [ ] `MsgNetCapture(0, 0, 0)` disables capture and clears the queue
- [ ] `MsgNetRecvCapture(50)` returns at most 50 packets; each has a valid direction, timestamp, and length
- [ ] `MsgNetRecvCapture` returns `count=0` when no packets are queued
- [ ] `MsgNetInject` with a valid ARP frame returns `success=1` when DEV9 is active and FIFO has space
- [ ] `MsgNetInject` returns `success=0` when DEV9 is not initialised
- [ ] `MsgNetInjectTx` with a valid frame returns `success=1` when an adapter is open
- [ ] `MsgNetInjectTx` returns `success=0` when no adapter is open
- [ ] `MsgNetDisassemble` with a valid ARP frame returns a text string containing `[Ethernet]` and `[ARP]`
- [ ] `MsgNetDisassemble` with a valid TCP/IP frame returns lines for `[Ethernet]`, `[IPv4]`, and `[TCP]`
- [ ] `MsgNetSetFilter(1, 1)` (TCP only) causes `MsgNetRecvCapture` to return only TCP packets
- [ ] `MsgNetSetFilter(0)` clears the filter so all packets pass through
- [ ] `MsgNetGetStats` returns increasing `tx_packets`/`rx_packets` as traffic flows
- [ ] `MsgNetGetStats` `dropped_count` increases when the capture queue overflows
