AUTOMAKE_OPTIONS = foreign
INCLUDES = -I@srcdir@/x86/  -I@srcdir@/../common/
noinst_LIBRARIES = libpcsx2.a

libpcsx2_a_SOURCES = \
CdRom.cpp       Decode_XA.h    Mdec.h          PsxBios.h         R3000A.cpp  Vif.h \
CdRom.h       EEregs.h       PsxCommon.h       R3000A.h  VU0.cpp \
CDVD.cpp        Elfheader.cpp    Memory.cpp        PsxCounters.cpp     R5900.cpp   VU0.h \
CDVD.h        Elfheader.h    Memory.h        PsxCounters.h     R5900.h   VU0micro.cpp \
CDVDiso.cpp     FiFo.cpp         Misc.cpp          PsxDma.cpp          Sif.cpp     VU1micro.cpp \
CDVDisodrv.cpp  FPU2.cpp       PsxDma.h          Sifcmd.h  VUflags.cpp \
CDVDisodrv.h  FPU.cpp          MMI.cpp     MTGS.cpp      Sif.h     VUflags.h \
CDVDiso.h     GS.cpp         PathUtils.cpp 	Patch.cpp         Sio.cpp    \
CDVDlib.h     GS.h           Patch.h         PsxHw.cpp           Sio.h     VUmicro.h \
Common.h      Hw.cpp           Plugins.cpp       PsxHw.h           SPR.cpp     VUops.cpp \
COP0.cpp  COP2.cpp       Hw.h           Plugins.h       PsxInterpreter.cpp  SPR.h     VUops.h \
COP0.h        Interpreter.cpp  PS2Edefs.h      PsxMem.cpp          System.h \
Counters.cpp    InterTables.cpp  PS2Etypes.h     PsxMem.h          Vif.cpp \
Counters.h    InterTables.h  PsxBios2.h      PsxSio2.cpp         VifDma.cpp \
Decode_XA.cpp         PsxBios.cpp       PsxSio2.h         VifDma.h Cache.cpp vtlb.cpp \
xmlpatchloader.cpp ThreadTools.cpp SourceLog.cpp SaveState.cpp System.cpp \
Console.cpp vssprintf.cpp StringUtils.cpp

SUBDIRS = x86 . DebugTools IPU RDebug tinyxml Linux