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

#include "stdafx.h"
#include "GSThread_CXX11.h"

GSThread::GSThread()
{
    #ifdef _WINDOWS

	m_ThreadId = 0;
	m_hThread = NULL;

    #else

    #endif
}

GSThread::~GSThread()
{
	CloseThread();
}

#ifdef _WINDOWS

DWORD WINAPI GSThread::StaticThreadProc(void* lpParam)
{
	((GSThread*)lpParam)->ThreadProc();

	return 0;
}

#else

void* GSThread::StaticThreadProc(void* param)
{
	((GSThread*)param)->ThreadProc();
#ifndef _STD_THREAD_ // exit is done implicitly by std::thread
	pthread_exit(NULL);
#endif
	return NULL;
}

#endif

void GSThread::CreateThread()
{
    #ifdef _WINDOWS

	m_hThread = ::CreateThread(NULL, 0, StaticThreadProc, (void*)this, 0, &m_ThreadId);

	#else
    
    #ifdef _STD_THREAD_
    t = new thread(StaticThreadProc,(void*)this);
    #else
    pthread_attr_init(&m_thread_attr);
    pthread_create(&m_thread, &m_thread_attr, StaticThreadProc, (void*)this);
    #endif

	#endif
}

void GSThread::CloseThread()
{
    #ifdef _WINDOWS

	if(m_hThread != NULL)
	{
		if(WaitForSingleObject(m_hThread, 5000) != WAIT_OBJECT_0)
		{
			printf("GSdx: WARNING: GSThread Thread did not close itself in time. Assuming hung. Terminating.\n");
			TerminateThread(m_hThread, 1);
		}

		CloseHandle(m_hThread);

		m_hThread = NULL;
		m_ThreadId = 0;
	}

    #else
    // Should be tested on windows too one day, native handle should be disabled there though, or adapted to windows thread
    #ifdef _STD_THREAD_
    
    #define _NATIVE_HANDLE_ // Using std::thread native handle, allows to just use posix stuff.
    #ifdef _NATIVE_HANDLE_ // std::thread join seems to be bugged, have to test it again every now and then, it did work at some point(gcc 5), seems there is bug in system lib...
    pthread_t m_thread = t->native_handle();
    void *ret = NULL;
    pthread_join(m_thread, &ret);
    /* We are sure thread is dead, not so bad.
     * Still no way to to delete that crap though... Really, wtf is this standard??
     * I guess we will have to wait that someone debug either the implementation or change standard.
     * There should be a moderate memory leak for now, I am trying to find a way to fix it.
     * 3kinox
     */
    #else
    if(t->joinable())
    {
        t->join();
    }
    delete(t);
    #endif
    #else
    void* ret = NULL;

    pthread_join(m_thread, &ret);
    pthread_attr_destroy(&m_thread_attr);
    #endif
    #endif
}

