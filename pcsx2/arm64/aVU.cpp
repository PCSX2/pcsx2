// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 microVU recompiler (Phase 7).
//
// Task 7.2a landed the arch-neutral data structures in aVU.h. This translation
// unit currently only validates that those structures compile and have the
// expected layout on ARM64. The real recompiler shell — program/block cache
// management and the recMicroVU0/1 providers — is task 7.2c, and the host
// register allocator (microRegAlloc) it depends on is task 7.2b. Until then this
// file is intentionally minimal so the header is actually exercised by the build.

#include "arm64/aVU.h"

// Layout sanity checks for the ported pipeline-state key / IR structs. These
// mirror the invariants the x86 microVU relies on (the 96-byte microRegInfo is
// compared as six 128-bit vectors by the generated compareStateF).
static_assert(sizeof(microRegInfo) == 96, "microRegInfo must stay 96 bytes (host pipeline-state compare)");
static_assert(alignof(microRegInfo) == 16, "microRegInfo must stay 16-byte aligned");
static_assert(alignof(microBlock) == 16, "microBlock must stay 16-byte aligned");
