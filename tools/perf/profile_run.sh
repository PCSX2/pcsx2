#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0+
#
# profile_run.sh — one-command, repeatable, attributed CPU profile of the PCSX2
# ARM64 port. Step 0 of the neither cherry-pick funnel: a CURRENT bottleneck
# baseline. Built/validated on M2 Asahi first; the SAME script runs on SD865 with
# a different devices/<label>.env (no code change).
#
# Two drivers:
#   eerunner  — whole-system (EE-JIT + MTVU + IOP + VIF + real GS), perf-recorded,
#               JIT-symbolized via jitdump, bucketed into a subsystem ranking.
#   gsrunner  — GS-only deterministic @HWSTAT@ frame-time + thread-% cross-check
#               (no perf/bucketing; gsrunner replays GIF packets, runs no EE/VU JIT).
#
# Methodology (CLAUDE.md): wallclock is the throughput truth, FPS is noisy; >=2 runs,
# report the median. Bucket *shares* are more stable than absolute cycles across the
# MTVU-nondeterministic liverun, so we median per-bucket share across runs.
#
# Usage:
#   tools/perf/profile_run.sh --device m2max-asahi --scene uya-gameplay --renderer both --runs 3
#   tools/perf/profile_run.sh --device m2max-asahi --driver gsrunner --gs-dump test-dumps/foo.gs.zst
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

DRIVER=eerunner
SCENE=""
DEVICE=""
RENDERER=""           # vk | null | both ; default from scene RENDERERS, else vk
RUNS=3
OUT="$HOME/pcsx2-profiles"
FREQ=""               # overrides device FREQ
GS_DUMP=""            # gsrunner driver
LOOP=3                # gsrunner loop count

die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --driver)   DRIVER="$2"; shift 2;;
    --scene)    SCENE="$2"; shift 2;;
    --device)   DEVICE="$2"; shift 2;;
    --renderer) RENDERER="$2"; shift 2;;
    --runs)     RUNS="$2"; shift 2;;
    --out)      OUT="$2"; shift 2;;
    --freq)     FREQ="$2"; shift 2;;
    --gs-dump)  GS_DUMP="$2"; shift 2;;
    --loop)     LOOP="$2"; shift 2;;
    -h|--help)  grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) die "unknown arg: $1";;
  esac
done

[[ -n "$DEVICE" ]] || die "--device <label> required (see tools/perf/devices/)"
DEVENV="$HERE/devices/$DEVICE.env"
[[ -f "$DEVENV" ]] || die "no device profile: $DEVENV"
# shellcheck disable=SC1090
source "$DEVENV"   # provides BIN_DIR, FREQ (default), CYCLES_EVENT, optional ISO_ROOT
: "${BIN_DIR:?devices/$DEVICE.env must set BIN_DIR}"
: "${CYCLES_EVENT:=cycles}"
[[ -n "$FREQ" ]] || FREQ="${FREQ_DEFAULT:-999}"

# --- precondition: perf sampling must be permitted -----------------------------
if [[ "$DRIVER" == "eerunner" ]]; then
  PARANOID="$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 99)"
  if [[ "$PARANOID" -gt 1 ]]; then
    die "perf_event_paranoid=$PARANOID (need <=1). Fix:  sudo sysctl kernel.perf_event_paranoid=1"
  fi
  command -v perf >/dev/null || die "perf not found"
fi

ts() { date +%s.%N; }

###############################################################################
# gsrunner driver — deterministic GS-only @HWSTAT@ cross-check (no perf record)
###############################################################################
if [[ "$DRIVER" == "gsrunner" ]]; then
  [[ -n "$GS_DUMP" ]] || die "--gs-dump <dump.gs[.zst]> required for gsrunner"
  GBIN="$BIN_DIR/pcsx2-gsrunner"
  [[ -x "$GBIN" ]] || die "missing $GBIN (build it: cmake --build build-perf --target pcsx2-gsrunner)"
  RUNDIR="$OUT/$DEVICE/gs-$(basename "$GS_DUMP" | tr './' '__')"
  mkdir -p "$RUNDIR"
  echo "== gsrunner -perf $GS_DUMP (loop=$LOOP) =="
  "$GBIN" -surfaceless -perf -loop "$LOOP" "$GS_DUMP" 2>&1 | tee "$RUNDIR/stdout.txt" | grep '@HWSTAT@' || true
  echo "artifacts: $RUNDIR/stdout.txt"
  exit 0
