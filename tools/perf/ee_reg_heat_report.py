#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
# SPDX-License-Identifier: GPL-3.0+
"""Aggregate EE register-heat CSVs into the pin-assignment evidence.

Input: eeregheat-*.csv files produced by running yaps2 with
PCSX2_EE_REGHEAT_DIR set (see pcsx2/arm64/EERegHeat.h). Each `B` row is one
compiled block: startpc, insns, exec count, then 4x32 per-GPR static
reference counts (r64, w64, r128, w128).

Output (per input set):
  - dynamic-reference histogram: per guest GPR, exec-weighted 64-bit
    read/write reference estimates (static count x block exec), plus the
    128-bit share (which pins can never serve);
  - the top-N coverage curve (share of dynamic 64-bit refs served by
    pinning the N hottest regs) - the EE analog of the DATE 2025
    access-coverage curve;
  - a pin-ladder proposal for the EE-SRA rung sizes (3/5/9/12), with
    $sp/$ra fixed (already pinned today).

Blocks with exec == 0 never ran and are excluded from weighted views.
Multiple records for one startpc (SMC/reset recompiles) all count: each
record's own exec covers exactly the period its compilation was live.

Usage:
  ee_reg_heat_report.py <csv-or-dir> [<csv-or-dir> ...] [--top N] [--per-file]
"""

import argparse
import os
import sys
from collections import defaultdict

GPR_NAMES = [
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
]

RUNGS = [3, 5, 9, 12]  # EE-SRA pin-ladder totals (incl. $sp/$ra)
ALREADY_PINNED = [29, 31]  # $sp, $ra


class Heat:
    def __init__(self):
        self.r64 = [0] * 32
        self.w64 = [0] * 32
        self.r128 = [0] * 32
        self.w128 = [0] * 32
        self.blocks = 0
        self.blocks_run = 0
        self.execs = 0

    def add_row(self, pc, insns, execn, counts):
        self.blocks += 1
        if execn == 0:
            return
        self.blocks_run += 1
        self.execs += execn
        for i in range(32):
            self.r64[i] += counts[i] * execn
            self.w64[i] += counts[32 + i] * execn
            self.r128[i] += counts[64 + i] * execn
            self.w128[i] += counts[96 + i] * execn


def parse_file(path, heat):
    with open(path) as f:
        for line in f:
            if not line.startswith("B,"):
                continue
            parts = line.rstrip("\n").split(",")
            if len(parts) != 4 + 128:
                print(f"warning: malformed row in {path}: {len(parts)} fields", file=sys.stderr)
                continue
            pc = int(parts[1], 16)
            insns = int(parts[2])
            execn = int(parts[3])
            counts = [int(x) for x in parts[4:]]
            heat.add_row(pc, insns, execn, counts)


def collect_inputs(args):
    files = []
    for a in args:
        if os.path.isdir(a):
            files += sorted(
                os.path.join(a, f) for f in os.listdir(a)
                if f.startswith("eeregheat-") and f.endswith(".csv"))
        else:
            files.append(a)
    return files


def report(heat, top, label):
    total64 = [heat.r64[i] + heat.w64[i] for i in range(32)]
    total128 = [heat.r128[i] + heat.w128[i] for i in range(32)]
    grand64 = sum(total64)
    grand128 = sum(total128)
    grand = grand64 + grand128

    print(f"\n=== {label} ===")
    print(f"blocks: {heat.blocks} compiled, {heat.blocks_run} executed; "
          f"block entries: {heat.execs}")
    if grand == 0:
        print("no dynamic references recorded (all exec counts zero?)")
        return
    print(f"dynamic GPR refs (est.): {grand:,}  "
          f"[64-bit {grand64:,} = {100.0 * grand64 / grand:.1f}%  |  "
          f"128-bit {grand128:,} = {100.0 * grand128 / grand:.1f}%]")

    # $zero should always be 0 (macro-guarded); flag if not.
    if total64[0] or total128[0]:
        print("warning: nonzero $zero counts - collector bug?", file=sys.stderr)

    order = sorted(range(1, 32), key=lambda i: total64[i], reverse=True)

    print(f"\n{'reg':>6} {'dyn64':>16} {'%':>6} {'cum%':>6} {'wr%':>5} "
          f"{'dyn128':>14} {'pin?':>5}")
    cum = 0
    for rank, i in enumerate(order[:top], 1):
        cum += total64[i]
        wr = 100.0 * heat.w64[i] / total64[i] if total64[i] else 0.0
        pin = "sp/ra" if i in ALREADY_PINNED else ""
        print(f"{GPR_NAMES[i]:>6} {total64[i]:>16,} "
              f"{100.0 * total64[i] / grand64:>5.1f}% "
              f"{100.0 * cum / grand64:>5.1f}% {wr:>4.0f}% "
              f"{total128[i]:>14,} {pin:>5}")

    print("\ncoverage curve (top-N hottest regs, share of dynamic 64-bit refs):")
    cum = 0
    curve = []
    for n, i in enumerate(order, 1):
        cum += total64[i]
        curve.append((n, 100.0 * cum / grand64))
    marks = {r: c for r, c in curve}
    line = "  " + "  ".join(f"N={n}:{marks[n]:.1f}%" for n, _ in curve[:16])
    print(line)

    print("\npin-ladder proposal ($sp/$ra fixed; remainder by measured heat):")
    proposal = list(ALREADY_PINNED)
    for i in order:
        if i not in proposal:
            proposal.append(i)
    for rung_total in RUNGS:
        regs = proposal[:rung_total]
        cov = 100.0 * sum(total64[i] for i in regs) / grand64
        print(f"  rung {RUNGS.index(rung_total) + 1} ({rung_total:>2} pins): "
              f"{' '.join(GPR_NAMES[i] for i in regs)}  -> {cov:.1f}% coverage")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("inputs", nargs="+", help="eeregheat CSV files or directories")
    ap.add_argument("--top", type=int, default=20, help="rows in the ranked table")
    ap.add_argument("--per-file", action="store_true",
                    help="also print one report per input file")
    args = ap.parse_args()

    files = collect_inputs(args.inputs)
    if not files:
        print("no eeregheat-*.csv inputs found", file=sys.stderr)
        return 1

    combined = Heat()
    for path in files:
        if args.per_file:
            single = Heat()
            parse_file(path, single)
            report(single, args.top, os.path.basename(path))
        parse_file(path, combined)

    report(combined, args.top, f"combined ({len(files)} file(s))")
    return 0


if __name__ == "__main__":
    sys.exit(main())
