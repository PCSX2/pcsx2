// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "run_vif_tests.h"
#include "../test_bridge.h"

#include <android/log.h>
#include <cstring>

#include "pcsx2/Vif.h"        // vif0Regs (static ref into eeHw[])
#include "pcsx2/Vif_Dma.h"    // vifStruct, vif0 / vif1 externals
#include "pcsx2/Vif_Unpack.h" // VIFfuncTable, nVifT

#define TAG "VifTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static int s_pass, s_fail;

// ──────────────────────────────────────────────────────────────
// VIFfuncTable index formula  (from Vif_Unpack.cpp):
//
//   index = (usn * 32) + upkNum
//   upkNum = (doMask << 4) | (VN << 2) | VL
//
// VN: 0=S (scalar broadcast), 1=V2, 2=V3, 3=V4 / V4-5
// VL: 0=32-bit, 1=16-bit, 2=8-bit, 3=5-bit (V4-5 only)
// doMask: true → apply per-component MASK register
// usn:    true → unsigned zero-extension for 8/16-bit sources
//
// Mode parameter 0..3 (second VIFfuncTable dimension):
//   0 = direct  (dest = data)
//   1 = add row  (dest = data + MaskRow[i]; row unchanged)
//   2 = accumulate  (dest = data + MaskRow[i]; MaskRow[i] = dest)
//   3 = set row  (dest = MaskRow[i] = data)
//
// Mask type nibble per component (2 bits each in vif0Regs.mask at cl=0..3):
//   0 = data  1 = MaskRow[i]  2 = MaskCol[cl]  3 = write-protect
// ──────────────────────────────────────────────────────────────

static constexpr int tbl_idx(int VN, int VL, bool doMask = false, bool usn = false)
{
    return (usn ? 32 : 0) + (doMask ? 16 : 0) + (VN << 2) + VL;
}

static void do_unpack(u32* dest, const void* src,
                      int VN, int VL, int mode = 0,
                      bool doMask = false, bool usn = false)
{
    VIFfuncTable[0][mode][tbl_idx(VN, VL, doMask, usn)](dest, src);
}

// Reset all vif0 state that affects unpack output.
// eeHw is a static array so vif0Regs is always accessible.
static void reset_vif0()
{
    vif0.cl = 0;
    vif0.MaskRow._u32[0] = vif0.MaskRow._u32[1] =
    vif0.MaskRow._u32[2] = vif0.MaskRow._u32[3] = 0u;
    vif0.MaskCol._u32[0] = vif0.MaskCol._u32[1] =
    vif0.MaskCol._u32[2] = vif0.MaskCol._u32[3] = 0u;
    vif0Regs.mask = 0u;
}

static void chk(const char* name, const u32* dst,
                u32 ex, u32 ey, u32 ez, u32 ew)
{
    if (dst[0] == ex && dst[1] == ey && dst[2] == ez && dst[3] == ew) {
        ++s_pass;
        LOGI("  PASS %s", name);
    } else {
        ++s_fail;
        LOGE("  FAIL %s: exp={%08X,%08X,%08X,%08X} got={%08X,%08X,%08X,%08X}",
             name, ex, ey, ez, ew, dst[0], dst[1], dst[2], dst[3]);
    }
}

static void chk_row(const char* name,
                    u32 r0, u32 r1, u32 r2, u32 r3)
{
    if (vif0.MaskRow._u32[0] == r0 && vif0.MaskRow._u32[1] == r1 &&
        vif0.MaskRow._u32[2] == r2 && vif0.MaskRow._u32[3] == r3) {
        ++s_pass;
        LOGI("  PASS %s", name);
    } else {
        ++s_fail;
        LOGE("  FAIL %s: exp row={%08X,%08X,%08X,%08X} got={%08X,%08X,%08X,%08X}",
             name, r0, r1, r2, r3,
             vif0.MaskRow._u32[0], vif0.MaskRow._u32[1],
             vif0.MaskRow._u32[2], vif0.MaskRow._u32[3]);
    }
}

