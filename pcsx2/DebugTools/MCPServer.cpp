// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MCPServer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "DebugInterface.h"
#include "Host.h"
#include "VMManager.h"
#include "common/Pcsx2Defs.h"

namespace DebugTools::MCPServer
{
namespace detail
{
using ResponseSink = std::function<void(std::string_view)>;

void SetResponseSinkForTesting(ResponseSink sink);
void ResetResponseSinkForTesting();
void ProcessCommandLine(std::string&& line);
} // namespace detail

namespace
{
std::atomic_bool g_running{false};
std::mutex s_state_mutex;
std::condition_variable s_state_cv;
std::thread s_stdio_thread;
bool s_worker_active = false;

std::mutex s_response_sink_mutex;
detail::ResponseSink s_response_sink;

void DispatchResponse(std::string&& response)
{
    detail::ResponseSink sink;
    {
        std::lock_guard<std::mutex> lock(s_response_sink_mutex);
        sink = s_response_sink;
    }

    if (sink)
    {
        sink(response);
        return;
    }

    std::cout << response;
    std::cout.flush();
}

void WriteResponseDocument(rapidjson::Document& document)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    std::string serialized(buffer.GetString(), buffer.GetSize());
    serialized.push_back('\n');

    DispatchResponse(std::move(serialized));
}

void WriteErrorResponse(const rapidjson::Value* id, const char* code, const char* message)
{
    rapidjson::Document response(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

    if (id)
    {
        rapidjson::Value id_copy(*id, allocator);
        response.AddMember("id", std::move(id_copy), allocator);
    }
    else
    {
        response.AddMember("id", rapidjson::Value().SetNull(), allocator);
    }

    response.AddMember("ok", false, allocator);

    rapidjson::Value error(rapidjson::kObjectType);
    error.AddMember("code", rapidjson::Value().SetString(code, allocator), allocator);
    error.AddMember("message", rapidjson::Value().SetString(message, allocator), allocator);
    response.AddMember("error", std::move(error), allocator);

    WriteResponseDocument(response);
}

void StdioWorker()
{
    std::array<char, 256> chunk{};
    std::string pending;
    pending.reserve(256);

    while (g_running.load(std::memory_order_acquire))
    {
        std::streambuf* const buffer = std::cin.rdbuf();
        if (buffer == nullptr)
            break;

        const std::streamsize available = buffer->in_avail();
        if (available < 0)
        {
            // Stream error; exit the thread.
            break;
        }

        if (available == 0)
        {
            std::unique_lock<std::mutex> lock(s_state_mutex);
            s_state_cv.wait_for(lock, std::chrono::milliseconds(50), [] {
                return !g_running.load(std::memory_order_acquire);
            });
            continue;
        }

        const std::streamsize to_read = std::min<std::streamsize>(available, static_cast<std::streamsize>(chunk.size()));
        const std::streamsize read = std::cin.readsome(chunk.data(), to_read);
        if (read <= 0)
        {
            if (!std::cin.good())
                break;
            continue;
        }

        pending.append(chunk.data(), static_cast<std::size_t>(read));

        std::size_t newline_pos = std::string::npos;
        while ((newline_pos = pending.find('\n')) != std::string::npos)
        {
            std::string line = pending.substr(0, newline_pos);
            pending.erase(0, newline_pos + 1);

            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.empty())
                detail::ProcessCommandLine(std::move(line));
        }
    }

    g_running.store(false, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_worker_active = false;
    }

    s_state_cv.notify_all();
}
} // namespace

