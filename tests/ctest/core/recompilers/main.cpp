// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/RecompilerTestEnvironment.h"

#include <gtest/gtest.h>

namespace
{

// gtest-side wrapper that delegates to the shared (gtest-free)
// RecompilerTestEnvironment::Initialize/Shutdown so pcsx2-vurunner can use
// the same setup logic without linking gtest.
class GtestEnvironment : public ::testing::Environment
{
public:
	void SetUp() override
	{
		if (!recompiler_tests::RecompilerTestEnvironment::Initialize())
			ADD_FAILURE() << "RecompilerTestEnvironment::Initialize() failed (likely SysMemory::Allocate)";
	}
	void TearDown() override
	{
		recompiler_tests::RecompilerTestEnvironment::Shutdown();
	}
};

} // namespace

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	::testing::AddGlobalTestEnvironment(new GtestEnvironment());
	return RUN_ALL_TESTS();
}
