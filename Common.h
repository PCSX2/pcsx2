/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <zlib.h>
#if defined(__WIN32__)

#include <windows.h>

typedef struct {
	HWND hWnd;           // Main window handle
	HINSTANCE hInstance; // Application instance
	HMENU hMenu;         // Main window menu
	HANDLE hConsole;
} AppData;

extern AppData gApp;
#define pthread_mutex__unlock pthread_mutex_unlock

#elif defined (__LINUX__) 

#include <sys/types.h>
#include <stdlib.h> // posix_memalign()
#define BOOL int

#undef TRUE
#define TRUE  1
#undef FALSE
#define FALSE 0

#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))

#define strnicmp strncasecmp
#define stricmp strcasecmp

// Definitions added Feb 16, 2006 by efp
#define __declspec(x)
#define __forceinline inline
#define _aligned_malloc(x,y) posix_memalign(y,x)
#define _aligned_free(x) free(x)
#define pthread_mutex__unlock pthread_mutex_unlock

#endif

#if defined(__MINGW32__)
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

#define fpusqrtf sqrtf
#define fpufabsf fabsf
#define fpusinf sinf
#define fpucosf cosf
#define fpuexpf expf
#define fpuatanf atanf
#define fpuatan2f atan2f

#endif
#ifdef ENABLE_NLS

#ifdef __MSCW32__
#include "libintlmsc.h"
#else
#include "libintl.h"
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

#include "PS2Etypes.h"

#ifdef PCSX2_DEVBUILD

typedef struct _TESTRUNARGS
{
	u8 enabled;
	u8 jpgcapture;
	int frame; // if < 0, frame is unlimited (run until crash).
	int numimages;
	int curimage;
	u32 autopad; // mask for auto buttons
	int efile;
	int snapdone;

	char* ptitle;
	char* pimagename;
	char* plogname;
	char* pgsdll, *pcdvddll, *pspudll;

} TESTRUNARGS;

extern TESTRUNARGS g_TestRun;

#endif

#define BIAS 2   // Bus is half of the actual ps2 speed
//#define PS2CLK   36864000	/* 294.912 mhz */
//#define PSXCLK	 9216000	/* 36.864 Mhz */
//#define PSXCLK	186864000	/* 36.864 Mhz */
#define PS2CLK 294912000	/* 294.912 mhz */
#define PSXCLK	36864000	/* 36.864 Mhz */


/* Config.PsxType == 1: PAL:
	 VBlank interlaced		50.00 Hz
	 VBlank non-interlaced	49.76 Hz
	 HBlank					15.625 KHz 
   Config.PsxType == 0: NSTC
	 VBlank interlaced		59.94 Hz
	 VBlank non-interlaced	59.82 Hz
	 HBlank					15.73426573 KHz */
#define PS2VBLANK_NTSC_INT		((int)(PS2CLK / 59.94))
#define PS2VBLANK_NTSC  		((int)(PS2CLK / 59.82))
#define PS2VBLANK_PAL_INT		((int)(PS2CLK / 50.00))
#define PS2VBLANK_PAL   		((int)(PS2CLK / 49.76))
#define VBLANK_NTSC			((Config.PsxType & 2) ? 59.94 : 59.82)
#define VBLANK_PAL			((Config.PsxType & 2) ? 50.00 : 49.76)
#define HBLANK_NTSC			(15734.26573)
#define HBLANK_PAL			(15625)

#define PS2HBLANK_NTSC	((int)(PS2CLK / HBLANK_NTSC))
#define PS2HBLANK_PAL	((int)(PS2CLK / HBLANK_PAL))
#define PS2HBLANK		((int)((Config.PsxType & 1) ? PS2HBLANK_PAL : PS2HBLANK_NTSC))
#define PSXVBLANK_NTSC	((int)(PSXCLK / VBLANK_NTSC))
#define PSXVBLANK_PAL	((int)(PSXCLK / VBLANK_PAL))
#define PSXVBLANK		((int)((Config.PsxType & 1) ? PSXVBLANK_PAL : PSXVBLANK_NTSC))
#define PSXHBLANK_NTSC	((int)(PSXCLK / HBLANK_NTSC))
#define PSXHBLANK_PAL	((int)(PSXCLK / HBLANK_PAL))
#define PSXHBLANK		((int)((Config.PsxType & 1) ? PSXHBLANK_PAL : PSXHBLANK_NTSC))
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

#include "R5900.h"
#include "Debug.h"
#include "System.h"
#include "Memory.h"
#include "Elfheader.h"
#include "Hw.h"
#include "GS.h"
#include "Vif.h"
#include "SPR.h"
#include "Sif.h"
#include "Plugins.h"
#include "PS2Edefs.h"
#include "Counters.h"
#include "IPU.h"
#include "Misc.h"
#include "Patch.h"
#include "Stats.h"
#include "COP0.h"
#include "VifDma.h"
#if (defined(__i386__) || defined(__x86_64__))
#include "ix86/ix86.h"
#endif

#define gzfreeze(ptr, size) \
	if (Mode == 1) gzwrite(f, ptr, size); \
	else if (Mode == 0) gzread(f, ptr, size);

#define gzfreezel(ptr) gzfreeze(ptr, sizeof(ptr))

int cdCaseopen;

extern void __Log(char *fmt, ...);
extern u16 logProtocol;
extern u8  logSource;
#define PCSX2_VERSION "0.9.1"

#ifdef __MSCW32__
// C++ code for sqrtf
void InitFPUOps();
extern float (*fpusqrtf)(float fval);
extern float (*fpufabsf)(float fval);
extern float (*fpusinf)(float fval);
extern float (*fpucosf)(float fval);
extern float (*fpuexpf)(float fval);
extern float (*fpuatanf)(float fval);
extern float (*fpuatan2f)(float fvalx, float fvaly);
#endif
// Added Feb 16, 2006 by efp
#ifdef __LINUX__
#include <errno.h> // EBUSY
#endif /* __LINUX__ */

#define DESTROY_MUTEX(mutex) { \
	int err = pthread_mutex_destroy(&mutex); \
	if( err == EBUSY ) \
		SysPrintf("cannot destroy"#mutex"\n"); \
} \

#define DESTROY_COND(cond) { \
	int err = pthread_cond_destroy(&cond); \
	if( err == EBUSY ) \
		SysPrintf("cannot destroy"#cond"\n"); \
} \

#endif /* __COMMON_H__ */
