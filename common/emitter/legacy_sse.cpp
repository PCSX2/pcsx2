// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/emitter/legacy_internal.h"

using namespace x86Emitter;

// ------------------------------------------------------------------------
//                         Begin SSE-Only Part!
// ------------------------------------------------------------------------

#define DEFINE_LEGACY_SSSD_OPCODE(mod)                                                                                                \
    emitterT void SSE_##mod##SS_XMM_to_XMM(x86SSERegType to, x86SSERegType from) { x##mod.SS(xRegisterSSE(to), xRegisterSSE(from)); } \
    emitterT void SSE2_##mod##SD_XMM_to_XMM(x86SSERegType to, x86SSERegType from) { x##mod.SD(xRegisterSSE(to), xRegisterSSE(from)); }

DEFINE_LEGACY_SSSD_OPCODE(SUB)
DEFINE_LEGACY_SSSD_OPCODE(ADD)

DEFINE_LEGACY_SSSD_OPCODE(MIN)
DEFINE_LEGACY_SSSD_OPCODE(MAX)
