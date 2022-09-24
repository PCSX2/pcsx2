/***************************************************************************************************

  Zyan Core Library (Zycore-C)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

/**
 * @file
 * @brief
 */

#ifndef ZYCORE_API_THREAD_H
#define ZYCORE_API_THREAD_H

#include <Zycore/Defines.h>
#include <Zycore/Status.h>

#ifndef ZYAN_NO_LIBC

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

#if   defined(ZYAN_POSIX)

#include <pthread.h>

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 *  Defines the `ZyanThread` data-type.
 */
typedef pthread_t ZyanThread;

/**
 *  Defines the `ZyanThreadId` data-type.
 */
typedef ZyanU64 ZyanThreadId;

/* ---------------------------------------------------------------------------------------------- */
/* Thread Local Storage (TLS)                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 *  Defines the `ZyanThreadTlsIndex` data-type.
 */
typedef pthread_key_t ZyanThreadTlsIndex;

/**
 *  Defines the `ZyanThreadTlsCallback` function prototype.
 */
typedef void(*ZyanThreadTlsCallback)(void* data);

/**
 * Declares a Thread Local Storage (TLS) callback function.
 *
 * @param   name        The callback function name.
 * @param   param_type  The callback data parameter type.
 * @param   param_name  The callback data parameter name.
 */
#define ZYAN_THREAD_DECLARE_TLS_CALLBACK(name, param_type, param_name) \
    void name(param_type* param_name)

/* ---------------------------------------------------------------------------------------------- */

#elif defined(ZYAN_WINDOWS)

#include <windows.h>

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 *  Defines the `ZyanThread` data-type.
 */
typedef HANDLE ZyanThread;

/**
 *  Defines the `ZyanThreadId` data-type.
 */
typedef DWORD ZyanThreadId;

/* ---------------------------------------------------------------------------------------------- */
/* Thread Local Storage (TLS)                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 *  Defines the `ZyanThreadTlsIndex` data-type.
 */
typedef DWORD ZyanThreadTlsIndex;

/**
 *  Defines the `ZyanThreadTlsCallback` function prototype.
 */
typedef PFLS_CALLBACK_FUNCTION ZyanThreadTlsCallback;

/**
 * Declares a Thread Local Storage (TLS) callback function.
 *
 * @param   name        The callback function name.
 * @param   param_type  The callback data parameter type.
 * @param   param_name  The callback data parameter name.
 */
#define ZYAN_THREAD_DECLARE_TLS_CALLBACK(name, param_type, param_name) \
    VOID NTAPI name(param_type* param_name)

/* ---------------------------------------------------------------------------------------------- */

#else
#   error "Unsupported platform detected"
#endif

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Returns the handle of the current thread.
 *
 * @param   thread  Receives the handle of the current thread.
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadGetCurrentThread(ZyanThread* thread);

/**
 * Returns the unique id of the current thread.
 *
 * @param   thread_id   Receives the unique id of the current thread.
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadGetCurrentThreadId(ZyanThreadId* thread_id);

/* ---------------------------------------------------------------------------------------------- */
/* Thread Local Storage (TLS)                                                                     */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Allocates a new Thread Local Storage (TLS) slot.
 *
 * @param   index       Receives the TLS slot index.
 * @param   destructor  A pointer to a destructor callback which is invoked to finalize the data
 *                      in the TLS slot or `ZYAN_NULL`, if not needed.
 *
 * The maximum available number of TLS slots is implementation specific and different on each
 * platform:
 * - Windows
 *   - A total amount of 128 slots per process are guaranteed
 * - POSIX
 *   - A total amount of 128 slots per process are guaranteed
 *   - Some systems guarantee larger amounts like e.g. 1024 slots per process
 *
 * Note that the invocation rules for the destructor callback are implementation specific and
 * different on each platform:
 * - Windows
 *   - The callback is invoked when a thread exits
 *   - The callback is invoked when the process exits
 *   - The callback is invoked when the TLS slot is released
 * - POSIX
 *   - The callback is invoked when a thread exits and the stored value is not null
 *   - The callback is NOT invoked when the process exits
 *   - The callback is NOT invoked when the TLS slot is released
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadTlsAlloc(ZyanThreadTlsIndex* index,
    ZyanThreadTlsCallback destructor);

/**
 * Releases a Thread Local Storage (TLS) slot.
 *
 * @param   index   The TLS slot index.
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadTlsFree(ZyanThreadTlsIndex index);

/**
 * Returns the value inside the given Thread Local Storage (TLS) slot for the
 * calling thread.
 *
 * @param   index   The TLS slot index.
 * @param   data    Receives the value inside the given Thread Local Storage
 *                  (TLS) slot for the calling thread.
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadTlsGetValue(ZyanThreadTlsIndex index, void** data);

/**
 * Set the value of the given Thread Local Storage (TLS) slot for the calling thread.
 *
 * @param   index   The TLS slot index.
 * @param   data    The value to store inside the given Thread Local Storage (TLS) slot for the
 *                  calling thread
 *
 * @return  A zyan status code.
 */
ZYCORE_EXPORT ZyanStatus ZyanThreadTlsSetValue(ZyanThreadTlsIndex index, void* data);

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYAN_NO_LIBC */

#endif /* ZYCORE_API_THREAD_H */
