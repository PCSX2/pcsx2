/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#if defined (__linux__)  // some distributions are lower case
#define __LINUX__
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <zlib.h>		// fixme: is zlib.h stuff used by that many modules that it should be here in common?
#include <string.h>

#include "PS2Etypes.h"

#if defined(__x86_64__)
#define DONT_USE_GETTEXT
#endif

#if defined(_MSC_VER)

#define strnicmp _strnicmp
#define stricmp _stricmp

#elif defined(__MINGW32__)

#include <sys/types.h>
#include <math.h>
#define BOOL int
#include <stdlib.h> // posix_memalign()
#undef TRUE
#define TRUE  1
#undef FALSE
#define FALSE 0

//#define max(a,b)            (((a) > (b)) ? (a) : (b))
//#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define __declspec(x)
#define __assume(x) ;
#define strnicmp strncasecmp
#define stricmp strcasecmp
#include <winbase.h>
//#pragma intrinsic (InterlockedAnd)
// Definitions added Feb 16, 2006 by efp
//#define __declspec(x)
#include <malloc.h>
#define __forceinline inline
#define _aligned_malloc(x,y) __mingw_aligned_malloc(x,y)
#define _aligned_free(x) __mingw_aligned_free(x)
#define pthread_mutex__unlock pthread_mutex_unlock

#else

#include <sys/types.h>
#include <stdlib.h> // posix_memalign()

// Definitions added Feb 16, 2006 by efp
//#ifndef __declspec
//#define __declspec(x)
//#endif

#endif

#ifdef ENABLE_NLS

#ifdef __MSCW32__
#include "libintlmsc.h"
#else
#include <locale.h>
#include <libintl.h>
#endif

#undef _
#define _(String) dgettext (PACKAGE, String)
#ifdef gettext_noop
#  define N_(String) gettext_noop (String)
#else
#  define N_(String) (String)
#endif

#else

#define _(msgid) msgid
#define N_(msgid) msgid

#endif

struct TESTRUNARGS
{
	u8 enabled;
	u8 jpgcapture;

	int frame; // if < 0, frame is unlimited (run until crash).
	int numimages;
	int curimage;
	u32 autopad; // mask for auto buttons
	int efile;
	int snapdone;

	const char* ptitle;
	const char* pimagename;
	const char* plogname;
	const char* pgsdll, *pcdvddll, *pspudll;

};

extern TESTRUNARGS g_TestRun;

#define BIAS 2   // Bus is half of the actual ps2 speed
//#define PS2CLK   36864000	/* 294.912 mhz */
//#define PSXCLK	 9216000	/* 36.864 Mhz */
//#define PSXCLK	186864000	/* 36.864 Mhz */
#define PS2CLK 294912000 //hz	/* 294.912 mhz */


/* Config.PsxType == 1: PAL:
	 VBlank interlaced		50.00 Hz
	 VBlank non-interlaced	49.76 Hz
	 HBlank					15.625 KHz 
   Config.PsxType == 0: NSTC
	 VBlank interlaced		59.94 Hz
	 VBlank non-interlaced	59.82 Hz
	 HBlank					15.73426573 KHz */

//Misc Clocks
#define PSXPIXEL        ((int)(PSXCLK / 13500000))
#define PSXSOUNDCLK		((int)(48000))

#define COLOR_BLACK		"\033[30m"
#define COLOR_RED		"\033[31m"
#define COLOR_GREEN		"\033[32m"
#define COLOR_YELLOW	"\033[33m"
#define COLOR_BLUE		"\033[34m"
#define COLOR_MAGENTA	"\033[35m"
#define COLOR_CYAN		"\033[36m"
#define COLOR_WHITE		"\033[37m"
#define COLOR_RESET		"\033[0m"

#include <pthread.h> // sync functions

#include "Plugins.h"
#include "DebugTools/Debug.h"
#include "R5900.h"
#include "System.h"
#include "Memory.h"
#include "Elfheader.h"
#include "Hw.h"
#include "Vif.h"
#include "SPR.h"
#include "Sif.h"
#include "Misc.h"
#include "Counters.h"
#include "IPU/IPU.h"
#include "Patch.h"
#include "COP0.h"
#include "VifDma.h"
#if (defined(__i386__) || defined(__x86_64__))
#include "x86/ix86/ix86.h"
#endif

extern void __Log(const char *fmt, ...);
extern u16 logProtocol;
extern u8  logSource;
#define PCSX2_VERSION "Playground (beta)"

#ifdef __LINUX__
#include <errno.h> // EBUSY
#endif

#endif /* __COMMON_H__ */
