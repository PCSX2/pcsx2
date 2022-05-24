/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/Threading.h"
#include "common/EventSource.h"
#include "common/Exceptions.h"
#include "common/Console.h"
#include "common/TraceLog.h"
#include "StringHelpers.h"
#include <wx/datetime.h>
#include <pthread.h>

#ifdef __APPLE__
#include <mach/semaphore.h>
#else
#include <semaphore.h>
#endif

// --------------------------------------------------------------------------------------
//  DiagnosticOrigin
// --------------------------------------------------------------------------------------
struct DiagnosticOrigin
{
	const char* srcfile;
	const char* function;
	const char* condition;
	int line;

	DiagnosticOrigin(const char* _file, int _line, const char* _func, const char* _cond = nullptr)
		: srcfile(_file)
		, function(_func)
		, condition(_cond)
		, line(_line)
	{
	}

	std::string ToString(const char* msg = nullptr) const;
};

#define pxDiagSpot DiagnosticOrigin(__FILE__, __LINE__, __pxFUNCTION__)
#define pxAssertSpot(cond) DiagnosticOrigin(__FILE__, __LINE__, __pxFUNCTION__, #cond)

namespace Threading
{
class pxThread;
}

#undef Yield // release the burden of windows.h global namespace spam.

#define AffinityAssert_AllowFrom_MainUI() \
	pxAssertMsg(wxThread::IsMain(), "Thread affinity violation: Call allowed from main thread only.")

// --------------------------------------------------------------------------------------
//  pxThreadLog / ConsoleLogSource_Threading
// --------------------------------------------------------------------------------------

class ConsoleLogSource_Threading : ConsoleLogSource
{
	typedef ConsoleLogSource _parent;

public:
	using _parent::IsActive;

	ConsoleLogSource_Threading();

	bool Write(const wxString& thrname, const wxChar* msg)
	{
		return _parent::Write("%s", (wxsFormat(L"(thread:%s) ", WX_STR(thrname)) + msg).ToUTF8().data());
	}
	bool Warn(const wxString& thrname, const wxChar* msg)
	{
		return _parent::Write("%s", (wxsFormat(L"(thread:%s) ", WX_STR(thrname)) + msg).ToUTF8().data());
	}
	bool Error(const wxString& thrname, const wxChar* msg)
	{
		return _parent::Write("%s", (wxsFormat(L"(thread:%s) ", WX_STR(thrname)) + msg).ToUTF8().data());
	}
};

extern ConsoleLogSource_Threading pxConLog_Thread;

#define pxThreadLog pxConLog_Thread.IsActive() && pxConLog_Thread


namespace Exception
{
	class BaseThreadError : public RuntimeError
	{
		DEFINE_EXCEPTION_COPYTORS(BaseThreadError, RuntimeError)
		DEFINE_EXCEPTION_MESSAGES(BaseThreadError)

	public:
		Threading::pxThread* m_thread;

	protected:
		BaseThreadError()
		{
			m_thread = NULL;
		}

	public:
		explicit BaseThreadError(Threading::pxThread* _thread)
		{
			m_thread = _thread;
			m_message_diag = "An unspecified thread-related error occurred (thread=%s)";
		}

		explicit BaseThreadError(Threading::pxThread& _thread)
		{
			m_thread = &_thread;
			m_message_diag = "An unspecified thread-related error occurred (thread=%s)";
		}

		virtual std::string FormatDiagnosticMessage() const;
		virtual std::string FormatDisplayMessage() const;

		Threading::pxThread& Thread();
		const Threading::pxThread& Thread() const;
	};

	class ThreadCreationError : public BaseThreadError
	{
		DEFINE_EXCEPTION_COPYTORS(ThreadCreationError, BaseThreadError)

	public:
		explicit ThreadCreationError(Threading::pxThread* _thread)
		{
			m_thread = _thread;
			SetBothMsgs("Thread creation failure.  An unspecified error occurred while trying to create the %s thread.");
		}

		explicit ThreadCreationError(Threading::pxThread& _thread)
		{
			m_thread = &_thread;
			SetBothMsgs("Thread creation failure.  An unspecified error occurred while trying to create the %s thread.");
		}
	};
} // namespace Exception

namespace Threading
{
	extern void pxTestCancel();
	extern void YieldToMain();

	extern const wxTimeSpan def_yieldgui_interval;

	extern pxThread* pxGetCurrentThread();
	extern wxString pxGetCurrentThreadName();
	extern bool _WaitGui_RecursionGuard(const wxChar* name);

	extern bool AllowDeletions();

	class Mutex
	{
	protected:
		pthread_mutex_t m_mutex;

