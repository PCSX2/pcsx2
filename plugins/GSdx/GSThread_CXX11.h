/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSdx.h"
#include "boost_spsc_queue.hpp"

template<class T, int CAPACITY> class GSJobQueue final
{
private:
	std::thread m_thread;
	std::function<void(T&)> m_func;
	std::atomic<int32_t> m_count;
	std::atomic<bool> m_exit;
	ringbuffer_base<T, CAPACITY> m_queue;

	std::mutex m_lock;
	std::mutex m_wait_lock;
	std::condition_variable m_empty;
	std::condition_variable m_notempty;

	void ThreadProc() {
		std::unique_lock<std::mutex> l(m_lock);

		while (true) {

			while (m_count == 0) {
				if (m_exit.load(memory_order_relaxed))
					return;

				m_notempty.wait(l);
			}

			int32_t nb = m_count;

			l.unlock();

			int32_t consumed = 0;
			for (; nb >= 0; nb--) {
				if (m_queue.consume_one(*this))
					consumed++;
			}

			l.lock();

			int32_t old_count = m_count.fetch_sub(consumed);

			if (old_count == consumed) {

				{
					std::lock_guard<std::mutex> wait_guard(m_wait_lock);
				}
				m_empty.notify_one();
			}

		}
	}

public:
	GSJobQueue(std::function<void(T&)> func) :
		m_func(func),
		m_count(0),
		m_exit(false)
	{
		m_thread = std::thread(&GSJobQueue::ThreadProc, this);
	}

	~GSJobQueue()
	{
		{
			std::lock_guard<std::mutex> l(m_lock);
			m_exit = true;
		}
		m_notempty.notify_one();

		m_thread.join();
	}

	bool IsEmpty() const {
		ASSERT(m_count >= 0);

		return m_count == 0;
	}

	void Push(const T& item) {
		while(!m_queue.push(item))
			std::this_thread::yield();

		std::unique_lock<std::mutex> l(m_lock);

		m_count++;

		l.unlock();

		m_notempty.notify_one();
	}

	void Wait() {
		if (m_count > 0) {
			std::unique_lock<std::mutex> l(m_wait_lock);
			while (m_count > 0) {
				m_empty.wait(l);
			}
		}

		ASSERT(m_count == 0);
	}

	void operator() (T& item) {
		m_func(item);
	}
};
