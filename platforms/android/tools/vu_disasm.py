#!/usr/bin/env python3
# VU disassembler for ARMSX2 VU1 program dumps.
# Decodes raw VU bytecode (.bin from VU1Fingerprint::DumpProgramCode) into
# pair-by-pair textual disassembly. Tables mirror DebugTools/DisVUmicro.h.
# Not a full VU disasm — just enough mnemonics + operands to identify the
# algorithm a hot program implements.

import struct
import sys
from pathlib import Path

# Upper FMAC ops by `code & 0x3F`. Entries 0x3C..0x3F dispatch to FD_xx
# subtables indexed by `_Fd_` (bits 6..10).
UPPER_TABLE = [
    "ADDx",  "ADDy",  "ADDz",  "ADDw",  "SUBx",  "SUBy",  "SUBz",  "SUBw",
    "MADDx", "MADDy", "MADDz", "MADDw", "MSUBx", "MSUBy", "MSUBz", "MSUBw",
    "MAXx",  "MAXy",  "MAXz",  "MAXw",  "MINIx", "MINIy", "MINIz", "MINIw",
    "MULx",  "MULy",  "MULz",  "MULw",  "MULq",  "MAXi",  "MULi",  "MINIi",
    "ADDq",  "MADDq", "ADDi",  "MADDi", "SUBq",  "MSUBq", "SUBi",  "MSUBi",
    "ADD",   "MADD",  "MUL",   "MAX",   "SUB",   "MSUB",  "OPMSUB","MINI",
    "?30","?31","?32","?33","?34","?35","?36","?37","?38","?39","?3a","?3b",
    "<FD_00>","<FD_01>","<FD_10>","<FD_11>",
]

# Upper subtable when top 6 bits = 0x3C (FD = 00, broadcast x), indexed by _Fd_.
UPPER_FD_00 = [
    "ADDAx", "SUBAx", "MADDAx","MSUBAx","ITOF0", "FTOI0", "MULAx", "MULAq",
    "ADDAq", "SUBAq", "ADDA",  "SUBA",  None,    None,    None,    None,
] + [None] * 16

UPPER_FD_01 = [
    "ADDAy", "SUBAy", "MADDAy","MSUBAy","ITOF4", "FTOI4", "MULAy", "ABS",
    "MADDAq","MSUBAq","MADDA", "MSUBA", None,    None,    None,    None,
] + [None] * 16

UPPER_FD_10 = [
    "ADDAz", "SUBAz", "MADDAz","MSUBAz","ITOF12","FTOI12","MULAz", "MULAi",
    "ADDAi", "SUBAi", "MULA",  "OPMULA",None,    None,    None,    None,
] + [None] * 16

UPPER_FD_11 = [
    "ADDAw", "SUBAw", "MADDAw","MSUBAw","ITOF15","FTOI15","MULAw", "CLIP",
    "MADDAi","MSUBAi",None,    "NOP",   None,    None,    None,    None,
] + [None] * 16

FD_SUBTABLES = {0x3C: UPPER_FD_00, 0x3D: UPPER_FD_01, 0x3E: UPPER_FD_10, 0x3F: UPPER_FD_11}

# Lower top-level by `code >> 25`. Entry 0x40 = LowerOP, sub-dispatched.
LOWER_TABLE = [None] * 128
LOWER_TABLE[0x00] = "LQ"
LOWER_TABLE[0x01] = "SQ"
LOWER_TABLE[0x04] = "ILW"
LOWER_TABLE[0x05] = "ISW"
LOWER_TABLE[0x08] = "IADDIU"
LOWER_TABLE[0x09] = "ISUBIU"
LOWER_TABLE[0x10] = "FCEQ"
LOWER_TABLE[0x11] = "FCSET"
LOWER_TABLE[0x12] = "FCAND"
LOWER_TABLE[0x13] = "FCOR"
LOWER_TABLE[0x14] = "FSEQ"
LOWER_TABLE[0x15] = "FSSET"
LOWER_TABLE[0x16] = "FSAND"
LOWER_TABLE[0x17] = "FSOR"
LOWER_TABLE[0x18] = "FMEQ"
LOWER_TABLE[0x1A] = "FMAND"
LOWER_TABLE[0x1B] = "FMOR"
LOWER_TABLE[0x1C] = "FCGET"
LOWER_TABLE[0x20] = "B"
LOWER_TABLE[0x21] = "BAL"
LOWER_TABLE[0x24] = "JR"
LOWER_TABLE[0x25] = "JALR"
LOWER_TABLE[0x28] = "IBEQ"
LOWER_TABLE[0x29] = "IBNE"
LOWER_TABLE[0x2C] = "IBLTZ"
LOWER_TABLE[0x2D] = "IBGTZ"
LOWER_TABLE[0x2E] = "IBLEZ"
LOWER_TABLE[0x2F] = "IBGEZ"
LOWER_TABLE[0x40] = "<LowerOP>"

