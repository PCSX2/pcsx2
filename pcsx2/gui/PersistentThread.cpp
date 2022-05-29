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

#include "PrecompiledHeader.h"

#ifdef __linux__
#include <signal.h> // for pthread_kill, which is in pthread.h on w32-pthreads
#endif

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h> // semaphore_create() and semaphore_destroy()
#include <mach/semaphore.h> // semaphore_*()
#include <mach/mach_error.h> // mach_error_string()
#include <mach/mach_time.h> // mach_absolute_time()
#include <mach/thread_act.h>
#endif

#include "PersistentThread.h"
#include "common/EventSource.inl"
#include "common/General.h"
#include <wx/app.h>

using namespace Threading;

template class EventSource<EventListener_Thread>;

// 100ms interval for waitgui (issued from blocking semaphore waits on the main thread,
// to avoid gui deadlock).
const wxTimeSpan Threading::def_yieldgui_interval(0, 0, 0, 100);

ConsoleLogSource_Threading::ConsoleLogSource_Threading()
{
	static const TraceLogDescriptor myDesc =
		{
			"p&xThread", "pxThread",
			"Threading activity: start, detach, sync, deletion, etc."};

	m_Descriptor = &myDesc;
}

ConsoleLogSource_Threading pxConLog_Thread;


class StaticMutex : public Mutex
{
protected:
	bool& m_DeletedFlag;

public:
	StaticMutex(bool& deletedFlag)
		: m_DeletedFlag(deletedFlag)
	{
	}

	virtual ~StaticMutex()
	{
		m_DeletedFlag = true;
	}
};

static pthread_key_t curthread_key = 0;
static s32 total_key_count = 0;

static bool tkl_destructed = false;
static StaticMutex total_key_lock(tkl_destructed);

static void make_curthread_key(const pxThread* thr)
{
	pxAssumeDev(!tkl_destructed, "total_key_lock is destroyed; program is shutting down; cannot create new thread key.");

	ScopedLock lock(total_key_lock);
	if (total_key_count++ != 0)
		return;

	if (0 != pthread_key_create(&curthread_key, NULL))
	{
		pxThreadLog.Error(thr->GetName(), L"Thread key creation failed (probably out of memory >_<)");
		curthread_key = 0;
	}
}

static void unmake_curthread_key()
{
	ScopedLock lock;
	if (!tkl_destructed)
		lock.AssignAndLock(total_key_lock);

	if (--total_key_count > 0)
		return;

	if (curthread_key)
		pthread_key_delete(curthread_key);

	curthread_key = 0;
}

// make life easier for people using VC++ IDE by using this format, which allows double-click
// response times from the Output window...
std::string DiagnosticOrigin::ToString(const char* msg) const
{
	std::string message;

	fmt::format_to(std::back_inserter(message), "{}({}) : assertion failed:\n", srcfile, line);

	if (function)
		fmt::format_to(std::back_inserter(message), "    Function:  {}\n", function);

	if (condition)
		fmt::format_to(std::back_inserter(message), "    Condition: {}\n", condition);

	if (msg)
		fmt::format_to(std::back_inserter(message), "    Message:   {}\n", msg);

	return message;
}

void Threading::pxTestCancel()
{
	pthread_testcancel();
}

// Returns a handle to the current persistent thread.  If the current thread does not belong
// to the pxThread table, NULL is returned.  Since the main/ui thread is not created
// through pxThread it will also return NULL.  Callers can use wxThread::IsMain() to
// test if the NULL thread is the main thread.
pxThread* Threading::pxGetCurrentThread()
{
	return !curthread_key ? NULL : (pxThread*)pthread_getspecific(curthread_key);
}

// returns the name of the current thread, or "Unknown" if the thread is neither a pxThread
// nor the Main/UI thread.
wxString Threading::pxGetCurrentThreadName()
{
	if (pxThread* thr = pxGetCurrentThread())
	{
		return thr->GetName();
	}
	else if (wxThread::IsMain())
	{
		return L"Main/UI";
	}

	return L"Unknown";
}

// (intended for internal use only)
// Returns true if the Wait is recursive, or false if the Wait is safe and should be
// handled via normal yielding methods.
bool Threading::_WaitGui_RecursionGuard(const wxChar* name)
{
	AffinityAssert_AllowFrom_MainUI();

	// In order to avoid deadlock we need to make sure we cut some time to handle messages.
	// But this can result in recursive yield calls, which would crash the app.  Protect
	// against them here and, if recursion is detected, perform a standard blocking wait.

	static int __Guard = 0;
	RecursionGuard guard(__Guard);

	//if( pxAssertDev( !guard.IsReentrant(), "Recursion during UI-bound threading wait object." ) ) return false;

	if (!guard.IsReentrant())
		return false;
	pxThreadLog.Write(pxGetCurrentThreadName(),
		pxsFmt(L"Yield recursion in %s; opening modal dialog.", name));
	return true;
}

