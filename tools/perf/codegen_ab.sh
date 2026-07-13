#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0+
#
# codegen_ab.sh — deterministic A/B of TWO eerunner binaries that differ ONLY in
# emitted JIT code, over a fixed scene. Reports retired-instructions and cycles
# deltas (median over N runs) plus IPC. This is THE metric for comparing two
# codegen variants on SD865 (and M2); profile_run.sh's wallclock is NOT.
#
# WHY THIS EXISTS / WHEN TO USE IT (see README.md "Codegen A/B"):
#   profile_run.sh's perf-record WALLCLOCK is invalid for comparing two binaries.
#   perf's sampling interrupts perturb the async EE<->MTVU<->MTGS sync pacing in a
#   BINARY-DEPENDENT way, manufacturing phantom wallclock deltas — a reproducible,
#   thermal-controlled +11% "regression" that was pure measurement artifact (see
#   memory feedback_sd865_codegen_ab_use_instructions_not_wallclock). No-perf
#   wallclock is too noisy (~±15% session drift) to resolve a <10% codegen effect.
#   The DETERMINISTIC metric is retired instructions via `perf stat`, drift-immune
#   and reproducible to <0.1%. Pair with cycles for IPC: instructions up but cycles
#   flat => the OoO A77 absorbed the extra code density (the EE-density-is-neutral
#   lesson). A pure-density change should show |Δinsns| > |Δcycles|.
#
# SHELL GOTCHA baked in: savestate/ISO paths contain spaces ("... (USA).iso"). They
#   are inlined QUOTED into the perf-stat command, NEVER via an unquoted variable —
#   word-splitting fast-fails the binary and `perf stat` then reports counts from the
#   ~0.01s startup window (a garbage ~tens-of-millions count that looks real). The
#   sanity floor below rejects any per-run instruction count < 1e8 for exactly this.
#
# Usage (run ON the target device, like profile_run.sh):
#   tools/perf/codegen_ab.sh --device sd865 --scene sotc-01 \
#       --base pcsx2-eerunner-base --new pcsx2-eerunner-new --runs 3
#   # --base/--new: an absolute path, or a bare name resolved against device BIN_DIR.
#   # --renderer null (default) is the most deterministic; use vk only if the change
#   #   touches GS-feeding (GIF/PATH3/XGKICK) codegen.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

SCENE=""
DEVICE=""
BASE=""
NEW=""
RUNS=3
RENDERER="null"       # null = most deterministic for codegen A/B
FRAMES_OVERRIDE=""
OUT="$HOME/pcsx2-profiles"

die() { echo "error: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scene)    SCENE="$2"; shift 2;;
    --device)   DEVICE="$2"; shift 2;;
    --base)     BASE="$2"; shift 2;;
    --new)      NEW="$2"; shift 2;;
    --runs)     RUNS="$2"; shift 2;;
    --renderer) RENDERER="$2"; shift 2;;
    --frames)   FRAMES_OVERRIDE="$2"; shift 2;;
    --out)      OUT="$2"; shift 2;;
    -h|--help)  grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) die "unknown arg: $1";;
  esac
done

[[ -n "$DEVICE" ]] || die "--device <label> required (see tools/perf/devices/)"
[[ -n "$SCENE"  ]] || die "--scene <id> required (see tools/perf/scenes/)"
[[ -n "$BASE"   ]] || die "--base <binary> required"
[[ -n "$NEW"    ]] || die "--new <binary> required"

DEVENV="$HERE/devices/$DEVICE.env"
[[ -f "$DEVENV" ]] || die "no device profile: $DEVENV"
# shellcheck disable=SC1090
source "$DEVENV"   # provides BIN_DIR, CYCLES_EVENT, optional PIN/ISO_ROOT, exports HOME/LD_LIBRARY_PATH
: "${BIN_DIR:?devices/$DEVICE.env must set BIN_DIR}"

SCENEENV="$HERE/scenes/$SCENE.env"
[[ -f "$SCENEENV" ]] || die "no scene profile: $SCENEENV"
# shellcheck disable=SC1090
source "$SCENEENV"  # provides ISO, SAVESTATE, FRAMES, LABEL
: "${ISO:?scenes/$SCENE.env must set ISO}"
: "${SAVESTATE:?scenes/$SCENE.env must set SAVESTATE}"
: "${FRAMES:=300}"
[[ -n "$FRAMES_OVERRIDE" ]] && FRAMES="$FRAMES_OVERRIDE"