// ──────────────────────────────────────────────────────────────
// Mode 0 — direct: dest = sign/zero-extended source data
// ──────────────────────────────────────────────────────────────

static void test_v4_32_direct()
{
    alignas(16) u32 src[4] = { 0x11223344u, 0xAABBCCDDu, 0x12345678u, 0xDEADBEEFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 0);
    chk("V4-32 direct", dst, src[0], src[1], src[2], src[3]);
}

static void test_v4_16_signed()
{
    // {1, -2, 32767, -32768} → sign-extended to 32 bits
    alignas(4) s16 src[4] = { 1, -2, 32767, -32768 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 1);
    chk("V4-16 signed", dst,
        0x00000001u, 0xFFFFFFFEu, 0x00007FFFu, 0xFFFF8000u);
}

static void test_v4_8_signed()
{
    // {1, -2, 127, -128} → sign-extended to 32 bits
    alignas(4) s8 src[4] = { 1, -2, 127, -128 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 2);
    chk("V4-8 signed", dst,
        0x00000001u, 0xFFFFFFFEu, 0x0000007Fu, 0xFFFFFF80u);
}

static void test_v4_16_unsigned()
{
    // {0xFFFF, 0x8000, 0x0001, 0x7FFF} → zero-extended (usn=1)
    alignas(4) u16 src[4] = { 0xFFFFu, 0x8000u, 0x0001u, 0x7FFFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 1, 0, false, /*usn=*/true);
    chk("V4-16 unsigned", dst,
        0x0000FFFFu, 0x00008000u, 0x00000001u, 0x00007FFFu);
}

static void test_v4_8_unsigned()
{
    // {0xFF, 0x80, 0x01, 0x7F} → zero-extended (usn=1)
    alignas(4) u8 src[4] = { 0xFFu, 0x80u, 0x01u, 0x7Fu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 2, 0, false, /*usn=*/true);
    chk("V4-8 unsigned", dst,
        0x000000FFu, 0x00000080u, 0x00000001u, 0x0000007Fu);
}

static void test_v4_5()
{
    // input = 0xFFFF:
    //   X = (0xFFFF & 0x001F) << 3 = 0x1F << 3 = 0xF8
    //   Y = (0xFFFF & 0x03E0) >> 2 = 0x03E0 >> 2 = 0xF8
    //   Z = (0xFFFF & 0x7C00) >> 7 = 0x7C00 >> 7 = 0xF8
    //   W = (0xFFFF & 0x8000) >> 8 = 0x8000 >> 8 = 0x80
    alignas(4) u32 src[1] = { 0x0000FFFFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 3);
    chk("V4-5 (0xFFFF)", dst, 0xF8u, 0xF8u, 0xF8u, 0x80u);
}

static void test_v4_5_zero()
{
    alignas(4) u32 src[1] = { 0u };
    alignas(16) u32 dst[4] = { 0xDEADu, 0xDEADu, 0xDEADu, 0xDEADu };
    reset_vif0();
    do_unpack(dst, src, 3, 3);
    chk("V4-5 (0x0000) all zero", dst, 0u, 0u, 0u, 0u);
}

static void test_s32_broadcast()
{
    alignas(4) u32 src[1] = { 0xDEADBEEFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 0);
    chk("S-32 broadcast", dst,
        0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu);
}

static void test_s16_signed_broadcast()
{
    // -1 (0xFFFF) → all 0xFFFFFFFF
    alignas(4) s16 src[1] = { -1 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 1);
    chk("S-16 signed broadcast (-1)", dst,
        0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);
}

static void test_s16_signed_broadcast_pos()
{
    // 256 → all 0x00000100
    alignas(4) s16 src[1] = { 256 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 1);
    chk("S-16 signed broadcast (256)", dst,
        0x00000100u, 0x00000100u, 0x00000100u, 0x00000100u);
}

static void test_s8_signed_broadcast()
{
    // -128 (0x80) → all 0xFFFFFF80
    alignas(4) s8 src[1] = { -128 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 2);
    chk("S-8 signed broadcast (-128)", dst,
        0xFFFFFF80u, 0xFFFFFF80u, 0xFFFFFF80u, 0xFFFFFF80u);
}

