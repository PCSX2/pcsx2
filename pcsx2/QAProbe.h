// QAProbe — Quality Assurance probe module for iPSX2 (V34 baseline, 2026-05-25)
//
// Purpose: thin hook-based probe infrastructure for automated JIT vs Interpreter
// regression gates. All probes are env-gated; when env vars unset, hooks are 1-line
// no-op early-returns with negligible cost.
//
// Hook points (3 total, all 1-line calls into this module):
//   1. Counters.cpp VSyncEnd  -> QAProbe::on_vsync_end()
//   2. Gif_Unit.h TransferGSPacketData entry -> QAProbe::on_gif_transfer()
//   3. (optional) QAProbe::on_gif_primitive() called per-PRIM emit
//
// Env vars consumed:
//   iPSX2_SS_AT_VS=N1,N2,...    : at each listed vs, dump 640x480 RGBA framebuffer
//                                  to /tmp/qa_ss_<TAG>_vs<N>.rgba
//   iPSX2_QA_TAG=<tag>           : tag prefix for output filenames (default: "run")
//   iPSX2_GIF_DUMP=1             : emit @@GIF_PKT@@ per transfer with CRC
//   iPSX2_GIF_DUMP_QUOTA=N       : cap @@GIF_PKT@@ count (default 2000000)
//   iPSX2_QA_LOG=1               : emit @@BL_FRAME@@ line per vsync end (default ON)
//
// Output (consumed by Scripts/eval_bench.py, gif_diff.py, ss_compare.py):
//   @@BL_FRAME@@ vs=N gif_pkt=N d_pkt=N d_prim=N d_unp=N d_bytes=N vif1_unpack=N
//   @@GIF_PKT@@  vs=N seq=N path=N tran=N size=N crc=HEX
//   @@QA_SS@@    vs=N path=/tmp/qa_ss_<tag>_vs<N>.rgba size=WxH

#pragma once

#include "common/Pcsx2Types.h"

namespace QAProbe
{
    // Called from Counters.cpp::VSyncEnd. frame_count = pre-increment g_FrameCount.
    void on_vsync_end(u32 frame_count);

    // Called from Gif_Unit.h::TransferGSPacketData on every transfer.
    // path: 0-2 (GIF_PATH_1/2/3 = 0/1/2). pMem may be null when size==0.
    void on_gif_transfer(u32 tran_type, u32 path, const u8* pMem, u32 size);
}
