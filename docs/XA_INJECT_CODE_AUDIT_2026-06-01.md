# PCSX2 XA Inject — Critical Code Audit

**Branch:** `ppc_decomp` (forked from `xa-fix-pause-drain`)  
**Commit:** 64049f88d (HEAD)  
**Date:** 2026-06-01  
**Files:** `XaInject.h`, `ReadInput.cpp`, `Ps1CD.cpp`, `spu2sys.cpp`

---

## 1. Context

This codebase is **legacy PoC quality**. It evolved through 30+ commits of trial-and-error
patching over the `xa-fix-pause-drain` branch, then was rebranded with PPC kernel-aligned
state tracking on `ppc_decomp`. The architecture is sound (ADMA path, fractional resampler,
ring buffer) but carries accumulated cruft from abandoned approaches (voice-path comments,
ADPCM encode stubs, dead half-buffer experiments).

The PPC kernel decompilation (XA_POC 1.7.3, DECKARD PPC440) provided the reference
double-buffer model but exposed gaps between real-hardware SPU management and our
host-side ADMA injection.

---

## 2. Architecture Summary

```
XA CD Sector → xa_decode_sector_to_pcm() → mono int16_t[4032]
                                              ↓ (duplicate to stereo if mono)
                                         g_xa_pcm_ring[524288]  ← ring buffer
                                              ↓ (fractional resampler 37800→48000)
                                         _spu2mem[0x2000..0x23FF] ← ADMA input area
                                              ↓ (SPU2 V_Core::ReadInput)
                                         StereoOut32 → mixer → cubeb output
```

**Key design decisions:**
- ADMA path (Core 0 external input), NOT voice-based playback — correct because PS1 games clobber all 24 SPU2 voices
- 524K-sample ring buffer (~7s stereo at 37800 Hz)
- Fractional phase-accumulator resampler (37800→48000 Hz) with linear interpolation
- Backpressure: drop sectors when `ring_avail > 393216` (75% full)
- Half-buffer fill at `ReadIndex == 0x100` and `ReadIndex == 0` boundaries (256 samples per half)

---

## 3. Verified Bugs

### BUG-01: Stereo 4-bit XA nibble layout broken
**File:** `XaInject.h:169-174`  
**Severity:** Critical (for stereo games — silent for mono)  
**Status:** Dormant (all tested games use mono XA)

```cpp
// Stereo 4-bit: always reads low nibble, wrong byte stride
byte_pos = 16 + s * 4 + u;   // accesses 8 bytes/row → should be 4 (u/2)
get_high = 0;                // always low nibble → L/R are in same nibbles
```

vgmstream reference: stereo 4-bit packs 2 units per byte (L=low nibble, R=high nibble):
```c
byte_pos = 0x10 + j*0x04 + i/2;  // 4 bytes per row
nibble = (i&1) ? byte>>4 : byte&0x0F;
```

**Impact:** Any stereo XA game (RE1 codec voices, some MGS dialogue) will produce
garbled audio — wrong samples from wrong byte positions. Fix requires testing
with a stereo XA game first.

---

### BUG-02: Stall counter false positive
**File:** `ReadInput.cpp:227-234`  
**Severity:** Medium (log spam, misleading diagnostics)

`g_xa_adma_stall_count` increments on every `ReadInput()` call where `InputDataLeft > 0`.
Since `InputDataLeft` is set to `0x200` at every half-boundary fill, it stays non-zero
between fills. The counter grows monotonically through hundreds of fills, only reaching
zero if SPU2 fully drains the half before the next fill (unlikely with our aggressive refill).

`SpuDmaTO` warnings fire at `stall >= 48000` — roughly every 1 second — but have zero
correlation with actual DMA stalls. They're just counting ReadInput calls between fills.

Log evidence: `stall=1999` at fill #1000, `stall=7378` at fill #10000. No DMA problem
existed. These are normal inter-fill intervals.

**Fix:** Reset `g_xa_adma_stall_count = 0` on every successful fill (line 332 area).

---

### BUG-03: Ring overflow drops only 1 sample per full lap
**File:** `XaInject.h:341-343`  
**Severity:** Medium (masked by backpressure, would corrupt if backpressure removed)

```cpp
if (g_xa_pcm_write == g_xa_pcm_read) {
    g_xa_pcm_read = (g_xa_pcm_read + 1) & (XA_PCM_RING_SIZE - 1);
}
```

When write catches up to read, only ONE sample is dropped. But write advances
at 6.3× input rate — it would continue lapping read ~6 times per sector write
loop if backpressure weren't active. With backpressure at 393K, sectors are
dropped before this check fires. Without backpressure, ring would corrupt quietly.

