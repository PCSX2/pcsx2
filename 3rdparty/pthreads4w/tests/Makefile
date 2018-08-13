# Makefile for the pthreads test suite.
# If all of the .pass files can be created, the test suite has passed.
#
# --------------------------------------------------------------------------
#
#      Pthreads4w - POSIX Threads Library for Win32
#      Copyright(C) 1998 John E. Bossom
#      Copyright(C) 1999,2012 Pthreads4w contributors
# 
#      The current list of contributors is contained
#      in the file CONTRIBUTORS included with the source
#      code distribution. The list can also be seen at the
#      following World Wide Web location:
#      https://sourceforge.net/projects/pthreads4w//contributors.html
# 
#      This library is free software; you can redistribute it and/or
#      modify it under the terms of the GNU Lesser General Public
#      License as published by the Free Software Foundation; either
#      version 2 of the License, or (at your option) any later version.
# 
#      This library is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#      Lesser General Public License for more details.
# 
#      You should have received a copy of the GNU Lesser General Public
#      License along with this library in the file COPYING.LIB;
#      if not, write to the Free Software Foundation, Inc.,
#      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
#

PTW32_VER	= 2$(EXTRAVERSION)

CC = cl /errorReport:none /nologo

CP	= copy
RM	= erase
RMDIR	= rmdir /s /q
CAT	= type
MKDIR	= mkdir
ECHO	= echo
TOUCH	= $(ECHO) touched >

# The next path is relative to $BUILD_DIR
QAPC	= # ..\QueueUserAPCEx\User\quserex.dll

CPHDR	= _ptw32.h pthread.h semaphore.h sched.h

OPTIM	= /O2 /Ob0

XXLIBS	= ws2_32.lib

# C++ Exceptions
VCEFLAGS	= /EHs /TP /DPtW32NoCatchWarn /D__CLEANUP_CXX
VCELIB	= libpthreadVCE$(PTW32_VER).lib
VCEIMP	= pthreadVCE$(PTW32_VER).lib
VCEDLL	= pthreadVCE$(PTW32_VER).dll
VCELIBD	= libpthreadVCE$(PTW32_VER)d.lib
VCEIMPD	= pthreadVCE$(PTW32_VER)d.lib
VCEDLLD	= pthreadVCE$(PTW32_VER)d.dll
# Structured Exceptions
VSEFLAGS	= /D__CLEANUP_SEH
VSELIB	= libpthreadVSE$(PTW32_VER).lib
VSEIMP	= pthreadVSE$(PTW32_VER).lib
VSEDLL	= pthreadVSE$(PTW32_VER).dll
VSELIBD	= libpthreadVSE$(PTW32_VER)d.lib
VSEIMPD	= pthreadVSE$(PTW32_VER)d.lib
VSEDLLD	= pthreadVSE$(PTW32_VER)d.dll
# C cleanup code
VCFLAGS	= /D__CLEANUP_C
VCLIB	= libpthreadVC$(PTW32_VER).lib
VCIMP	= pthreadVC$(PTW32_VER).lib
VCDLL	= pthreadVC$(PTW32_VER).dll
VCLIBD	= libpthreadVC$(PTW32_VER)d.lib
VCIMPD	= pthreadVC$(PTW32_VER)d.lib
VCDLLD	= pthreadVC$(PTW32_VER)d.dll
# C++ Exceptions in application - using VC version of pthreads dll
VCXFLAGS	= /EHs /TP /D__CLEANUP_C

# Defaults
CPLIB	= $(VCLIB)
CPDLL	= $(VCDLL)

CFLAGS				= $(OPTIM) /W3 /Z7
CFLAGS_DEBUG		= $(OPTIM) /W3 /Z7
LFLAGS				= /INCREMENTAL:NO
INCLUDES			= -I.
BUILD_DIR			= ..

