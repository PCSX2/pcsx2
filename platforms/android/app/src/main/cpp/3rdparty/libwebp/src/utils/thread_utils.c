// Copyright 2011 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Multi-threaded worker
//
// Author: Skal (pascal.massimino@gmail.com)

#include "src/utils/thread_utils.h"

#include <assert.h>
#include <string.h>  // for memset()

#include "src/utils/bounds_safety.h"
#include "src/utils/utils.h"

WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

#ifdef WEBP_USE_THREAD

#if defined(_WIN32)

#include <windows.h>
typedef HANDLE pthread_t;

#if _WIN32_WINNT < 0x0600
#error _WIN32_WINNT must target Windows Vista / Server 2008 or newer.
#endif
typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#ifndef WINAPI_FAMILY_PARTITION
#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_FAMILY_PARTITION(x) x
#endif

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define USE_CREATE_THREAD
#endif

#else  // !_WIN32

#include <pthread.h>

#endif  // _WIN32

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t condition;
  pthread_t thread;
} WebPWorkerImpl;

#if defined(_WIN32)

//------------------------------------------------------------------------------
// simplistic pthread emulation layer

#include <process.h>

// _beginthreadex requires __stdcall
#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

static int pthread_create(pthread_t* const thread, const void* attr,
                          unsigned int(__stdcall* start)(void*), void* arg) {
  (void)attr;
#ifdef USE_CREATE_THREAD
  *thread = CreateThread(/*lpThreadAttributes=*/NULL,
                         /*dwStackSize=*/0, start, arg, /*dwStackSize=*/0,
                         /*lpThreadId=*/NULL);
#else
  *thread =
      (pthread_t)_beginthreadex(/*security=*/NULL,
                                /*stack_size=*/0, start, arg, /*initflag=*/0,
                                /*thrdaddr=*/NULL);
#endif
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static int pthread_join(pthread_t thread, void** value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

// Mutex
static int pthread_mutex_init(pthread_mutex_t* const mutex, void* mutexattr) {
  (void)mutexattr;
  InitializeSRWLock(mutex);
  return 0;
}

static int pthread_mutex_lock(pthread_mutex_t* const mutex) {
  AcquireSRWLockExclusive(mutex);
  return 0;
}

static int pthread_mutex_unlock(pthread_mutex_t* const mutex) {
  ReleaseSRWLockExclusive(mutex);
  return 0;
}

static int pthread_mutex_destroy(pthread_mutex_t* const mutex) {
  (void)mutex;
  return 0;
}

// Condition
static int pthread_cond_destroy(pthread_cond_t* const condition) {
  (void)condition;
  return 0;
}

static int pthread_cond_init(pthread_cond_t* const condition, void* cond_attr) {
  (void)cond_attr;
  InitializeConditionVariable(condition);
  return 0;
}

static int pthread_cond_signal(pthread_cond_t* const condition) {
  WakeConditionVariable(condition);
  return 0;
}

static int pthread_cond_wait(pthread_cond_t* const condition,
                             pthread_mutex_t* const mutex) {
  const int ok = SleepConditionVariableSRW(condition, mutex, INFINITE, 0);
  return !ok;
}

#else  // !_WIN32
#define THREADFN void*
#define THREAD_RETURN(val) val
#endif  // _WIN32

//------------------------------------------------------------------------------

static THREADFN ThreadLoop(void* ptr) {
  WebPWorker* const worker = (WebPWorker*)ptr;
  WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
  int done = 0;
  while (!done) {
    pthread_mutex_lock(&impl->mutex);
    while (worker->status == OK) {  // wait in idling mode
      pthread_cond_wait(&impl->condition, &impl->mutex);
    }
    if (worker->status == WORK) {
      WebPGetWorkerInterface()->Execute(worker);
      worker->status = OK;
    } else if (worker->status == NOT_OK) {  // finish the worker
      done = 1;
    }
    // signal to the main thread that we're done (for Sync())
    // Note the associated mutex does not need to be held when signaling the
    // condition. Unlocking the mutex first may improve performance in some
    // implementations, avoiding the case where the waiting thread can't
    // reacquire the mutex when woken.
    pthread_mutex_unlock(&impl->mutex);
    pthread_cond_signal(&impl->condition);
  }
  return THREAD_RETURN(NULL);  // Thread is finished
}

// main thread state control
static void ChangeState(WebPWorker* const worker, WebPWorkerStatus new_status) {
  // No-op when attempting to change state on a thread that didn't come up.
  // Checking 'status' without acquiring the lock first would result in a data
  // race.
  WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
  if (impl == NULL) return;

  pthread_mutex_lock(&impl->mutex);
  if (worker->status >= OK) {
    // wait for the worker to finish
    while (worker->status != OK) {
      pthread_cond_wait(&impl->condition, &impl->mutex);
    }
    // assign new status and release the working thread if needed
    if (new_status != OK) {
      worker->status = new_status;
      // Note the associated mutex does not need to be held when signaling the
      // condition. Unlocking the mutex first may improve performance in some
      // implementations, avoiding the case where the waiting thread can't
      // reacquire the mutex when woken.
      pthread_mutex_unlock(&impl->mutex);
      pthread_cond_signal(&impl->condition);
      return;
    }
  }
  pthread_mutex_unlock(&impl->mutex);
}

#endif  // WEBP_USE_THREAD

//------------------------------------------------------------------------------

static void Init(WebPWorker* const worker) {
  WEBP_UNSAFE_MEMSET(worker, 0, sizeof(*worker));
  worker->status = NOT_OK;
}

static int Sync(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, OK);
#endif
  assert(worker->status <= OK);
  return !worker->had_error;
}

static int Reset(WebPWorker* const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status < OK) {
#ifdef WEBP_USE_THREAD
    WebPWorkerImpl* const impl =
        (WebPWorkerImpl*)WebPSafeCalloc(1, sizeof(WebPWorkerImpl));
    worker->impl = (void*)impl;
    if (worker->impl == NULL) {
      return 0;
    }
    if (pthread_mutex_init(&impl->mutex, NULL)) {
      goto Error;
    }
    if (pthread_cond_init(&impl->condition, NULL)) {
      pthread_mutex_destroy(&impl->mutex);
      goto Error;
    }
    pthread_mutex_lock(&impl->mutex);
    ok = !pthread_create(&impl->thread, NULL, ThreadLoop, worker);
    if (ok) worker->status = OK;
    pthread_mutex_unlock(&impl->mutex);
    if (!ok) {
      pthread_mutex_destroy(&impl->mutex);
      pthread_cond_destroy(&impl->condition);
    Error:
      WebPSafeFree(impl);
      worker->impl = NULL;
      return 0;
    }
#else
    worker->status = OK;
#endif
  } else if (worker->status > OK) {
    ok = Sync(worker);
  }
  assert(!ok || (worker->status == OK));
  return ok;
}