namespace detail
{
namespace
{
constexpr const char* ERROR_CODE_PARSE = "parse_error";
constexpr const char* ERROR_CODE_INVALID_REQUEST = "invalid_request";
constexpr const char* ERROR_CODE_UNSUPPORTED = "not_implemented";
constexpr const char* ERROR_CODE_TOOL_ERROR = "tool_error";

constexpr const char* ERROR_MESSAGE_PARSE = "Failed to parse request";
constexpr const char* ERROR_MESSAGE_INVALID_ID = "Missing id";
constexpr const char* ERROR_MESSAGE_INVALID_OBJECT = "Request must be an object";
constexpr const char* ERROR_MESSAGE_INVALID_CMD = "Missing or invalid cmd";
constexpr const char* ERROR_MESSAGE_UNSUPPORTED = "Command handling is not implemented";

// Helper to convert string to BreakPointCpu
std::optional<BreakPointCpu> ParseCpuSpace(const char* space)
{
	if (strcmp(space, "EE") == 0)
		return BREAKPOINT_EE;
	if (strcmp(space, "IOP") == 0)
		return BREAKPOINT_IOP;
	return std::nullopt;
}

// Helper to check if writes are enabled
bool AreWritesEnabled()
{
	const char* env = std::getenv("PCSX2_ALLOW_WRITES");
	return env && (strcmp(env, "true") == 0 || strcmp(env, "1") == 0);
}

// Helper to format bytes as hex string
std::string BytesToHex(const std::vector<u8>& bytes)
{
	std::ostringstream oss;
	for (u8 byte : bytes)
		oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
	return oss.str();
}

// Helper to parse hex string to bytes
std::optional<std::vector<u8>> HexToBytes(const std::string& hex)
{
	if (hex.size() % 2 != 0)
		return std::nullopt;

	std::vector<u8> bytes;
	bytes.reserve(hex.size() / 2);

	for (size_t i = 0; i < hex.size(); i += 2)
	{
		char high = hex[i];
		char low = hex[i + 1];

		auto hexCharToNibble = [](char c) -> std::optional<u8> {
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'a' && c <= 'f')
				return 10 + (c - 'a');
			if (c >= 'A' && c <= 'F')
				return 10 + (c - 'A');
			return std::nullopt;
		};

		auto highNibble = hexCharToNibble(high);
		auto lowNibble = hexCharToNibble(low);

		if (!highNibble || !lowNibble)
			return std::nullopt;

		bytes.push_back((*highNibble << 4) | *lowNibble);
	}

	return bytes;
}

// Write a successful response
void WriteSuccessResponse(const rapidjson::Value* id, const std::function<void(rapidjson::Document&, rapidjson::Document::AllocatorType&)>& addResult)
{
	rapidjson::Document response(rapidjson::kObjectType);
	rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

	if (id)
	{
		rapidjson::Value id_copy(*id, allocator);
		response.AddMember("id", std::move(id_copy), allocator);
	}
	else
	{
		response.AddMember("id", rapidjson::Value().SetNull(), allocator);
	}

	response.AddMember("ok", true, allocator);

	if (addResult)
		addResult(response, allocator);

	WriteResponseDocument(response);
}

// Tool implementations
void HandleEmulatorControl(const rapidjson::Value* id, const rapidjson::Value& params)
{
	if (!params.IsObject() || !params.HasMember("action") || !params["action"].IsString())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'action' parameter");
		return;
	}

	const char* action = params["action"].GetString();

	auto executeAction = [action, id, &params]() {
		try
		{
			if (strcmp(action, "pause") == 0)
			{
				VMManager::SetPaused(true);
				WriteSuccessResponse(id, nullptr);
			}
			else if (strcmp(action, "resume") == 0)
			{
				VMManager::SetPaused(false);
				WriteSuccessResponse(id, nullptr);
			}
			else if (strcmp(action, "step") == 0)
			{
				VMManager::FrameAdvance(1);
				WriteSuccessResponse(id, nullptr);
			}
			else if (strcmp(action, "save_state") == 0)
			{
				if (!params.HasMember("path") || !params["path"].IsString())
				{
					WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'path' parameter for save_state");
					return;
				}
				const char* path = params["path"].GetString();
				bool success = VMManager::SaveState(path);
				if (success)
					WriteSuccessResponse(id, nullptr);
				else
					WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, "Failed to save state");
			}
			else if (strcmp(action, "load_state") == 0)
			{
				if (!params.HasMember("path") || !params["path"].IsString())
				{
					WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'path' parameter for load_state");
					return;
				}
				const char* path = params["path"].GetString();
				bool success = VMManager::LoadState(path);
				if (success)
					WriteSuccessResponse(id, nullptr);
				else
					WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, "Failed to load state");
			}
			else
			{
				WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Invalid action. Must be one of: pause, resume, step, save_state, load_state");
			}
		}
		catch (const std::exception& e)
		{
			WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, e.what());
		}
	};

	Host::RunOnCPUThread(executeAction, true);
}