# LowerOP sub-table (`code & 0x3F` when top7 == 0x40). Entries 0x3C..0x3F dispatch to T3_xx.
LOWEROP_TABLE = [None] * 64
LOWEROP_TABLE[0x30] = "IADD"
LOWEROP_TABLE[0x31] = "ISUB"
LOWEROP_TABLE[0x32] = "IADDI"
LOWEROP_TABLE[0x34] = "IAND"
LOWEROP_TABLE[0x35] = "IOR"
LOWEROP_TABLE[0x3C] = "<T3_00>"
LOWEROP_TABLE[0x3D] = "<T3_01>"
LOWEROP_TABLE[0x3E] = "<T3_10>"
LOWEROP_TABLE[0x3F] = "<T3_11>"

# T3 subtables (indexed by _Fd_).
T3_00 = [None] * 32
T3_00[0x0C] = "MOVE"
T3_00[0x0D] = "LQI"
T3_00[0x0E] = "DIV"
T3_00[0x0F] = "MTIR"
T3_00[0x10] = "RNEXT"
T3_00[0x19] = "MFP"
T3_00[0x1A] = "XTOP"
T3_00[0x1B] = "XGKICK"
T3_00[0x1C] = "ESADD"
T3_00[0x1D] = "EATANxy"
T3_00[0x1E] = "ESQRT"
T3_00[0x1F] = "ESIN"

T3_01 = [None] * 32
T3_01[0x0C] = "MR32"
T3_01[0x0D] = "SQI"
T3_01[0x0E] = "SQRT"
T3_01[0x0F] = "MFIR"
T3_01[0x10] = "RGET"
T3_01[0x1A] = "XITOP"
T3_01[0x1C] = "ERSADD"
T3_01[0x1D] = "EATANxz"
T3_01[0x1E] = "ERSQRT"
T3_01[0x1F] = "EATAN"

T3_10 = [None] * 32
T3_10[0x0D] = "LQD"
T3_10[0x0E] = "RSQRT"
T3_10[0x0F] = "ILWR"
T3_10[0x10] = "RINIT"
T3_10[0x1C] = "ELENG"
T3_10[0x1D] = "ESUM"
T3_10[0x1E] = "ERCPR"
T3_10[0x1F] = "EEXP"

T3_11 = [None] * 32
T3_11[0x0D] = "SQD"
T3_11[0x0E] = "WAITQ"
T3_11[0x0F] = "ISWR"
T3_11[0x10] = "RXOR"
T3_11[0x1C] = "ERLENG"
T3_11[0x1E] = "WAITP"

T3_SUBS = {0x3C: T3_00, 0x3D: T3_01, 0x3E: T3_10, 0x3F: T3_11}


def decode_xyzw(code):
    """Upper field-mask bits 21..24, x=24, y=23, z=22, w=21."""
    m = (code >> 21) & 0xF
    s = ""
    if m & 0x8: s += "x"
    if m & 0x4: s += "y"
    if m & 0x2: s += "z"
    if m & 0x1: s += "w"
    return s if s else "(none)"