static void Execute(WebPWorker* const worker) {
  if (worker->hook != NULL) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

static void Launch(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, WORK);
#else
  Execute(worker);
#endif
}

static void End(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  if (worker->impl != NULL) {
    WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
    ChangeState(worker, NOT_OK);
    pthread_join(impl->thread, NULL);
    pthread_mutex_destroy(&impl->mutex);
    pthread_cond_destroy(&impl->condition);
    WebPSafeFree(impl);
    worker->impl = NULL;
  }
#else
  worker->status = NOT_OK;
  assert(worker->impl == NULL);
#endif
  assert(worker->status == NOT_OK);
}

//------------------------------------------------------------------------------

static WebPWorkerInterface g_worker_interface = {Init,   Reset,   Sync,
                                                 Launch, Execute, End};

int WebPSetWorkerInterface(const WebPWorkerInterface* const winterface) {
  if (winterface == NULL || winterface->Init == NULL ||
      winterface->Reset == NULL || winterface->Sync == NULL ||
      winterface->Launch == NULL || winterface->Execute == NULL ||
      winterface->End == NULL) {
    return 0;
  }
  g_worker_interface = *winterface;
  return 1;
}

const WebPWorkerInterface* WebPGetWorkerInterface(void) {
  return &g_worker_interface;
}

//------------------------------------------------------------------------------