void HandleMemRead(const rapidjson::Value* id, const rapidjson::Value& params)
{
	if (!params.IsObject())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "params must be an object");
		return;
	}

	if (!params.HasMember("space") || !params["space"].IsString())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'space' parameter");
		return;
	}

	if (!params.HasMember("addr") || !params["addr"].IsInt())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'addr' parameter");
		return;
	}

	if (!params.HasMember("size") || !params["size"].IsInt())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'size' parameter");
		return;
	}

	const char* space = params["space"].GetString();
	u32 addr = static_cast<u32>(params["addr"].GetInt());
	int size = params["size"].GetInt();

	// Validate size
	if (size < 1 || size > 1048576)
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "size must be between 1 and 1048576 (1MB)");
		return;
	}

	auto cpu = ParseCpuSpace(space);
	if (!cpu)
	{
		// For VU0, VU1, GS - not yet implemented
		if (strcmp(space, "VU0") == 0 || strcmp(space, "VU1") == 0 || strcmp(space, "GS") == 0)
		{
			WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "VU0, VU1, and GS memory spaces not yet implemented");
			return;
		}
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Invalid space. Must be one of: EE, IOP, VU0, VU1, GS");
		return;
	}

	auto executeRead = [id, cpu, addr, size]() {
		try
		{
			DebugInterface& debug = DebugInterface::get(*cpu);
			std::vector<u8> bytes;
			bytes.reserve(size);

			for (int i = 0; i < size; i++)
			{
				bool valid = false;
				u32 byte = debug.read8(addr + i, valid);
				if (!valid)
				{
					WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, "Invalid memory address");
					return;
				}
				bytes.push_back(static_cast<u8>(byte));
			}

			std::string hex = BytesToHex(bytes);

			WriteSuccessResponse(id, [&hex](rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc) {
				rapidjson::Value result(rapidjson::kObjectType);
				result.AddMember("hex", rapidjson::Value().SetString(hex.c_str(), alloc), alloc);
				doc.AddMember("result", std::move(result), alloc);
			});
		}
		catch (const std::exception& e)
		{
			WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, e.what());
		}
	};

	Host::RunOnCPUThread(executeRead, true);
}

void HandleMemWrite(const rapidjson::Value* id, const rapidjson::Value& params)
{
	if (!AreWritesEnabled())
	{
		WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, "Memory writes disabled. Set PCSX2_ALLOW_WRITES=true to enable.");
		return;
	}

	if (!params.IsObject())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "params must be an object");
		return;
	}

	if (!params.HasMember("space") || !params["space"].IsString())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'space' parameter");
		return;
	}

	if (!params.HasMember("addr") || !params["addr"].IsInt())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'addr' parameter");
		return;
	}

	if (!params.HasMember("hex_bytes") || !params["hex_bytes"].IsString())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'hex_bytes' parameter");
		return;
	}

	const char* space = params["space"].GetString();
	u32 addr = static_cast<u32>(params["addr"].GetInt());
	std::string hex_bytes = params["hex_bytes"].GetString();

	auto bytes = HexToBytes(hex_bytes);
	if (!bytes)
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Invalid hex_bytes format");
		return;
	}

	auto cpu = ParseCpuSpace(space);
	if (!cpu)
	{
		// For VU0, VU1, GS - not yet implemented
		if (strcmp(space, "VU0") == 0 || strcmp(space, "VU1") == 0 || strcmp(space, "GS") == 0)
		{
			WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "VU0, VU1, and GS memory spaces not yet implemented");
			return;
		}
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Invalid space. Must be one of: EE, IOP, VU0, VU1, GS");
		return;
	}

	auto executeWrite = [id, cpu, addr, bytes = std::move(*bytes)]() {
		try
		{
			DebugInterface& debug = DebugInterface::get(*cpu);

			for (size_t i = 0; i < bytes.size(); i++)
			{
				debug.write8(addr + i, bytes[i]);
			}

			WriteSuccessResponse(id, nullptr);
		}
		catch (const std::exception& e)
		{
			WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, e.what());
		}
	};

	Host::RunOnCPUThread(executeWrite, true);
}

