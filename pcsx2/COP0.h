// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

struct tlbs;

extern void WriteCP0Status(u32 value);
extern void WriteCP0Config(u32 value);
extern void cpuUpdateOperationMode();
extern void WriteTLB(int i);
extern void UnmapTLB(const tlbs& t, int i);
extern void MapTLB(const tlbs& t, int i);

extern void COP0_UpdatePCCR();
extern void COP0_DiagnosticPCCR();
