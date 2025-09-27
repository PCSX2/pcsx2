// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MCPServer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

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

constexpr const char* ERROR_MESSAGE_PARSE = "Failed to parse request";
constexpr const char* ERROR_MESSAGE_INVALID_ID = "Missing id";
constexpr const char* ERROR_MESSAGE_INVALID_OBJECT = "Request must be an object";
constexpr const char* ERROR_MESSAGE_INVALID_CMD = "Missing or invalid cmd";
constexpr const char* ERROR_MESSAGE_UNSUPPORTED = "Command handling is not implemented";
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

    WriteErrorResponse(id_value, ERROR_CODE_UNSUPPORTED, ERROR_MESSAGE_UNSUPPORTED);
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
