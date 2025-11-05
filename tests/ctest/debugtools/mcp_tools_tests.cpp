// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "gtest/gtest.h"

#include "rapidjson/document.h"

#include <cstdlib>
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
class MCPToolsTest : public ::testing::Test
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

	rapidjson::Document ParseResponse(size_t index = 0)
	{
		EXPECT_LT(index, responses.size());
		rapidjson::Document doc;
		doc.Parse(responses[index].c_str(), responses[index].size());
		EXPECT_FALSE(doc.HasParseError());
		return doc;
	}

	std::vector<std::string> responses;
};

// Test emulator_control tool
TEST_F(MCPToolsTest, EmulatorControlMissingAction)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":1,"cmd":"emulator_control","params":{}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

TEST_F(MCPToolsTest, EmulatorControlInvalidAction)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":2,"cmd":"emulator_control","params":{"action":"invalid"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
}

TEST_F(MCPToolsTest, EmulatorControlSaveStateMissingPath)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":3,"cmd":"emulator_control","params":{"action":"save_state"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

// Test mem_read tool
TEST_F(MCPToolsTest, MemReadMissingSpace)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":10,"cmd":"mem_read","params":{"addr":0,"size":16}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

TEST_F(MCPToolsTest, MemReadMissingAddr)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":11,"cmd":"mem_read","params":{"space":"EE","size":16}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
}

TEST_F(MCPToolsTest, MemReadMissingSize)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":12,"cmd":"mem_read","params":{"space":"EE","addr":0}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
}

TEST_F(MCPToolsTest, MemReadInvalidSpace)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":13,"cmd":"mem_read","params":{"space":"INVALID","addr":0,"size":16}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

TEST_F(MCPToolsTest, MemReadSizeTooLarge)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":14,"cmd":"mem_read","params":{"space":"EE","addr":0,"size":2000000}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

TEST_F(MCPToolsTest, MemReadUnsupportedSpace)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":15,"cmd":"mem_read","params":{"space":"VU0","addr":0,"size":16}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
}

// Test mem_write tool
TEST_F(MCPToolsTest, MemWriteWithoutPermission)
{
	// Ensure PCSX2_ALLOW_WRITES is not set
	unsetenv("PCSX2_ALLOW_WRITES");

	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":20,"cmd":"mem_write","params":{"space":"EE","addr":0,"hex_bytes":"deadbeef"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("tool_error", error["code"].GetString());
}

TEST_F(MCPToolsTest, MemWriteMissingSpace)
{
	setenv("PCSX2_ALLOW_WRITES", "true", 1);

	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":21,"cmd":"mem_write","params":{"addr":0,"hex_bytes":"deadbeef"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());

	unsetenv("PCSX2_ALLOW_WRITES");
}

TEST_F(MCPToolsTest, MemWriteInvalidHexBytes)
{
	setenv("PCSX2_ALLOW_WRITES", "true", 1);

	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":22,"cmd":"mem_write","params":{"space":"EE","addr":0,"hex_bytes":"xyz"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());

	unsetenv("PCSX2_ALLOW_WRITES");
}

// Test regs_get tool
TEST_F(MCPToolsTest, RegsGetMissingTarget)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":30,"cmd":"regs_get","params":{}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

TEST_F(MCPToolsTest, RegsGetInvalidTarget)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":31,"cmd":"regs_get","params":{"target":"INVALID"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
}

TEST_F(MCPToolsTest, RegsGetUnsupportedTarget)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":32,"cmd":"regs_get","params":{"target":"VU0"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
}

// Test scan_memory tool (stub)
TEST_F(MCPToolsTest, ScanMemoryNotImplemented)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":40,"cmd":"scan_memory","params":{"space":"EE","type":"u32","query":"12345"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
	EXPECT_NE(std::string(error["message"].GetString()).find("MemoryScanner"), std::string::npos);
}

// Test trace_start tool (stub)
TEST_F(MCPToolsTest, TraceStartNotImplemented)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":50,"cmd":"trace_start","params":{"cpu":"EE"}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
	EXPECT_NE(std::string(error["message"].GetString()).find("InstructionTracer"), std::string::npos);
}

// Test trace_stop tool (stub)
TEST_F(MCPToolsTest, TraceStopNotImplemented)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":60,"cmd":"trace_stop","params":{}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
	EXPECT_NE(std::string(error["message"].GetString()).find("InstructionTracer"), std::string::npos);
}

// Test dump_memory tool
TEST_F(MCPToolsTest, DumpMemoryValidatesParams)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":70,"cmd":"dump_memory","params":"invalid"})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("invalid_request", error["code"].GetString());
}

// Test unknown command
TEST_F(MCPToolsTest, UnknownCommand)
{
	DebugTools::MCPServer::detail::ProcessCommandLine(
		R"({"id":100,"cmd":"unknown_cmd","params":{}})");

	ASSERT_EQ(responses.size(), 1u);
	auto doc = ParseResponse();

	ASSERT_TRUE(doc.IsObject());
	ASSERT_TRUE(doc.HasMember("ok"));
	EXPECT_FALSE(doc["ok"].GetBool());
	ASSERT_TRUE(doc.HasMember("error"));
	const rapidjson::Value& error = doc["error"];
	EXPECT_STREQ("not_implemented", error["code"].GetString());
}

} // namespace
