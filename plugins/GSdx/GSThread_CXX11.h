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

template<class T> class GSJobQueue : private GSThread
{
protected:
	std::atomic<size_t> m_count;
	std::atomic<bool> m_exit;
	boost::lockfree::spsc_queue<T, boost::lockfree::capacity<256> > m_queue;

	void ThreadProc() {
		while (!m_exit || m_count > 0) {
			size_t n = m_queue.consume_all(*this);
			if (n)
				m_count -= n;
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
		ASSERT(m_count >= 0);
		return m_count == 0;
	}

	void Push(const T& item) {
		m_count++;
		while(!m_queue.push(item))
			;
	}

	void Wait() {
		while (m_count > 0)
			;
	}

	virtual void Process(T& item) = 0;

	void operator()(T& item) {
		Process(item);
	}
};
