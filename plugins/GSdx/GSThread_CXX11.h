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
#include <boost/lockfree/spsc_queue.hpp>

class IGSThread
{
protected:
	virtual void ThreadProc() = 0;
};

// let us use std::thread for now, comment out the definition to go back to pthread
// There are currently some bugs/limitations to std::thread (see various comment)
// For the moment let's keep pthread but uses new std object (mutex, cond_var)
//#define _STD_THREAD_

#ifdef _WINDOWS

class GSThread : public IGSThread
{
    DWORD m_ThreadId;
    HANDLE m_hThread;

	static DWORD WINAPI StaticThreadProc(void* lpParam);

protected:
	void CreateThread();
	void CloseThread();

public:
	GSThread();
	virtual ~GSThread();
};

#else

#ifdef _STD_THREAD_
#include <thread>
#else
#include <pthread.h>
#endif

class GSThread : public IGSThread
{
    #ifdef _STD_THREAD_
    std::thread *t;
    #else
    pthread_attr_t m_thread_attr;
    pthread_t m_thread;
    #endif
    static void* StaticThreadProc(void* param);

protected:
	void CreateThread();
	void CloseThread();

public:
	GSThread();
	virtual ~GSThread();
};

#endif

// Activate only a single define (From the lowest latency to better CPU usage)

// This queue locks RENDERING threads + GS threads onto dedicated CPU
// pros: best fps by thread
// cons: requires (1 + eThreads) cores for GS emulation only ! Reserved to 8 cores CPU.
//#define NO_WAIT_BUT_CPU_INTENSIVE

// This queue locks 'only' RENDERING threads mostly the same performance as above it the CPU is fast enough
// pros: nearly best fps by thread
// cons: requires (1 + eThreads) cores for GS emulation only ! Reserved to 6/8 cores CPU.
//#define WAIT_ON_GS_STILL_CPU_INTENSIVE

// This queue doesn't lock any thread. It would be nicer for 2c/4c CPU.
// pros: no hard limit on thread numbers
// cons: less performance by thread
#define FULL_WAIT_LESS_CPU_INTENSIVE

#if defined(FULL_WAIT_LESS_CPU_INTENSIVE)

template<class T> class GSJobQueue : private GSThread
{
protected:
	std::atomic<int16_t> m_count;
	std::atomic<bool> m_exit;
	boost::lockfree::spsc_queue<T, boost::lockfree::capacity<256> > m_queue;

	std::mutex m_lock;
	std::condition_variable m_empty;
	std::condition_variable m_notempty;

	void ThreadProc() {
		std::unique_lock<std::mutex> l(m_lock);

		while (true) {

			while (m_count == 0) {
				if (m_exit.load(memory_order_acquire)) return;
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
	GSJobQueue() :
		m_count(0),
		m_exit(false)
	{
		CreateThread();
	};

	virtual ~GSJobQueue() {
		m_exit = true;
		m_notempty.notify_one();
		CloseThread();
	}

	bool IsEmpty() const {
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
	}

	virtual void Process(T& item) = 0;

	void operator()(T& item) {
		Process(item);
	}
};

#elif defined(WAIT_ON_GS_STILL_CPU_INTENSIVE)

template<class T> class GSJobQueue : private GSThread
{
protected:
	std::atomic<int16_t> m_count;
	std::atomic<bool> m_exit;
	boost::lockfree::spsc_queue<T, boost::lockfree::capacity<256> > m_queue;

	std::mutex m_lock;
	std::condition_variable m_empty;

	void ThreadProc() {
		std::unique_lock<std::mutex> l(m_lock, defer_lock);

		while (true) {

			while (m_count == 0) {
				if (m_exit.load(memory_order_acquire)) return;
				std::this_thread::yield();
			}

			int16_t consumed = 0;
			for (int16_t nb = m_count; nb >= 0; nb--) {
				if (m_queue.consume_one(*this))
					consumed++;
			}

			l.lock();

			m_count -= consumed;

			l.unlock();

			if (m_count <= 0)
				m_empty.notify_one();

		}
	}

public:
	GSJobQueue() :
		m_count(0),
		m_exit(false)
	{
		CreateThread();
	};

	virtual ~GSJobQueue() {
		m_exit = true;
		CloseThread();
	}

	bool IsEmpty() const {
		return m_count == 0;
	}

	void Push(const T& item) {
		while(!m_queue.push(item))
			std::this_thread::yield();

		m_count++;
	}

	void Wait() {
		if (m_count > 0) {
			std::unique_lock<std::mutex> l(m_lock);
			while (m_count > 0) {
				m_empty.wait(l);
			}
		}
	}

	virtual void Process(T& item) = 0;

	void operator()(T& item) {
		Process(item);
	}
};

#elif defined(NO_WAIT_BUT_CPU_INTENSIVE)

template<class T> class GSJobQueue : private GSThread
{
protected:
	std::atomic<int16_t> m_count;
	std::atomic<bool> m_exit;
	boost::lockfree::spsc_queue<T, boost::lockfree::capacity<256> > m_queue;

	void ThreadProc() {
		while (true) {
			while (m_count == 0) {
				if (m_exit.load(memory_order_acquire)) return;
				std::this_thread::yield();
			}

			m_count -= m_queue.consume_all(*this);
		}
	}

public:
	GSJobQueue() :
		m_count(0),
		m_exit(false)
	{
		CreateThread();
	};

	virtual ~GSJobQueue() {
		m_exit = true;
		CloseThread();
	}

	bool IsEmpty() const {
		return m_count == 0;
	}

	void Push(const T& item) {
		m_count++;
		while(!m_queue.push(item))
			std::this_thread::yield();
	}

	void Wait() {
		while (m_count > 0)
			std::this_thread::yield();
	}

	virtual void Process(T& item) = 0;

	void operator()(T& item) {
		Process(item);
	}
};

#else
	#very bad
#endif
