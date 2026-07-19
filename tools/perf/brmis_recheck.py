#!/usr/bin/env python3
"""Re-verify the brmis attribution: quantify r22 sampling skid and symbolize
the native bucket. Challenges tested:
  1. entry8 (8B) is far narrower than real skid -> funnel underestimated?
  2. "native 53%" bucket never symbolized -> what is actually in it?
Usage: brmis_recheck.py <jitdump> <perf.data...>
"""
import os, sys, struct, bisect, subprocess
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ee_code_census import parse_jitdump, _dedupe_blocks, _iter_samples, _insn_class

jd, pds = sys.argv[1], sys.argv[2:]
blocks = _dedupe_blocks(parse_jitdump(jd))
meta = []
for addr, size, name, code in sorted(blocks):
    n = len(code) // 4
    ins = list(struct.unpack_from(f"<{n}I", code)) if n else []
    group = name.split("_")[0] if " " not in name else f"[{name}]"
    meta.append((addr, addr + size, name, group, ins))
lows = [m[0] for m in meta]

def locate(ip):
    j = bisect.bisect_right(lows, ip) - 1
    return j if (j >= 0 and ip < meta[j][1]) else -1

# ---- pass 1: in-block sample geometry ---------------------------------------
OFFB = [(0, 2), (2, 4), (4, 8), (8, 16), (16, 32), (32, 64), (64, 128),
        (128, 1 << 30)]  # insn-offset buckets
off_h = {}      # group -> Counter(bucket)
updist = {}     # group -> Counter(dist class)
n_tot = 0
n_native = 0
grp_tot = Counter()
region_tot = Counter()
entry_sweep = {}  # group -> Counter(W) for W in bytes
SWEEP = (8, 16, 32, 64, 128, 256)

for pd in pds:
    for _t, ip in _iter_samples(pd):
        n_tot += 1
        j = locate(ip)
        if j < 0:
            n_native += 1
            continue
        lo, hi, name, group, ins = meta[j]
        if group.startswith("["):
            region_tot[name] += 1
            continue
        off = ip - lo
        k = off // 4
        grp_tot[group] += 1
        for a, b in OFFB:
            if a <= k < b:
                off_h.setdefault(group, Counter())[(a, b)] += 1
                break
        for W in SWEEP:
            if off < W:
                entry_sweep.setdefault(group, Counter())[W] += 1
        # nearest upstream branch insn (address order == execution order in a
        # straight-line run; a taken upstream branch would not flow here)
        d = None
        for back in range(1, min(k, 64) + 1):
            if _insn_class(ins[k - back]) != "non-branch":
                d = back
                break
        if d is None:
            cls = f"entry@{min(k,64)}" if k <= 64 else "nobranch>64up"
        elif d <= 4:
            cls = "br<=4"
        elif d <= 16:
            cls = "br5-16"
        else:
            cls = "br17-64"
        updist.setdefault(group, Counter())[cls] += 1

print(f"== {n_tot} samples | native {n_native} ({100*n_native/n_tot:.1f}%) ==")
for name, v in region_tot.most_common():
    print(f"  region {name}: {v} ({100*v/n_tot:.2f}%)")

print("\n== in-block insn-offset-from-entry histogram (% of group) ==")
hdr = " ".join(f"{a}-{b if b < 1<<29 else ''}".rstrip("-").ljust(7)
               for a, b in OFFB)
print(f"{'group':6s} {'n':>7s}  " + hdr)
for g in sorted(grp_tot, key=lambda g: -grp_tot[g]):
    c = off_h.get(g, Counter())
    t = grp_tot[g]
    print(f"{g:6s} {t:7d}  " + " ".join(
        f"{100*c.get((a,b),0)/t:6.2f}%" for a, b in OFFB))

print("\n== nearest-UPSTREAM-branch distance (execution-order candidates for "
      "the retired mispredict) ==")
cats = ["entry@0", "br<=4", "br5-16", "br17-64", "nobranch>64up"]
# merge entry@k into one bucket
for g in sorted(grp_tot, key=lambda g: -grp_tot[g]):
    c = updist.get(g, Counter())
    t = grp_tot[g]
    entry = sum(v for k, v in c.items() if k.startswith("entry@"))
    row = {"entry-reach": entry, "br<=4": c.get("br<=4", 0),
           "br5-16": c.get("br5-16", 0), "br17-64": c.get("br17-64", 0),
           "nobranch>64up": c.get("nobranch>64up", 0)}
    print(f"{g:6s} " + "  ".join(f"{k}:{100*v/t:.1f}%" for k, v in row.items()))

print("\n== entry-window sweep: samples within W bytes of a block entry ==")
print("(upper bound on incoming-edge shadow: dispatcher Br + link B + taken "
      "cond edges all land at entries)")
print(f"{'group':6s} " + " ".join(f"W={w:<4d}" for w in SWEEP))
for g in sorted(grp_tot, key=lambda g: -grp_tot[g]):
    c = entry_sweep.get(g, Counter())
    print(f"{g:6s} " + " ".join(f"{100*c.get(w,0)/n_tot:5.2f}%" for w in SWEEP))
disp = sum(v for k, v in region_tot.items() if "Dispatcher" in k)
print(f"\nfunnel(W) = dispatcher regions ({100*disp/n_tot:.2f}%) + EE/IOP entryW:")
for w in SWEEP:
    ee = entry_sweep.get("EE", Counter()).get(w, 0)
    iop = entry_sweep.get("IOP", Counter()).get(w, 0)
    print(f"  W={w:<4d} funnel = {100*(disp+ee+iop)/n_tot:5.2f}% of all samples")

# ---- pass 2: DSO split of the native bucket ---------------------------------
print("\n== native-bucket DSO split (perf script ip,dso) ==")
dso = Counter()
for pd in pds:
    proc = subprocess.Popen(["perf", "script", "-i", pd, "-F", "ip,dso"],
                            stdout=subprocess.PIPE, text=True)
    for line in proc.stdout:
        parts = line.split()
        if len(parts) < 2:
            continue
        try:
            ip = int(parts[0], 16)
        except ValueError:
            continue
        if locate(ip) >= 0:
            continue  # JIT — already attributed
        d = parts[1].strip("()")
        dso[d] += 1
tot = sum(dso.values())
print(f"native total via dso pass: {tot}")
for d, v in dso.most_common(15):
    print(f"  {100*v/tot:6.2f}%  {d}")