EHFLAGS		=
EHFLAGS_DLL				= /MD
EHFLAGS_DLL_DEBUG		= /MDd
EHFLAGS_STATIC			= /MT
EHFLAGS_STATIC_DEBUG	= /MTd

# If a test case returns a non-zero exit code to the shell, make will
# stop.

include common.mk

#
# To build and run "foo.exe" and "bar.exe" only use, e.g.:
# nmake clean VC NO_DEPS=1 TESTS="foo bar"
#
# To build and run "foo.exe" and "bar.exe" and run all prerequisite tests
# use, e.g.:
# nmake clean VC TESTS="foo bar"
#
# Set TESTS to one or more tests.
#
!IFNDEF NO_DEPS
include runorder.mk
!ENDIF

help:
	@ $(ECHO) Run one of the following command lines:
	@ $(ECHO) nmake clean VC
	@ $(ECHO) nmake clean VC-bench
	@ $(ECHO) nmake clean VC-static
	@ $(ECHO) nmake clean VC-static-bench
	@ $(ECHO) nmake clean VC-debug
	@ $(ECHO) nmake clean VC-static-debug
	@ $(ECHO) nmake clean VC-small-static-debug
	@ $(ECHO) nmake clean VCX
	@ $(ECHO) nmake clean VCX-bench
	@ $(ECHO) nmake clean VCX-static
	@ $(ECHO) nmake clean VCX-static-bench
	@ $(ECHO) nmake clean VCX-debug
	@ $(ECHO) nmake clean VCX-static-debug
	@ $(ECHO) nmake clean VCX-small-static-debug
	@ $(ECHO) nmake clean VCE
	@ $(ECHO) nmake clean VCE-bench
	@ $(ECHO) nmake clean VCE-static
	@ $(ECHO) nmake clean VCE-static-bench
	@ $(ECHO) nmake clean VCE-debug
	@ $(ECHO) nmake clean VCE-static-debug
	@ $(ECHO) nmake clean VCE-small-static-debug
	@ $(ECHO) nmake clean VSE
	@ $(ECHO) nmake clean VSE-bench
	@ $(ECHO) nmake clean VSE-static
	@ $(ECHO) nmake clean VSE-static-bench
	@ $(ECHO) nmake clean VSE-debug
	@ $(ECHO) nmake clean VSE-static-debug
	@ $(ECHO) nmake clean VSE-small-static-debug

VC:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMP)" CPDLL="$(VCDLL)" EHFLAGS="$(VCFLAGS) $(EHFLAGS_DLL)" allpassed

VCE:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCEIMP)" CPDLL="$(VCEDLL)" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_DLL)" allpassed

VSE:	
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSEIMP)" CPDLL="$(VSEDLL)" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_DLL)" allpassed

VCX:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMP)" CPDLL="$(VCDLL)" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_DLL)" allpassed

VC-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMP)" CPDLL="$(VCDLL)" EHFLAGS="$(VCFLAGS) $(EHFLAGS_DLL)" $(BENCHTESTS)

VCE-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCEIMP)" CPDLL="$(VCEDLL)" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_DLL)" $(BENCHTESTS)

VSE-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSEIMP)" CPDLL="$(VSEDLL)" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_DLL)" $(BENCHTESTS)

VCX-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMP)" CPDLL="$(VCDLL)" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_DLL)" $(BENCHTESTS)

VC-static VC-small-static:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIB)" CPDLL="" EHFLAGS="$(VCFLAGS) $(EHFLAGS_STATIC)" allpassed

VCE-static VCE-small-static:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCELIB)" CPDLL="" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_STATIC)" allpassed

VSE-static VSE-small-static:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSELIB)" CPDLL="" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_STATIC)" allpassed

VCX-static VCX-small-static:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIBD)" CPDLL="" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_STATIC)" allpassed

VC-static-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIB)" CPDLL="" EHFLAGS="$(VCFLAGS) $(EHFLAGS_STATIC)" $(BENCHTESTS)