void Threading::pxThread::_pt_callback_cleanup(void* handle)
{
	((pxThread*)handle)->_ThreadCleanup();
}

Threading::pxThread::pxThread(const wxString& name)
	: m_name(name)
	, m_thread()
	, m_detached(true) // start out with m_thread in detached/invalid state
	, m_running(false)
{
}

// This destructor performs basic "last chance" cleanup, which is a blocking join
// against the thread. Extending classes should almost always implement their own
// thread closure process, since any pxThread will, by design, not terminate
// unless it has been properly canceled (resulting in deadlock).
//
// Thread safety: This class must not be deleted from its own thread.  That would be
// like marrying your sister, and then cheating on her with your daughter.
Threading::pxThread::~pxThread()
{
	try
	{
		pxThreadLog.Write(GetName(), L"Executing default destructor!");

		if (m_running)
		{
			pxThreadLog.Write(GetName(), L"Waiting for running thread to end...");
			m_mtx_InThread.Wait();
			pxThreadLog.Write(GetName(), L"Thread ended gracefully.");
		}
		Threading::Sleep(1);
		Detach();
	}
	DESTRUCTOR_CATCHALL
}

bool Threading::pxThread::AffinityAssert_AllowFromSelf(const DiagnosticOrigin& origin) const
{
	if (IsSelf())
		return true;

	if (IsDevBuild)
		pxOnAssertFail(origin.srcfile, origin.line, origin.function, pxsFmt(L"Thread affinity violation: Call allowed from '%s' thread only.", WX_STR(GetName())).ToUTF8().data());

	return false;
}

bool Threading::pxThread::AffinityAssert_DisallowFromSelf(const DiagnosticOrigin& origin) const
{
	if (!IsSelf())
		return true;

	if (IsDevBuild)
		pxOnAssertFail(origin.srcfile, origin.line, origin.function, pxsFmt(L"Thread affinity violation: Call is *not* allowed from '%s' thread.", WX_STR(GetName())).ToUTF8().data());

	return false;
}

void Threading::pxThread::FrankenMutex(Mutex& mutex)
{
	if (mutex.RecreateIfLocked())
	{
		// Our lock is bupkis, which means  the previous thread probably deadlocked.
		// Let's create a new mutex lock to replace it.

		pxThreadLog.Error(GetName(), L"Possible deadlock detected on restarted mutex!");
	}
}

// Main entry point for starting or e-starting a persistent thread.  This function performs necessary
// locks and checks for avoiding race conditions, and then calls OnStart() immediately before
// the actual thread creation.  Extending classes should generally not override Start(), and should
// instead override DoPrepStart instead.
//
// This function should not be called from the owner thread.
void Threading::pxThread::Start()
{
	// Prevents sudden parallel startup, and or parallel startup + cancel:
	ScopedLock startlock(m_mtx_start);
	if (m_running)
	{
		pxThreadLog.Write(GetName(), L"Start() called on running thread; ignorning...");
		return;
	}

	Detach(); // clean up previous thread handle, if one exists.
	OnStart();

	m_except = NULL;

	pxThreadLog.Write(GetName(), L"Calling pthread_create...");
	if (pthread_create(&m_thread, NULL, _internal_callback, this) != 0)
		throw Exception::ThreadCreationError(this).SetDiagMsg(StringUtil::StdStringFromFormat("Thread creation error: %s" , std::strerror(errno)));

#ifdef ASAN_WORKAROUND
	// Recent Asan + libc6 do pretty bad stuff on the thread init => https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77982
	//
	// In our case, the semaphore was posted (counter is 1) but thread is still
	// waiting...  So waits 100ms and checks the counter value manually
	if (!m_sem_startup.WaitWithoutYield(wxTimeSpan(0, 0, 0, 100)))
	{
		if (m_sem_startup.Count() == 0)
			throw Exception::ThreadCreationError(this).SetDiagMsg("Thread creation error: %s thread never posted startup semaphore.");
	}
#else
	if (!m_sem_startup.WaitWithoutYield(wxTimeSpan(0, 0, 3, 0)))
	{
		RethrowException();

		// And if the thread threw nothing of its own:
		throw Exception::ThreadCreationError(this).SetDiagMsg("Thread creation error: %s thread never posted startup semaphore.");
	}
#endif

	// Event Rationale (above): Performing this semaphore wait on the created thread is "slow" in the
	// sense that it stalls the calling thread completely until the new thread is created
	// (which may not always be desirable).  But too bad.  In order to safely use 'running' locks
	// and detachment management, this *has* to be done.  By rule, starting new threads shouldn't
	// be done very often anyway, hence the concept of Threadpooling for rapidly rotating tasks.
	// (and indeed, this semaphore wait might, in fact, be very swift compared to other kernel
	// overhead in starting threads).

	// (this could also be done using operating system specific calls, since any threaded OS has
	// functions that allow us to see if a thread is running or not, and to block against it even if
	// it's been detached -- removing the need for m_mtx_InThread and the semaphore wait above.  But
	// pthreads kinda lacks that stuff, since pthread_join() has no timeout option making it im-
	// possible to safely block against a running thread)
}