def decode_upper(code):
    """Decode upper FMAC instruction. Returns (mnemonic, operand_str, flags_str)."""
    flags = []
    if (code >> 31) & 1: flags.append("I")
    if (code >> 30) & 1: flags.append("E")
    if (code >> 29) & 1: flags.append("M")
    if (code >> 28) & 1: flags.append("D")
    if (code >> 27) & 1: flags.append("T")
    flags_str = "[" + ",".join(flags) + "]" if flags else ""

    op6 = code & 0x3F
    fd = (code >> 6) & 0x1F
    fs = (code >> 11) & 0x1F
    ft = (code >> 16) & 0x1F
    xyzw = decode_xyzw(code)

    mnem = UPPER_TABLE[op6]
    if mnem.startswith("<FD_"):
        sub = FD_SUBTABLES.get(op6)
        mnem = sub[fd] if sub and sub[fd] else f"<bad upper {op6:02x}/{fd:02x}>"
        # ADDA/MADDA etc. → operands are (ACC.{m}, VF[fs].{m}, VF[ft].{m})
        ops = f"ACC.{xyzw}, VF{fs:02d}.{xyzw}, VF{ft:02d}.{xyzw}"
    elif mnem == "NOP":
        ops = ""
    elif mnem.startswith("CLIP"):
        ops = f"VF{fs:02d}.xyz, VF{ft:02d}.w"
    elif mnem in ("OPMSUB",):
        ops = f"VF{fd:02d}.xyz, VF{fs:02d}.xyz, VF{ft:02d}.xyz"
    elif mnem[-1] in "xyzwiq" and mnem[-2:] not in ("ix", "iy", "iz", "iw"):
        # Broadcast variants
        ops = f"VF{fd:02d}.{xyzw}, VF{fs:02d}.{xyzw}, VF{ft:02d}.{mnem[-1]}"
    else:
        ops = f"VF{fd:02d}.{xyzw}, VF{fs:02d}.{xyzw}, VF{ft:02d}.{xyzw}"

    return mnem, ops, flags_str


def decode_lower(code, next_word_is_immediate):
    """Decode lower instruction. If next_word_is_immediate (I-bit on prev pair),
    treat this code as a 32-bit float literal instead of an opcode."""
    if next_word_is_immediate:
        f = struct.unpack("<f", struct.pack("<I", code))[0]
        return "<I>", f"0x{code:08x} ({f:g})", ""

    top7 = (code >> 25) & 0x7F
    mnem = LOWER_TABLE[top7]
    if mnem is None:
        return f"?L{top7:02x}", f"raw=0x{code:08x}", ""
    if mnem == "<LowerOP>":
        sub6 = code & 0x3F
        m = LOWEROP_TABLE[sub6]
        if m is None:
            return f"?L_OP{sub6:02x}", f"raw=0x{code:08x}", ""
        if m.startswith("<T3_"):
            t3 = T3_SUBS.get(sub6)
            fd = (code >> 6) & 0x1F
            m = t3[fd] if t3 and t3[fd] else f"<bad T3 {sub6:02x}/{fd:02x}>"
        return m, _lower_ops(m, code), ""
    return mnem, _lower_ops(mnem, code), ""


