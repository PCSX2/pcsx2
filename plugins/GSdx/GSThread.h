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

class IGSThread
{
protected:
	virtual void ThreadProc() = 0;
};

class IGSLock
{
public:
    virtual void Lock() = 0;
    virtual bool TryLock() = 0;
    virtual void Unlock() = 0;
	virtual ~IGSLock() {}
};

class IGSEvent
{
public:
    virtual void Set() = 0;
	virtual bool Wait(IGSLock* l) = 0;
	virtual ~IGSEvent() {}
};

#ifdef _WINDOWS

typedef void (WINAPI * InitializeConditionVariablePtr)(CONDITION_VARIABLE* ConditionVariable);
typedef void (WINAPI * WakeConditionVariablePtr)(CONDITION_VARIABLE* ConditionVariable);
typedef void (WINAPI * WakeAllConditionVariablePtr)(CONDITION_VARIABLE* ConditionVariable);
typedef BOOL (WINAPI * SleepConditionVariableSRWPtr)(CONDITION_VARIABLE* ConditionVariable, SRWLOCK* SRWLock, DWORD dwMilliseconds, ULONG Flags);
typedef void (WINAPI * InitializeSRWLockPtr)(SRWLOCK* SRWLock);
typedef void (WINAPI * AcquireSRWLockExclusivePtr)(SRWLOCK* SRWLock);
typedef BOOLEAN (WINAPI * TryAcquireSRWLockExclusivePtr)(SRWLOCK* SRWLock);
typedef void (WINAPI * ReleaseSRWLockExclusivePtr)(SRWLOCK* SRWLock);
typedef void (WINAPI * AcquireSRWLockSharedPtr)(SRWLOCK* SRWLock);
typedef BOOLEAN (WINAPI * TryAcquireSRWLockSharedPtr)(SRWLOCK* SRWLock);
typedef void (WINAPI * ReleaseSRWLockSharedPtr)(SRWLOCK* SRWLock);

extern InitializeConditionVariablePtr pInitializeConditionVariable;
extern WakeConditionVariablePtr pWakeConditionVariable;
extern WakeAllConditionVariablePtr pWakeAllConditionVariable;
extern SleepConditionVariableSRWPtr pSleepConditionVariableSRW;
extern InitializeSRWLockPtr pInitializeSRWLock;
extern AcquireSRWLockExclusivePtr pAcquireSRWLockExclusive;
extern TryAcquireSRWLockExclusivePtr pTryAcquireSRWLockExclusive;
extern ReleaseSRWLockExclusivePtr pReleaseSRWLockExclusive;
extern AcquireSRWLockSharedPtr pAcquireSRWLockShared;
extern TryAcquireSRWLockSharedPtr pTryAcquireSRWLockShared;
extern ReleaseSRWLockSharedPtr pReleaseSRWLockShared;

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

class GSCritSec : public IGSLock
{
    CRITICAL_SECTION m_cs;

public:
    GSCritSec() {InitializeCriticalSection(&m_cs);}
    ~GSCritSec() {DeleteCriticalSection(&m_cs);}

	void Lock() {EnterCriticalSection(&m_cs);}
	bool TryLock() {return TryEnterCriticalSection(&m_cs) == TRUE;}
	void Unlock() {LeaveCriticalSection(&m_cs);}
};

class GSEvent : public IGSEvent
{
protected:
    HANDLE m_hEvent;

public:
	GSEvent() {m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);}
	~GSEvent() {CloseHandle(m_hEvent);}

    void Set() {SetEvent(m_hEvent);}
	bool Wait(IGSLock* l) {if(l) l->Unlock(); bool b = WaitForSingleObject(m_hEvent, INFINITE) == WAIT_OBJECT_0; if(l) l->Lock(); return b;}
};

class GSCondVarLock : public IGSLock
{
	SRWLOCK m_lock;

public:
	GSCondVarLock() {pInitializeSRWLock(&m_lock);}

	void Lock() {pAcquireSRWLockExclusive(&m_lock);}
	bool TryLock() {return pTryAcquireSRWLockExclusive(&m_lock) == TRUE;}
	void Unlock() {pReleaseSRWLockExclusive(&m_lock);}

	operator SRWLOCK* () {return &m_lock;}
};

class GSCondVar : public IGSEvent
{
	CONDITION_VARIABLE m_cv;

public:
	GSCondVar() {pInitializeConditionVariable(&m_cv);}

	void Set() {pWakeConditionVariable(&m_cv);}
	bool Wait(IGSLock* l) {return pSleepConditionVariableSRW(&m_cv, *(GSCondVarLock*)l, INFINITE, 0) != 0;}

	operator CONDITION_VARIABLE* () {return &m_cv;}
};

#else
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

class GSCritSec : public IGSLock
{
	// XXX Do we really need a recursive mutex
	// It would allow to use condition_variable instead of condition_variable_any
    recursive_mutex *mutex_critsec;

public:
    GSCritSec(bool recursive = true)
    {
        mutex_critsec = new recursive_mutex();    
    }

    ~GSCritSec()
    {
        delete(mutex_critsec);
    }
    
    void Lock()
    {
        mutex_critsec->lock();
    }
    
    bool TryLock()
    {
        return mutex_critsec->try_lock();
    }

    void Unlock()
    {
        mutex_critsec->unlock();
    }
    
    recursive_mutex& GetMutex() {return ref(*mutex_critsec);}
};

