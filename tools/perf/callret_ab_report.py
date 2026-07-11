#!/usr/bin/env python3
"""Analyze a callret_ab.sh results directory.

Per game: paired A (callret0) vs B (callret1) comparison of
  - wall seconds per 1000 frames (PerfLog line; THE throughput metric)
  - process cycles, instructions, IPC
  - BrMis(r22)/kinsn and return/branch mix (r79/r21)
Reports per-round values, medians, and the median A->B delta. Positive
wall/cycles delta = callret is SLOWER; negative = faster.
"""
import re
import sys
import statistics
from collections import defaultdict
from pathlib import Path

def parse_stat(path):
    # Event names arrive plain on ARMv8 PMU ("r22") but wrapped on Apple
    # Silicon ("apple_avalanche_pmu/r22/u", one line per cluster) — normalize
    # to the inner name and keep the LARGEST count (the big cluster; the
    # little-cluster line is residue from before taskset kicked in).
    ev = {}
    for line in path.read_text().splitlines():
        m = re.match(r"\s*([\d,]+)\s+(\S+)", line)
        if m:
            name = m.group(2)
            inner = re.match(r"(?:[\w-]+/)?([\w-]+?)(?:/u)?$", name)
            if inner:
                name = inner.group(1)
            count = int(m.group(1).replace(",", ""))
            ev[name] = max(ev.get(name, 0), count)
        m = re.search(r"([\d.]+)\s+seconds time elapsed", line)
        if m:
            ev["elapsed"] = float(m.group(1))
    return ev

def parse_log(path):
    m = None
    for line in path.read_text(errors="replace").splitlines():
        m2 = re.search(r"PerfLog session: (\d+) frames in ([\d.]+)s wall", line)
        if m2:
            m = (int(m2.group(1)), float(m2.group(2)))
    return m

def main(outdir):
    outdir = Path(outdir)
    runs = defaultdict(dict)  # (game, arm, round) -> metrics
    for stat in sorted(outdir.glob("*.stat")):
        m = re.match(r"(.+)-([AB])-r(\d+)$", stat.stem)
        if not m:
            continue
        game, arm, rnd = m.group(1), m.group(2), int(m.group(3))
        ev = parse_stat(stat)
        log = outdir / f"{stat.stem}.log"
        perflog = parse_log(log) if log.exists() else None
        row = {}
        if "elapsed" in ev:
            row["elapsed"] = ev["elapsed"]
        if perflog and perflog[0]:
            row["wall_per_kframe"] = 1000.0 * perflog[1] / perflog[0]
            row["frames"] = perflog[0]
        insns = ev.get("instructions")
        if insns:
            row["cycles"] = ev.get("cycles")
            row["ipc"] = ev.get("cycles") and insns / ev["cycles"]
            row["insns"] = insns
            if "r22" in ev:
                row["brmis_kinsn"] = 1000.0 * ev["r22"] / insns
            if "r79" in ev and "r21" in ev and ev["r21"]:
                row["ret_share"] = 100.0 * ev["r79"] / ev["r21"]
        runs[(game, arm)][rnd] = row

    games = sorted({g for g, _ in runs})
    for game in games:
        a_rounds, b_rounds = runs.get((game, "A"), {}), runs.get((game, "B"), {})
        common = sorted(set(a_rounds) & set(b_rounds))
        if not common:
            print(f"{game}: no paired rounds")
            continue
        print(f"\n== {game} ({len(common)} paired rounds) ==")
        print(f"{'metric':18s} {'A (base)':>12s} {'B (callret)':>12s} {'delta':>8s}")
        for key, fmt, scale in (
            ("wall_per_kframe", "%.2f", "s/kframe"),
            ("elapsed", "%.3f", "s (incl. startup)"),
            ("cycles", "%.3e", ""),
            ("insns", "%.3e", ""),
            ("ipc", "%.3f", ""),
            ("brmis_kinsn", "%.3f", "/kinsn"),
            ("ret_share", "%.2f", "% of br"),
        ):
            av = [a_rounds[r][key] for r in common if key in a_rounds[r]]
            bv = [b_rounds[r][key] for r in common if key in b_rounds[r]]
            if not av or not bv:
                continue
            am, bm = statistics.median(av), statistics.median(bv)
            delta = 100.0 * (bm - am) / am if am else 0.0
            print(f"{key:18s} {fmt % am:>12s} {fmt % bm:>12s} {delta:+7.2f}%  {scale}")
        # per-round wall for eyeballing drift
        wa = [(r, a_rounds[r].get("wall_per_kframe")) for r in common]
        wb = [(r, b_rounds[r].get("wall_per_kframe")) for r in common]
        if all(v for _, v in wa) and all(v for _, v in wb):
            print("  per-round s/kframe  A: " + "  ".join(f"r{r}:{v:.2f}" for r, v in wa))
            print("                      B: " + "  ".join(f"r{r}:{v:.2f}" for r, v in wb))

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    main(sys.argv[1])
