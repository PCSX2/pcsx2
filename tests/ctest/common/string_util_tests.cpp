/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"
#include "common/StringUtil.h"
#include <locale>
#include <clocale>
#include <gtest/gtest.h>

TEST(StringUtil, ToChars)
{
	ASSERT_EQ(StringUtil::ToChars(false), "false");
	ASSERT_EQ(StringUtil::ToChars(true), "true");
	ASSERT_EQ(StringUtil::ToChars(0), "0");
	ASSERT_EQ(StringUtil::ToChars(-1337), "-1337");
	ASSERT_EQ(StringUtil::ToChars(1337), "1337");
	ASSERT_EQ(StringUtil::ToChars(1337u), "1337");
	ASSERT_EQ(StringUtil::ToChars(13.37f), "13.37");
	ASSERT_EQ(StringUtil::ToChars(255, 16), "ff");
}

TEST(StringUtil, FromChars)
{
	ASSERT_EQ(StringUtil::FromChars<bool>("false").value_or(true), false);
	ASSERT_EQ(StringUtil::FromChars<bool>("true").value_or(false), true);
	ASSERT_EQ(StringUtil::FromChars<int>("0").value_or(-1), 0);
	ASSERT_EQ(StringUtil::FromChars<int>("-1337").value_or(0), -1337);
	ASSERT_EQ(StringUtil::FromChars<int>("1337").value_or(0), 1337);
	ASSERT_EQ(StringUtil::FromChars<u32>("1337").value_or(0), 1337);
	ASSERT_TRUE(std::abs(StringUtil::FromChars<float>("13.37").value_or(0.0f) - 13.37) < 0.01f);
	ASSERT_EQ(StringUtil::FromChars<int>("ff", 16).value_or(0), 255);
}

#if 0
// NOTE: These tests are disabled, because they require the da_DK locale to actually be present.
// Which probably isn't going to be the case on the CI.

TEST(StringUtil, ToCharsIsLocaleIndependent)
{
	const auto old_locale = std::locale();
	std::locale::global(std::locale::classic());

	std::string classic_result(StringUtil::ToChars(13.37f));

	std::locale::global(std::locale("da_DK"));

	std::string dk_result(StringUtil::ToChars(13.37f));

	std::locale::global(old_locale);

	ASSERT_EQ(classic_result, dk_result);
}

TEST(StringUtil, FromCharsIsLocaleIndependent)
{
	const auto old_locale = std::locale();
	std::locale::global(std::locale::classic());

	const float classic_result = StringUtil::FromChars<float>("13.37").value_or(0.0f);

	std::locale::global(std::locale("da_DK"));

	const float dk_result = StringUtil::FromChars<float>("13.37").value_or(0.0f);

	std::locale::global(old_locale);

	ASSERT_EQ(classic_result, dk_result);
}

#endif
