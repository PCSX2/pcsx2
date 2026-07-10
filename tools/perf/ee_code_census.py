#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0+
"""EE-block emitted-code census over a PCSX2 perf jitdump.

The per-item gate metric for emitted-code-density work (EE-SRA 2): classifies
every instruction in EE_* JIT blocks so a codegen change's predicted category
delta can be checked against reality (and unexplained regressions in OTHER
categories surface immediately).

Capture:  pcsx2-eerunner --liverun --renderer null --frames 600 \
              --savestate <p2s> --iso <iso> --perf-jitdump
          (jitdump lands under EmuFolders::Cache/pcsx2-perf-<pid>/)
Optionally wrap in `perf record -F 999 -o perf.data` and pass perf.data for a
sample-weighted dynamic mix — samples are binned into block address ranges
directly (no `perf inject` needed).

Usage:    ee_code_census.py <jit-XXXX.dump> [perf.data] [--pins s3|s0|none]
          ee_code_census.py <jit-XXXX.dump> perf.data --footprint [--window 1.0]

--footprint (needs perf.data): instead of the category census, measure the hot
JIT working set — bin sample IPs by 64-byte host cacheline over the jitdump
block ranges and report distinct lines/KB covering 50/90/99% of samples, per
JIT (EE / mVU0 / mVU1 / ...) and combined. Whole-run numbers overstate the
instantaneous set, so it also reports the per-`--window`-second median. The
number to compare against L1I capacity: A77 64KB, A55/A53 32KB.

Categories:
  ldr_gpr_pinned(reload)   loads of a pinned guest GPR into its own pin (seam reloads)
  ldr_gpr_pinned(MISSED)   loads of a pinned guest GPR into some other reg (should be ~0)
  ldr_gpr_unpinned         loads of unpinned guest GPRs
  str_gpr_*                guest-GPR stores (pinned = write-through)
  mov_from_pin             Mov scratch<-pin copies (Ldr->Mov substitution shape)
  mov_to_pin               Mov pin<-scratch mirror refreshes
  pin_maint                sxtw/bfi/lsr mirror maintenance
  pin_as_operand           pin used directly as an ALU/mem operand (the win shape)
  state fields             cpuRegs.branch / pc / cycle / FPU file / other, by offset
  bl_call                  C calls

Offset map: GPR file 0..511; HI 512; LO 528; the named state offsets below are
current as of 2026-07-07 — re-derive with gdb (`print (int)(long)
&((cpuRegistersPack*)0)->cpuRegs.branch` etc.) if cpuRegisters changes shape.
"""
import argparse
import bisect
import struct
import subprocess
import sys
from collections import Counter

RSTATE = 20  # x20 = &cpuRegs (iR5900-arm64.h)
PIN_MAPS = {
    # EE-SRA 3 Arm D tier-2 re-home: $k0→x12, $s0→x13, $at→x11
    # (caller-saved, preserve_most-spared).
    "s5": {29: 22, 31: 23, 2: 29, 3: 26, 4: 27, 26: 12, 5: 21, 16: 13, 1: 11},
    # EE-SRA 3 Arm C tier-1 re-home: $v1→x26, $a0→x27, $a1→x21 (callee-saved).
    "s4": {29: 22, 31: 23, 2: 29, 3: 26, 4: 27, 26: 4, 5: 21, 16: 6, 1: 7},
    "s3": {29: 22, 31: 23, 2: 29, 3: 12, 4: 13, 26: 4, 5: 5, 16: 6, 1: 7},
    "s0": {29: 22, 31: 23},  # pre-EE-SRA: only the a7312122b $sp/$ra pair
    "none": {},
}
GPR_NAMES = ("zero at v0 v1 a0 a1 a2 a3 t0 t1 t2 t3 t4 t5 t6 t7 "
             "s0 s1 s2 s3 s4 s5 s6 s7 t8 t9 k0 k1 gp sp fp ra").split()
# gdb-derived cpuRegisters offsets (see docstring). fpuRegs occupies
# [1168, 1600): fpr[32] + fprc[32] + ACC + ACCflag.
STATE_FIELDS = {680: "pc", 684: "code", 1088: "cycle", 1100: "branch",
                1116: "pcWriteback", 1120: "nextEventCycle"}
FPU_BASE, FPU_END = 1168, 1600

