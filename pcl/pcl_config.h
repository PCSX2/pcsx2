/*
 *  PCL by Davide Libenzi ( Portable Coroutine Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(PCL_CONFIG_H)
#define PCL_CONFIG_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif /* #if defined(HAVE_CONFIG_H) */


#if defined(HAVE_GETCONTEXT) && defined(HAVE_MAKECONTEXT) && defined(HAVE_SWAPCONTEXT)

/*
 * Use this if the system has a working getcontext/makecontext/swapcontext
 * implementation.
 */
#define CO_USE_UCONEXT

#elif defined(HAVE_SIGACTION)

/*
 * Use this to have the generic signal implementation ( not working on
 * Windows ). Suggested on generic Unix implementations or on Linux with
 * CPU different from x86 family.
 */
#define CO_USE_SIGCONTEXT

/*
 * Use this in conjuction with CO_USE_SIGCONTEXT to use the sigaltstack
 * environment ( suggested when CO_USE_SIGCONTEXT is defined ).
 */
#if defined(HAVE_SIGALTSTACK)
#define CO_HAS_SIGALTSTACK
#endif

#endif


#endif