	public:
		Mutex();
		virtual ~Mutex();
		virtual bool IsRecursive() const { return false; }

		void Recreate();
		bool RecreateIfLocked();
		void Detach();

		void Acquire();
		bool Acquire(const wxTimeSpan& timeout);
		bool TryAcquire();
		void Release();

		void AcquireWithoutYield();
		bool AcquireWithoutYield(const wxTimeSpan& timeout);

		void Wait();
		void WaitWithSpin();
		bool Wait(const wxTimeSpan& timeout);
		void WaitWithoutYield();
		bool WaitWithoutYield(const wxTimeSpan& timeout);

	protected:
		// empty constructor used by MutexLockRecursive
		Mutex(bool) {}
	};

	class MutexRecursive : public Mutex
	{
	public:
		MutexRecursive();
		virtual ~MutexRecursive();
		virtual bool IsRecursive() const { return true; }
	};

	// --------------------------------------------------------------------------------------
	//  ScopedLock
	// --------------------------------------------------------------------------------------
	// Helper class for using Mutexes.  Using this class provides an exception-safe (and
	// generally clean) method of locking code inside a function or conditional block.  The lock
	// will be automatically released on any return or exit from the function.
	//
	// Const qualification note:
	//  ScopedLock takes const instances of the mutex, even though the mutex is modified
	//  by locking and unlocking.  Two rationales:
	//
	//  1) when designing classes with accessors (GetString, GetValue, etc) that need mutexes,
	//     this class needs a const hack to allow those accessors to be const (which is typically
	//     *very* important).
	//
	//  2) The state of the Mutex is guaranteed to be unchanged when the calling function or
	//     scope exits, by any means.  Only via manual calls to Release or Acquire does that
	//     change, and typically those are only used in very special circumstances of their own.
	//
	class ScopedLock
	{
		DeclareNoncopyableObject(ScopedLock);

	protected:
		Mutex* m_lock;
		bool m_IsLocked;

	public:
		virtual ~ScopedLock();
		explicit ScopedLock(const Mutex* locker = NULL);
		explicit ScopedLock(const Mutex& locker);
		void AssignAndLock(const Mutex& locker);
		void AssignAndLock(const Mutex* locker);

		void Assign(const Mutex& locker);
		void Assign(const Mutex* locker);

		void Release();
		void Acquire();

		bool IsLocked() const { return m_IsLocked; }

	protected:
		// Special constructor used by ScopedTryLock
		ScopedLock(const Mutex& locker, bool isTryLock);
	};

	// --------------------------------------------------------------------------------------
	//  ScopedPtrMT
	// --------------------------------------------------------------------------------------

	template <typename T>
	class ScopedPtrMT
	{
		DeclareNoncopyableObject(ScopedPtrMT);

	protected:
		std::atomic<T*> m_ptr;
		Threading::Mutex m_mtx;

	public:
		typedef T element_type;

		wxEXPLICIT ScopedPtrMT(T* ptr = nullptr)
		{
			m_ptr = ptr;
		}

		~ScopedPtrMT() { _Delete_unlocked(); }

		ScopedPtrMT& Reassign(T* ptr = nullptr)
		{
			T* doh = m_ptr.exchange(ptr);
			if (ptr != doh)
				delete doh;
			return *this;
		}

		ScopedPtrMT& Delete() noexcept
		{
			ScopedLock lock(m_mtx);
			_Delete_unlocked();
		}

		// Removes the pointer from scoped management, but does not delete!
		// (ScopedPtr will be nullptr after this method)
		T* DetachPtr()
		{
			ScopedLock lock(m_mtx);

			return m_ptr.exchange(nullptr);
		}

		// Returns the managed pointer.  Can return nullptr as a valid result if the ScopedPtrMT
		// has no object in management.
		T* GetPtr() const
		{
			return m_ptr;
		}

		void SwapPtr(ScopedPtrMT& other)
		{
			ScopedLock lock(m_mtx);
			m_ptr.exchange(other.m_ptr.exchange(m_ptr.load()));
			T* const tmp = other.m_ptr;
			other.m_ptr = m_ptr;
			m_ptr = tmp;
		}

		// ----------------------------------------------------------------------------
		//  ScopedPtrMT Operators
		// ----------------------------------------------------------------------------
		// I've decided to use the ATL's approach to pointer validity tests, opposed to
		// the wx/boost approach (which uses some bizarre member method pointer crap, and can't
		// allow the T* implicit casting.

		bool operator!() const noexcept
		{
			return m_ptr.load() == nullptr;
		}

