// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// Monorepo Android build: the recompiler/VIF/patch unit-test harnesses under
// tests/ are compiled OUT of the emucore target because they depend on the
// arm64/mac recompiler backend's test hooks (EE_Test*/mVU0_Test*) that are not
// part of this core, plus a gtest scaffold. native-lib.cpp still exposes the
// dev-only "run tests" JNI entry points, so we provide no-op definitions here
// to keep the library linkable. Restore the real tests once a compatible test
// backend is reconciled into this core.

#include "tests/arm64/run_tests.h"
#include "tests/core/run_patch_tests.h"
#include "tests/mvu/run_mvu_tests.h"
#include "tests/ee/run_ee_tests.h"
#include "tests/ee/run_ee_seq_tests.h"
#include "tests/vif/run_vif_tests.h"

#include <android/log.h>

namespace
{
	void LogTestsDisabled(const char* which)
	{
		__android_log_print(ANDROID_LOG_INFO, "ARMSX2",
			"%s: recompiler self-tests are disabled in this build", which);
	}
} // namespace

void RunArmCodegenTests() { LogTestsDisabled("RunArmCodegenTests"); }
void RunPatchTests() { LogTestsDisabled("RunPatchTests"); }
void RunVuJitTests() { LogTestsDisabled("RunVuJitTests"); }
void RunEeJitTests() { LogTestsDisabled("RunEeJitTests"); }
void RunEeSeqTests() { LogTestsDisabled("RunEeSeqTests"); }
void RunVifTests() { LogTestsDisabled("RunVifTests"); }
