/*
  threadpool.h - part of lz4 project
  Copyright (c) Yann Collet. All rights reserved.
  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - LZ4 source repository : https://github.com/lz4/lz4
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/


/* ======   Dependencies   ======= */
#include <assert.h>
#include "lz4conf.h"  /* LZ4IO_MULTITHREAD */
#include "threadpool.h"


/* ======   Compiler specifics   ====== */
#if defined(_MSC_VER)
#  pragma warning(disable : 4204)        /* disable: C4204: non-constant aggregate initializer */
#endif

#if !LZ4IO_MULTITHREAD

/* ===================================================== */
/* Backup implementation with no multi-threading support */
/* ===================================================== */

/* Non-zero size, to ensure g_poolCtx != NULL */
struct TPool_s {
    int dummy;
};
static TPool g_poolCtx;

TPool* TPool_create(int numThreads, int queueSize) {
    (void)numThreads;
    (void)queueSize;
    return &g_poolCtx;
}

void TPool_free(TPool* ctx) {
    assert(!ctx || ctx == &g_poolCtx);
    (void)ctx;
}

void TPool_submitJob(TPool* ctx, void (*job_function)(void*), void* arg) {
    (void)ctx;
    job_function(arg);
}

void TPool_jobsCompleted(TPool* ctx) {
    assert(!ctx || ctx == &g_poolCtx);
    (void)ctx;
}


#elif defined(_WIN32)

/* Window TPool implementation using Completion Ports */
#include <windows.h>

typedef struct TPool_s {
    HANDLE completionPort;
    HANDLE* workerThreads;
    int nbWorkers;
    int queueSize;
    LONG nbPendingJobs;
    HANDLE jobSlotAvail;  /* For queue size control */
    HANDLE allJobsCompleted; /* Event */
} TPool;

void TPool_free(TPool* pool)
{
    if (!pool) return;

    /* Signal workers to exit by posting NULL completions */
    {   int i;
        for (i = 0; i < pool->nbWorkers; i++) {
            PostQueuedCompletionStatus(pool->completionPort, 0, 0, NULL);
        }
    }

    /* Wait for worker threads to finish */
    WaitForMultipleObjects(pool->nbWorkers, pool->workerThreads, TRUE, INFINITE);

    /* Close thread handles and completion port */
    {   int i;
        for (i = 0; i < pool->nbWorkers; i++) {
            CloseHandle(pool->workerThreads[i]);
        }
    }
    free(pool->workerThreads);
    CloseHandle(pool->completionPort);

    /* Clean up synchronization objects */
    CloseHandle(pool->jobSlotAvail);
    CloseHandle(pool->allJobsCompleted);

    free(pool);
}

static DWORD WINAPI WorkerThread(LPVOID lpParameter)
{
    TPool* const pool = (TPool*)lpParameter;
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    while (GetQueuedCompletionStatus(pool->completionPort,
                                    &bytesTransferred, &completionKey,
                                    &overlapped, INFINITE)) {

        /* End signal */
        if (overlapped == NULL) { break; }

        /* Execute job */
        ((void (*)(void*))completionKey)(overlapped);

        /* Signal job completion */
        if (InterlockedDecrement(&pool->nbPendingJobs) == 0) {
            SetEvent(pool->allJobsCompleted);
        }
        ReleaseSemaphore(pool->jobSlotAvail, 1, NULL);
    }

    return 0;
}

TPool* TPool_create(int nbWorkers, int queueSize)
{
    TPool* pool;

    /* parameters sanitization */
    if (nbWorkers <= 0 || queueSize <= 0) return NULL;
    if (nbWorkers>LZ4_NBWORKERS_MAX) nbWorkers=LZ4_NBWORKERS_MAX;

    pool = calloc(1, sizeof(TPool));
    if (!pool) return NULL;

    /* Create completion port */
    pool->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, nbWorkers);
    if (!pool->completionPort) { goto _cleanup; }

    /* Create worker threads */
    pool->nbWorkers = nbWorkers;
    pool->workerThreads = (HANDLE*)malloc(sizeof(HANDLE) * nbWorkers);
    if (pool->workerThreads == NULL) { goto _cleanup; }

    {   int i;
        for (i = 0; i < nbWorkers; i++) {
            pool->workerThreads[i] = CreateThread(NULL, 0, WorkerThread, pool, 0, NULL);
            if (!pool->workerThreads[i]) { goto _cleanup; }
        }
    }

    /* Initialize sync objects members */
    pool->queueSize = queueSize;
    pool->nbPendingJobs = 0;

    pool->jobSlotAvail = CreateSemaphore(NULL, queueSize+nbWorkers, queueSize+nbWorkers, NULL);
    if (!pool->jobSlotAvail) { goto _cleanup; }

    pool->allJobsCompleted = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!pool->allJobsCompleted) { goto _cleanup; }

    return pool;

