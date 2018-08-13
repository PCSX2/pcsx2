/*
 * tryentercs.c
 *
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * This file is part of Pthreads4w.
 *
 *    Pthreads4w is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Pthreads4w is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Pthreads4w.  If not, see <http://www.gnu.org/licenses/>. *
 *
 * --------------------------------------------------------------------------
 *
 * See if we have the TryEnterCriticalSection function.
 * Does not use any part of pthreads.
 */

#include <windows.h>
#include <process.h>
#include <stdio.h>

/*
 * Function pointer to TryEnterCriticalSection if it exists
 * - otherwise NULL
 */
BOOL (WINAPI *_try_enter_critical_section)(LPCRITICAL_SECTION) = NULL;

/*
 * Handle to kernel32.dll
 */
static HINSTANCE _h_kernel32;


int
main()
{
  LPCRITICAL_SECTION lpcs = NULL;

  SetLastError(0);

  printf("Last Error [main enter] %ld\n", (long) GetLastError());

  /*
   * Load KERNEL32 and try to get address of TryEnterCriticalSection
   */
  _h_kernel32 = LoadLibrary(TEXT("KERNEL32.DLL"));
  _try_enter_critical_section =
        (BOOL (PT_STDCALL *)(LPCRITICAL_SECTION))
        GetProcAddress(_h_kernel32,
                         (LPCSTR) "TryEnterCriticalSection");

  if (_try_enter_critical_section != NULL)
    {
      SetLastError(0);

      (*_try_enter_critical_section)(lpcs);

      printf("Last Error [try enter] %ld\n", (long) GetLastError());
    }

  (void) FreeLibrary(_h_kernel32);

  printf("This system %s TryEnterCriticalSection.\n",
         (_try_enter_critical_section == NULL) ? "DOES NOT SUPPORT" : "SUPPORTS");
  printf("POSIX Mutexes will be based on Win32 %s.\n",
         (_try_enter_critical_section == NULL) ? "Mutexes" : "Critical Sections");

  return(0);
}

