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

class IGSThread
{
protected:
	virtual void ThreadProc() = 0;
};

// let us use std::thread for now, comment out the definition to go back to pthread
// There are currently some bugs/limitations to std::thread (see various comment)
// For the moment let's keep pthread but uses new std object (mutex, cond_var)
//#define _STD_THREAD_

#ifdef _WIN32

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

template<class T> class IGSJobQueue : public GSThread
{
public:
	IGSJobQueue() {}
	virtual ~IGSJobQueue() {}

	virtual bool IsEmpty() const = 0;
	virtual void Push(const T& item) = 0;
	virtual void Wait() = 0;

	virtual void Process(T& item) = 0;
	virtual int GetPixels(bool reset) = 0;
};

template<class T, int CAPACITY> class GSJobQueue : public IGSJobQueue<T>
{
protected:
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
		this->CreateThread();
	}

	virtual ~GSJobQueue() {
		m_exit.store(true, memory_order_release);
		m_notempty.notify_one();
		this->CloseThread();
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
		this->Process(item);
	}
};
