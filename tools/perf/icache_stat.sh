#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0+
#
# icache_stat.sh — EE-thread-scoped icache / frontend PMU characterization of ONE
# binary over a fixed scene. This is the Phase-1a tool of the icache-locality
# campaign (tools/perf/icache-2026-07.md): it answers "is icache locality a wall
# AT ALL on this device?", which no codegen A/B delta can (those only measure
# changes, not absolute shares).
#
# Method: launch the runner, find the EE thread ("CPU Thread") by comm, attach
# `perf stat -t <tid>` for a fixed window, kill the runner. Two complementary
# event groups run in separate passes so nothing multiplexes:
#   stalls: cycles,instructions,STALL_FRONTEND(r23),STALL_BACKEND(r24)
#   l1i:    cycles,instructions,L1I_CACHE_REFILL(r01),L1I_CACHE(r14),
#           L1I_TLB_REFILL(r02),BR_MIS_PRED(r10)
# Derived: IPC, frontend/backend stall shares, L1I MPKI + miss rate, iTLB MPKI,
# branch-mispredicts/kinsn. A77/A55 support all of these; A53 (rk3562) may not
# implement r23/r24 — those cells degrade to NA, the L1I group still works.
#
# --delay chooses the window: default 8s after the EE thread appears (steady
# state, past the savestate-load compile storm); --delay 0 measures the storm
# itself (compile/link/flush burst) — capture BOTH when filling in the campaign
# gate table.
#
# Same operational discipline as codegen_ab.sh: run ON the device, pin the fan
# + governor first (memory: feedback_sd865_ab_pin_the_fan), paths with spaces
# stay quoted, and a sanity floor rejects windows whose instruction count says
# the runner fast-failed.
#
# Usage (on the target device):
#   tools/perf/icache_stat.sh --device sd865 --scene sotc-01 --bin pcsx2-eerunner-base
#   tools/perf/icache_stat.sh --device sd865 --scene uya-gameplay --bin pcsx2-eerunner-base --delay 0
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SCENE=""
DEVICE=""
BIN=""
RUNS=2
RENDERER="null"
FRAMES_OVERRIDE=""
WINDOW=10          # perf stat attach window, seconds
DELAY=8            # seconds after EE thread appears before attaching (0 = storm)
THREAD_NAME="CPU Thread"
OUT="$HOME/pcsx2-profiles"

die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scene)    SCENE="$2"; shift 2;;
    --device)   DEVICE="$2"; shift 2;;
    --bin)      BIN="$2"; shift 2;;
    --runs)     RUNS="$2"; shift 2;;
    --renderer) RENDERER="$2"; shift 2;;
    --frames)   FRAMES_OVERRIDE="$2"; shift 2;;
    --window)   WINDOW="$2"; shift 2;;
    --delay)    DELAY="$2"; shift 2;;
    --thread)   THREAD_NAME="$2"; shift 2;;
    --out)      OUT="$2"; shift 2;;
    -h|--help)  grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) die "unknown arg: $1";;
  esac
done

[[ -n "$DEVICE" ]] || die "--device <label> required (see tools/perf/devices/)"
[[ -n "$SCENE"  ]] || die "--scene <id> required (see tools/perf/scenes/)"
[[ -n "$BIN"    ]] || die "--bin <binary> required"

DEVENV="$HERE/devices/$DEVICE.env"
[[ -f "$DEVENV" ]] || die "no device profile: $DEVENV"
# shellcheck disable=SC1090
source "$DEVENV"
: "${BIN_DIR:?devices/$DEVICE.env must set BIN_DIR}"

SCENEENV="$HERE/scenes/$SCENE.env"
[[ -f "$SCENEENV" ]] || die "no scene profile: $SCENEENV"
# shellcheck disable=SC1090
source "$SCENEENV"
: "${ISO:?scenes/$SCENE.env must set ISO}"
: "${SAVESTATE:?scenes/$SCENE.env must set SAVESTATE}"
: "${FRAMES:=600}"
[[ -n "$FRAMES_OVERRIDE" ]] && FRAMES="$FRAMES_OVERRIDE"

