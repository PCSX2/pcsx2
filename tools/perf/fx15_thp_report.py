#!/usr/bin/env python3
# fx15_thp_report.py — summarize an fx15_thp_ab.sh output dir.
#
# Per (game, arm): mean/stdev of wall (PerfLog session line — the throughput
# truth), cycles, instructions, IPC, and the TLB mechanism events per kinsn
# (r02 L1I_TLB_REFILL, r35 ITLB_WALK, r34 DTLB_WALK, r23 STALL_FRONTEND).
# Then B (THP) vs A (YAPS2_NO_THP=1) deltas. Counters are whole-process;
# ABBA interleave + identical boot work make the delta meaningful.
#
# usage: fx15_thp_report.py <outdir>

import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path

EVENTS = ["cycles", "instructions", "r02", "r35", "r34", "r23"]
EVENT_LABEL = {
    "r02": "L1I_TLB_REFILL",
    "r35": "ITLB_WALK",
    "r34": "DTLB_WALK",
    "r23": "STALL_FRONTEND",
}


def parse_stat(path):
    vals = {}
    for line in path.read_text().splitlines():
        m = re.match(r"\s*([\d,]+)\s+(\S+)", line)
        if m and m.group(2) in EVENTS:
            vals[m.group(2)] = int(m.group(1).replace(",", ""))
        m = re.match(r"\s*([\d.]+)\s+seconds time elapsed", line)
        if m:
            vals["elapsed"] = float(m.group(1))
    return vals


def parse_wall(logpath):
    if not logpath.exists():
        return None
    m = None
    for line in logpath.read_text(errors="replace").splitlines():
        mm = re.search(r"PerfLog session: (\d+) frames in ([\d.]+)s wall", line)
        if mm:
            m = float(mm.group(2))
    return m


def main():
    outdir = Path(sys.argv[1])
    runs = defaultdict(list)  # (game, arm) -> [dict]
    for stat in sorted(outdir.glob("*.stat")):
        m = re.match(r"(.+)-([AB])-r(\d+)\.stat", stat.name)
        if not m:
            continue
        game, arm = m.group(1), m.group(2)
        vals = parse_stat(stat)
        wall = parse_wall(stat.with_suffix(".log"))
        if wall is not None:
            vals["wall"] = wall
        if "cycles" in vals:
            runs[(game, arm)].append(vals)

    games = sorted({g for g, _ in runs})
    for game in games:
        print(f"\n=== {game} ===")
        agg = {}
        for arm in "AB":
            rr = runs.get((game, arm), [])
            if not rr:
                continue
            a = {}
            for k in ["wall", "elapsed", *EVENTS]:
                xs = [r[k] for r in rr if k in r]
                if xs:
                    a[k] = (statistics.mean(xs),
                            statistics.stdev(xs) if len(xs) > 1 else 0.0)
            agg[arm] = a
            label = "A thp-off" if arm == "A" else "B thp-on "
            ki = a["instructions"][0] / 1e3
            per_ki = " ".join(
                f"{EVENT_LABEL[e]}={a[e][0] / ki:.3f}"
                for e in ["r02", "r35", "r34"] if e in a)
            stallpct = 100.0 * a["r23"][0] / a["cycles"][0] if "r23" in a else 0
            wall = f"wall={a['wall'][0]:.2f}s±{a['wall'][1]:.2f}" if "wall" in a else ""
            print(f"  {label} n={len(rr)} {wall} cycles={a['cycles'][0]/1e9:.3f}G "
                  f"IPC={a['instructions'][0]/a['cycles'][0]:.3f} "
                  f"| per-kinsn: {per_ki} | stall_fe={stallpct:.1f}%cyc")
        if "A" in agg and "B" in agg:
            def delta(k):
                return 100.0 * (agg["B"][k][0] - agg["A"][k][0]) / agg["A"][k][0]
            parts = [f"wall {delta('wall'):+.2f}%"] if "wall" in agg["A"] and "wall" in agg["B"] else []
            parts += [f"cycles {delta('cycles'):+.2f}%"]
            parts += [f"{EVENT_LABEL[e]} {delta(e):+.1f}%"
                      for e in ["r02", "r35", "r34", "r23"]
                      if e in agg["A"] and e in agg["B"]]
            print(f"  B-vs-A: {'  '.join(parts)}")

    thps = sorted(outdir.glob("*.thp"))
    if thps:
        print("\nTHP probes (arm B, round 1):")
        for t in thps:
            total = [l for l in t.read_text().splitlines() if l.startswith("total-kB")]
            print(f"  {t.name}: {total[0] if total else 'no data'}")


if __name__ == "__main__":
    main()
