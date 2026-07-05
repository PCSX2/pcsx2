# SotC EE register-heat capture — SD865, 2026-07-05 (EE-SRA S0)

First real-game dataset for the EE-SRA pin assignment. Collected with the
S0 reg-heat collector (`PCSX2_EE_REGHEAT_DIR`, commit 2b45a6004) and
aggregated by `tools/perf/ee_reg_heat_report.py`.

## Capture

- Device: SD865 Rocknix handheld (root@192.168.4.42), binary = main @ b4e04ad58.
- Scene: SotC gameplay savestate `SCUS-97472 (C19A374E).01.p2s`, 206 s wall.
- Launch shape (headless): `PCSX2_EE_REGHEAT_DIR=/storage/regheat ./run-pcsx2.sh
  -batch -fastboot -fullscreen -statefile <p2s> <iso>`; one SIGTERM at the end
  (graceful shutdown writes the dump). Do NOT use `-bigpicture` (sits in UI).
- Session perf: **57.88 fps average over 11,918 frames** (PerfLog session line);
  rolling lines showed EE 84–100 %, GS 44–76 %, VU 49–78 %, GPU 30–57 % —
  live confirmation the scene is EE-bound on SD865.
- 31,219 block records, 6.44 B block entries, est. 73.6 B dynamic GPR refs.

## Headline numbers

- **93.7 % of dynamic GPR refs are 64-bit** (pin-servable); only 6.3 % are
  128-bit MMI refs (never pin-served — upper half not mirrored).
- Coverage curve (top-N hottest regs, share of dynamic 64-bit refs):
  N=1: 20.3 %, N=2: 35.4 %, N=3: 48.0 %, N=4: 55.9 %, N=5: 62.2 %,
  N=8: 75.8 %, N=12: 87.1 %, N=16: 93.7 %.
  (DATE 2025 analog for RISC-V/MiBench was 69.5 % @ 8 — the EE curve is
  slightly better at the same N.)

## Ranked heat (dyn64 = exec-weighted 64-bit read+write refs)

| reg | dyn64 | % | cum% | wr% | notes |
|---|---|---|---|---|---|
| $v0 | 13.98 B | 20.3 | 20.3 | 49 | hottest by far |
| $sp | 10.40 B | 15.1 | 35.4 | 12 | already pinned (x22) |
| $v1 |  8.70 B | 12.6 | 48.0 | 44 | |
| $a0 |  5.44 B |  7.9 | 55.9 | 40 | |
| $k0 |  4.34 B |  6.3 | 62.2 | 32 | ⚠ ABI prior said cold — SotC disagrees; also owns ~30 % of ALL 128-bit refs |
| $a1 |  3.26 B |  4.7 | 66.9 | 41 | |
| $s0 |  3.19 B |  4.6 | 71.6 | 32 | |
| $ra |  2.90 B |  4.2 | 75.8 | 49 | already pinned (x23) |
| $at |  2.27 B |  3.3 | 79.1 | 46 | ⚠ ABI prior said cold |
| $a2 |  2.15 B |  3.1 | 82.2 | 37 | |
| $s1 |  1.87 B |  2.7 | 84.9 | 34 | |
| $s2 |  1.54 B |  2.2 | 87.1 | 34 | |

Tail: $a3 2.1 %, $s3 1.8 %, $t0 1.6 %, $s4 1.1 %, $t1 1.0 %, $s5/$t9/$s6 <1 %.

## Pin-ladder proposal (SotC-only data; $sp/$ra fixed)

- rung 1 (3 pins): + **$v0** → 39.6 % of dyn64 refs pin-served
- rung 2 (5 pins): + **$v1 $a0** → 60.1 %
- rung 3 (9 pins): + **$k0 $a1 $s0 $at** → 79.1 %
- rung 4 (12 pins): + **$a2 $s1 $s2** → 87.1 %

The early rungs are strongly front-loaded: rung 1 alone doubles the
pin-served share vs today's $sp/$ra (19.3 % → 39.6 %), and rung 2 triples
it — exactly where the seam risk is lowest (x29 free callee-saved,
x12/x13 preserve_most).

## Caveats / next

- One game, one scene. Per the plan, capture UYA and Gradius III gameplay
  before FREEZING the rung-3/4 guest assignment. $v0/$v1/$a0 are safe at
  any plausible weighting; **$k0 and $at need cross-game confirmation**
  (Transkernel/pstef priors called them cold — SotC's compiler/kernel
  usage says otherwise; this is why we measured instead of trusting ABI
  folklore).
- This scene runs ~58 fps; heat distribution is an engine property and
  should transfer to the slow scenes, but a slow-scene capture
  (traversal/combat) is cheap insurance if rung results underwhelm.
- Raw CSV kept out of the repo (8.5 MB); regenerate with the launch shape
  above.