**Fix:** Advance read to `write - XA_PCM_RING_SIZE/2` to maintain 50% headroom:
```cpp
if (g_xa_pcm_write == g_xa_pcm_read) {
    g_xa_pcm_read = (g_xa_pcm_write - XA_PCM_RING_SIZE/2) & (XA_PCM_RING_SIZE - 1);
}
```

---

### BUG-04: Resampler denominator — 48000 vs 44100 Hz
**File:** `ReadInput.cpp:294`  
**Severity:** Medium (audio pitch error ~9% if wrong)  
**Status:** Needs verification

```cpp
uint32_t phase_inc = ((uint32_t)g_xa_freq << 16) / 48000;
```

PS1DRV switches SPU2 output clock to 44100 Hz in PS1 mode. If `ReadInput()` runs
at 44100 Hz in PS1 mode, our denominator is wrong:

- Current: phase_inc(37800, 48000) = 0xC999 → output pitch = 1.000× at 48000 Hz
- Correct: phase_inc(37800, 44100) = 0xDB6D → output pitch = 0.918× at 48000 Hz
- If SPU2 actually runs at 44100: audio plays 1.09× too fast with current code

Skill doc says "PCSX2 internal SPU2 mixer always runs at 48000 Hz regardless of
output stream rate." This needs verification: does `V_Core::ReadInput()` tick at
48000 Hz always, or does it follow the PS1-mode 44100 Hz clock?

**Fix:** Determine actual sample rate at runtime. Add log line:
```cpp
Console.WriteLn("[XA-PPC] SPU2 sample rate = %d Hz (PS1 mode: %d)",
    SampleRate, SPU2::IsRunningPSXMode());
```

---

### BUG-05: Phase accumulator discontinuity on underrun recovery
**File:** `ReadInput.cpp:315-320, 335-348`  
**Severity:** Low (audible pop/click on stream boundaries)

On underrun, `s_xa_cur_L/R` fades toward zero (`* 3/4`). When ring refills,
the resampler interpolates between near-zero `prev` and real-audio `cur` →
discontinuity → audible pop. After 8 consecutive underrun fills, zero-fill
destroys all resampler state (`prev=cur=0`), guaranteeing a hard pop on resume.

**Fix:** On underrun, don't fade — hold last good sample (zero-order hold):
```cpp
// Instead of fading:
// s_xa_cur_L = s_xa_cur_L * 3 / 4;
// Hold last value — no pop on resume, natural silence
underrun_this++;
```

---

### BUG-06: InpVol log string hardcodes wrong value
**File:** `XaInject.h:386`  
**Severity:** Trivial (misleading diagnostics)

```cpp
Console.WriteLn("[XA-INJECT] ADMA configured: AutoDMACtrl=%d InpVol=0x7FFF stereo=%d",
    Cores[0].AutoDMACtrl, s_xa.stereo);
```

Actual `InpVol` was changed to `0x5A7F` (-3dB) at line 380-381. Log string
was not updated. Diagnostics lie to the operator.

**Fix:** Print actual value: `(u16)Cores[0].InpVol.Left`

---

### BUG-07: `g_xa_eofu_detected` — set, logged, cleared, never read
**File:** `XaInject.h:534-542`  
**Severity:** Trivial (dead state)

```cpp
g_xa_eofu_detected = true;
Console.WriteLn("[XA-PPC] eofu: ...");
// ... 2 lines later:
g_xa_eofu_detected = false;
```

Flag is set, logged, cleared within the same function. No other code reads it.
The eofu event IS logged (useful), but the flag serves no purpose.

**Fix:** Either remove the flag and just log, or use it to trigger decoder
history reset (already done at line 540-541 with `hist1=hist2=0`).

---

### BUG-08: `static` variables in header
**File:** `XaInject.h:322, 420, 435`  
**Severity:** Trivial (safe with single-TU, brittle otherwise)

```cpp
static int feed_count = 0;           // line 322
static uint32_t reject_sectors = 0;  // line 420
static int16_t pcm_buf[8192];        // line 435
```

These are function-local statics in a header. Each translation unit that includes
`XaInject.h` gets independent copies. Currently only `Ps1CD.cpp` includes it, so
safe. If `ReadInput.cpp` or any other file ever includes it, counters diverge.

**Fix:** Move `pcm_buf` to file scope in `Ps1CD.cpp`. Move counters to `s_xa` struct
or global scope (already have `g_xa_sector_matched/rejected` for most).

---

## 4. Design Issues (not bugs, but questionable)

### D-01: Double init on game startup
`CdlSetfilter` → `xa_inject_init()` (line 920), then `CdlReadS` → `xa_inject_init()`
(line 1009). Two Init log lines appear per game. Harmless (both zero already-zero
state) but noisy and suggests fragile init order.