		// Equality
		bool operator==(T* pT) const noexcept
		{
			return m_ptr == pT;
		}

		// Inequality
		bool operator!=(T* pT) const noexcept
		{
			return !operator==(pT);
		}

		// Convenient assignment operator.  ScopedPtrMT = nullptr will issue an automatic deletion
		// of the managed pointer.
		ScopedPtrMT& operator=(T* src)
		{
			return Reassign(src);
		}

	#if 0
		operator T*() const
		{
			return m_ptr;
		}

		// Dereference operator, returns a handle to the managed pointer.
		// Generates a debug assertion if the object is nullptr!
		T& operator*() const
		{
			pxAssert(m_ptr != nullptr);
			return *m_ptr;
		}

		T* operator->() const
		{
			pxAssert(m_ptr != nullptr);
			return m_ptr;
		}
	#endif

	protected:
		void _Delete_unlocked() noexcept
		{
			delete m_ptr.exchange(nullptr);
		}
	};


	// ----------------------------------------------------------------------------------------
	//  RecursionGuard  -  Basic protection against function recursion
	// ----------------------------------------------------------------------------------------
	// Thread safety note: If used in a threaded environment, you shoud use a handle to a __threadlocal
	// storage variable (protects aaginst race conditions and, in *most* cases, is more desirable
	// behavior as well.
	//
	// Rationale: wxWidgets has its own wxRecursionGuard, but it has a sloppy implementation with
	// entirely unnecessary assertion checks.
	//
	class RecursionGuard
	{
	public:
		int& Counter;

		RecursionGuard(int& counter)
			: Counter(counter)
		{
			++Counter;
		}

		virtual ~RecursionGuard()
		{
			--Counter;
		}

		bool IsReentrant() const { return Counter > 1; }
	};

	// --------------------------------------------------------------------------------------
	//  ThreadDeleteEvent
	// --------------------------------------------------------------------------------------
	class EventListener_Thread : public IEventDispatcher<int>
	{
	public:
		typedef int EvtParams;

	protected:
		pxThread* m_thread;

	public:
		EventListener_Thread()
		{
			m_thread = NULL;
		}

		virtual ~EventListener_Thread() = default;

		void SetThread(pxThread& thr) { m_thread = &thr; }
		void SetThread(pxThread* thr) { m_thread = thr; }

		void DispatchEvent(const int& params)
		{
			OnThreadCleanup();
		}

	protected:
		// Invoked by the pxThread when the thread execution is ending.  This is
		// typically more useful than a delete listener since the extended thread information
		// provided by virtualized functions/methods will be available.
		// Important!  This event is executed *by the thread*, so care must be taken to ensure
		// thread sync when necessary (posting messages to the main thread, etc).
		virtual void OnThreadCleanup() = 0;
	};

	class Semaphore
	{
	protected:
#ifdef __APPLE__
		semaphore_t m_sema;
		int m_counter;
#else
		sem_t m_sema;
#endif

	public:
		Semaphore();
		virtual ~Semaphore();

		void Reset();
		void Post();
		void Post(int multiple);

		void WaitWithoutYield();
		bool WaitWithoutYield(const wxTimeSpan& timeout);
		void WaitNoCancel();
		void WaitNoCancel(const wxTimeSpan& timeout);
		void WaitWithoutYieldWithSpin()
		{
			u32 waited = 0;
			while (true)
			{
				if (TryWait())
					return;
				if (waited >= SPIN_TIME_NS)
					break;
				waited += ShortSpin();
			}
			WaitWithoutYield();
		}
		bool TryWait();
		int Count();

		void Wait();
		bool Wait(const wxTimeSpan& timeout);
	};

	// --------------------------------------------------------------------------------------
	// pxThread - Helper class for the basics of starting/managing persistent threads.
	// --------------------------------------------------------------------------------------
	// This class is meant to be a helper for the typical threading model of "start once and
	// reuse many times."  This class incorporates a lot of extra overhead in stopping and
	// starting threads, but in turn provides most of the basic thread-safety and event-handling
	// functionality needed for a threaded operation.  In practice this model is usually an
	// ideal one for efficiency since Operating Systems themselves typically subscribe to a
	// design where sleeping, suspending, and resuming threads is very efficient, but starting
	// new threads has quite a bit of overhead.
	//
	// To use this as a base class for your threaded procedure, overload the following virtual
	// methods:
	//  void OnStart();
	//  void ExecuteTaskInThread();
	//  void OnCleanupInThread();
	//
	// Use the public methods Start() and Cancel() to start and shutdown the thread, and use
	// m_sem_event internally to post/receive events for the thread (make a public accessor for
	// it in your derived class if your thread utilizes the post).
	//
	// Notes:
	//  * Constructing threads as static global vars isn't recommended since it can potentially
	//    confuse w32pthreads, if the static initializers are executed out-of-order (C++ offers
	//    no dependency options for ensuring correct static var initializations).  Use heap
	//    allocation to create thread objects instead.
	//
	class pxThread
	{
		DeclareNoncopyableObject(pxThread);