static void test_s16_unsigned_broadcast()
{
    // 0xFFFF → zero-extended → all 0x0000FFFF
    alignas(4) u16 src[1] = { 0xFFFFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 1, 0, false, /*usn=*/true);
    chk("S-16 unsigned broadcast (0xFFFF)", dst,
        0x0000FFFFu, 0x0000FFFFu, 0x0000FFFFu, 0x0000FFFFu);
}

static void test_s8_unsigned_broadcast()
{
    // 0xFF → zero-extended → all 0x000000FF
    alignas(4) u8 src[1] = { 0xFFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 0, 2, 0, false, /*usn=*/true);
    chk("S-8 unsigned broadcast (0xFF)", dst,
        0x000000FFu, 0x000000FFu, 0x000000FFu, 0x000000FFu);
}

static void test_v2_32()
{
    // {A, B} → {A, B, A, B}  (v1v0v1v0 PS2 hardware behavior)
    alignas(4) u32 src[2] = { 0x11111111u, 0x22222222u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 1, 0);
    chk("V2-32 v1v0v1v0", dst,
        0x11111111u, 0x22222222u, 0x11111111u, 0x22222222u);
}

static void test_v2_16_signed()
{
    // {1, -1} → {1, 0xFFFFFFFF, 1, 0xFFFFFFFF}
    alignas(4) s16 src[2] = { 1, -1 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 1, 1);
    chk("V2-16 signed v1v0v1v0", dst,
        0x00000001u, 0xFFFFFFFFu, 0x00000001u, 0xFFFFFFFFu);
}

static void test_v2_16_unsigned()
{
    // {0x8000, 0x7FFF} → {0x8000, 0x7FFF, 0x8000, 0x7FFF} (zero-extend)
    alignas(4) u16 src[2] = { 0x8000u, 0x7FFFu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 1, 1, 0, false, /*usn=*/true);
    chk("V2-16 unsigned v1v0v1v0", dst,
        0x00008000u, 0x00007FFFu, 0x00008000u, 0x00007FFFu);
}

static void test_v3_32()
{
    // V3-32 uses UNPACK_V4 template — all 4 words including W are written.
    // W content is defined by src[3] (on real PS2 hardware W is overwritten
    // by the next V3 quadword in the stream; here we test the single-call output).
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 2, 0); // VN=2 → tbl entry [8] = UNPACK_V4<0,0,0,u32>
    chk("V3-32 (V4 template, W from src[3])", dst, 0xAAu, 0xBBu, 0xCCu, 0xDDu);
}

// ──────────────────────────────────────────────────────────────
// Mode 1 — add row: dest = data + MaskRow[i]; row unchanged
// ──────────────────────────────────────────────────────────────