fi

###############################################################################
# eerunner driver — whole-system perf profile
###############################################################################
[[ -n "$SCENE" ]] || die "--scene <id> required for eerunner (see tools/perf/scenes/)"
SCENEENV="$HERE/scenes/$SCENE.env"
[[ -f "$SCENEENV" ]] || die "no scene profile: $SCENEENV"
# shellcheck disable=SC1090
source "$SCENEENV"  # provides ISO, SAVESTATE, FRAMES, RENDERERS, LABEL
: "${ISO:?scenes/$SCENE.env must set ISO}"
: "${SAVESTATE:?scenes/$SCENE.env must set SAVESTATE}"
: "${FRAMES:=300}"
[[ -n "$RENDERER" ]] || RENDERER="${RENDERERS:-vk}"
[[ "$RENDERER" == "both" ]] && RENDERER="vk null"

EBIN="$BIN_DIR/pcsx2-eerunner"
[[ -x "$EBIN" ]] || die "missing $EBIN (build: cmake --build build-perf --target pcsx2-eerunner)"
# ( ... || true ) so pipefail sees grep's status, not eerunner --help's exit 1
# (eerunner treats --help as an unknown arg: prints usage, then exits non-zero).
( "$EBIN" --help 2>&1 || true ) | grep -q -- '--perf-jitdump' \
  || die "$EBIN has no --perf-jitdump — rebuild from the patched source (clang-perf)."
[[ -f "$ISO" ]]       || die "ISO not found: $ISO"
[[ -f "$SAVESTATE" ]] || die "savestate not found: $SAVESTATE"

# jitdump dir is EmuFolders::Cache = ~/.config/PCSX2/cache (set by eerunner).
JITDIR="${PCSX2_CACHE:-$HOME/.config/PCSX2/cache}"

for REND in $RENDERER; do
  SDIR="$OUT/$DEVICE/$SCENE/$REND"
  mkdir -p "$SDIR"
  echo "== $LABEL [$SCENE] renderer=$REND frames=$FRAMES runs=$RUNS =="
  for k in $(seq 1 "$RUNS"); do
    RUNDIR="$SDIR/run$k"; mkdir -p "$RUNDIR"
    rm -rf "$JITDIR"/pcsx2-perf-* 2>/dev/null || true
    echo "-- run $k/$RUNS --"
    t0="$(ts)"
    # Production-representative knobs: async MTGS, MTVU on, EE jit. (eerunner test
    # harness env vars — not shipped gates.) SPU2 stays Null (audio excluded by design).
    # NOTE: do NOT use --per-thread — on the Apple M2 PMU it follows only the
    # blocked main thread and captures ~0 samples. Default inherit mode samples all
    # worker threads (CPU/MTVU/GS) and still records comm/tid per sample, so the
    # by-comm axis survives. --call-graph omitted (this perf rejects =no; flat is what
    # we want anyway). Verified working on m2max-asahi 2026-06-24.
    # $PIN (from the device env) optionally prefixes the binary to pin it to a
    # CPU set — on the SD865 big.LITTLE this is `taskset -c 4-7` so the whole
    # emulator runs on the A77 prime/gold cores (the perf-relevant target), not
    # the A55 silver cores. Empty on the M2 (single P-cluster, no pinning needed).
    EERUNNER_SYNCMTGS=0 EERUNNER_MTVU=1 EERUNNER_EE=jit \
      perf record -e "$CYCLES_EVENT" -F "$FREQ" -k mono \
        -o "$RUNDIR/perf.data" -- \
        ${PIN:-} "$EBIN" --liverun --renderer "$REND" --frames "$FRAMES" \
                --savestate "$SAVESTATE" --iso "$ISO" --perf-jitdump \
        >"$RUNDIR/stdout.txt" 2>&1 || true
    t1="$(ts)"
    echo "$(awk "BEGIN{printf \"%.3f\", $t1-$t0}")" > "$RUNDIR/wallclock.txt"

    # Pull in the jitdump this PID produced so JIT symbols resolve.
    JD="$(ls -t "$JITDIR"/pcsx2-perf-*/jit-*.dump 2>/dev/null | head -1 || true)"
    if [[ -n "$JD" ]]; then cp "$JD" "$RUNDIR/jit.dump"; fi
    ( cd "$RUNDIR" && perf inject --jit -i perf.data -o perf.jit.data 2>/dev/null ) || \
      cp "$RUNDIR/perf.data" "$RUNDIR/perf.jit.data"
    perf report -i "$RUNDIR/perf.jit.data" --stdio --percent-limit 0 -g none \
      > "$RUNDIR/report.txt" 2>/dev/null || true
    python3 "$HERE/bucket_perf.py" --json "$RUNDIR/report.txt" > "$RUNDIR/buckets.json" || true
    # Progress line: report samples + the top bucket from buckets.json (NOT a grep of
    # report.txt — the @BUCKET@ markers only exist in bucket_perf.py's table mode).
    SAMP="samples=$(awk '/^# Samples:/{print $3; exit}' "$RUNDIR/report.txt" 2>/dev/null)"
    TOP="$(python3 -c "import json;b=json.load(open('$RUNDIR/buckets.json'))['buckets'];k=max(b,key=b.get);print(f'{k} {b[k]:.0f}%')" 2>/dev/null || echo '?')"
    echo "   wallclock=$(cat "$RUNDIR/wallclock.txt")s  $SAMP  top=$TOP"
  done

  # Median wallclock + median per-bucket share across the K runs -> summary.md.
  python3 - "$SDIR" "$REND" "$SCENE" "$LABEL" "$DEVICE" "$FREQ" "$FRAMES" "$RUNS" "$CYCLES_EVENT" <<'PY'