// Returns: TRUE if the detachment was performed, or FALSE if the thread was
// already detached or isn't running at all.
// This function should not be called from the owner thread.
bool Threading::pxThread::Detach()
{
	AffinityAssert_DisallowFromSelf(pxDiagSpot);

	if (m_detached.exchange(true))
		return false;
	pthread_detach(m_thread);
	return true;
}

bool Threading::pxThread::_basecancel()
{
	if (!m_running)
		return false;

	if (m_detached)
	{
		pxThreadLog.Warn(GetName(), L"Ignoring attempted cancellation of detached thread.");
		return false;
	}

	pthread_cancel(m_thread);
	return true;
}

// Remarks:
//   Provision of non-blocking Cancel() is probably academic, since destroying a pxThread
//   object performs a blocking Cancel regardless of if you explicitly do a non-blocking Cancel()
//   prior, since the ExecuteTaskInThread() method requires a valid object state.  If you really need
//   fire-and-forget behavior on threads, use pthreads directly for now.
//
// This function should not be called from the owner thread.
//
// Parameters:
//   isBlocking - indicates if the Cancel action should block for thread completion or not.
//
// Exceptions raised by the blocking thread will be re-thrown into the main thread.  If isBlocking
// is false then no exceptions will occur.
//
void Threading::pxThread::Cancel(bool isBlocking)
{
	AffinityAssert_DisallowFromSelf(pxDiagSpot);

	// Prevent simultaneous startup and cancel, necessary to avoid
	ScopedLock startlock(m_mtx_start);

	if (!_basecancel())
		return;

	if (isBlocking)
	{
		WaitOnSelf(m_mtx_InThread);
		Detach();
	}
}

bool Threading::pxThread::Cancel(const wxTimeSpan& timespan)
{
	AffinityAssert_DisallowFromSelf(pxDiagSpot);

	// Prevent simultaneous startup and cancel:
	ScopedLock startlock(m_mtx_start);

	if (!_basecancel())
		return true;

	if (!WaitOnSelf(m_mtx_InThread, timespan))
		return false;
	Detach();
	return true;
}


// Blocks execution of the calling thread until this thread completes its task.  The
// caller should make sure to signal the thread to exit, or else blocking may deadlock the
// calling thread.  Classes which extend pxThread should override this method
// and signal any necessary thread exit variables prior to blocking.
//
// Returns the return code of the thread.
// This method is roughly the equivalent of pthread_join().
//
// Exceptions raised by the blocking thread will be re-thrown into the main thread.
//
void Threading::pxThread::Block()
{
	AffinityAssert_DisallowFromSelf(pxDiagSpot);
	WaitOnSelf(m_mtx_InThread);
}

bool Threading::pxThread::Block(const wxTimeSpan& timeout)
{
	AffinityAssert_DisallowFromSelf(pxDiagSpot);
	return WaitOnSelf(m_mtx_InThread, timeout);
}

bool Threading::pxThread::IsSelf() const
{
	// Detached threads may have their pthread handles recycled as newer threads, causing
	// false IsSelf reports.
	return !m_detached && (pthread_self() == m_thread);
}

bool Threading::pxThread::IsRunning() const
{
	return m_running;
}

void Threading::pxThread::AddListener(EventListener_Thread& evt)
{
	evt.SetThread(this);
	m_evtsrc_OnDelete.Add(evt);
}

// Throws an exception if the thread encountered one.  Uses the BaseException's Rethrow() method,
// which ensures the exception type remains consistent.  Debuggable stacktraces will be lost, since
// the thread will have allowed itself to terminate properly.
void Threading::pxThread::RethrowException() const
{
	// Thread safety note: always detach the m_except pointer.  If we checked it for NULL, the
	// pointer might still be invalid after detachment, so might as well just detach and check
	// after.

	ScopedExcept ptr(const_cast<pxThread*>(this)->m_except.DetachPtr());
	if (ptr)
		ptr->Rethrow();
}

static bool m_BlockDeletions = false;

bool Threading::AllowDeletions()
{
	AffinityAssert_AllowFrom_MainUI();
	return !m_BlockDeletions;
}

void Threading::YieldToMain()
{
	m_BlockDeletions = true;
	wxTheApp->Yield(true);
	m_BlockDeletions = false;
}

