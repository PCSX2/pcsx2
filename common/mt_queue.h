/*  OnePAD - PCSX2 dev
 *  Copyright (C) 2017-2017
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include <mutex>
#include <queue>

template <typename T>
class MtQueue
{
    std::queue<T> m_queue;
    std::mutex m_mtx;

public:
    MtQueue()
    {
    }

    ~MtQueue()
    {
    }

    void push(const T &e)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        m_queue.push(e);
    }

    size_t size()
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        return m_queue.size();
    }

    T dequeue()
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        T item = m_queue.front();
        m_queue.pop();
        return item;
    }

    template <typename F>
    void consume_all(F f)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        while (!m_queue.empty()) {
            f(m_queue.front());
            m_queue.pop();
        }
    }

    void reset()
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        while (!m_queue.empty())
            m_queue.pop();
    }
};