void HandleRegsGet(const rapidjson::Value* id, const rapidjson::Value& params)
{
	if (!params.IsObject())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "params must be an object");
		return;
	}

	if (!params.HasMember("target") || !params["target"].IsString())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Missing or invalid 'target' parameter");
		return;
	}

	const char* target = params["target"].GetString();

	auto cpu = ParseCpuSpace(target);
	if (!cpu)
	{
		// For VU0, VU1 - not yet implemented
		if (strcmp(target, "VU0") == 0 || strcmp(target, "VU1") == 0)
		{
			WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "VU0 and VU1 register access not yet implemented");
			return;
		}
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "Invalid target. Must be one of: EE, IOP, VU0, VU1");
		return;
	}

	auto executeRegsGet = [id, cpu]() {
		try
		{
			DebugInterface& debug = DebugInterface::get(*cpu);
			rapidjson::Document regs(rapidjson::kObjectType);
			rapidjson::Document::AllocatorType& allocator = regs.GetAllocator();

			// Get PC
			u32 pc = debug.getPC();
			std::ostringstream pc_oss;
			pc_oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << pc;
			regs.AddMember("pc", rapidjson::Value().SetString(pc_oss.str().c_str(), allocator), allocator);

			// Get all register categories
			int catCount = debug.getRegisterCategoryCount();
			for (int cat = 0; cat < catCount; cat++)
			{
				const char* catName = debug.getRegisterCategoryName(cat);
				rapidjson::Value catObj(rapidjson::kObjectType);

				int regCount = debug.getRegisterCount(cat);
				for (int reg = 0; reg < regCount; reg++)
				{
					const char* regName = debug.getRegisterName(cat, reg);
					std::string regValue = debug.getRegisterString(cat, reg);

					catObj.AddMember(
						rapidjson::Value().SetString(regName, allocator),
						rapidjson::Value().SetString(regValue.c_str(), allocator),
						allocator);
				}

				regs.AddMember(
					rapidjson::Value().SetString(catName, allocator),
					std::move(catObj),
					allocator);
			}

			WriteSuccessResponse(id, [&regs](rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc) {
				rapidjson::Value result(regs, alloc);
				doc.AddMember("result", std::move(result), alloc);
			});
		}
		catch (const std::exception& e)
		{
			WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, e.what());
		}
	};

	Host::RunOnCPUThread(executeRegsGet, true);
}

void HandleScanMemory(const rapidjson::Value* id, const rapidjson::Value& params)
{
	WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "scan_memory depends on MemoryScanner component (not yet implemented)");
}

void HandleTraceStart(const rapidjson::Value* id, const rapidjson::Value& params)
{
	WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "trace_start depends on InstructionTracer component (not yet implemented)");
}

void HandleTraceStop(const rapidjson::Value* id, const rapidjson::Value& params)
{
	WriteErrorResponse(id, ERROR_CODE_UNSUPPORTED, "trace_stop depends on InstructionTracer component (not yet implemented)");
}

