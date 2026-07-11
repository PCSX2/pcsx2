#!/bin/bash
# callret_ab.sh — interleaved A/B for the EE call-ret shadow stack (P2-2).
#
# Runs two pcsx2-eerunner binaries built from the SAME tip (arm A =
# -DEE_CALLRET_STACK=0 baseline, arm B = default/callret) over the same
# savestate-anchored --liverun workloads, ABBA-interleaved so thermal/clock
# drift cancels, with `perf stat` around each run. Throughput truth = wall
# seconds per N frames (unlimited limiter, headless --renderer null);
# mechanism check = r22 (BR_MIS_PRED_RETIRED) per kinsn.
#
# Usage:
#   callret_ab.sh -a <bin-A> -b <bin-B> -g <games.cfg> [-f frames] [-r rounds]
#                 [-c cpulist] [-o outdir]
#   games.cfg lines: name|savestate|iso
#
# Device notes (SD865/Rocknix): run with -c 4-7 (A77 cluster). If a pwmfan
# hwmon exists the script pins the fan to max and RESTORES auto (enable=2)
# on exit — never leave a manual pin behind. EERUNNER_SYNCMTGS=0 is set for
# every run (bare --liverun wedges at frame 1 without it).
set -u

FRAMES=2000
ROUNDS=4
CPUS=""
OUT="callret-ab-$(date +%m%d-%H%M%S)"
BIN_A="" BIN_B="" GAMES=""

while getopts "a:b:g:f:r:c:o:" opt; do
	case $opt in
		a) BIN_A=$OPTARG ;;
		b) BIN_B=$OPTARG ;;
		g) GAMES=$OPTARG ;;
		f) FRAMES=$OPTARG ;;
		r) ROUNDS=$OPTARG ;;
		c) CPUS=$OPTARG ;;
		o) OUT=$OPTARG ;;
		*) exit 2 ;;
	esac
done
[ -x "$BIN_A" ] && [ -x "$BIN_B" ] && [ -f "$GAMES" ] || {
	echo "usage: $0 -a <bin-A=callret0> -b <bin-B=callret1> -g <games.cfg> [-f frames] [-r rounds] [-c cpus] [-o outdir]" >&2
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

# One event set, no multiplexing on A77 (6 programmable + fixed cycles):
EVENTS="cycles,instructions,r21,r22,r78,r79"
TASKSET=""
[ -n "$CPUS" ] && TASKSET="taskset -c $CPUS"

run_one() { # $1=arm(A|B) $2=bin $3=name $4=state $5=iso $6=round
	local tag="$3-$1-r$6"
	echo "=== $tag ($(date +%H:%M:%S)) ==="
	EERUNNER_SYNCMTGS=0 $TASKSET perf stat -e "$EVENTS" \
		-o "$OUT/$tag.stat" -- \
		"$2" --liverun --renderer null --savestate "$4" --iso "$5" \
		--frames "$FRAMES" > "$OUT/$tag.log" 2>&1
	echo "exit=$? $(grep -o 'PerfLog session:.*' "$OUT/$tag.log" | tail -1)"
	sleep 5 # cool-down between runs
}

for round in $(seq 1 "$ROUNDS"); do
	# ABBA: odd rounds run A first, even rounds B first.
	if [ $((round % 2)) -eq 1 ]; then order="A B"; else order="B A"; fi
	for arm in $order; do
		[ "$arm" = A ] && bin=$BIN_A || bin=$BIN_B
		while IFS='|' read -r name state iso; do
			case $name in ''|\#*) continue ;; esac
			run_one "$arm" "$bin" "$name" "$state" "$iso" "$round"
		done < "$GAMES"
	done
done

echo "done — analyze with: python3 tools/perf/callret_ab_report.py $OUT"
