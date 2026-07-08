// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

// [P34] SIF/RPC イベント ring buffer — タイムアウト診断用
// iPSX2_SIF_RING=1 でenabled化。デフォルト OFF。

#include "common/Pcsx2Types.h"
#include <cstdlib>
#include <atomic>

namespace SifRing {

enum EventType : u8 {
	BIND_REQ       = 1,  // EE sceSifBindRpc
	CALL_REQ       = 2,  // EE sceSifCallRpc
	RPC_REGISTER   = 3,  // IOP sceSifRegisterRpc
	ICTRL_CHANGE   = 4,  // 0x1078 change
	ISTAT_SET      = 5,  // iopIntcIrq
	ISTAT_CONSUME  = 6,  // iopEventTest consume
	PSXDMA9        = 7,  // IOP→EE SIF0
	PSXDMA10       = 8,  // EE→IOP SIF1 (IOP side)
	IOP_TAG        = 9,  // ProcessIOPTag
	EE_XFER        = 10, // HandleEETransfer
	GAME_EXIT      = 11, // EE → EELOAD idle
};

struct Event {
	u8  type;
	u8  pad;
	u16 pad2;
	u32 ee_cyc;
	u32 iop_cyc;
	u32 d0;
	u32 d1;
};

static constexpr int RING_SIZE = 256;
static constexpr int RING_MASK = RING_SIZE - 1;

inline Event g_ring[RING_SIZE];
inline u32 g_idx = 0;
inline bool g_enabled = false;
inline bool g_initialized = false;

inline bool IsEnabled() {
	if (!g_initialized) {
		g_initialized = true;
		const char* v = getenv("iPSX2_SIF_RING");
		g_enabled = (v && v[0] == '1');
	}
	return g_enabled;
}

// 外部から ee_cyc / iop_cyc を渡す (ヘッダ依存を避けるため)
inline void Record(u8 type, u32 ee_cyc, u32 iop_cyc, u32 d0, u32 d1) {
	if (!IsEnabled()) return;
	u32 i = g_idx++ & RING_MASK;
	g_ring[i] = {type, 0, 0, ee_cyc, iop_cyc, d0, d1};
}

// ダンプ (Console.WriteLn はcall側で行う)
inline void DumpTo(void (*emit)(const char*, u32, u32, u32, u32, u32, u32)) {
	if (!IsEnabled()) return;
	u32 start = (g_idx > RING_SIZE) ? (g_idx - RING_SIZE) : 0;
	u32 end = g_idx;
	for (u32 i = start; i < end; i++) {
		const Event& e = g_ring[i & RING_MASK];
		emit("@@SIF_RING@@ type=%u ee=%u iop=%u d0=%08x d1=%08x seq=%u",
			(u32)e.type, e.ee_cyc, e.iop_cyc, e.d0, e.d1, i);
	}
}

} // namespace SifRing