void HandleDumpMemory(const rapidjson::Value* id, const rapidjson::Value& params)
{
	if (!params.IsObject())
	{
		WriteErrorResponse(id, ERROR_CODE_INVALID_REQUEST, "params must be an object");
		return;
	}

	// Get output path
	std::string out_path = "dump.ps2mem";
	if (params.HasMember("out_path") && params["out_path"].IsString())
	{
		out_path = params["out_path"].GetString();
	}

	// Get spaces to dump (default: EE, IOP)
	std::vector<std::string> spaces = {"EE", "IOP"};
	if (params.HasMember("spaces") && params["spaces"].IsArray())
	{
		spaces.clear();
		const rapidjson::Value& spacesArray = params["spaces"];
		for (rapidjson::SizeType i = 0; i < spacesArray.Size(); i++)
		{
			if (spacesArray[i].IsString())
				spaces.push_back(spacesArray[i].GetString());
		}
	}

	auto executeDump = [id, out_path, spaces]() {
		try
		{
			std::ofstream outFile(out_path, std::ios::binary);
			if (!outFile)
			{
				WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, "Failed to open output file");
				return;
			}

			// Write JSON header
			rapidjson::Document header(rapidjson::kObjectType);
			rapidjson::Document::AllocatorType& allocator = header.GetAllocator();

			header.AddMember("version", 1, allocator);
			header.AddMember("endianness", "little", allocator);

			rapidjson::Value segments(rapidjson::kArrayType);

			// Process each space
			for (const auto& space : spaces)
			{
				auto cpu = ParseCpuSpace(space.c_str());
				if (!cpu)
				{
					if (space == "VU0" || space == "VU1" || space == "GS")
					{
						// Skip unsupported spaces
						continue;
					}
					continue;
				}

				DebugInterface& debug = DebugInterface::get(*cpu);

				// Define memory ranges (simplified)
				u32 base = 0;
				u32 size = 0;

				if (space == "EE")
				{
					base = 0x00000000;
					size = 0x02000000; // 32MB
				}
				else if (space == "IOP")
				{
					base = 0x00000000;
					size = 0x00200000; // 2MB
				}

				rapidjson::Value segment(rapidjson::kObjectType);
				segment.AddMember("space", rapidjson::Value().SetString(space.c_str(), allocator), allocator);
				segment.AddMember("base", base, allocator);
				segment.AddMember("size", size, allocator);
				segments.PushBack(std::move(segment), allocator);
			}

			header.AddMember("segments", std::move(segments), allocator);

			// Write header
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			header.Accept(writer);
			outFile << buffer.GetString() << "\n--\n";

			// Write memory data
			for (const auto& space : spaces)
			{
				auto cpu = ParseCpuSpace(space.c_str());
				if (!cpu)
					continue;

				DebugInterface& debug = DebugInterface::get(*cpu);

				u32 base = 0;
				u32 size = 0;

				if (space == "EE")
				{
					base = 0x00000000;
					size = 0x02000000; // 32MB
				}
				else if (space == "IOP")
				{
					base = 0x00000000;
					size = 0x00200000; // 2MB
				}

				// Read and write memory in chunks
				const size_t CHUNK_SIZE = 65536;
				std::vector<u8> chunk(CHUNK_SIZE);

				for (u32 offset = 0; offset < size; offset += CHUNK_SIZE)
				{
					size_t chunk_size = std::min(CHUNK_SIZE, static_cast<size_t>(size - offset));

					for (size_t i = 0; i < chunk_size; i++)
					{
						bool valid = false;
						u32 byte = debug.read8(base + offset + i, valid);
						chunk[i] = valid ? static_cast<u8>(byte) : 0;
					}

					outFile.write(reinterpret_cast<const char*>(chunk.data()), chunk_size);
				}
			}

			outFile.close();

			WriteSuccessResponse(id, [&out_path](rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc) {
				rapidjson::Value result(rapidjson::kObjectType);
				result.AddMember("path", rapidjson::Value().SetString(out_path.c_str(), alloc), alloc);
				doc.AddMember("result", std::move(result), alloc);
			});
		}
		catch (const std::exception& e)
		{
			WriteErrorResponse(id, ERROR_CODE_TOOL_ERROR, e.what());
		}
	};

	Host::RunOnCPUThread(executeDump, true);
}

} // namespace

