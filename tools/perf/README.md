# tools/perf — PCSX2 ARM64 CPU profiling rig

Step 0 of the `neither` cherry-pick funnel (`/home/bmd/pcsx2/neither/CLAUDE.md`): get a
**current, repeatable, attributable** bottleneck baseline for our own port. The old
RK3562 numbers are stale and from the wrong device; we re-profile on **M2 Max / Asahi**
first, then SD865.

## Pieces

| File | Role |
|---|---|
| `bucket_perf.py` | parse a `perf report --stdio` dump → subsystem ranking (EE-JIT/VU0/VU1/IOP/VIF-JIT + GS/SPU2/vtlb/dispatcher/sync/kernel) + thread-comm axis; `--json` for aggregation. Pure stdlib. |
| `profile_run.sh` | one-command wrapper: precondition gate → `perf record`/`inject --jit`/`report` → bucket → median wallclock + median shares → `summary.md`. Device-parameterized. Answers *where does time go* (attribution). |
| `codegen_ab.sh` | deterministic A/B of **two** eerunner binaries differing only in emitted code: `perf stat -e instructions,cycles` median over N runs → Δinsns/Δcycles/IPC → `summary.md`. Answers *did this codegen change help* (comparison). |
| `devices/<label>.env` | per-device `BIN_DIR`/`FREQ_DEFAULT`/`CYCLES_EVENT` (`m2max-asahi.env` shipped). |
| `scenes/<id>.env` | per-scene `ISO`/`SAVESTATE`/`FRAMES`/`RENDERERS`/`LABEL` (4 scenes; you fill ISO+savestate). |

## Drivers

- **eerunner** (whole-system): `pcsx2-eerunner --liverun` runs EE-JIT + MTVU + IOP + VIF + real GS headless for a fixed frame count from a savestate. perf-recorded, JIT-symbolized via the new `--perf-jitdump` flag, bucketed. **`vk` is the representative profile; `null` is a secondary "scalar EE/IOP minus GS-feeding" diagnostic** (Null drops GIF/PATH3). Audio is excluded (SPU2 forced Null) — noted in every summary.
- **gsrunner** (GS-only cross-check): `pcsx2-gsrunner -perf` replays a `.gs` dump deterministically and prints `@HWSTAT@` frame time + CPU/GS/GPU thread %. No perf/bucketing.
- VU isolation: `pcsx2-vurunner --bench <cap.vucap>` (deterministic PMU cycles) — run directly; cross-checks the `VU1-JIT` bucket.

## One-time setup (M2)

1. Build: `cmake --preset clang-perf && cmake --build build-perf --target pcsx2-eerunner pcsx2-gsrunner pcsx2-vurunner`
2. **Allow perf sampling:** `sudo sysctl kernel.perf_event_paranoid=1` (currently 2; the wrapper aborts with this hint otherwise).
3. Provide assets (copyrighted, not checked in): the 2 ISOs + 4 savestates referenced in `scenes/*.env`. Capture each savestate in pcsx2-qt paused exactly at the scene start.

## Run

```bash
# whole-system, both renderers, 3 runs, median
tools/perf/profile_run.sh --device m2max-asahi --scene uya-gameplay --renderer both --runs 3
# → ~/pcsx2-profiles/m2max-asahi/uya-gameplay/{vk,null}/summary.md

# GS-only cross-check
tools/perf/profile_run.sh --device m2max-asahi --driver gsrunner --gs-dump test-dumps/<dump>.gs
```

## Codegen A/B (comparing two binaries) — use `codegen_ab.sh`, NOT wallclock

`profile_run.sh`'s wallclock is **invalid** for comparing two binaries: perf-record
sampling perturbs the async EE↔MTVU↔MTGS sync in a *binary-dependent* way and
manufactures phantom wallclock deltas (a reproducible, thermal-controlled **+11%
"regression"** that was pure artifact — memory
`feedback_sd865_codegen_ab_use_instructions_not_wallclock`). No-perf wallclock is too
noisy (±15% session drift) to resolve a <10% codegen effect. The **deterministic**
metric is retired instructions via `perf stat` (reproducible to <0.1%):

```bash
# build base + new eerunner, stage both on the device, then:
tools/perf/codegen_ab.sh --device sd865 --scene sotc-01 \
    --base pcsx2-eerunner-base --new pcsx2-eerunner-new --runs 3
# → ~/pcsx2-profiles/sd865/codegen-ab/sotc-01/summary.md  (Δinsns, Δcycles, IPC)
```

Reading the result: **|Δinsns| > |Δcycles|** ⇒ the change is *pure code density* and
the OoO A77 absorbed it via IPC (≈neutral — the EE-density-is-neutral lesson). A
change that moves cycles ≈ as much as instructions is a *real compute* delta. Default
`--renderer null` (most deterministic); use `vk` only if the change touches
GS-feeding codegen (GIF/PATH3/XGKICK). The script enforces the quoted-path / 1e8
instruction sanity floor so a word-split fast-fail can't masquerade as a measurement.

## Go/no-go checklist

- **#1 (perf works on M2):** a tiny liverun under `perf record -k mono` + `perf inject --jit` + `perf report` must show real `EE_*`/`VU1_*` symbols, **not** raw `[JIT]` addresses. Resolve the Apple-PMU unknowns here (cycles event binding across the two PMUs `apple_avalanche_pmu`/`apple_blizzard_pmu`; max `-F`; `-k mono` honored). Record the working `CYCLES_EVENT` in `devices/m2max-asahi.env`.
- **#2 (share stability):** top-bucket median-share MAD across 3 runs is small (a few %) → MTVU nondeterminism doesn't break the ranking.
- **#3 (before SD865):** all 4 scenes yield a stable, attributable ranking (`unattributed` < ~5%) with a median wallclock. Then add `devices/sd865.env` + re-captured savestates; no code change.

## Notes

- The new `--perf-jitdump` flag lives in `pcsx2-eerunner/Main.cpp` (test harness only; no production gate). jitdump path: `~/.config/PCSX2/cache/pcsx2-perf-<pid>/jit-<pid>.dump`.
- Deferred: audio profiling (opt-in real-SPU2 knob), a dedicated EE numeric microbench.