VCE-static-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCELIB)" CPDLL="" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_STATIC)" $(BENCHTESTS)

VSE-static-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSELIB)" CPDLL="" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_STATIC)" $(BENCHTESTS)

VCX-static-bench:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIBD)" CPDLL="" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_STATIC)" $(BENCHTESTS)

VC-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMPD)" CPDLL="$(VCDLLD)" EHFLAGS="$(VCFLAGS) $(EHFLAGS_DLL_DEBUG)" allpassed

VC-static-debug VC-small-static-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIBD)" CPDLL="" EHFLAGS="$(VCFLAGS) $(EHFLAGS_STATIC_DEBUG)" allpassed

VCE-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCEIMPD)" CPDLL="$(VCEDLLD)" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_DLL_DEBUG)" allpassed

VCE-static-debug VCE-small-static-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCELIBD)" CPDLL="" EHFLAGS="$(VCEFLAGS) $(EHFLAGS_STATIC_DEBUG)" allpassed

VSE-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSEIMPD)" CPDLL="$(VSEDLLD)" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_DLL_DEBUG)" allpassed

VSE-static-debug VSE-small-static-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VSELIBD)" CPDLL="" EHFLAGS="$(VSEFLAGS) $(EHFLAGS_STATIC_DEBUG)" allpassed

VCX-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCIMPD)" CPDLL="$(VCDLLD)" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_DLL_DEBUG)" allpassed

VCX-static-debug VCX-small-static-debug:
	@ $(MAKE) /E /nologo TEST="$@" CPLIB="$(VCLIBD)" CPDLL="" EHFLAGS="$(VCXFLAGS) $(EHFLAGS_STATIC_DEBUG)" allpassed

clean:
	if exist *.dll $(RM) *.dll
	if exist *.lib $(RM) *.lib
	if exist _ptw32.h $(RM) _ptw32.h
	if exist pthread.h $(RM) pthread.h
	if exist semaphore.h $(RM) semaphore.h
	if exist sched.h $(RM) sched.h
	if exist *.e $(RM) *.e
	if exist *.i $(RM) *.i
	if exist *.obj $(RM) *.obj
	if exist *.pdb $(RM) *.pdb
	if exist *.o $(RM) *.o
	if exist *.asm $(RM) *.asm
	if exist *.exe $(RM) *.exe
	if exist *.manifest $(RM) *.manifest
	if exist *.pass $(RM) *.pass
	if exist *.bench $(RM) *.bench
	if exist *.log $(RM) *.log

.c.pass:
	$(CC) $(CFLAGS) $(INCLUDES) $(EHFLAGS) $*.c /Fe$*.exe /link $(LFLAGS) $(CPLIB) $(XXLIBS)
	@ $(ECHO) ... Running $(TEST) test: $*.exe
	@ .\$*.exe
	@ $(ECHO) ...... Passed
	@ $(TOUCH) $*.pass

$(ALL_KNOWN_TESTS): $(CPHDR) $(CPLIB) $(CPDLL) $(QAPC) $@.pass

allpassed: $(TESTS)
	@ $(ECHO) ALL TESTS PASSED! Congratulations!

$(BENCHTESTS): $(CPHDR) $(CPLIB) $(CPDLL) $(QAPC)
	$(CC) $(EHFLAGS) $(CFLAGS) $(INCLUDES) benchlib.c $*.c /Fe$*.exe /link $(LFLAGS) $(CPLIB) $(XXLIBS)
	@ $(ECHO) ... Running $(TEST) benchmark: $*.exe
	@ .\$*.exe
	@ $(ECHO) ...... Done

.c.i:
	$(CC) /P $(EHFLAGS) $(CFLAGS) $(INCLUDES) $<

$(CPHDR) $(CPLIB) $(CPDLL) $(QAPC):
	@ if exist $(BUILD_DIR)\$@ $(ECHO) Copying $(BUILD_DIR)\$@ && $(CP) $(BUILD_DIR)\$@ .