	protected:
		wxString m_name; // diagnostic name for our thread.
		pthread_t m_thread;

		WorkSema m_sem_event; // general wait event that's needed by most threads
		Semaphore m_sem_startup; // startup sync tool
		Mutex m_mtx_InThread; // used for canceling and closing threads in a deadlock-safe manner
		MutexRecursive m_mtx_start; // used to lock the Start() code from starting simultaneous threads accidentally.
		Mutex m_mtx_ThreadName;

		std::atomic<bool> m_detached; // a boolean value which indicates if the m_thread handle is valid
		std::atomic<bool> m_running; // set true by Start(), and set false by Cancel(), Block(), etc.

		// exception handle, set non-NULL if the thread terminated with an exception
		// Use RethrowException() to re-throw the exception using its original exception type.
		ScopedPtrMT<BaseException> m_except;

		EventSource<EventListener_Thread> m_evtsrc_OnDelete;


	public:
		virtual ~pxThread();
		pxThread(const wxString& name = L"pxThread");

		pthread_t GetId() const { return m_thread; }

		virtual void Start();
		virtual void Cancel(bool isBlocking = true);
		virtual bool Cancel(const wxTimeSpan& timeout);
		virtual bool Detach();
		virtual void Block();
		virtual bool Block(const wxTimeSpan& timeout);
		virtual void RethrowException() const;

		void AddListener(EventListener_Thread& evt);
		void AddListener(EventListener_Thread* evt)
		{
			if (evt == NULL)
				return;
			AddListener(*evt);
		}

		void WaitOnSelf(Semaphore& mutex) const;
		void WaitOnSelf(Mutex& mutex) const;
		bool WaitOnSelf(Semaphore& mutex, const wxTimeSpan& timeout) const;
		bool WaitOnSelf(Mutex& mutex, const wxTimeSpan& timeout) const;

		bool IsRunning() const;
		bool IsSelf() const;
		bool HasPendingException() const { return !!m_except; }

		wxString GetName() const;
		void SetName(const wxString& newname);

	protected:
		// Extending classes should always implement your own OnStart(), which is called by
		// Start() once necessary locks have been obtained.  Do not override Start() directly
		// unless you're really sure that's what you need to do. ;)
		virtual void OnStart();

		virtual void OnStartInThread();

		// This is called when the thread has been canceled or exits normally.  The pxThread
		// automatically binds it to the pthread cleanup routines as soon as the thread starts.
		virtual void OnCleanupInThread();

		// Implemented by derived class to perform actual threaded task!
		virtual void ExecuteTaskInThread() = 0;

		void TestCancel() const;

		// Yields this thread to other threads and checks for cancellation.  A sleeping thread should
		// always test for cancellation, however if you really don't want to, you can use Threading::Sleep()
		// or better yet, disable cancellation of the thread completely with DisableCancellation().
		//
		// Parameters:
		//   ms - 'minimum' yield time in milliseconds (rough -- typically yields are longer by 1-5ms
		//         depending on operating system/platform).  If ms is 0 or unspecified, then a single
		//         timeslice is yielded to other contending threads.  If no threads are contending for
		//         time when ms==0, then no yield is done, but cancellation is still tested.
		void Yield(int ms = 0)
		{
			pxAssert(IsSelf());
			Threading::Sleep(ms);
			TestCancel();
		}

		void FrankenMutex(Mutex& mutex);

		bool AffinityAssert_AllowFromSelf(const DiagnosticOrigin& origin) const;
		bool AffinityAssert_DisallowFromSelf(const DiagnosticOrigin& origin) const;

		// ----------------------------------------------------------------------------
		// Section of methods for internal use only.

		bool _basecancel();
		void _selfRunningTest(const wxChar* name) const;
		void _DoSetThreadName(const wxString& name);
		void _DoSetThreadName(const char* name) { SetNameOfCurrentThread(name); }
		void _internal_execute();
		void _try_virtual_invoke(void (pxThread::*method)());
		void _ThreadCleanup();

		static void* _internal_callback(void* func);
		static void internal_callback_helper(void* func);
		static void _pt_callback_cleanup(void* handle);
	};
} // namespace Threading