void SetResponseSinkForTesting(ResponseSink sink)
{
    std::lock_guard<std::mutex> lock(s_response_sink_mutex);
    s_response_sink = std::move(sink);
}

void ResetResponseSinkForTesting()
{
    std::lock_guard<std::mutex> lock(s_response_sink_mutex);
    s_response_sink = ResponseSink{};
}

void ProcessCommandLine(std::string&& line)
{
    rapidjson::Document request;
    request.Parse(line.c_str(), line.size());

    if (request.HasParseError())
    {
        WriteErrorResponse(nullptr, ERROR_CODE_PARSE, ERROR_MESSAGE_PARSE);
        return;
    }

    if (!request.IsObject())
    {
        WriteErrorResponse(nullptr, ERROR_CODE_INVALID_REQUEST, ERROR_MESSAGE_INVALID_OBJECT);
        return;
    }

    auto id_member = request.FindMember("id");
    if (id_member == request.MemberEnd())
    {
        WriteErrorResponse(nullptr, ERROR_CODE_INVALID_REQUEST, ERROR_MESSAGE_INVALID_ID);
        return;
    }

    const rapidjson::Value* id_value = &id_member->value;

    auto cmd_member = request.FindMember("cmd");
    if (cmd_member == request.MemberEnd() || !cmd_member->value.IsString())
    {
        WriteErrorResponse(id_value, ERROR_CODE_INVALID_REQUEST, ERROR_MESSAGE_INVALID_CMD);
        return;
    }

    const char* cmd = cmd_member->value.GetString();

    // Get params (optional)
    rapidjson::Value emptyParams(rapidjson::kObjectType);
    const rapidjson::Value* params = &emptyParams;
    auto params_member = request.FindMember("params");
    if (params_member != request.MemberEnd())
    {
        params = &params_member->value;
    }

    // Dispatch to tool handlers
    if (strcmp(cmd, "emulator_control") == 0)
    {
        HandleEmulatorControl(id_value, *params);
    }
    else if (strcmp(cmd, "mem_read") == 0)
    {
        HandleMemRead(id_value, *params);
    }
    else if (strcmp(cmd, "mem_write") == 0)
    {
        HandleMemWrite(id_value, *params);
    }
    else if (strcmp(cmd, "regs_get") == 0)
    {
        HandleRegsGet(id_value, *params);
    }
    else if (strcmp(cmd, "scan_memory") == 0)
    {
        HandleScanMemory(id_value, *params);
    }
    else if (strcmp(cmd, "trace_start") == 0)
    {
        HandleTraceStart(id_value, *params);
    }
    else if (strcmp(cmd, "trace_stop") == 0)
    {
        HandleTraceStop(id_value, *params);
    }
    else if (strcmp(cmd, "dump_memory") == 0)
    {
        HandleDumpMemory(id_value, *params);
    }
    else
    {
        WriteErrorResponse(id_value, ERROR_CODE_UNSUPPORTED, "Unknown command");
    }
}
} // namespace detail

bool Initialize()
{
    std::unique_lock<std::mutex> lock(s_state_mutex);
    if (s_worker_active)
        return false;

    if (s_stdio_thread.joinable())
    {
        lock.unlock();
        s_stdio_thread.join();
        lock.lock();
    }

    g_running.store(true, std::memory_order_release);
    s_worker_active = true;

    try
    {
        s_stdio_thread = std::thread(&StdioWorker);
    }
    catch (...)
    {
        s_worker_active = false;
        g_running.store(false, std::memory_order_release);
        throw;
    }

    return true;
}

void Shutdown()
{
    std::thread worker;
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        if (!s_worker_active)
        {
            if (s_stdio_thread.joinable())
                worker = std::move(s_stdio_thread);
        }
        else
        {
            g_running.store(false, std::memory_order_release);
            s_state_cv.notify_all();
            worker = std::move(s_stdio_thread);
            s_state_cv.wait(lock, [] {
                return !s_worker_active;
            });
        }
    }

    if (worker.joinable())
        worker.join();
}

} // namespace DebugTools::MCPServer