CATS = [
    "ldr_gpr_pinned(reload)", "ldr_gpr_pinned(MISSED)", "ldr_gpr_unpinned",
    "str_gpr_pinned(wthru)", "str_gpr_unpinned", "ldrstr_hilo",
    "state_branch", "state_pc", "state_cycle", "state_fpu", "state_other",
    "mov_from_pin", "mov_to_pin", "pin_maint(sxtw/bfi/lsr)", "pin_as_operand",
    "bl_call", "other",
]

MEM_TOPS = {
    0xF9400000: (1, 8), 0xF9000000: (0, 8),    # LDR/STR Xt
    0xB9400000: (1, 4), 0xB9000000: (0, 4),    # LDR/STR Wt
    0xB9800000: (1, 4),                        # LDRSW
    0x79400000: (1, 2), 0x79000000: (0, 2),    # LDRH/STRH
    0x39400000: (1, 1), 0x39000000: (0, 1),    # LDRB/STRB
    0x3DC00000: (1, 16), 0x3D800000: (0, 16),  # LDR/STR Qt
    0xFD400000: (1, 8), 0xFD000000: (0, 8),    # LDR/STR Dt
    0xBD400000: (1, 4), 0xBD000000: (0, 4),    # LDR/STR St
}


def parse_jitdump(path):
    """Yield (code_addr, size, name, code_bytes) from JIT_CODE_LOAD records."""
    with open(path, "rb") as f:
        hdr = f.read(40)
        magic, _version, total_size = struct.unpack("<III", hdr[:12])
        assert magic in (0x4A695444, 0x4454694A), f"not a jitdump: {hex(magic)}"
        f.seek(total_size)
        while True:
            rh = f.read(16)
            if len(rh) < 16:
                return
            rid, rtot, _ts = struct.unpack("<IIQ", rh)
            body = f.read(rtot - 16)
            if rid == 0:  # JIT_CODE_LOAD
                _pid, _tid, _vma, addr, size, _idx = struct.unpack("<IIQQQQ", body[:40])
                rest = body[40:]
                nul = rest.index(b"\0")
                yield addr, size, rest[:nul].decode(errors="replace"), rest[nul + 1 : nul + 1 + size]


def classify_block(code, pin_gpr, pins):
    c = Counter()
    per_gpr_ld = Counter()
    n = len(code) // 4
    for k in range(n):
        (i,) = struct.unpack_from("<I", code, k * 4)
        rd, rn, rm = i & 31, (i >> 5) & 31, (i >> 16) & 31
        if rn == RSTATE:
            m = MEM_TOPS.get(i & 0xFFC00000)
            if m:
                ld, w = m
                off = ((i >> 10) & 0xFFF) * w
                if off < 512:
                    g = off // 16
                    lane0 = (off % 16) == 0
                    if ld:
                        per_gpr_ld[g] += 1
                        if g in pin_gpr and lane0:
                            c["ldr_gpr_pinned(reload)" if rd == pin_gpr[g] else "ldr_gpr_pinned(MISSED)"] += 1
                        else:
                            c["ldr_gpr_unpinned"] += 1
                    else:
                        c["str_gpr_pinned(wthru)" if (g in pin_gpr and lane0) else "str_gpr_unpinned"] += 1
                elif off < 544:
                    c["ldrstr_hilo"] += 1
                elif off in STATE_FIELDS and STATE_FIELDS[off] in ("branch", "pc", "cycle"):
                    c["state_" + STATE_FIELDS[off]] += 1
                elif FPU_BASE <= off < FPU_END:
                    c["state_fpu"] += 1
                else:
                    c["state_other"] += 1
                continue
        # MOV Xd,Xm / Wd,Wm (ORR shifted-reg, Rn=zr, shift 0)
        if (i & 0xFFE0FFE0) in (0xAA0003E0, 0x2A0003E0):
            if rm in pins and rd not in pins:
                c["mov_from_pin"] += 1
                continue
            if rd in pins:
                c["mov_to_pin"] += 1
                continue
        if ((i & 0xFFFFFC00) == 0x93407C00 or (i & 0xFFFFFC00) == 0xD360FC00
                or (i & 0x7F800000) == 0x33000000) and (rd in pins or rn in pins):
            c["pin_maint(sxtw/bfi/lsr)"] += 1
            continue
        if (i & 0xFC000000) == 0x94000000:
            c["bl_call"] += 1
            continue
        # Branch-format insns (B/B.cond/CBZ/CBNZ/TBZ/TBNZ) carry immediates in
        # the Rn/Rm bit positions — exclude them from the pin-operand heuristic
        # or code-layout shifts reshuffle these buckets meaninglessly.
        if ((i & 0x7C000000) == 0x14000000 or (i & 0xFF000000) == 0x54000000
                or (i & 0x7E000000) == 0x34000000 or (i & 0x7E000000) == 0x36000000):
            c["other"] += 1
            continue
        if rn in pins or (rm in pins and ((i >> 25) & 0x7) == 0x5):
            c["pin_as_operand"] += 1
            continue
        c["other"] += 1
    return c, per_gpr_ld, n