### D-02: `xa_encode_adpcm_block_v2` and `xa_find_best_shift` — dead code
Lines 270-321. These re-encode PCM to SPU2 ADPCM blocks. Leftover from the
abandoned voice-based approach (v5-v6). Never called. ~50 lines of dead code.

### D-03: Extensive comment archaeology
Lines 222-268 document the evolution of approaches (SPU2writeDMA4Mem crash →
direct _spu2mem write → voice path → ADMA path). Useful as history but should
move to a doc file, not live in the header.

### D-04: Ring buffer size vs interleave gap
Ring = 524K samples = ~7s stereo at 37800 Hz. Interleaved XA has 1 audio sector
per ~8 data sectors = ~9.4 audio sectors/sec. Each sector = 8064 ring entries.
9.4 × 8064 = 75.8K entries/sec input. Drain = 37.8K × 2 = 75.6K entries/sec.
Input ≈ drain → ring stays at equilibrium near backpressure (395K). Sawtooth pattern:
burst of 5-6 sectors fills 40K, then 0.3s data gap drains 23K, net +17K per cycle.
Over ~25 cycles: ring hits 393K, backpressure engages. Ring oscillates 390-400K.

### D-05: `AdmaInProgress = 1` and `InputDataLeft = 0x200` on every fill
Lines 332-333. These aggressively override normal SPU2 DMA tracking. Games that
read `AdmaInProgress` or `InputDataLeft` registers would see incorrect values.
No PS1 game does this (they don't know about SPU2 ADMA), but it's not clean.

---

## 5. What The PPC Kernel Actually Tells Us

The XA_POC 1.7.3 PPC kernel (DECKARD PPC440 firmware) has a complete SPU
double-buffer engine with:

- **Voice-based playback** (not ADMA) — because the PPC runs BELOW the MIPS
  emulation, it owns the SPU2 hardware. PS1 games can't clobber voices because
  their SPU writes are intercepted by the PPC's MIPS→SPU2 translation layer.
  We can't use this approach — PS1 games DO clobber all 24 voices in PCSX2.

- **Half-buffer state machine** — `spuDBfCpuHlCurrStored` / `spuDBfHalfCurrPlayed` /
  `spuHlBfSw` — our current tracking matches this model. The fill-direction bug
  (commit b13bbbd8f) was the result of misinterpreting which half the PPC fills
  vs which half SPU reads.

- **Timeout detection** — `SpuDmaTO`, `SPU ctrlCmdTO` — the PPC detects stalled
  DMA transfers and SPU command timeouts. Our implementation (BUG-02) produces
  false positives because it measures ReadInput intervals, not actual DMA stalls.

- **Voice stop condition** — `voiceStopCondReached: spu&cpuHlBf-empty: play,store` —
  the PPC stops voices when BOTH SPU half and CPU half are empty. Our underrun
  detection only checks CPU half emptiness. We don't track SPU half fullness.

---

## 6. Fix Priority

| Priority | Bug | Effort | Risk |
|----------|-----|--------|------|
| 1 | BUG-02 (stall false positive) | 1 line | None |
| 2 | BUG-06 (log string) | 1 line | None |
| 3 | BUG-03 (ring overflow) | 1 line | Low (backpressure protects today) |
| 4 | BUG-05 (underrun pop) | 3 lines | Low (changes silence behavior) |
| 5 | BUG-04 (44100 verification) | 2 lines + test | None (just logging) |
| 6 | BUG-01 (stereo XA) | ~20 lines | Medium (untested code path) |
| 7 | D-01 (double init) | 3 lines | None |
| 8 | D-02 (dead code removal) | delete ~50 lines | None |

---

## 7. Verification Status

| Test | Result |
|------|--------|
| CTR (mono XA, file=1 ch=13) | Music plays, ring at 395K equilibrium, no WARNs |
| CTR (filter change ch=3) | Clean eofu + reinit, new stream decodes |
| SotN (mono XA, file=40 ch=4) | Prologue voice plays, 400+ sectors matched |
| SotN (filter change file=20 ch=4) | Clean eofu + reinit |
| SotN menu (SPU ADPCM, not XA) | Not tested here (separate audio path) |
| Stereo XA games | NOT TESTED — BUG-01 would manifest |
| 18900 Hz XA (Level A 8-bit) | NOT TESTED |
| Multi-game session (CTR→SotN) | Works, CdlInit stops XA cleanly |

---

*Report generated from full code review of ppc_decomp branch, commit 64049f88d.*
*Cross-referenced against DKWDRV PPC kernel XA_POC 1.7.3 decompilation and vgmstream/pcsx-redux reference decoders.*
