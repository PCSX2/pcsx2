# tools/perf — PCSX2 ARM64 CPU profiling rig

Step 0 of the `neither` cherry-pick funnel (`/home/bmd/pcsx2/neither/CLAUDE.md`): get a
**current, repeatable, attributable** bottleneck baseline for our own port. The old
RK3562 numbers are stale and from the wrong device; we re-profile on **M2 Max / Asahi**
first, then SD865.

## Pieces

| File | Role |
|---|---|
| `bucket_perf.py` | parse a `perf report --stdio` dump → subsystem ranking (EE-JIT/VU0/VU1/IOP/VIF-JIT + GS/SPU2/vtlb/dispatcher/sync/kernel) + thread-comm axis; `--json` for aggregation. Pure stdlib. |
| `profile_run.sh` | one-command wrapper: precondition gate → `perf record`/`inject --jit`/`report` → bucket → median wallclock + median shares → `summary.md`. Device-parameterized. |
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

## Go/no-go checklist

- **#1 (perf works on M2):** a tiny liverun under `perf record -k mono` + `perf inject --jit` + `perf report` must show real `EE_*`/`VU1_*` symbols, **not** raw `[JIT]` addresses. Resolve the Apple-PMU unknowns here (cycles event binding across the two PMUs `apple_avalanche_pmu`/`apple_blizzard_pmu`; max `-F`; `-k mono` honored). Record the working `CYCLES_EVENT` in `devices/m2max-asahi.env`.
- **#2 (share stability):** top-bucket median-share MAD across 3 runs is small (a few %) → MTVU nondeterminism doesn't break the ranking.
- **#3 (before SD865):** all 4 scenes yield a stable, attributable ranking (`unattributed` < ~5%) with a median wallclock. Then add `devices/sd865.env` + re-captured savestates; no code change.

## Notes

- The new `--perf-jitdump` flag lives in `pcsx2-eerunner/Main.cpp` (test harness only; no production gate). jitdump path: `~/.config/PCSX2/cache/pcsx2-perf-<pid>/jit-<pid>.dump`.
- Deferred: audio profiling (opt-in real-SPU2 knob), a dedicated EE numeric microbench.
