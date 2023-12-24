// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

TEST(StringUtil, FromCharsWithEndPtr)
{
	using namespace std::literals;

	std::string_view endptr;
	ASSERT_EQ(StringUtil::FromChars<u32>("123x456", 16, &endptr), std::optional<u32>(0x123));
	ASSERT_EQ(endptr, "x456"sv);

	ASSERT_EQ(StringUtil::FromChars<u32>("0x1234", 16, &endptr), std::optional<u32>(0u));
	ASSERT_EQ(endptr, "x1234"sv);

	ASSERT_EQ(StringUtil::FromChars<u32>("1234", 16, &endptr), std::optional<u32>(0x1234u));
	ASSERT_TRUE(endptr.empty());

	ASSERT_EQ(StringUtil::FromChars<u32>("abcdefg", 16, &endptr), std::optional<u32>(0xabcdef));
	ASSERT_EQ(endptr, "g"sv);

	ASSERT_EQ(StringUtil::FromChars<u32>("123abc", 10, &endptr), std::optional<u32>(123));
	ASSERT_EQ(endptr, "abc"sv);

	ASSERT_EQ(StringUtil::FromChars<float>("1.0g", &endptr), std::optional<float>(1.0f));
	ASSERT_EQ(endptr, "g"sv);

	ASSERT_EQ(StringUtil::FromChars<float>("2x", &endptr), std::optional<float>(2.0f));
	ASSERT_EQ(endptr, "x"sv);

	ASSERT_EQ(StringUtil::FromChars<float>(".1p", &endptr), std::optional<float>(0.1f));
	ASSERT_EQ(endptr, "p"sv);

	ASSERT_EQ(StringUtil::FromChars<float>("1", &endptr), std::optional<float>(1.0f));
	ASSERT_TRUE(endptr.empty());
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

TEST(StringUtil, Ellipsise)
{
	ASSERT_EQ(StringUtil::Ellipsise("HelloWorld", 6, "..."), "Hel...");
	ASSERT_EQ(StringUtil::Ellipsise("HelloWorld", 7, ".."), "Hello..");
	ASSERT_EQ(StringUtil::Ellipsise("HelloWorld", 20, ".."), "HelloWorld");
	ASSERT_EQ(StringUtil::Ellipsise("", 20, "..."), "");
	ASSERT_EQ(StringUtil::Ellipsise("Hello", 10, "..."), "Hello");
}

TEST(StringUtil, EllipsiseInPlace)
{
	std::string s;
	s = "HelloWorld";
	StringUtil::EllipsiseInPlace(s, 6, "...");
	ASSERT_EQ(s, "Hel...");
	s = "HelloWorld";
	StringUtil::EllipsiseInPlace(s, 7, "..");
	ASSERT_EQ(s, "Hello..");
	s = "HelloWorld";
	StringUtil::EllipsiseInPlace(s, 20, "..");
	ASSERT_EQ(s, "HelloWorld");
	s = "";
	StringUtil::EllipsiseInPlace(s, 20, "...");
	ASSERT_EQ(s, "");
	s = "Hello";
	StringUtil::EllipsiseInPlace(s, 10, "...");
	ASSERT_EQ(s, "Hello");
}
