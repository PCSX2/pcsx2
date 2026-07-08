// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// [P30] iOS Native EE Test Harness
// BIOS/SIF/IOP 非依存で EE JIT 命令精度を検証する。
// iPSX2_TEST_HARNESS=1 でenabled化。デフォルト OFF。
//
// テストコード (MIPS R5900 マシンコード) を eeMem に直接書き込み、
// cpuRegs.pc をconfigして実行。resultは EE memoryに書き込まれ、
// vsync handlerがログ出力する。

#include "common/Pcsx2Types.h"
#include <string>

namespace TestHarness
{
	// テストresultmemoryレイアウト
	// kseg0 address (TLB not needed、物理addressに直接mapping)
	// 0x01F00000 (31MB) は BIOS/OSDSYS ワークエリア外
	static constexpr u32 PHYS_CODE   = 0x01F00000u; // eeMem 書き込み先 (物理)
	static constexpr u32 PHYS_RESULT = 0x01FF0000u;
	static constexpr u32 CODE_BASE   = 0x81F00000u; // EE PC 用 kseg0 address
	static constexpr u32 RESULT_BASE = 0x81FF0000u;
	static constexpr u32 STACK_TOP   = 0x81FE0000u;

	// ヘッダ (CODE_BASE に配置)
	struct Header {
		u32 magic;        // 0x54455354 ("TEST")
		u32 test_count;
		u32 pass_count;
		u32 fail_count;
		u32 current_test;
		u32 status;       // 0=running, 1=complete, 2=error
	};

	// 個別テストresult (RESULT_BASE + n*16)
	struct Result {
		u32 test_id;
		u32 expected;
		u32 actual;
		u32 pass; // 1=pass, 0=fail
	};

	// テストハーネスがenabledかどうか
	bool IsEnabled();

	// テストコードを eeMem に注入し cpuRegs.pc をconfig
	// eeloadHook n=1 のタイミングで呼ばれる
	void InjectTests();

	// vsync でresultを読み取りログ出力
	// 戻り値: true=テスト完了 (status==1)
	bool CheckResults(u32 vsync_count);

	// Force inject (sets flag, actual inject happens at next vsync on CPU thread)
	void ForceInject();
	void ForceInjectMini(); // Mini stress test for hang investigation
	bool CheckForceInject();

	// Get last results as string
	std::string GetResultsString();
}
