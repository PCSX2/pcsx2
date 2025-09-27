// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MCPServer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace DebugTools::MCPServer
{
namespace
{
std::atomic_bool g_running{false};
std::mutex s_state_mutex;
std::condition_variable s_state_cv;
std::thread s_stdio_thread;
bool s_worker_active = false;

[[maybe_unused]] void ProcessCommandLine(std::string&& line)
{
    // Placeholder for command handling integration.
    (void)line;
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
                ProcessCommandLine(std::move(line));
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
