#!/bin/bash
# fx15_thp_ab.sh — interleaved A/B for FX-15 (MADV_HUGEPAGE on the JIT code caches).
#
# ONE pcsx2-eerunner binary, two arms: A = YAPS2_NO_THP=1 (4K pages, baseline),
# B = default (THP madvise on). Same savestate-anchored --liverun workloads as
# callret_ab.sh, ABBA-interleaved so thermal/clock drift cancels, perf stat
# around each run. Throughput truth = wall seconds per N frames (unlimited
# limiter, headless --renderer null); mechanism check = ITLB_WALK (r35) and
# L1I_TLB_REFILL (r02) per kinsn.
#
# During the first B round of each game a background probe snapshots
# AnonHugePages from /proc/<pid>/smaps for the JIT arena (VA >= 0x100000000)
# into <out>/<tag>.thp — if those are all zero the A/B is null by construction
# (madvise didn't take) and the numbers mean nothing.
#
# Usage:
#   fx15_thp_ab.sh -b <eerunner> -g <games.cfg> [-f frames] [-r rounds]
#                  [-c cpulist] [-o outdir]
#   games.cfg lines: name|savestate|iso
#
# Device notes (SD865/Rocknix): run with -c 4-7 (A77 cluster). Fan pinned to
# max and RESTORED to auto on exit. EERUNNER_SYNCMTGS=0 always (bare --liverun
# wedges at frame 1 without it).
set -u

FRAMES=2000
ROUNDS=4
CPUS=""
OUT="fx15-thp-ab-$(date +%m%d-%H%M%S)"
BIN="" GAMES=""

while getopts "b:g:f:r:c:o:" opt; do
	case $opt in
		b) BIN=$OPTARG ;;
		g) GAMES=$OPTARG ;;
		f) FRAMES=$OPTARG ;;
		r) ROUNDS=$OPTARG ;;
		c) CPUS=$OPTARG ;;
		o) OUT=$OPTARG ;;
		*) exit 2 ;;
	esac
done
[ -x "$BIN" ] && [ -f "$GAMES" ] || {
	echo "usage: $0 -b <eerunner> -g <games.cfg> [-f frames] [-r rounds] [-c cpus] [-o outdir]" >&2
	exit 2
}
mkdir -p "$OUT"

# --- device pinning (no-op off-device) --------------------------------------
FAN=""
restore() {
	if [ -n "$FAN" ]; then
		echo 2 > "$FAN/pwm1_enable" 2>/dev/null
		echo "fan restored to auto ($FAN)"
	fi
}
trap restore EXIT
for h in /sys/class/hwmon/hwmon*; do
	[ -f "$h/name" ] && [ "$(cat "$h/name" 2>/dev/null)" = "pwmfan" ] || continue
	FAN=$h
	echo 1 > "$FAN/pwm1_enable" && echo 255 > "$FAN/pwm1"
	echo "fan pinned to max ($FAN)"
done
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
	echo performance > "$g" 2>/dev/null
done
echo "global THP mode: $(cat /sys/kernel/mm/transparent_hugepage/enabled 2>/dev/null)" | tee "$OUT/thp-mode.txt"

# Rocknix device: the eerunner binaries need the bundled libs (libjpeg/lz4).
[ -d /storage/pcsx2/lib ] && export LD_LIBRARY_PATH=/storage/pcsx2/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

# TLB event set (A77: 6 programmable + fixed cycles, no multiplexing):
#   r02 L1I_TLB_REFILL, r35 ITLB_WALK, r34 DTLB_WALK, r23 STALL_FRONTEND
EVENTS="cycles,instructions,r02,r35,r34,r23"
TASKSET=""
[ -n "$CPUS" ] && TASKSET="taskset -c $CPUS"

probe_thp() { # $1=outfile — sample JIT-arena AnonHugePages of the live run
	sleep 25
	local pid
	pid=$(pgrep -n -f pcsx2-eerunner)
	[ -n "$pid" ] || { echo "no pid" > "$1"; return; }
# JIT arena = 0x1_00000000..: 9-hex-digit VMA start beginning with 1.
	# (busybox awk: no strtonum)
	awk '/^[0-9a-f]+-[0-9a-f]+ /{vma=$1; keep=(index($1,"-")==10 && substr($1,1,1)=="1")}
	     /^AnonHugePages:/{if (keep && $2 > 0) print vma, $0}' \
		"/proc/$pid/smaps" > "$1" 2>/dev/null
	echo "total-kB $(awk '{s+=$3} END{print s+0}' "$1")" >> "$1"
}

run_one() { # $1=arm(A|B) $2=name $3=state $4=iso $5=round
	local tag="$2-$1-r$5" envA=""
	[ "$1" = A ] && envA="YAPS2_NO_THP=1"
	echo "=== $tag ($(date +%H:%M:%S)) ==="
	if [ "$1" = B ] && [ "$5" = 1 ]; then
		probe_thp "$OUT/$tag.thp" &
	fi
	env $envA EERUNNER_SYNCMTGS=0 $TASKSET perf stat -e "$EVENTS" \
		-o "$OUT/$tag.stat" -- \
		"$BIN" --liverun --renderer null --savestate "$3" --iso "$4" \
		--frames "$FRAMES" > "$OUT/$tag.log" 2>&1
	echo "exit=$? $(grep -o 'PerfLog session:.*' "$OUT/$tag.log" | tail -1)"
	sleep 5 # cool-down between runs
}

for round in $(seq 1 "$ROUNDS"); do
	# ABBA: odd rounds run A first, even rounds B first.
	if [ $((round % 2)) -eq 1 ]; then order="A B"; else order="B A"; fi
	for arm in $order; do
		while IFS='|' read -r name state iso; do
			case $name in ''|\#*) continue ;; esac
			run_one "$arm" "$name" "$state" "$iso" "$round"
		done < "$GAMES"
	done
done
wait
echo "done — stats in $OUT/*.stat, THP probes in $OUT/*-B-r1.thp"