import json, glob, os, statistics, sys
sdir, rend, scene, label, device, freq, frames, runs, event = sys.argv[1:10]
bj = sorted(glob.glob(os.path.join(sdir, "run*", "buckets.json")))
wc = []
for d in sorted(glob.glob(os.path.join(sdir, "run*"))):
    p = os.path.join(d, "wallclock.txt")
    if os.path.exists(p):
        try: wc.append(float(open(p).read().strip()))
        except ValueError: pass
agg = {}
for f in bj:
    try: data = json.load(open(f))
    except Exception: continue
    for k, v in data.get("buckets", {}).items():
        agg.setdefault(k, []).append(v)
med = {k: statistics.median(v) for k, v in agg.items() if v}
ranked = sorted(med.items(), key=lambda kv: kv[1], reverse=True)
med_wc = statistics.median(wc) if wc else float("nan")
out = os.path.join(sdir, "summary.md")
with open(out, "w") as o:
    o.write(f"# {label} — {scene} (renderer={rend})\n\n")
    o.write(f"- device: **{device}**  · runs: {runs}  · frames: {frames}  · perf -F {freq} -e {event}\n")
    o.write(f"- median wallclock (incl. startup+savestate-load): **{med_wc:.3f} s**\n")
    o.write(f"- ⚠️ audio OUTPUT excluded (eerunner forces SPU2 Backend=Null); the SPU2 DSP core still runs and is counted.\n")
    o.write(f"- renderer note: `vk` = whole-system (on M2 the GPU-driver/kernel buckets are the host Asahi stack, NOT an SD865 proxy); `null` = CPU-emulation shape with the GS thread dropped (closest to the SD865-relevant CPU cost, but GIF/PATH3 under-consumed so VU1/XGKICK are distorted).\n\n")
    o.write("| bucket | median self% |\n|---|---|\n")
    for k, v in ranked:
        o.write(f"| {k} | {v:.2f} |\n")
    if med.get("unattributed", 0) > 5:
        o.write(f"\n⚠️ unattributed {med['unattributed']:.1f}% > 5% — tune bucket_perf.py NATIVE_RULES.\n")
print(f"wrote {out}")
PY
done
echo "done. summaries under $OUT/$DEVICE/$SCENE/"
