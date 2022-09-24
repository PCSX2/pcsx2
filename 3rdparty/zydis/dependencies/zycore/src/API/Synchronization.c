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

#include <Zycore/API/Synchronization.h>

#ifndef ZYAN_NO_LIBC

/* ============================================================================================== */
/* Internal functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/*                                                                                                */
/* ---------------------------------------------------------------------------------------------- */



/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

#if   defined(ZYAN_POSIX)

#include <errno.h>

/* ---------------------------------------------------------------------------------------------- */
/* Critical Section                                                                               */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanCriticalSectionInitialize(ZyanCriticalSection* critical_section)
{
    pthread_mutexattr_t attribute;

    int error = pthread_mutexattr_init(&attribute);
    if (error != 0)
    {
        if (error == ENOMEM)
        {
            return ZYAN_STATUS_NOT_ENOUGH_MEMORY;
        }
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }
    pthread_mutexattr_settype(&attribute, PTHREAD_MUTEX_RECURSIVE);

    error = pthread_mutex_init(critical_section, &attribute);
    pthread_mutexattr_destroy(&attribute);
    if (error != 0)
    {
        if (error == EAGAIN)
        {
            return ZYAN_STATUS_OUT_OF_RESOURCES;
        }
        if (error == ENOMEM)
        {
            return ZYAN_STATUS_NOT_ENOUGH_MEMORY;
        }
        if (error == EPERM)
        {
            return ZYAN_STATUS_ACCESS_DENIED;
        }
        if ((error == EBUSY) || (error == EINVAL))
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanCriticalSectionEnter(ZyanCriticalSection* critical_section)
{
    const int error = pthread_mutex_lock(critical_section);
    if (error != 0)
    {
        if (error == EINVAL)
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        if (error == EAGAIN)
        {
            return ZYAN_STATUS_INVALID_OPERATION;
        }
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanBool ZyanCriticalSectionTryEnter(ZyanCriticalSection* critical_section)
{
    // No fine grained error handling for this one
    return pthread_mutex_trylock(critical_section) ? ZYAN_FALSE : ZYAN_TRUE;
}

ZyanStatus ZyanCriticalSectionLeave(ZyanCriticalSection* critical_section)
{
    const int error = pthread_mutex_unlock(critical_section);
    if (error != 0)
    {
        if (error == EINVAL)
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        if (error == EPERM)
        {
            return ZYAN_STATUS_INVALID_OPERATION;
        }
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanCriticalSectionDelete(ZyanCriticalSection* critical_section)
{
    const int error = pthread_mutex_destroy(critical_section);
    if (error != 0)
    {
        if ((error == EBUSY) || (error == EINVAL))
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        return ZYAN_STATUS_BAD_SYSTEMCALL;
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

#elif defined(ZYAN_WINDOWS)

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZyanCriticalSectionInitialize(ZyanCriticalSection* critical_section)
{
    InitializeCriticalSection(critical_section);

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanCriticalSectionEnter(ZyanCriticalSection* critical_section)
{
    EnterCriticalSection(critical_section);

    return ZYAN_STATUS_SUCCESS;
}

ZyanBool ZyanCriticalSectionTryEnter(ZyanCriticalSection* critical_section)
{
    return TryEnterCriticalSection(critical_section) ? ZYAN_TRUE : ZYAN_FALSE;
}

ZyanStatus ZyanCriticalSectionLeave(ZyanCriticalSection* critical_section)
{
    LeaveCriticalSection(critical_section);

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZyanCriticalSectionDelete(ZyanCriticalSection* critical_section)
{
    DeleteCriticalSection(critical_section);

    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */

#else
#   error "Unsupported platform detected"
#endif

/* ============================================================================================== */

#endif /* ZYAN_NO_LIBC */