class GSCondVarLock : public GSCritSec
{
public:
	GSCondVarLock() : GSCritSec(false)
	{
	}
};

class GSCondVar : public IGSEvent
{
    condition_variable_any *cond_var;

public:
	GSCondVar() 
	{
        cond_var = new condition_variable_any();
	}

	virtual ~GSCondVar() 
	{
        delete(cond_var);
    }
    
	void Set() 
    {
        cond_var->notify_one();
    }
    
    bool Wait(IGSLock* l) 
    {
        cond_var->wait(((GSCondVarLock*)l)->GetMutex()); // Predicate is not useful, it is implicit in the loop
        return 1; // Anyway this value is not used(and no way to get it from std::thread)
    }

    operator condition_variable_any* () {return cond_var;}
};

#endif

class GSAutoLock
{
    IGSLock* m_lock;

public:
    GSAutoLock(IGSLock* l) {(m_lock = l)->Lock();}
    ~GSAutoLock() {m_lock->Unlock();}
};

template<class T> class GSJobQueue : private GSThread
{
protected:
	queue<T> m_queue;
	volatile long m_count; // NOTE: it is the safest to have our own counter because m_queue.pop() might decrement its own before the last item runs out of its scope and gets destroyed (implementation dependent)
	volatile bool m_exit;
	IGSEvent* m_notempty;
	IGSEvent* m_empty;
	IGSLock* m_lock;

	void ThreadProc()
	{
		m_lock->Lock();

		while(true)
		{
			while(m_queue.empty())
			{
				m_notempty->Wait(m_lock);

				if(m_exit) {m_lock->Unlock(); return;}
			}

			T& item = m_queue.front();

			m_lock->Unlock();

			Process(item);

			m_lock->Lock();

			m_queue.pop();

			if(--m_count == 0)
			{
				m_empty->Set();
			}
		}
	}

public:
	GSJobQueue()
		: m_count(0)
		, m_exit(false)
	{
		bool condvar = true;

		#ifdef _WINDOWS

		if(pInitializeConditionVariable == NULL) 
		{
			condvar = false;
		}

		#endif

		if(condvar)
		{
			m_notempty = new GSCondVar();
			m_empty = new GSCondVar();
			m_lock = new GSCondVarLock();
		}
		else
		{
			#ifdef _WINDOWS
			m_notempty = new GSEvent();
			m_empty = new GSEvent();
			m_lock = new GSCritSec();
			#endif
		}

		CreateThread();
	}
	
	virtual ~GSJobQueue()
	{
		m_exit = true;

		m_notempty->Set();

		CloseThread();

		delete m_notempty;
		delete m_empty;
		delete m_lock;
	}

	bool IsEmpty() const
	{
		ASSERT(m_count >= 0);

		return m_count == 0;
	}

	void Push(const T& item)
	{
		m_lock->Lock();
	
		m_queue.push(item);

		if(m_count++ == 0)
		{
			m_notempty->Set();
		}

		m_lock->Unlock();
	}

	void Wait()
	{
		if(m_count > 0)
		{
			m_lock->Lock();

			while(m_count != 0) 
			{
				m_empty->Wait(m_lock);
			}

			ASSERT(m_queue.empty());
	
			m_lock->Unlock();
		}
	}

	virtual void Process(T& item) = 0;
};

// http://software.intel.com/en-us/blogs/2012/11/06/exploring-intel-transactional-synchronization-extensions-with-intel-software
#if 0
class TransactionScope
{
public:
	class Lock
	{
		volatile long state;

	public:
		Lock() 
			: state(0) 
		{
		}

		void lock()
		{
			while(_InterlockedCompareExchange(&state, 1, 0) != 0)
			{
				do {_mm_pause();} while(state == 1);
			}
		}

		void unlock() 
		{
			_InterlockedExchange(&state, 0);
		}

		bool isLocked() const 
		{
			return state == 1;
		}
	};

private:
	Lock& fallBackLock;

	TransactionScope();

public:
	TransactionScope(Lock& fallBackLock_, int max_retries = 3) 
		: fallBackLock(fallBackLock_)
	{
		// The TSX (RTM/HLE) instructions on Intel AVX2 CPUs may either be
		// absent or disabled (see errata HSD136 and specification change at
		// http://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/4th-gen-core-family-desktop-specification-update.pdf)
		// This can cause builds for AVX2 CPUs to fail with GCC/Clang on Linux,
		// so check that the RTM instructions are actually available.
		#if (_M_SSE >= 0x501 && !defined(__GNUC__)) || defined(__RTM__)

		int nretries = 0;
		
		while(1)
		{
			++nretries;

			unsigned status = _xbegin();

			if(status == _XBEGIN_STARTED)
			{
				if(!fallBackLock.isLocked()) return;

				_xabort(0xff); 
			}

			if((status & _XABORT_EXPLICIT) && _XABORT_CODE(status) == 0xff && !(status & _XABORT_NESTED))
			{
				while(fallBackLock.isLocked()) _mm_pause();
			}
			else if(!(status & _XABORT_RETRY))
			{
				break;
			}

			if(nretries >= max_retries) 
			{
				break;
			}
		}

		#endif

		fallBackLock.lock();
	}

	~TransactionScope()
	{
		if(fallBackLock.isLocked())
		{
			fallBackLock.unlock();
		}
		#if (_M_SSE >= 0x501 && !defined(__GNUC__)) || defined(__RTM__)
		else
		{
			_xend();
		}
		#endif
	}
};
#endif