def _lower_ops(mnem, code):
    """Best-effort operand pretty-print for lower-op mnemonics we care about."""
    it = (code >> 16) & 0x1F   # IT (target reg)
    is_ = (code >> 11) & 0x1F  # IS (source reg)
    ft = (code >> 16) & 0x1F   # FT (for LQ/SQ/etc.)
    fs = (code >> 11) & 0x1F
    fd = (code >> 6) & 0x1F
    imm11 = code & 0x7FF
    if imm11 & 0x400:
        imm11 -= 0x800
    imm15 = code & 0x7FFF
    if mnem in ("LQ", "SQ"):
        xyzw = decode_xyzw(code)
        return f"VF{ft:02d}.{xyzw}, {imm11}(VI{is_:02d})"
    if mnem in ("ILW", "ISW"):
        xyzw = decode_xyzw(code)
        return f"VI{it:02d}.{xyzw}, {imm11}(VI{is_:02d})"
    if mnem in ("IADDIU", "ISUBIU"):
        return f"VI{it:02d}, VI{is_:02d}, 0x{imm15:04x}"
    if mnem == "IADDI":
        # IADDI uses bits 6..10 as immediate (signed 5-bit)
        imm5 = (code >> 6) & 0x1F
        if imm5 & 0x10: imm5 -= 0x20
        return f"VI{it:02d}, VI{is_:02d}, {imm5}"
    if mnem in ("IADD", "ISUB", "IAND", "IOR"):
        return f"VI{fd:02d}, VI{fs:02d}, VI{(code>>16)&0x1F:02d}"
    if mnem == "MOVE":
        return f"VF{ft:02d}, VF{fs:02d}"
    if mnem == "MR32":
        return f"VF{ft:02d}, VF{fs:02d}"
    if mnem in ("LQI", "LQD"):
        xyzw = decode_xyzw(code)
        return f"VF{ft:02d}.{xyzw}, (VI{is_:02d}{'++' if mnem=='LQI' else '--'})"
    if mnem in ("SQI", "SQD"):
        xyzw = decode_xyzw(code)
        return f"VF{fs:02d}.{xyzw}, (VI{(code>>16)&0x1F:02d}{'++' if mnem=='SQI' else '--'})"
    if mnem == "DIV":
        return f"Q = VF{fs:02d}.{'xyzw'[(code>>21)&3]} / VF{ft:02d}.{'xyzw'[(code>>23)&3]}"
    if mnem == "SQRT":
        return f"Q = sqrt(VF{ft:02d}.{'xyzw'[(code>>23)&3]})"
    if mnem == "RSQRT":
        return f"Q = VF{fs:02d}.{'xyzw'[(code>>21)&3]} / sqrt(VF{ft:02d}.{'xyzw'[(code>>23)&3]})"
    if mnem == "MTIR":
        return f"VI{it:02d}, VF{fs:02d}.{'xyzw'[(code>>21)&3]}"
    if mnem == "MFIR":
        return f"VF{ft:02d}.{decode_xyzw(code)}, VI{is_:02d}"
    if mnem == "XGKICK":
        return f"(VI{is_:02d})"
    if mnem == "XTOP":
        return f"VI{it:02d}"
    if mnem in ("B", "BAL"):
        # 11-bit signed PC-relative * 8
        off = imm11 * 8
        return f"PC{'+' if off>=0 else ''}{off}"
    if mnem in ("JR", "JALR"):
        return f"(VI{is_:02d})"
    if mnem in ("IBEQ", "IBNE"):
        off = imm11 * 8
        return f"VI{is_:02d}, VI{it:02d}, PC{'+' if off>=0 else ''}{off}"
    if mnem in ("IBLTZ", "IBGTZ", "IBLEZ", "IBGEZ"):
        off = imm11 * 8
        return f"VI{is_:02d}, PC{'+' if off>=0 else ''}{off}"
    return f"raw=0x{code:08x}"


def disasm_file(path, out=sys.stdout, max_pairs=None):
    data = Path(path).read_bytes()
    if len(data) % 8 != 0:
        print(f"WARN: file size {len(data)} not a multiple of 8", file=sys.stderr)
    n_pairs = len(data) // 8
    if max_pairs:
        n_pairs = min(n_pairs, max_pairs)
    print(f"# {path}  ({len(data)} bytes, {n_pairs} pairs)", file=out)
    print(f"# offset  lower                                          | upper", file=out)
    for i in range(n_pairs):
        off = i * 8
        lo, up = struct.unpack("<II", data[off:off+8])
        # VU I-bit semantics: when upper.31 is set on a pair, THAT SAME pair's
        # lower word is a 32-bit immediate float for the I register (not an
        # opcode). Mirrors PCSX2's PairHasBranch check at iVU1micro_arm64.cpp:1968.
        ibit_this_pair = bool((up >> 31) & 1)
        lo_m, lo_o, _ = decode_lower(lo, ibit_this_pair)
        up_m, up_o, up_f = decode_upper(up)
        lo_str = f"{lo_m:<8} {lo_o}"
        up_str = f"{up_m:<8} {up_o} {up_f}".rstrip()
        print(f"  0x{off:04x}  {lo_str:<46} | {up_str}", file=out)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: vu_disasm.py <file.bin> [max_pairs]", file=sys.stderr)
        sys.exit(1)
    mp = int(sys.argv[2]) if len(sys.argv) > 2 else None
    disasm_file(sys.argv[1], max_pairs=mp)
