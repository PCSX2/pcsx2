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


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("jitdump")
    ap.add_argument("perfdata", nargs="?", help="perf.data for sample weighting")
    ap.add_argument("--pins", choices=sorted(PIN_MAPS), default="s4")
    args = ap.parse_args()

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
