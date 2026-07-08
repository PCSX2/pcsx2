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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct TPool_s TPool;

/*! TPool_create() :
 *  Create a thread pool with at most @nbThreads.
 * @nbThreads must be at least 1.
 * @queueSize is the maximum number of pending jobs before blocking.
 * @return : TPool* pointer on success, else NULL.
*/
TPool* TPool_create(int nbThreads, int queueSize);

/*! TPool_free() :
 *  Free a thread pool returned by TPool_create().
 *  Waits for the completion of running jobs before freeing resources.
 */
void TPool_free(TPool* ctx);

/*! TPool_submitJob() :
 *  Add @job_function(arg) to the thread pool.
 * @ctx must be valid.
 *  Invocation can block if queue is full.
 *  Note: Ensure @arg's lifetime extends until @job_function completes.
 *  Alternatively, @arg's lifetime must be managed by @job_function.
 */
void TPool_submitJob(TPool* ctx, void (*job_function)(void*), void* arg);

/*! TPool_jobsCompleted() :
 *  Blocks until all queued jobs are completed.
 */
void TPool_jobsCompleted(TPool* ctx);



#if defined (__cplusplus)
}
#endif

#endif /* THREADPOOL_H */