case "$BIN" in */*) BIN_PATH="$BIN";; *) BIN_PATH="$BIN_DIR/$BIN";; esac
command -v perf >/dev/null || die "perf not found"
[[ -x "$BIN_PATH" ]] || die "binary not executable: $BIN_PATH"
[[ -f "$ISO" ]]       || die "ISO not found: $ISO"
[[ -f "$SAVESTATE" ]] || die "savestate not found: $SAVESTATE"

MODE=$([[ "$DELAY" == "0" ]] && echo storm || echo steady)
OUTDIR="$OUT/$DEVICE/icache/$SCENE/$MODE"
mkdir -p "$OUTDIR"
RESULTS="$OUTDIR/results.tsv"
: > "$RESULTS"   # group \t run \t event \t count

GROUP_NAMES=(stalls l1i)
GROUP_EVENTS=("cycles,instructions,r23,r24" "cycles,instructions,r01,r14,r02,r10")

echo "== icache stat [$LABEL · $SCENE] $MODE: delay=${DELAY}s window=${WINDOW}s renderer=$RENDERER runs=$RUNS =="
echo "   bin = $BIN_PATH   thread = '$THREAD_NAME'"

read_temp_c() {
  local t
  t="$(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | sort -rn | head -1)"
  [[ -n "$t" ]] && echo $((t / 1000)) || echo "?"
}

find_tid() {  # $1 = pid, $2 = thread comm
  local t
  for t in /proc/"$1"/task/*; do
    [[ -r "$t/comm" && "$(cat "$t/comm" 2>/dev/null)" == "$2" ]] && { basename "$t"; return 0; }
  done
  return 1
}

run_one() {
  local gname="$1" events="$2" k="$3"
  local stat="$OUTDIR/$gname.run$k.stat.txt"
  local runlog="$OUTDIR/$gname.run$k.out.log"
  local temp_c; temp_c="$(read_temp_c)"

  # NOTE: $PIN is an intentional word-split prefix; ISO/savestate quoted (spaces).
  EERUNNER_SYNCMTGS=0 EERUNNER_MTVU=1 EERUNNER_EE=jit \
    ${PIN:-} "$BIN_PATH" --liverun --renderer "$RENDERER" --frames "$FRAMES" \
      --savestate "$SAVESTATE" --iso "$ISO" > "$runlog" 2>&1 &
  local pid=$!

  local tid="" i
  for i in $(seq 1 60); do
    tid="$(find_tid "$pid" "$THREAD_NAME")" && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.5
  done
  if [[ -z "$tid" ]]; then
    kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
    die "$gname run$k: EE thread '$THREAD_NAME' never appeared (runner died? see $runlog)"
  fi

  [[ "$DELAY" != "0" ]] && sleep "$DELAY"
  if ! kill -0 "$pid" 2>/dev/null; then
    die "$gname run$k: runner exited before the measurement window (see $runlog)"
  fi

  perf stat -e "$events" -t "$tid" -- sleep "$WINDOW" > /dev/null 2> "$stat"

  kill "$pid" 2>/dev/null
  wait "$pid" 2>/dev/null

  # Parse "count event" lines; "<not supported>/<not counted>" -> NA. Strip
  # the privilege-modifier suffix (":u" when not root) so tokens match.
  awk -v g="$gname" -v k="$k" '
    $2 != "" {
      v=$1; gsub(/,/,"",v)
      ev=$2; sub(/:[a-zA-Z]+$/, "", ev)
      if (v ~ /^[0-9]+$/)            printf "%s\t%s\t%s\t%s\n", g, k, ev, v
      else if ($1 ~ /^<not/) {
        ev=$3; sub(/:[a-zA-Z]+$/, "", ev)
        printf "%s\t%s\t%s\tNA\n", g, k, ev
      }
    }
  ' "$stat" >> "$RESULTS"

  local ins
  ins="$(awk -F'\t' -v g="$gname" -v k="$k" '$1==g && $2==k && $3=="instructions" {print $4}' "$RESULTS")"
  if [[ "${ins:-NA}" == "NA" || "${ins:-0}" -lt 100000000 ]]; then
    echo "   $gname run$k: FAST-FAIL (instructions=${ins:-none} < 1e8 in ${WINDOW}s window)." >&2
    tail -n 20 "$stat" >&2 || true
    die "aborting: $gname run$k did not measure a real workload"
  fi
  echo "   $gname run$k: done (temp=${temp_c}°C, tid=$tid)"
}

for k in $(seq 1 "$RUNS"); do
  for gi in "${!GROUP_NAMES[@]}"; do
    run_one "${GROUP_NAMES[$gi]}" "${GROUP_EVENTS[$gi]}" "$k"
  done
done

# Median per event across runs -> derived metrics -> summary.md.
python3 - "$OUTDIR" "$RESULTS" "$LABEL" "$SCENE" "$DEVICE" "$MODE" "$WINDOW" "$RUNS" "$BIN_PATH" <<'PY'
import statistics, sys, os
outdir, results, label, scene, device, mode, window, runs, bin_path = sys.argv[1:10]
ev = {}
for line in open(results):
    g, k, e, v = line.rstrip("\n").split("\t")
    if v != "NA":
        ev.setdefault((g, e), []).append(int(v))

def med(g, e):
    xs = ev.get((g, e))
    if not xs:
        return None
    m = statistics.median(xs)
    # A53 reports 0 for unimplemented events (r23/r24) instead of
    # <not supported>; a true zero is impossible on a real workload.
    return m if m > 0 else None

def spread(g, e):
    xs = ev.get((g, e))
    return (max(xs) - min(xs)) / statistics.median(xs) * 100 if xs and len(xs) > 1 else 0.0

def fmt(x, unit=""):
    return f"{x:.2f}{unit}" if x is not None else "NA"

rows = []
for g in ("stalls", "l1i"):
    cyc, ins = med(g, "cycles"), med(g, "instructions")
    if not cyc or not ins:
        continue
    rows.append((f"[{g}] IPC", ins / cyc, ""))
    if g == "stalls":
        sf, sb = med(g, "r23"), med(g, "r24")
        rows.append(("frontend-stall share", 100 * sf / cyc if sf is not None else None, "%"))
        rows.append(("backend-stall share", 100 * sb / cyc if sb is not None else None, "%"))
    else:
        l1r, l1a, itlb, brm = med(g, "r01"), med(g, "r14"), med(g, "r02"), med(g, "r10")
        rows.append(("L1I MPKI", 1000 * l1r / ins if l1r is not None else None, ""))
        rows.append(("L1I miss rate", 100 * l1r / l1a if (l1r is not None and l1a) else None, "%"))
        rows.append(("iTLB MPKI (L1I_TLB_REFILL)", 1000 * itlb / ins if itlb is not None else None, ""))
        rows.append(("BR_MIS_PRED per kinsn", 1000 * brm / ins if brm is not None else None, ""))

out = os.path.join(outdir, "summary.md")
with open(out, "w") as o:
    o.write(f"# icache stat — {label} · {scene} · {device} · {mode}\n\n")
    o.write(f"- bin: `{bin_path}` · window {window}s ×{runs} runs, EE-thread-scoped (`perf stat -t`)\n\n")
    o.write("| metric | value |\n|---|---|\n")
    for name, val, unit in rows:
        o.write(f"| {name} | {fmt(val, unit)} |\n")
    o.write(f"\n- run spread instructions: stalls {spread('stalls','instructions'):.1f}%, "
            f"l1i {spread('l1i','instructions'):.1f}% (window-based, not frame-locked — "
            f"expect a few %; this tool measures SHARES, not deltas)\n")
    o.write("- context: L1I 64KB on A77, 32KB on A55/A53. NA = PMU event not supported.\n")
print(f"wrote {out}")
for name, val, unit in rows:
    print(f"  {name:30s} {fmt(val, unit)}")
PY
echo "done. summary: $OUTDIR/summary.md"