_cleanup:
    TPool_free(pool);
    return NULL;
}


void TPool_submitJob(TPool* pool, void (*job_function)(void*), void* arg)
{
    assert(pool);

    /* Atomically increment pending jobs and check for overflow */
    WaitForSingleObject(pool->jobSlotAvail, INFINITE);
    ResetEvent(pool->allJobsCompleted);
    InterlockedIncrement(&pool->nbPendingJobs);

    /* Post the job directly to the completion port */
    PostQueuedCompletionStatus(pool->completionPort,
                               0, /* Bytes transferred not used */
                               (ULONG_PTR)job_function, /* Store function pointer in completionKey */
                               (LPOVERLAPPED)arg);      /* Store argument in overlapped */
}

void TPool_jobsCompleted(TPool* pool)
{
    assert(pool);
    WaitForSingleObject(pool->allJobsCompleted, INFINITE);
}

#else

/* pthread availability assumed */
#include <stdlib.h>  /* malloc, free */
#include <pthread.h> /* pthread_* */

/* A job is just a function with an opaque argument */
typedef struct TPool_job_s {
    void (*job_function)(void*);
    void *arg;
} TPool_job;

struct TPool_s {
    pthread_t* threads;
    size_t threadCapacity;
    size_t threadLimit;

    /* The queue is a circular buffer */
    TPool_job* queue;
    size_t queueHead;
    size_t queueTail;
    size_t queueSize;

    /* The number of threads working on jobs */
    size_t numThreadsBusy;
    /* Indicates if the queue is empty */
    int queueEmpty;

    /* The mutex protects the queue */
    pthread_mutex_t queueMutex;
    /* Condition variable for pushers to wait on when the queue is full */
    pthread_cond_t queuePushCond;
    /* Condition variables for poppers to wait on when the queue is empty */
    pthread_cond_t queuePopCond;
    /* Indicates if the queue is shutting down */
    int shutdown;
};

static void TPool_shutdown(TPool* ctx);

void TPool_free(TPool* ctx) {
    if (!ctx) { return; }
    TPool_shutdown(ctx);
    pthread_mutex_destroy(&ctx->queueMutex);
    pthread_cond_destroy(&ctx->queuePushCond);
    pthread_cond_destroy(&ctx->queuePopCond);
    free(ctx->queue);
    free(ctx->threads);
    free(ctx);
}

static void* TPool_thread(void* opaque);

TPool* TPool_create(int nbThreads, int queueSize)
{
    TPool* ctx;
    /* Check parameters */
    if (nbThreads<1 || queueSize<1) { return NULL; }
    /* Allocate the context and zero initialize */
    ctx = (TPool*)calloc(1, sizeof(TPool));
    if (!ctx) { return NULL; }
    /* init pthread variables */
    {   int error = 0;
        error |= pthread_mutex_init(&ctx->queueMutex, NULL);
        error |= pthread_cond_init(&ctx->queuePushCond, NULL);
        error |= pthread_cond_init(&ctx->queuePopCond, NULL);
        if (error) { TPool_free(ctx); return NULL; }
    }
    /* Initialize the job queue.
     * It needs one extra space since one space is wasted to differentiate
     * empty and full queues.
     */
    ctx->queueSize = (size_t)queueSize + 1;
    ctx->queue = (TPool_job*)calloc(1, ctx->queueSize * sizeof(TPool_job));
    if (ctx->queue == NULL) {
        TPool_free(ctx);
        return NULL;
    }
    ctx->queueHead = 0;
    ctx->queueTail = 0;
    ctx->numThreadsBusy = 0;
    ctx->queueEmpty = 1;
    ctx->shutdown = 0;
    /* Allocate space for the thread handles */
    ctx->threads = (pthread_t*)calloc(1, (size_t)nbThreads * sizeof(pthread_t));
    if (ctx->threads == NULL) {
        TPool_free(ctx);
        return NULL;
    }
    ctx->threadCapacity = 0;
    /* Initialize the threads */
    {   int i;
        for (i = 0; i < nbThreads; ++i) {
            if (pthread_create(&ctx->threads[i], NULL, &TPool_thread, ctx)) {
                ctx->threadCapacity = (size_t)i;
                TPool_free(ctx);
                return NULL;
        }   }
        ctx->threadCapacity = (size_t)nbThreads;
        ctx->threadLimit = (size_t)nbThreads;
    }
    return ctx;
}