void Threading::pxThread::_selfRunningTest(const wxChar* name) const
{
	if (HasPendingException())
	{
		pxThreadLog.Error(GetName(), pxsFmt(L"An exception was thrown while waiting on a %s.", name));
		RethrowException();
	}

	if (!m_running)
	{
		throw Exception::CancelEvent(pxsFmt(
			L"Blocking thread %s was terminated while another thread was waiting on a %s.",
			WX_STR(GetName()), name).ToStdString());
	}

	// Thread is still alive and kicking (for now) -- yield to other messages and hope
	// that impending chaos does not ensue.  [it shouldn't since we block pxThread
	// objects from being deleted until outside the scope of a mutex/semaphore wait).

	if ((wxTheApp != NULL) && wxThread::IsMain() && !_WaitGui_RecursionGuard(L"WaitForSelf"))
		Threading::YieldToMain();
}

// This helper function is a deadlock-safe method of waiting on a semaphore in a pxThread.  If the
// thread is terminated or canceled by another thread or a nested action prior to the semaphore being
// posted, this function will detect that and throw a CancelEvent exception is thrown.
//
// Note: Use of this function only applies to semaphores which are posted by the worker thread.  Calling
// this function from the context of the thread itself is an error, and a dev assertion will be generated.
//
// Exceptions:
//   This function will rethrow exceptions raised by the persistent thread, if it throws an error
//   while the calling thread is blocking (which also means the persistent thread has terminated).
//
void Threading::pxThread::WaitOnSelf(Semaphore& sem) const
{
	if (!AffinityAssert_DisallowFromSelf(pxDiagSpot))
		return;

	while (true)
	{
		if (sem.WaitWithoutYield(wxTimeSpan(0, 0, 0, 333)))
			return;
		_selfRunningTest(L"semaphore");
	}
}

// This helper function is a deadlock-safe method of waiting on a mutex in a pxThread.
// If the thread is terminated or canceled by another thread or a nested action prior to the
// mutex being unlocked, this function will detect that and a CancelEvent exception is thrown.
//
// Note: Use of this function only applies to mutexes which are acquired by a worker thread.
// Calling this function from the context of the thread itself is an error, and a dev assertion
// will be generated.
//
// Exceptions:
//   This function will rethrow exceptions raised by the persistent thread, if it throws an
//   error while the calling thread is blocking (which also means the persistent thread has
//   terminated).
//
void Threading::pxThread::WaitOnSelf(Mutex& mutex) const
{
	if (!AffinityAssert_DisallowFromSelf(pxDiagSpot))
		return;

	while (true)
	{
		if (mutex.WaitWithoutYield(wxTimeSpan(0, 0, 0, 333)))
			return;
		_selfRunningTest(L"mutex");
	}
}

static const wxTimeSpan SelfWaitInterval(0, 0, 0, 333);

bool Threading::pxThread::WaitOnSelf(Semaphore& sem, const wxTimeSpan& timeout) const
{
	if (!AffinityAssert_DisallowFromSelf(pxDiagSpot))
		return true;

	wxTimeSpan runningout(timeout);

	while (runningout.GetMilliseconds() > 0)
	{
		const wxTimeSpan interval((SelfWaitInterval < runningout) ? SelfWaitInterval : runningout);
		if (sem.WaitWithoutYield(interval))
			return true;
		_selfRunningTest(L"semaphore");
		runningout -= interval;
	}
	return false;
}

bool Threading::pxThread::WaitOnSelf(Mutex& mutex, const wxTimeSpan& timeout) const
{
	if (!AffinityAssert_DisallowFromSelf(pxDiagSpot))
		return true;

	wxTimeSpan runningout(timeout);

	while (runningout.GetMilliseconds() > 0)
	{
		const wxTimeSpan interval((SelfWaitInterval < runningout) ? SelfWaitInterval : runningout);
		if (mutex.WaitWithoutYield(interval))
			return true;
		_selfRunningTest(L"mutex");
		runningout -= interval;
	}
	return false;
}

// Inserts a thread cancellation point.  If the thread has received a cancel request, this
// function will throw an SEH exception designed to exit the thread (so make sure to use C++
// object encapsulation for anything that could leak resources, to ensure object unwinding
// and cleanup, or use the DoThreadCleanup() override to perform resource cleanup).
void Threading::pxThread::TestCancel() const
{
	AffinityAssert_AllowFromSelf(pxDiagSpot);
	pthread_testcancel();
}

