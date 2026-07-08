// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "run_tests.h"
#include "../test_bridge.h"

#include "vixl/aarch64/macro-assembler-aarch64.h"

#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace vixl::aarch64;

#define TAG "ARM64CodegenTest"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

static int s_pass;
static int s_fail;

static void runTest(void (*exec)(MacroAssembler&), const char* desc, const char* expected)
{
	static vixl::byte code[4096];
	memset(code, 0, sizeof(code));

	MacroAssembler masm(code, sizeof(code));
	exec(masm);
	masm.FinalizeCode();

	const size_t size = masm.GetSizeOfCodeGenerated();

	char str[4096 * 3] = {};
	char* p = str;
	for (size_t i = 0; i < size; i++)
	{
		sprintf(p, "%02x ", reinterpret_cast<const uint8_t*>(code)[i]);
		p += 3;
	}
	if (p != str)
		*--p = '\0';

	if (strcasecmp(expected, str) == 0)
	{
		s_pass++;
		LOGI("PASS  %s", desc);
	}
	else
	{
		s_fail++;
		LOGE("FAIL  %s", desc);
		LOGE("      expected: %s", expected);
		LOGE("      got:      %s", str);
	}
}

#define T(cmd, expected) \
	runTest([](MacroAssembler& armAsm) { using namespace vixl::aarch64; cmd; }, #cmd, expected)

void RunArmCodegenTests()
{
	s_pass = 0;
	s_fail = 0;
	LOGI("=== ARM64 Codegen Tests ===");

	// --- Integer register moves ---
	T(armAsm.Mov(x0, x1),   "e0 03 01 aa");
	T(armAsm.Mov(x1, x0),   "e1 03 00 aa");
	T(armAsm.Mov(w0, w1),   "e0 03 01 2a");
	T(armAsm.Mov(w2, w3),   "e2 03 03 2a");

	// --- Integer arithmetic (register) ---
	T(armAsm.Add(x0, x1, x2),  "20 00 02 8b");
	T(armAsm.Add(w0, w1, w2),  "20 00 02 0b");
	T(armAsm.Sub(x0, x1, x2),  "20 00 02 cb");
	T(armAsm.Sub(w0, w1, w2),  "20 00 02 4b");
	T(armAsm.And(x0, x1, x2),  "20 00 02 8a");
	T(armAsm.Orr(x0, x1, x2),  "20 00 02 aa");
	T(armAsm.Eor(x0, x1, x2),  "20 00 02 ca");
	T(armAsm.Subs(x0, x1, x2), "20 00 02 eb");
	T(armAsm.Cmp(x1, x2),      "3f 00 02 eb");

	// --- Integer arithmetic (immediate) ---
	T(armAsm.Add(x0, x1, 1),  "20 04 00 91");
	T(armAsm.Add(x0, x1, 4),  "20 10 00 91");
	T(armAsm.Sub(x0, x1, 1),  "20 04 00 d1");
	T(armAsm.Sub(x0, x1, 4),  "20 10 00 d1");
	T(armAsm.Add(w0, w1, 4),  "20 10 00 11");

	// --- Move immediate ---
	T(armAsm.Mov(x0, uint64_t{0}),      "00 00 80 d2");
	T(armAsm.Mov(x0, uint64_t{1}),      "20 00 80 d2");
	T(armAsm.Mov(x0, uint64_t{0x1234}), "80 46 82 d2");
	T(armAsm.Mov(x0, uint64_t{0xffff}), "e0 ff 9f d2");

	// --- Shifts (immediate) ---
	T(armAsm.Lsl(x0, x1, 4), "20 ec 7c d3");
	T(armAsm.Lsr(x0, x1, 4), "20 fc 44 d3");
	T(armAsm.Asr(x0, x1, 4), "20 fc 44 93");
	T(armAsm.Lsl(w0, w1, 4), "20 6c 1c 53");

	// --- Load / Store ---
	T(armAsm.Ldr(x0, MemOperand(x1)),    "20 00 40 f9");
	T(armAsm.Str(x0, MemOperand(x1)),    "20 00 00 f9");
	T(armAsm.Ldr(w0, MemOperand(x1)),    "20 00 40 b9");
	T(armAsm.Str(w0, MemOperand(x1)),    "20 00 00 b9");
	T(armAsm.Ldr(x0, MemOperand(x1, 8)), "20 04 40 f9");
	T(armAsm.Ldr(w0, MemOperand(x1, 4)), "20 04 40 b9");
	T(armAsm.Ldr(x0, MemOperand(x1, x2)), "20 68 62 f8");

	// --- Load / Store pair ---
	T(armAsm.Stp(x0, x1, MemOperand(x2, -16, PreIndex)), "40 04 bf a9");
	T(armAsm.Ldp(x0, x1, MemOperand(x2, 16, PostIndex)), "40 04 c1 a8");

	// --- Misc ---
	T(armAsm.Ret(),    "c0 03 5f d6");
	T(armAsm.Nop(),    "1f 20 03 d5");
	T(armAsm.Blr(x0), "00 00 3f d6");
	T(armAsm.Br(x0),  "00 00 1f d6");

	// --- NEON vector ---
	T(armAsm.Mov(v0.V16B(),  v1.V16B()),            "20 1c a1 4e");
	T(armAsm.Add(v0.V4S(),   v1.V4S(),  v2.V4S()),  "20 84 a2 4e");
	T(armAsm.And(v0.V16B(),  v1.V16B(), v2.V16B()), "20 1c 22 4e");
	T(armAsm.Eor(v0.V16B(),  v1.V16B(), v2.V16B()), "20 1c 22 6e");
	T(armAsm.Orr(v0.V16B(),  v1.V16B(), v2.V16B()), "20 1c a2 4e");
	T(armAsm.Fadd(v0.V4S(),  v1.V4S(),  v2.V4S()),  "20 d4 22 4e");
	T(armAsm.Fmul(v0.V4S(),  v1.V4S(),  v2.V4S()),  "20 dc 22 6e");
	T(armAsm.Fsub(v0.V4S(),  v1.V4S(),  v2.V4S()),  "20 d4 a2 4e");
	T(armAsm.Fdiv(v0.V4S(),  v1.V4S(),  v2.V4S()),  "20 fc 22 6e");
	T(armAsm.Dup(v0.V4S(),   w1),                   "20 0c 04 4e");

	// --- NEON shuffle ---
	T(armAsm.Zip1(v0.V4S(), v1.V4S(), v2.V4S()), "20 38 82 4e");
	T(armAsm.Zip2(v0.V4S(), v1.V4S(), v2.V4S()), "20 78 82 4e");
	T(armAsm.Uzp1(v0.V4S(), v1.V4S(), v2.V4S()), "20 18 82 4e");
	T(armAsm.Uzp2(v0.V4S(), v1.V4S(), v2.V4S()), "20 58 82 4e");
	T(armAsm.Trn1(v0.V4S(), v1.V4S(), v2.V4S()), "20 28 82 4e");
	T(armAsm.Trn2(v0.V4S(), v1.V4S(), v2.V4S()), "20 68 82 4e");

	// --- NEON conversion ---
	T(armAsm.Scvtf(v0.V4S(),  v1.V4S()), "20 d8 21 4e");
	T(armAsm.Fcvtzs(v0.V4S(), v1.V4S()), "20 b8 a1 4e");

	LOGI("=== Results: %d passed, %d failed ===", s_pass, s_fail);
	ReportTestResults("CodegenTests", s_pass, s_pass + s_fail);
}

#undef T
#undef LOGI
#undef LOGE
#undef TAG
