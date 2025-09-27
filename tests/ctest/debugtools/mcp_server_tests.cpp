// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "gtest/gtest.h"

#include "rapidjson/document.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace DebugTools::MCPServer::detail
{
using ResponseSink = std::function<void(std::string_view)>;

void SetResponseSinkForTesting(ResponseSink sink);
void ResetResponseSinkForTesting();
void ProcessCommandLine(std::string&& line);
} // namespace DebugTools::MCPServer::detail

namespace
{
class ResponseCaptureTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        responses.clear();
        DebugTools::MCPServer::detail::SetResponseSinkForTesting(
                [this](std::string_view payload) { responses.emplace_back(payload); });
    }

    void TearDown() override
    {
        DebugTools::MCPServer::detail::ResetResponseSinkForTesting();
    }

    std::vector<std::string> responses;
};
} // namespace

TEST_F(ResponseCaptureTest, EmitsErrorForMalformedJson)
{
    DebugTools::MCPServer::detail::ProcessCommandLine(std::string("{oops"));

    ASSERT_EQ(responses.size(), 1u);

    rapidjson::Document parsed;
    parsed.Parse(responses.front().c_str(), responses.front().size());

    ASSERT_FALSE(parsed.HasParseError());
    ASSERT_TRUE(parsed.IsObject());
    ASSERT_TRUE(parsed.HasMember("id"));
    EXPECT_TRUE(parsed["id"].IsNull());
    ASSERT_TRUE(parsed.HasMember("ok"));
    EXPECT_FALSE(parsed["ok"].GetBool());
    ASSERT_TRUE(parsed.HasMember("error"));
    const rapidjson::Value& error = parsed["error"];
    ASSERT_TRUE(error.IsObject());
    ASSERT_TRUE(error.HasMember("code"));
    EXPECT_STREQ("parse_error", error["code"].GetString());
    ASSERT_TRUE(error.HasMember("message"));
    EXPECT_STREQ("Failed to parse request", error["message"].GetString());
}

TEST_F(ResponseCaptureTest, EmitsErrorWhenIdMissing)
{
    DebugTools::MCPServer::detail::ProcessCommandLine(std::string(R"({"cmd":"noop"})"));

    ASSERT_EQ(responses.size(), 1u);

    rapidjson::Document parsed;
    parsed.Parse(responses.front().c_str(), responses.front().size());

    ASSERT_FALSE(parsed.HasParseError());
    ASSERT_TRUE(parsed.IsObject());
    ASSERT_TRUE(parsed.HasMember("id"));
    EXPECT_TRUE(parsed["id"].IsNull());
    ASSERT_TRUE(parsed.HasMember("ok"));
    EXPECT_FALSE(parsed["ok"].GetBool());
    ASSERT_TRUE(parsed.HasMember("error"));
    const rapidjson::Value& error = parsed["error"];
    ASSERT_TRUE(error.IsObject());
    ASSERT_TRUE(error.HasMember("code"));
    EXPECT_STREQ("invalid_request", error["code"].GetString());
    ASSERT_TRUE(error.HasMember("message"));
    EXPECT_STREQ("Missing id", error["message"].GetString());
}

