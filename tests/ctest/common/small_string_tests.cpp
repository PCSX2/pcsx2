// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/SmallString.h"
#include <gtest/gtest.h>

TEST(StackString, SelfAssignment)
{
	SmallStackString<6> string("Hello");
	string = string;
	ASSERT_STREQ(string.c_str(), "Hello");
}