// Executes the virtual member method
void Threading::pxThread::_try_virtual_invoke(void (pxThread::*method)())
{
	try
	{
		(this->*method)();
	}

	// ----------------------------------------------------------------------------
	// Neat repackaging for STL Runtime errors...
	//
	catch (std::runtime_error& ex)
	{
		m_except = new Exception::RuntimeError(ex, GetName().ToUTF8());
	}

	// ----------------------------------------------------------------------------
	catch (Exception::RuntimeError& ex)
	{
		BaseException* woot = ex.Clone();
		woot->DiagMsg() += pxsFmt(L"(thread:%s)", WX_STR(GetName())).ToUTF8();
		m_except = woot;
	}
#ifndef PCSX2_DEVBUILD
	// ----------------------------------------------------------------------------
	// Bleh... don't bother with std::exception.  runtime_error should catch anything
	// useful coming out of the core STL libraries anyway, and these are best handled by
	// the MSVC debugger (or by silent random annoying fail on debug-less linux).
	/*catch( std::logic_error& ex )
	{
		throw BaseException( pxsFmt( L"STL Logic Error (thread:%s): %s",
			WX_STR(GetName()), WX_STR(fromUTF8( ex.what() )) )
		);
	}
	catch( std::exception& ex )
	{
		throw BaseException( pxsFmt( L"STL exception (thread:%s): %s",
			WX_STR(GetName()), WX_STR(fromUTF8( ex.what() )) )
		);
	}*/
	// ----------------------------------------------------------------------------
	// BaseException --  same deal as LogicErrors.
	//
	catch (BaseException& ex)
	{
		BaseException* woot = ex.Clone();
		woot->DiagMsg() += pxsFmt(L"(thread:%s)", WX_STR(GetName())).ToUTF8();
		m_except = woot;
	}
#endif
}

// invoked internally when canceling or exiting the thread.  Extending classes should implement
// OnCleanupInThread() to extend cleanup functionality.
void Threading::pxThread::_ThreadCleanup()
{
	AffinityAssert_AllowFromSelf(pxDiagSpot);
	_try_virtual_invoke(&pxThread::OnCleanupInThread);
	m_mtx_InThread.Release();

	// Must set m_running LAST, as thread destructors depend on this value (it is used
	// to avoid destruction of the thread until all internal data use has stopped.
	m_running = false;
}

wxString Threading::pxThread::GetName() const
{
	ScopedLock lock(m_mtx_ThreadName);
	return m_name;
}

void Threading::pxThread::SetName(const wxString& newname)
{
	ScopedLock lock(m_mtx_ThreadName);
	m_name = newname;
}

// This override is called by PeristentThread when the thread is first created, prior to
// calling ExecuteTaskInThread, and after the initial InThread lock has been claimed.
// This code is also executed within a "safe" environment, where the creating thread is
// blocked against m_sem_event.  Make sure to do any necessary variable setup here, without
// worry that the calling thread might attempt to test the status of those variables
// before initialization has completed.
//
void Threading::pxThread::OnStartInThread()
{
	m_detached = false;
	m_running = true;
}

void Threading::pxThread::_internal_execute()
{
	m_mtx_InThread.Acquire();

	_DoSetThreadName(GetName());
	make_curthread_key(this);
	if (curthread_key)
		pthread_setspecific(curthread_key, this);

	OnStartInThread();
	m_sem_startup.Post();

	_try_virtual_invoke(&pxThread::ExecuteTaskInThread);
}

// Called by Start, prior to actual starting of the thread, and after any previous
// running thread has been canceled or detached.
void Threading::pxThread::OnStart()
{
	FrankenMutex(m_mtx_InThread);
	m_sem_event.Reset();
	m_sem_startup.Reset();
}

// Extending classes that override this method should always call it last from their
// personal implementations.
void Threading::pxThread::OnCleanupInThread()
{
	if (curthread_key)
		pthread_setspecific(curthread_key, NULL);

	unmake_curthread_key();

	m_evtsrc_OnDelete.Dispatch(0);
}

// passed into pthread_create, and is used to dispatch the thread's object oriented
// callback function
void* Threading::pxThread::_internal_callback(void* itsme)
{
	if (!pxAssert(itsme != NULL))
		return NULL;

	internal_callback_helper(itsme);
	return nullptr;
}

// __try is used in pthread_cleanup_push when CLEANUP_SEH is used as the cleanup model.
// That can't be used in a function that has objects that require unwinding (compile
// error C2712), so move it into a separate function.
void Threading::pxThread::internal_callback_helper(void* itsme)
{
	pxThread& owner = *static_cast<pxThread*>(itsme);

	pthread_cleanup_push(_pt_callback_cleanup, itsme);
	owner._internal_execute();
	pthread_cleanup_pop(true);
}

void Threading::pxThread::_DoSetThreadName(const wxString& name)
{
	_DoSetThreadName(static_cast<const char*>(name.ToUTF8()));
}