# Resolve binary refs: bare name -> BIN_DIR/<name>, else use the path as given.
resolve_bin() { case "$1" in */*) echo "$1";; *) echo "$BIN_DIR/$1";; esac; }
BASE_BIN="$(resolve_bin "$BASE")"
NEW_BIN="$(resolve_bin "$NEW")"

command -v perf >/dev/null || die "perf not found"
[[ -x "$BASE_BIN" ]] || die "base binary not executable: $BASE_BIN"
[[ -x "$NEW_BIN"  ]] || die "new binary not executable: $NEW_BIN"
[[ -f "$ISO" ]]       || die "ISO not found: $ISO"
[[ -f "$SAVESTATE" ]] || die "savestate not found: $SAVESTATE"

OUTDIR="$OUT/$DEVICE/codegen-ab/$SCENE"
mkdir -p "$OUTDIR"
RESULTS="$OUTDIR/results.tsv"
: > "$RESULTS"   # tag \t run \t instructions \t cycles

echo "== codegen A/B [$LABEL · $SCENE] renderer=$RENDERER frames=$FRAMES runs=$RUNS =="
echo "   base = $BASE_BIN"
echo "   new  = $NEW_BIN"
echo "   metric = perf stat -e instructions,cycles --inherit (deterministic; wallclock is NOT used)"

# Hottest thermal zone in °C (integer), or "?" when unavailable. Logged per
# run so a fan/thermal confound is visible in the record instead of silent.
read_temp_c() {
  local t
  t="$(cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | sort -rn | head -1)"
  [[ -n "$t" ]] && echo $((t / 1000)) || echo "?"
}

# Run ONE measurement of one binary under perf stat; parse retired
# instructions + cycles. Callers interleave base/new (see the run loop below):
# an all-base-then-all-new order lets a session-long thermal ramp (fan
# profile, heat soak) correlate with binary identity and masquerade as a
# codegen delta in the cycles metric. Interleaving decorrelates it;
# instructions were never exposed (retired counts are clock-independent).
# NOTE: $PIN is an intentional word-split command prefix (e.g. `taskset -c 4-7`);
# the savestate/ISO are inlined QUOTED so their spaces never split (the gotcha).
run_one() {
  local tag="$1" bin="$2" k="$3"
  local stat="$OUTDIR/$tag.run$k.stat.txt"
  local runlog="$OUTDIR/$tag.run$k.out.log"
  local temp_c; temp_c="$(read_temp_c)"
    EERUNNER_SYNCMTGS=0 EERUNNER_MTVU=1 EERUNNER_EE=jit \
      perf stat -e instructions,cycles --inherit -- \
        ${PIN:-} "$bin" --liverun --renderer "$RENDERER" --frames "$FRAMES" \
                --savestate "$SAVESTATE" --iso "$ISO" \
        > "$runlog" 2> "$stat" || true
    # EE-thread CPU seconds from the runner's @THREADCPU@ shutdown lines.
    # The process-wide perf counts above include the GS/MTVU/worker threads,
    # which DILUTE an EE-only codegen delta by the EE thread's share of
    # process cycles (~50% measured on SotC/null) — an EE-thread-scoped
    # secondary metric keeps effect sizes honest. (2026-07-06 audit.)
    local ee_s
    ee_s="$(awk -F': ' '/@THREADCPU@ CPU Thread:/ { gsub(/ s$/,"",$2); if ($2+0 > m) m=$2+0 } END { printf "%.2f", m }' "$runlog")"
    # EE-thread-SCOPED hardware counters from the runner's @THREADPERF@ lines
    # (per-tid perf_event_open over the frame window). THE primary metric since
    # 2026-07-12: deterministic like process insns, but undiluted by GS/MTVU
    # threads and excluding boot/savestate-load work. Zero when unsupported
    # (old binary / perf_event_paranoid) — summary falls back to process-wide.
    # (max across same-named threads: spawned helpers can inherit the "CPU Thread"
    # comm; the EE core is always the one with the most retired instructions)
    local ee_ins ee_cyc
    ee_ins="$(awk '/@THREADPERF@ CPU Thread:/ { for (i=1;i<=NF;i++) if ($i ~ /^instructions=/) { v=$i; sub(/^instructions=/,"",v); if (v+0 > m) m=v+0 } } END { printf "%.0f", m }' "$runlog")"
    ee_cyc="$(awk '/@THREADPERF@ CPU Thread:/ { for (i=1;i<=NF;i++) if ($i ~ /^cycles=/) { v=$i; sub(/^cycles=/,"",v); if (v+0 > m) m=v+0 } } END { printf "%.0f", m }' "$runlog")"
    # perf stat writes counts to stderr. Anchor on the EVENT token ($2), so the
    # "insn per cycle" comment on the instructions line can't be misread as cycles.
    # On multi-PMU boxes (Apple M2: apple_avalanche_pmu/instructions/u +
    # apple_blizzard_pmu/instructions/u) the generic event expands to one line per
    # PMU; take the FIRST numeric match — perf lists the P-core PMU first, and the
    # pinned emulator threads make the E-core count sub-1% noise. On sd865 the
    # event is a bare "instructions"/"cycles" token, matched the same way.
    local parsed ins cyc
    parsed="$(awk '
      { v=$1; gsub(/,/,"",v) }
      $2 ~ /(^|\/)instructions([:\/]|$)/ && v ~ /^[0-9]+$/ && ins=="" { ins=v }
      $2 ~ /(^|\/)cycles([:\/]|$)/        && v ~ /^[0-9]+$/ && cyc=="" { cyc=v }
      END { printf "%s %s", (ins==""?0:ins), (cyc==""?0:cyc) }
    ' "$stat")"
    ins="${parsed% *}"; cyc="${parsed#* }"
    # Sanity floor: a real liverun retires billions of instructions. A count this
    # small means the binary fast-failed (word-split args / bad path / crash) and
    # perf counted only its startup window — NOT a real measurement.
    if [[ "${ins:-0}" -lt 100000000 ]]; then
      echo "   $tag run$k: FAST-FAIL (instructions=$ins < 1e8). Check $stat (word-split? bad path? crash)." >&2
      tail -n 20 "$stat" >&2 || true
      die "aborting: $tag run$k did not run a real workload"
    fi
    # A missing @THREADCPU@ line means the VM never actually ran (e.g. GS init
    # failure: renderer rejected over ssh) — startup alone can retire billions of
    # instructions on a slow core, sailing past the 1e8 floor while measuring
    # nothing. The shutdown thread-CPU report only prints after a real run.
    if ! awk "BEGIN{exit !(${ee_s:-0} > 0)}"; then
      echo "   $tag run$k: FAST-FAIL (no @THREADCPU@ EE-thread line — VM never ran). Check $runlog." >&2
      tail -n 15 "$runlog" >&2 || true
      die "aborting: $tag run$k did not boot the VM (GS init failure?)"
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$tag" "$k" "$ins" "$cyc" "${ee_s:-0}" "${ee_ins:-0}" "${ee_cyc:-0}" >> "$RESULTS"
    awk -v t="$tag" -v k="$k" -v i="$ins" -v c="$cyc" -v tc="$temp_c" -v e="${ee_s:-?}" \
        -v ei="${ee_ins:-0}" -v ec="${ee_cyc:-0}" \
      'BEGIN{printf "   %-4s run%s: EE insns=%.3fB cycles=%.3fB | proc insns=%.3fB cycles=%.3fB | ee_thread=%ss temp=%s°C\n", t, k, ei/1e9, ec/1e9, i/1e9, c/1e9, e, tc}'
}

# Interleaved run order (base,new,base,new,...) — see run_one for why.
for k in $(seq 1 "$RUNS"); do
  run_one base "$BASE_BIN" "$k"
  run_one new  "$NEW_BIN"  "$k"
done

# Median per tag + deltas -> summary.md.
python3 - "$OUTDIR" "$RESULTS" "$LABEL" "$SCENE" "$DEVICE" "$RENDERER" "$FRAMES" "$RUNS" "$BASE_BIN" "$NEW_BIN" <<'PY'
import statistics, sys, os
outdir, results, label, scene, device, rend, frames, runs, base_bin, new_bin = sys.argv[1:11]
rows = {"base": {"ins": [], "cyc": [], "ee": [], "eins": [], "ecyc": []},
        "new":  {"ins": [], "cyc": [], "ee": [], "eins": [], "ecyc": []}}
for line in open(results):
    parts = line.rstrip("\n").split("\t")
    if len(parts) not in (4, 5, 7): continue
    tag, _k, ins, cyc = parts[:4]
    if tag in rows:
        rows[tag]["ins"].append(int(ins)); rows[tag]["cyc"].append(int(cyc))
        if len(parts) >= 5 and float(parts[4]) > 0:
            rows[tag]["ee"].append(float(parts[4]))
        if len(parts) == 7:
            if int(parts[5]) > 0: rows[tag]["eins"].append(int(parts[5]))
            if int(parts[6]) > 0: rows[tag]["ecyc"].append(int(parts[6]))

def med(xs): return statistics.median(xs) if xs else float("nan")
def delta(b, n): return (n / b - 1) * 100 if b == b and n == n and b else float("nan")
b_ins, b_cyc = med(rows["base"]["ins"]), med(rows["base"]["cyc"])
n_ins, n_cyc = med(rows["new"]["ins"]),  med(rows["new"]["cyc"])
d_ins, d_cyc = delta(b_ins, n_ins), delta(b_cyc, n_cyc)
b_ipc = b_ins / b_cyc if b_cyc else float("nan")
n_ipc = n_ins / n_cyc if n_cyc else float("nan")
be_ins, be_cyc = med(rows["base"]["eins"]), med(rows["base"]["ecyc"])
ne_ins, ne_cyc = med(rows["new"]["eins"]),  med(rows["new"]["ecyc"])
de_ins, de_cyc = delta(be_ins, ne_ins), delta(be_cyc, ne_cyc)
have_ee_hw = be_ins == be_ins and ne_ins == ne_ins  # NaN-safe

# Per-tag run spread (max-min)/median, to flag if determinism slipped.
def spread(xs): return (max(xs) - min(xs)) / statistics.median(xs) * 100 if xs else float("nan")

out = os.path.join(outdir, "summary.md")
with open(out, "w") as o:
    o.write(f"# Codegen A/B — {label} · {scene} (renderer={rend})\n\n")
    o.write(f"- device: **{device}**  · runs: {runs}  · frames: {frames}\n")
    o.write(f"- base: `{base_bin}`\n- new:  `{new_bin}`\n\n")
    if have_ee_hw:
        o.write("**Primary metric: EE-thread-scoped retired instructions** (@THREADPERF@ per-tid "
                "perf_event_open over the frame window — undiluted by GS/MTVU threads, excludes boot).\n\n")
        o.write("| EE thread | base (median) | new (median) | Δ |\n|---|---|---|---|\n")
        o.write(f"| instructions | {be_ins/1e9:.3f}B | {ne_ins/1e9:.3f}B | **{de_ins:+.2f}%** |\n")
        o.write(f"| cycles       | {be_cyc/1e9:.3f}B | {ne_cyc/1e9:.3f}B | **{de_cyc:+.2f}%** |\n")
        o.write(f"| IPC          | {be_ins/be_cyc:.3f} | {ne_ins/ne_cyc:.3f} | {delta(be_ins/be_cyc, ne_ins/ne_cyc):+.2f}% |\n\n")
    o.write("| whole process | base (median) | new (median) | Δ |\n|---|---|---|---|\n")
    o.write(f"| instructions | {b_ins/1e9:.3f}B | {n_ins/1e9:.3f}B | {d_ins:+.2f}% |\n")
    o.write(f"| cycles       | {b_cyc/1e9:.3f}B | {n_cyc/1e9:.3f}B | {d_cyc:+.2f}% |\n")
    o.write(f"| IPC          | {b_ipc:.3f} | {n_ipc:.3f} | {(n_ipc/b_ipc-1)*100:+.2f}% |\n")
    b_ee, n_ee = med(rows["base"]["ee"]), med(rows["new"]["ee"])
    if b_ee == b_ee and n_ee == n_ee and b_ee:  # NaN-safe
        d_ee = (n_ee / b_ee - 1) * 100
        o.write(f"| EE-thread CPU s | {b_ee:.2f} | {n_ee:.2f} | {d_ee:+.2f}% |\n")
    if not have_ee_hw:
        o.write(f"\n- ⚠ no @THREADPERF@ EE-thread counters in this run (old binary or perf_event_paranoid): "
                f"the whole-process rows above are the only hardware counts, and an EE-only codegen change "
                f"is diluted by the GS/MTVU threads' share of process cycles.\n")
    o.write("\n")
    sp = rows['base']['eins'] if have_ee_hw else rows['base']['ins']
    sn = rows['new']['eins'] if have_ee_hw else rows['new']['ins']
    o.write(f"- run spread (max−min)/median on the primary insns metric: base {spread(sp):.3f}%, "
            f"new {spread(sn):.3f}% "
            f"(want <0.1% — larger means determinism slipped; raise --runs or pin harder).\n")
    pi, pc = (de_ins, de_cyc) if have_ee_hw else (d_ins, d_cyc)
    verdict = ("density-only (cycles absorbed by OoO)" if abs(pc) + 0.3 < abs(pi)
               else "real cycle effect")
    o.write(f"- read: |Δinsns|={abs(pi):.2f}% vs |Δcycles|={abs(pc):.2f}% → **{verdict}**.\n")
print(f"wrote {out}")
if have_ee_hw:
    print(f"  EE-thread: insns {de_ins:+.2f}%  cycles {de_cyc:+.2f}%   (proc: insns {d_ins:+.2f}%, cycles {d_cyc:+.2f}%)")
else:
    print(f"  instructions: {d_ins:+.2f}%   cycles: {d_cyc:+.2f}%   (base IPC {b_ipc:.3f} -> new {n_ipc:.3f})")
PY
echo "done. summary: $OUTDIR/summary.md"
