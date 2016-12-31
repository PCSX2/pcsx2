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

#define QUEUE_SEM

#ifdef QUEUE_SEM

#include <semaphore.h>
#include <errno.h> // EBUSY
#include <pthread.h>

template<class T, int CAPACITY> class GSJobQueue final
{
private:
	std::thread m_thread;
	std::function<void(T&)> m_func;
	std::atomic<bool> m_exit;
	std::atomic<int32_t> m_count;
	ringbuffer_base<T, CAPACITY> m_queue;

	std::mutex m_busy_lock;
	sem_t m_sem_work;

	void ThreadProc() {
		while (true) {
			sem_wait(&m_sem_work);

			std::unique_lock<std::mutex> l(m_busy_lock);

			if (m_exit.load(memory_order_relaxed)) {
				return;
			}

			do {
				m_queue.consume_one(*this);

				m_count.fetch_sub(1);

				// If there is a pending job, ack the semaphore (won't block in most case)
				// It avoids to lock/unlock & check m_exit for each job.
			} while (sem_trywait(&m_sem_work) == 0);
		}
	}

public:
	GSJobQueue(std::function<void(T&)> func) :
		m_func(func),
		m_exit(false),
		m_count(0),
		m_queue()
	{
		// It must be done before thread creation. Otherwise the
		// semaphore WAIT (inside the thread) won't work
		sem_init(&m_sem_work, false, 0);
		m_thread = std::thread(&GSJobQueue::ThreadProc, this);
	}

	~GSJobQueue()
	{
		m_exit = true;
		m_count = -1; // Stop the thread inner-loop
		sem_post(&m_sem_work); // Awake the thread to check m_exit condition

		m_thread.join();

		sem_destroy(&m_sem_work);

	}

	bool IsEmpty() {
		// Warning: You can't check the value of the semaphore (it could be busy processing last element)
		return m_count <= 0;
	}

	void Push(const T& item) {
		while(!m_queue.push(item))
			std::this_thread::yield();

		m_count++;

		sem_post(&m_sem_work);
	}

	void Wait() {
		while(!IsEmpty()) {
			// Avoid any issue if ThreadProc didn't take the lock yet
			std::this_thread::yield();
#if 1
			std::unique_lock<std::mutex> l(m_busy_lock);
#else
			// Spin wait if a single job remain on the queue.
			if (m_count > 1)
				std::unique_lock<std::mutex> l(m_busy_lock);
#endif
		}

		ASSERT(IsEmpty());
	}

	void operator() (T& item) {
		m_func(item);
	}
};

#else

template<class T, int CAPACITY> class GSJobQueue final
{
private:
	std::thread m_thread;
	std::function<void(T&)> m_func;
	std::atomic<int16_t> m_count;
	std::atomic<bool> m_exit;
	ringbuffer_base<T, CAPACITY> m_queue;

	std::mutex m_lock;
	std::condition_variable m_empty;
	std::condition_variable m_notempty;

	void ThreadProc() {
		std::unique_lock<std::mutex> l(m_lock);

		while (true) {

			while (m_count == 0) {
				if (m_exit.load(memory_order_relaxed)) {
					m_exit = false;
					return;
				}
				m_notempty.wait(l);
			}

			l.unlock();

			int16_t consumed = 0;
			for (int16_t nb = m_count; nb >= 0; nb--) {
				if (m_queue.consume_one(*this))
					consumed++;
			}

			l.lock();

			m_count -= consumed;

			if (m_count <= 0)
				m_empty.notify_one();

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
		m_exit = true;
		do {
			m_notempty.notify_one();
		} while (m_exit);

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
			std::unique_lock<std::mutex> l(m_lock);
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

#endif