def _coverage(cnt, fracs=(0.5, 0.9, 0.99)):
    """{addr_unit: samples} -> {frac: n_units covering that fraction, hottest-first}."""
    tot = sum(cnt.values())
    out = {}
    acc = 0
    targets = sorted(fracs)
    ti = 0
    for i, (_, v) in enumerate(cnt.most_common(), 1):
        acc += v
        while ti < len(targets) and acc >= targets[ti] * tot:
            out[targets[ti]] = i
            ti += 1
        if ti == len(targets):
            break
    for f in fracs:
        out.setdefault(f, len(cnt))
    return out


def run_footprint(blocks, perfdata, window):
    ranges = sorted((addr, addr + size, name.split("_")[0])
                    for addr, size, name, _ in blocks if size)
    lows = [r[0] for r in ranges]

    # Stream perf script output — big captures (700MB+ perf.data) produce
    # more text than is sane to hold in one string.
    proc = subprocess.Popen(["perf", "script", "-i", perfdata, "-F", "time,ip"],
                            stdout=subprocess.PIPE, text=True)
    lines_by = {}   # group -> Counter(line addr)
    pages_by = {}   # group -> Counter(4K page)
    wins_by = {}    # group -> {window idx -> Counter(line addr)}
    n_jit = 0
    n_samples = 0
    t0 = tmax = None
    # Two output shapes: flat samples print "TIME: IP" on one line; call-graph
    # captures print "TIME:" alone with the callchain on following lines. Take
    # only the LEAF (first IP after a time line) — that's where cycles retired.
    pending_t = None
    for line in proc.stdout:
        parts = line.split()
        if not parts:
            continue
        t = ip = None
        if parts[0].endswith(":"):
            try:
                t = float(parts[0][:-1])
            except ValueError:
                continue
            if len(parts) >= 2:
                try:
                    ip = int(parts[1], 16)
                except ValueError:
                    pass
            if ip is None:
                pending_t = t
                continue
        elif pending_t is not None:
            try:
                ip = int(parts[0], 16)
            except ValueError:
                continue
            t, pending_t = pending_t, None
        else:
            continue
        n_samples += 1
        if t0 is None:
            t0 = t
        tmax = t
        j = bisect.bisect_right(lows, ip) - 1
        if j < 0 or ip >= ranges[j][1]:
            continue
        n_jit += 1
        for g in (ranges[j][2], "ALL-JIT"):
            lines_by.setdefault(g, Counter())[ip >> 6] += 1
            pages_by.setdefault(g, Counter())[ip >> 12] += 1
            wins_by.setdefault(g, {}).setdefault(int((t - t0) / window), Counter())[ip >> 6] += 1
    proc.wait()
    if not n_samples:
        sys.exit("no time,ip samples parsed from perf.data")

    dur = (tmax - t0) if tmax is not None else 0.0
    print(f"samples: {n_samples} total over {dur:.1f}s, "
          f"{n_jit} ({100.0 * n_jit / n_samples:.1f}%) in JIT blocks")
    print(f"footprint units: 64B cachelines (KB = lines*64/1024); pages = 4KB")
    print(f"L1I capacity for comparison: A77 64KB, A55/A53 32KB\n")
    hdr = (f"{'group':8s} {'samples':>9s} {'lines':>7s} {'KB':>7s} "
           f"{'50%KB':>7s} {'90%KB':>7s} {'99%KB':>7s} {'90%pg':>6s} "
           f"{'w-med90%KB':>10s} {'w-medKB':>8s}")
    print(hdr)
    for g in sorted(lines_by, key=lambda g: -sum(lines_by[g].values()) if g != "ALL-JIT" else 1):
        cnt = lines_by[g]
        cov = _coverage(cnt)
        pcov = _coverage(pages_by[g], fracs=(0.9,))
        # per-window medians: instantaneous working-set estimate
        w90 = sorted(_coverage(wc, fracs=(0.9,))[0.9] for wc in wins_by[g].values())
        wall = sorted(len(wc) for wc in wins_by[g].values())
        med = lambda v: v[len(v) // 2] if v else 0
        print(f"{g:8s} {sum(cnt.values()):9d} {len(cnt):7d} {len(cnt) * 64 / 1024:7.1f} "
              f"{cov[0.5] * 64 / 1024:7.1f} {cov[0.9] * 64 / 1024:7.1f} {cov[0.99] * 64 / 1024:7.1f} "
              f"{pcov[0.9]:6d} {med(w90) * 64 / 1024:10.1f} {med(wall) * 64 / 1024:8.1f}")
    print("\nw-med90%KB = median across windows of the KB of hottest lines covering 90%")
    print(f"of that window's samples (window = {window:.2f}s); w-medKB = median distinct KB/window.")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("jitdump")
    ap.add_argument("perfdata", nargs="?", help="perf.data for sample weighting")
    ap.add_argument("--pins", choices=sorted(PIN_MAPS), default="s5")
    ap.add_argument("--footprint", action="store_true",
                    help="hot-working-set report (needs perf.data); skips the census")
    ap.add_argument("--window", type=float, default=1.0,
                    help="footprint time-window seconds (default 1.0)")
    args = ap.parse_args()

    if args.footprint:
        if not args.perfdata:
            sys.exit("--footprint requires perf.data")
        run_footprint(list(parse_jitdump(args.jitdump)), args.perfdata, args.window)
        return

    pin_gpr = PIN_MAPS[args.pins]
    pins = set(pin_gpr.values())

    blocks = list(parse_jitdump(args.jitdump))
    prefixes = Counter(n.split("_")[0] for _, _, n, _ in blocks)
    ee = [b for b in blocks if b[2].startswith("EE_")]
    print(f"jitdump: {len(blocks)} symbols {dict(prefixes)}")
    print(f"EE blocks: {len(ee)}, {sum(b[1] for b in ee)} bytes")

    static = Counter()
    ld_by_gpr = Counter()
    total = 0
    per_block = {}
    for addr, size, name, code in ee:
        c, ld, n = classify_block(code, pin_gpr, pins)
        static.update(c)
        ld_by_gpr.update(ld)
        total += n
        per_block[(addr, addr + size)] = (name, c, n)

    print(f"\n== STATIC census: {total} EE-block instructions ==")
    for cat in CATS:
        v = static.get(cat, 0)
        print(f"  {cat:26s} {v:8d}  {100.0 * v / total:6.2f}%")
    print("\n  GPR loads by guest reg (top 10):")
    for g, v in ld_by_gpr.most_common(10):
        print(f"    {GPR_NAMES[g]:5s} {'PIN' if g in pin_gpr else '   '} {v:7d}")

    if args.perfdata:
        out = subprocess.run(["perf", "script", "-i", args.perfdata, "-F", "ip"],
                             capture_output=True, text=True)
        ips = [int(l.strip(), 16) for l in out.stdout.splitlines() if l.strip()]
        ranges = sorted(per_block)
        lows = [r[0] for r in ranges]
        bsamp = Counter()
        in_ee = 0
        for ip in ips:
            j = bisect.bisect_right(lows, ip) - 1
            if j >= 0 and ip < ranges[j][1]:
                bsamp[ranges[j]] += 1
                in_ee += 1
        print(f"\n== SAMPLES: {len(ips)} total, {in_ee} ({100.0 * in_ee / max(1, len(ips)):.1f}%) in EE blocks ==")
        wmix = Counter()
        wtot = 0.0
        for rng, ns in bsamp.items():
            _, c, n = per_block[rng]
            if not n:
                continue
            for cat, v in c.items():
                wmix[cat] += ns * v / n
            wtot += ns
        print("  sample-weighted dynamic mix (uniform-intra-block approx):")
        for cat in CATS:
            print(f"  {cat:26s} {100.0 * wmix.get(cat, 0.0) / wtot:6.2f}%")


if __name__ == "__main__":
    main()
