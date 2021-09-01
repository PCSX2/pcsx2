/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