// --------------------------------------------------------------------------------------
//  pthread Cond is an evil api that is not suited for Pcsx2 needs.
//  Let's not use it. (Air)
// --------------------------------------------------------------------------------------

#if 0
Threading::WaitEvent::WaitEvent()
{
	int err = 0;

	err = pthread_cond_init(&cond, NULL);
	err = pthread_mutex_init(&mutex, NULL);
}

Threading::WaitEvent::~WaitEvent()
{
	pthread_cond_destroy( &cond );
	pthread_mutex_destroy( &mutex );
}

void Threading::WaitEvent::Set()
{
	pthread_mutex_lock( &mutex );
	pthread_cond_signal( &cond );
	pthread_mutex_unlock( &mutex );
}

void Threading::WaitEvent::Wait()
{
	pthread_mutex_lock( &mutex );
	pthread_cond_wait( &cond, &mutex );
	pthread_mutex_unlock( &mutex );
}
#endif

// --------------------------------------------------------------------------------------
//  BaseThreadError
// --------------------------------------------------------------------------------------

std::string Exception::BaseThreadError::FormatDiagnosticMessage() const
{
	// This is dangerous and stupid. Thanks wx/px/nonsense.
	wxString thread_name = (m_thread == NULL) ? L"Null Thread Object" : m_thread->GetName();
	return StringUtil::StdStringFromFormat(m_message_diag.c_str(), thread_name.ToUTF8().data());
}

std::string Exception::BaseThreadError::FormatDisplayMessage() const
{
	wxString thread_name = (m_thread == NULL) ? L"Null Thread Object" : m_thread->GetName();
	return StringUtil::StdStringFromFormat(m_message_diag.c_str(), thread_name.ToUTF8().data());
}

pxThread& Exception::BaseThreadError::Thread()
{
	pxAssertDev(m_thread != NULL, "NULL thread object on ThreadError exception.");
	return *m_thread;
}
const pxThread& Exception::BaseThreadError::Thread() const
{
	pxAssertDev(m_thread != NULL, "NULL thread object on ThreadError exception.");
	return *m_thread;
}

#ifndef __APPLE__

Threading::Semaphore::Semaphore()
{
	sem_init(&m_sema, false, 0);
}

Threading::Semaphore::~Semaphore()
{
	sem_destroy(&m_sema);
}

void Threading::Semaphore::Reset()
{
	sem_destroy(&m_sema);
	sem_init(&m_sema, false, 0);
}

void Threading::Semaphore::Post()
{
	sem_post(&m_sema);
}

void Threading::Semaphore::Post(int multiple)
{
#if defined(_MSC_VER)
	sem_post_multiple(&m_sema, multiple);
#else
	// Only w32pthreads has the post_multiple, but it's easy enough to fake:
	while (multiple > 0)
	{
		multiple--;
		sem_post(&m_sema);
	}
#endif
}

void Threading::Semaphore::WaitWithoutYield()
{
	pxAssertMsg(!wxThread::IsMain(), "Unyielding semaphore wait issued from the main/gui thread.  Please use Wait() instead.");
	sem_wait(&m_sema);
}

bool Threading::Semaphore::WaitWithoutYield(const wxTimeSpan& timeout)
{
	wxDateTime megafail(wxDateTime::UNow() + timeout);
	const timespec fail = {megafail.GetTicks(), megafail.GetMillisecond() * 1000000};
	return sem_timedwait(&m_sema, &fail) == 0;
}


// This is a wxApp-safe implementation of Wait, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windoes continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
void Threading::Semaphore::Wait()
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		sem_wait(&m_sema);
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::Wait"))
	{
		sem_wait(&m_sema);
	}
	else
	{
		//ScopedBusyCursor hourglass( Cursor_KindaBusy );
		while (!WaitWithoutYield(def_yieldgui_interval))
			YieldToMain();
	}
#else
	sem_wait(&m_sema);
#endif
}

// This is a wxApp-safe implementation of WaitWithoutYield, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windows continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
// Returns:
//   false if the wait timed out before the semaphore was signaled, or true if the signal was
//   reached prior to timeout.
//
bool Threading::Semaphore::Wait(const wxTimeSpan& timeout)
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		return WaitWithoutYield(timeout);
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::TimedWait"))
	{
		return WaitWithoutYield(timeout);
	}
	else
	{
		//ScopedBusyCursor hourglass( Cursor_KindaBusy );
		wxTimeSpan countdown((timeout));

		do
		{
			if (WaitWithoutYield(def_yieldgui_interval))
				break;
			YieldToMain();
			countdown -= def_yieldgui_interval;
		} while (countdown.GetMilliseconds() > 0);

		return countdown.GetMilliseconds() > 0;
	}
#else
	return WaitWithoutYield(timeout);
#endif
}

// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
// after the wait has completed.  Useful for situations where the semaphore itself is stored on
// the stack and passed to another thread via GUI message or such, avoiding complications where
// the thread might be canceled and the stack value becomes invalid.
//
// Performance note: this function has quite a bit more overhead compared to Semaphore::WaitWithoutYield(), so
// consider manually specifying the thread as uncancellable and using WaitWithoutYield() instead if you need
// to do a lot of no-cancel waits in a tight loop worker thread, for example.
void Threading::Semaphore::WaitNoCancel()
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	//WaitWithoutYield();
	Wait();
	pthread_setcancelstate(oldstate, NULL);
}

void Threading::Semaphore::WaitNoCancel(const wxTimeSpan& timeout)
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	//WaitWithoutYield( timeout );
	Wait(timeout);
	pthread_setcancelstate(oldstate, NULL);
}

bool Threading::Semaphore::TryWait()
{
	return sem_trywait(&m_sema) == 0;
}

int Threading::Semaphore::Count()
{
	int retval;
	sem_getvalue(&m_sema, &retval);
	return retval;
}

#else

static void MACH_CHECK(kern_return_t mach_retval)
{
	switch (mach_retval)
	{
		case KERN_SUCCESS:
			break;
		case KERN_ABORTED: // Awoken due reason unrelated to semaphore (e.g. pthread_cancel)
			pthread_testcancel(); // Unlike sem_wait, mach semaphore ops aren't cancellation points
			// fallthrough
		default:
			fprintf(stderr, "mach error: %s", mach_error_string(mach_retval));
			assert(mach_retval == KERN_SUCCESS);
	}
}

/// Wait up to the given time
/// Returns true if successful, false if timed out
static bool WaitUpTo(semaphore_t sema, wxTimeSpan wxtime)
{
	mach_timespec_t time;
	u64 ms = wxtime.GetMilliseconds().GetValue();
	time.tv_sec = ms / 1000;
	time.tv_nsec = (ms % 1000) * 1000000;
	kern_return_t res = semaphore_timedwait(sema, time);
	if (res == KERN_OPERATION_TIMED_OUT)
		return false;
	MACH_CHECK(res);
	return true;
}

Threading::Semaphore::Semaphore()
{
	// other platforms explicitly make a thread-private (unshared) semaphore
	// here. But it seems Mach doesn't support that.
	MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t*)&m_sema, SYNC_POLICY_FIFO, 0));
	__atomic_store_n(&m_counter, 0, __ATOMIC_RELEASE);
}

Threading::Semaphore::~Semaphore()
{
	MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
}

void Threading::Semaphore::Reset()
{
	MACH_CHECK(semaphore_destroy(mach_task_self(), (semaphore_t)m_sema));
	MACH_CHECK(semaphore_create(mach_task_self(), (semaphore_t*)&m_sema, SYNC_POLICY_FIFO, 0));
	__atomic_store_n(&m_counter, 0, __ATOMIC_SEQ_CST);
}

void Threading::Semaphore::Post()
{
	if (__atomic_fetch_add(&m_counter, 1, __ATOMIC_RELEASE) < 0)
		MACH_CHECK(semaphore_signal(m_sema));
}

void Threading::Semaphore::Post(int multiple)
{
	for (int i = 0; i < multiple; ++i)
	{
		Post();
	}
}

void Threading::Semaphore::WaitWithoutYield()
{
	pxAssertMsg(!wxThread::IsMain(), "Unyielding semaphore wait issued from the main/gui thread.  Please use Wait() instead.");
	if (__atomic_sub_fetch(&m_counter, 1, __ATOMIC_ACQUIRE) < 0)
		MACH_CHECK(semaphore_wait(m_sema));
}