static void test_mode1_add_row()
{
    alignas(16) u32 src[4] = { 1u, 2u, 3u, 4u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 10u;
    vif0.MaskRow._u32[1] = 20u;
    vif0.MaskRow._u32[2] = 30u;
    vif0.MaskRow._u32[3] = 40u;
    do_unpack(dst, src, 3, 0, /*mode=*/1);
    chk("Mode1 V4-32 add row: dest={11,22,33,44}", dst, 11u, 22u, 33u, 44u);
    chk_row("Mode1 row unchanged after add",
            10u, 20u, 30u, 40u);
}

static void test_mode1_s16_add_row()
{
    // V4-16 signed with mode 1: sign-extend then add row
    // src={100, -50, 0, 1} → data={100, 0xFFFFFFCE, 0, 1}
    // row={1000, 2000, 3000, 4000}
    // dest={1100, 0xFFFFFFCE+2000, 3000, 4001}
    //   = {1100, 0x000007CE, 3000, 4001}
    alignas(4) s16 src[4] = { 100, -50, 0, 1 };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 1000u;
    vif0.MaskRow._u32[1] = 2000u;
    vif0.MaskRow._u32[2] = 3000u;
    vif0.MaskRow._u32[3] = 4000u;
    do_unpack(dst, src, 3, 1, /*mode=*/1);
    // -50 sign-extended = 0xFFFFFFCE; +2000 (u32 add) = 0xFFFFFFCE + 2000 = 0x000007BE
    // Wait: 0xFFFFFFCE = 4294967246; + 2000 = 4294969246 = 0x000007BE mod 2^32
    // Let me recalculate: 0xFFFFFFCE + 0x000007D0 = 0x1000079E... no
    // 0xFFFFFFCE = -50 as u32 = 4294967246
    // 4294967246 + 2000 = 4294969246
    // 4294969246 mod 2^32 = 4294969246 - 4294967296 = 1950 = 0x0000079E
    // Hmm, but it's u32 add so it wraps: yes 0x79E = 1950.
    // Actually -50 + 2000 = 1950. The add is u32 but the result is the same.
    chk("Mode1 V4-16s add row: {1100,1950,3000,4001}", dst,
        1100u, 1950u, 3000u, 4001u);
}

// ──────────────────────────────────────────────────────────────
// Mode 2 — accumulate: dest = data + MaskRow[i]; MaskRow[i] = dest
// ──────────────────────────────────────────────────────────────

static void test_mode2_accumulate()
{
    alignas(16) u32 src[4] = { 1u, 2u, 3u, 4u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 100u;
    vif0.MaskRow._u32[1] = 200u;
    vif0.MaskRow._u32[2] = 300u;
    vif0.MaskRow._u32[3] = 400u;
    do_unpack(dst, src, 3, 0, /*mode=*/2);
    chk("Mode2 accumulate dest: {101,202,303,404}", dst,
        101u, 202u, 303u, 404u);
    chk_row("Mode2 row updated to dest",
            101u, 202u, 303u, 404u);
}

static void test_mode2_accumulate_twice()
{
    // Second call with same src accumulates again
    alignas(16) u32 src[4] = { 5u, 5u, 5u, 5u };
    alignas(16) u32 dst1[4] = {};
    alignas(16) u32 dst2[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = vif0.MaskRow._u32[1] =
    vif0.MaskRow._u32[2] = vif0.MaskRow._u32[3] = 0u;
    do_unpack(dst1, src, 3, 0, 2); // row: 0+5=5; dst1={5,5,5,5}; row={5,5,5,5}
    do_unpack(dst2, src, 3, 0, 2); // row: 5+5=10; dst2={10,10,10,10}; row={10,10,10,10}
    chk("Mode2 accumulate twice dst2: {10,...}", dst2, 10u, 10u, 10u, 10u);
    chk_row("Mode2 accumulate twice row={10,...}", 10u, 10u, 10u, 10u);
}

// ──────────────────────────────────────────────────────────────
// Mode 3 — set row: dest = MaskRow[i] = data
// ──────────────────────────────────────────────────────────────

static void test_mode3_set_row()
{
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    do_unpack(dst, src, 3, 0, /*mode=*/3);
    chk("Mode3 set row dest=data", dst, 0xAAu, 0xBBu, 0xCCu, 0xDDu);
    chk_row("Mode3 row set to data", 0xAAu, 0xBBu, 0xCCu, 0xDDu);
}

// ──────────────────────────────────────────────────────────────
// Mask tests (doMask=1, mode=0)
//
// At cl=0, mask bits for each component (2-bit nibbles in vif0Regs.mask):
//   bits[1:0] = X (offnum 0)
//   bits[3:2] = Y (offnum 1)
//   bits[5:4] = Z (offnum 2)
//   bits[7:6] = W (offnum 3)
//   0=data  1=MaskRow[i]  2=MaskCol[cl]  3=write-protect
// ──────────────────────────────────────────────────────────────

static void test_mask_x_from_row()
{
    // mask=0x01 → X from MaskRow[0]; Y/Z/W from data
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 0x1234u;
    vif0Regs.mask = 0x01u;
    do_unpack(dst, src, 3, 0, 0, /*doMask=*/true);
    chk("Mask X from row: {0x1234,BB,CC,DD}", dst, 0x1234u, 0xBBu, 0xCCu, 0xDDu);
}

static void test_mask_all_from_row()
{
    // mask=0x55 = 01 01 01 01 → all from MaskRow
    alignas(16) u32 src[4] = { 0x11u, 0x22u, 0x33u, 0x44u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 0xA0u;
    vif0.MaskRow._u32[1] = 0xB0u;
    vif0.MaskRow._u32[2] = 0xC0u;
    vif0.MaskRow._u32[3] = 0xD0u;
    vif0Regs.mask = 0x55u;
    do_unpack(dst, src, 3, 0, 0, true);
    chk("Mask all from row: {A0,B0,C0,D0}", dst, 0xA0u, 0xB0u, 0xC0u, 0xD0u);
}

static void test_mask_x_from_col()
{
    // mask=0x02 (bits[1:0]=10) → X from MaskCol[cl=0]; Y/Z/W from data
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskCol._u32[0] = 0x5678u;
    vif0Regs.mask = 0x02u;
    do_unpack(dst, src, 3, 0, 0, true);
    chk("Mask X from col[0]: {0x5678,BB,CC,DD}", dst, 0x5678u, 0xBBu, 0xCCu, 0xDDu);
}

static void test_mask_x_write_protect()
{
    // mask=0x03 (bits[1:0]=11) → X write-protected; Y/Z/W from data
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = { 0xDEADu, 0u, 0u, 0u }; // X preset
    reset_vif0();
    vif0Regs.mask = 0x03u;
    do_unpack(dst, src, 3, 0, 0, true);
    chk("Mask X write-protect: {0xDEAD,BB,CC,DD}", dst, 0xDEADu, 0xBBu, 0xCCu, 0xDDu);
}

static void test_mask_w_write_protect()
{
    // mask=0xC0 (bits[7:6]=11) → W write-protected; X/Y/Z from data
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = { 0u, 0u, 0u, 0xBEEFu }; // W preset
    reset_vif0();
    vif0Regs.mask = 0xC0u;
    do_unpack(dst, src, 3, 0, 0, true);
    chk("Mask W write-protect: {AA,BB,CC,0xBEEF}", dst, 0xAAu, 0xBBu, 0xCCu, 0xBEEFu);
}

static void test_mask_mixed()
{
    // mask=0x63:  bits[1:0]=11(X wp), bits[3:2]=00(Y data), bits[5:4]=10(Z col), bits[7:6]=01(W row)
    // X write-protect, Y data, Z from col, W from row
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = { 0x1111u, 0u, 0u, 0u }; // X preset
    reset_vif0();
    vif0.MaskRow._u32[3] = 0x9999u;
    vif0.MaskCol._u32[0] = 0x7777u; // col at cl=0
    vif0Regs.mask = 0x63u;           // 01 10 00 11
    do_unpack(dst, src, 3, 0, 0, true);
    // X=write-protect→0x1111, Y=data→0xBB, Z=col[0]→0x7777, W=row[3]→0x9999
    chk("Mask mixed wp/data/col/row", dst, 0x1111u, 0xBBu, 0x7777u, 0x9999u);
}

static void test_mask_cl1()
{
    // At cl=1, mask bits are in mask[15:8]:
    //   offnum=0(X): bits[9:8];  offnum=1(Y): bits[11:10]
    //   mask=0x0400: bits[11:10]=01 → Y from row at cl=1; X/Z/W from data
    alignas(16) u32 src[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.cl = 1;
    vif0.MaskRow._u32[1] = 0x9999u;
    vif0Regs.mask = 0x0400u;
    do_unpack(dst, src, 3, 0, 0, true);
    chk("Mask cl=1: Y from row, X/Z/W from data", dst, 0xAAu, 0x9999u, 0xCCu, 0xDDu);
}

// ──────────────────────────────────────────────────────────────
// Combined scenarios
// ──────────────────────────────────────────────────────────────

static void test_s8_broadcast_w_protect()
{
    // S-8 signed broadcast (-2) with W write-protected
    // Each component = 0xFFFFFFFE, but W stays preset
    alignas(4) s8 src[1] = { -2 };
    alignas(16) u32 dst[4] = { 0u, 0u, 0u, 0xCAFEu };
    reset_vif0();
    vif0Regs.mask = 0xC0u; // W write-protect
    do_unpack(dst, src, 0, 2, 0, true); // S-8 doMask
    chk("S-8 broadcast W write-protect", dst,
        0xFFFFFFFEu, 0xFFFFFFFEu, 0xFFFFFFFEu, 0xCAFEu);
}

static void test_v4_16_unsigned_mode1()
{
    // V4-16 unsigned with mode 1: zero-extend then add row
    // src={0xFFFF, 0, 1, 0x8000} → data={0x0000FFFF, 0, 1, 0x8000}
    // row={1, 2, 3, 4}
    // dest={0x10000, 2, 4, 0x8004}
    alignas(4) u16 src[4] = { 0xFFFFu, 0u, 1u, 0x8000u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 1u;
    vif0.MaskRow._u32[1] = 2u;
    vif0.MaskRow._u32[2] = 3u;
    vif0.MaskRow._u32[3] = 4u;
    do_unpack(dst, src, 3, 1, 1, false, /*usn=*/true);
    chk("V4-16u mode1 add row", dst, 0x10000u, 2u, 4u, 0x8004u);
}

static void test_v2_32_mode2()
{
    // V2-32 with mode 2 (accumulate): {A,B,A,B} + row, then row=dest
    // src={10, 20} → data={10, 20, 10, 20}; row={1, 2, 3, 4}
    // dest={11, 22, 13, 24}; row updated to same
    alignas(4) u32 src[2] = { 10u, 20u };
    alignas(16) u32 dst[4] = {};
    reset_vif0();
    vif0.MaskRow._u32[0] = 1u;
    vif0.MaskRow._u32[1] = 2u;
    vif0.MaskRow._u32[2] = 3u;
    vif0.MaskRow._u32[3] = 4u;
    do_unpack(dst, src, 1, 0, /*mode=*/2);
    chk("V2-32 mode2 accumulate: {11,22,13,24}", dst, 11u, 22u, 13u, 24u);
    chk_row("V2-32 mode2 row updated", 11u, 22u, 13u, 24u);
}

// ──────────────────────────────────────────────────────────────
// Entry point
// ──────────────────────────────────────────────────────────────

void RunVifTests()
{
    s_pass = s_fail = 0;
    LOGI("=== VIF UNPACK tests start ===");

    // ── Mode 0: direct unpack ──────────────────────────────────
    test_v4_32_direct();
    test_v4_16_signed();
    test_v4_8_signed();
    test_v4_16_unsigned();
    test_v4_8_unsigned();
    test_v4_5();
    test_v4_5_zero();
    test_s32_broadcast();
    test_s16_signed_broadcast();
    test_s16_signed_broadcast_pos();
    test_s8_signed_broadcast();
    test_s16_unsigned_broadcast();
    test_s8_unsigned_broadcast();
    test_v2_32();
    test_v2_16_signed();
    test_v2_16_unsigned();
    test_v3_32();

    // ── Mode 1: add row ────────────────────────────────────────
    test_mode1_add_row();
    test_mode1_s16_add_row();

    // ── Mode 2: accumulate ─────────────────────────────────────
    test_mode2_accumulate();
    test_mode2_accumulate_twice();

    // ── Mode 3: set row ────────────────────────────────────────
    test_mode3_set_row();

    // ── Mask: per-component override ──────────────────────────
    test_mask_x_from_row();
    test_mask_all_from_row();
    test_mask_x_from_col();
    test_mask_x_write_protect();
    test_mask_w_write_protect();
    test_mask_mixed();
    test_mask_cl1();

    // ── Combined scenarios ─────────────────────────────────────
    test_s8_broadcast_w_protect();
    test_v4_16_unsigned_mode1();
    test_v2_32_mode2();

    const int total = s_pass + s_fail;
    LOGI("=== VIF UNPACK tests done: %d/%d passed ===", s_pass, total);
    ReportTestResults("VifTests", s_pass, total);
}