/* TPool_thread() :
 * Work thread for the thread pool.
 * Waits for jobs and executes them.
 * @returns : NULL on failure else non-null.
 */
static void* TPool_thread(void* opaque) {
    TPool* const ctx = (TPool*)opaque;
    if (!ctx) { return NULL; }
    for (;;) {
        /* Lock the mutex and wait for a non-empty queue or until shutdown */
        pthread_mutex_lock(&ctx->queueMutex);

        while ( ctx->queueEmpty
            || (ctx->numThreadsBusy >= ctx->threadLimit) ) {
            if (ctx->shutdown) {
                /* even if !queueEmpty, (possible if numThreadsBusy >= threadLimit),
                 * a few threads will be shutdown while !queueEmpty,
                 * but enough threads will remain active to finish the queue */
                pthread_mutex_unlock(&ctx->queueMutex);
                return opaque;
            }
            pthread_cond_wait(&ctx->queuePopCond, &ctx->queueMutex);
        }
        /* Pop a job off the queue */
        {   TPool_job const job = ctx->queue[ctx->queueHead];
            ctx->queueHead = (ctx->queueHead + 1) % ctx->queueSize;
            ctx->numThreadsBusy++;
            ctx->queueEmpty = (ctx->queueHead == ctx->queueTail);
            /* Unlock the mutex, signal a pusher, and run the job */
            pthread_cond_signal(&ctx->queuePushCond);
            pthread_mutex_unlock(&ctx->queueMutex);

            job.job_function(job.arg);

            /* If the intended queue size was 0, signal after finishing job */
            pthread_mutex_lock(&ctx->queueMutex);
            ctx->numThreadsBusy--;
            pthread_cond_signal(&ctx->queuePushCond);
            pthread_mutex_unlock(&ctx->queueMutex);
        }
    }  /* for (;;) */
    assert(0);  /* Unreachable */
}

/*! TPool_shutdown() :
    Shutdown the queue, wake any sleeping threads, and join all of the threads.
*/
static void TPool_shutdown(TPool* ctx) {
    /* Shut down the queue */
    pthread_mutex_lock(&ctx->queueMutex);
    ctx->shutdown = 1;
    pthread_mutex_unlock(&ctx->queueMutex);
    /* Wake up sleeping threads */
    pthread_cond_broadcast(&ctx->queuePushCond);
    pthread_cond_broadcast(&ctx->queuePopCond);
    /* Join all of the threads */
    {   size_t i;
        for (i = 0; i < ctx->threadCapacity; ++i) {
            pthread_join(ctx->threads[i], NULL);  /* note : could fail */
    }   }
}


/*! TPool_jobsCompleted() :
 *  Waits for all queued jobs to finish executing.
 */
void TPool_jobsCompleted(TPool* ctx){
    pthread_mutex_lock(&ctx->queueMutex);
    while(!ctx->queueEmpty || ctx->numThreadsBusy > 0) {
        pthread_cond_wait(&ctx->queuePushCond, &ctx->queueMutex);
    }
    pthread_mutex_unlock(&ctx->queueMutex);
}

/**
 * Returns 1 if the queue is full and 0 otherwise.
 *
 * When queueSize is 1 (pool was created with an intended queueSize of 0),
 * then a queue is empty if there is a thread free _and_ no job is waiting.
 */
static int isQueueFull(TPool const* ctx) {
    if (ctx->queueSize > 1) {
        return ctx->queueHead == ((ctx->queueTail + 1) % ctx->queueSize);
    } else {
        return (ctx->numThreadsBusy == ctx->threadLimit) ||
               !ctx->queueEmpty;
    }
}

static void
TPool_submitJob_internal(TPool* ctx, void (*job_function)(void*), void *arg)
{
    TPool_job job;
    job.job_function = job_function;
    job.arg = arg;
    assert(ctx != NULL);
    if (ctx->shutdown) return;

    ctx->queueEmpty = 0;
    ctx->queue[ctx->queueTail] = job;
    ctx->queueTail = (ctx->queueTail + 1) % ctx->queueSize;
    pthread_cond_signal(&ctx->queuePopCond);
}

void TPool_submitJob(TPool* ctx, void (*job_function)(void*), void* arg)
{
    assert(ctx != NULL);
    pthread_mutex_lock(&ctx->queueMutex);
    /* Wait until there is space in the queue for the new job */
    while (isQueueFull(ctx) && (!ctx->shutdown)) {
        pthread_cond_wait(&ctx->queuePushCond, &ctx->queueMutex);
    }
    TPool_submitJob_internal(ctx, job_function, arg);
    pthread_mutex_unlock(&ctx->queueMutex);
}

#endif  /* LZ4IO_NO_MT */