bool Threading::Semaphore::WaitWithoutYield(const wxTimeSpan& timeout)
{
	// This method is the reason why there has to be a special Darwin
	// implementation of Semaphore. Note that semaphore_timedwait() is prone
	// to returning with KERN_ABORTED, which basically signifies that some
	// signal has worken it up. The best official "documentation" for
	// semaphore_timedwait() is the way it's used in Grand Central Dispatch,
	// which is open-source.

	if (__atomic_sub_fetch(&m_counter, 1, __ATOMIC_ACQUIRE) >= 0)
		return true;

	// on x86 platforms, mach_absolute_time() returns nanoseconds
	// TODO(aktau): on iOS a scale value from mach_timebase_info will be necessary
	u64 const kOneThousand = 1000;
	u64 const kOneBillion = kOneThousand * kOneThousand * kOneThousand;
	u64 const delta = timeout.GetMilliseconds().GetValue() * (kOneThousand * kOneThousand);
	mach_timespec_t ts;
	kern_return_t kr = KERN_ABORTED;
	for (u64 now = mach_absolute_time(), deadline = now + delta;
		 kr == KERN_ABORTED; now = mach_absolute_time())
	{
		if (now > deadline)
		{
			// timed out by definition
			kr = KERN_OPERATION_TIMED_OUT;
			break;
		}

		u64 timeleft = deadline - now;
		ts.tv_sec = timeleft / kOneBillion;
		ts.tv_nsec = timeleft % kOneBillion;

		// possible return values of semaphore_timedwait() (from XNU sources):
		// internal kernel val -> return value
		// THREAD_INTERRUPTED  -> KERN_ABORTED
		// THREAD_TIMED_OUT    -> KERN_OPERATION_TIMED_OUT
		// THREAD_AWAKENED     -> KERN_SUCCESS
		// THREAD_RESTART      -> KERN_TERMINATED
		// default             -> KERN_FAILURE
		kr = semaphore_timedwait(m_sema, ts);
	}

	if (kr == KERN_OPERATION_TIMED_OUT)
	{
		int orig = __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
		while (orig < 0)
		{
			if (__atomic_compare_exchange_n(&m_counter, &orig, orig + 1, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
				return false;
		}
		// Semaphore was signalled between our wait expiring and now, keep kernel sema in sync
		kr = semaphore_wait(m_sema);
	}

	// while it's entirely possible to have KERN_FAILURE here, we should
	// probably assert so we can study and correct the actual error here
	// (the thread dying while someone is wainting for it).
	MACH_CHECK(kr);
	return true;
}

// This is a wxApp-safe implementation of Wait, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread. This ensures that
// user input continues to be handled and that windows continue to repaint. If the Wait is
// called from another thread, no message pumping is performed.
void Threading::Semaphore::Wait()
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		WaitWithoutYield();
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::Wait"))
	{
		WaitWithoutYield();
	}
	else
	{
		if (__atomic_sub_fetch(&m_counter, 1, __ATOMIC_ACQUIRE) >= 0)
			return;
		while (!WaitUpTo(m_sema, def_yieldgui_interval))
		{
			YieldToMain();
		}
	}
#else
	WaitWithoutYield();
#endif
}

// This is a wxApp-safe implementation of WaitWithoutYield, which makes sure and executes the App's
// pending messages *if* the Wait is performed on the Main/GUI thread.  This ensures that
// user input continues to be handled and that windows continue to repaint.  If the Wait is
// called from another thread, no message pumping is performed.
//
// Returns:
//   false if the wait timed out before the semaphore was signaled, or true if the signal was
//   reached prior to timeout.
//
bool Threading::Semaphore::Wait(const wxTimeSpan& timeout)
{
#if wxUSE_GUI
	if (!wxThread::IsMain() || (wxTheApp == NULL))
	{
		return WaitWithoutYield(timeout);
	}
	else if (_WaitGui_RecursionGuard(L"Semaphore::TimedWait"))
	{
		return WaitWithoutYield(timeout);
	}
	else
	{
		wxTimeSpan countdown((timeout));

		do
		{
			if (WaitWithoutYield(def_yieldgui_interval))
				break;
			YieldToMain();
			countdown -= def_yieldgui_interval;
		} while (countdown.GetMilliseconds() > 0);

		return countdown.GetMilliseconds() > 0;
	}
#else
	return WaitWithoutYield(timeout);
#endif
}

bool Threading::Semaphore::TryWait()
{
	int counter = __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
	while (counter > 0 && !__atomic_compare_exchange_n(&m_counter, &counter, counter - 1, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		;
	return counter > 0;
}

// Performs an uncancellable wait on a semaphore; restoring the thread's previous cancel state
// after the wait has completed.  Useful for situations where the semaphore itself is stored on
// the stack and passed to another thread via GUI message or such, avoiding complications where
// the thread might be canceled and the stack value becomes invalid.
//
// Performance note: this function has quite a bit more overhead compared to Semaphore::WaitWithoutYield(), so
// consider manually specifying the thread as uncancellable and using WaitWithoutYield() instead if you need
// to do a lot of no-cancel waits in a tight loop worker thread, for example.
//
// I'm unsure how to do this with pure Mach primitives, the docs in
// osfmk/man seem a bit out of date so perhaps there's a possibility, but
// since as far as I know Mach threads are 1-to-1 on BSD uthreads (and thus
// POSIX threads), this should work. -- aktau
void Threading::Semaphore::WaitNoCancel()
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	Wait();
	pthread_setcancelstate(oldstate, NULL);
}

void Threading::Semaphore::WaitNoCancel(const wxTimeSpan& timeout)
{
	int oldstate;
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
	Wait(timeout);
	pthread_setcancelstate(oldstate, NULL);
}

int Threading::Semaphore::Count()
{
	return __atomic_load_n(&m_counter, __ATOMIC_RELAXED);
}

#endif
