#
# PCSX2 Makefile for MINGW32
#


all: pcsx2

CPU = ix86
PCSX2 = pcsx2.exe

CC = gcc
RM = rm -f
AR = ar
STRIP = strip
RC = windres

_OPTIMIZE = -fomit-frame-pointer -finline-functions -fno-exceptions -ffast-math -fno-strict-aliasing -m128bit-long-double 
OPTIMIZE = -g -O2 ${_OPTIMIZE}
ROPTIMIZE = -O2 ${_OPTIMIZE}
RSOPTIMIZE = -O3 ${_OPTIMIZE} -mfpmath=sse -march=athlon64
FLAGS = -D__WIN32__ -D__MINGW32__ -DENABLE_NLS -DPACKAGE=\"pcsx2\"
RC1FLAGS = -d__MINGW32__
LIBS = -L./ -lcomctl32 -lwsock32 -lwinmm -lgdi32 -lcomdlg32 -lintl -lpthreadGC
RESOBJ = pcsx2.o

OBJS = ../Counters.o ../DebugTools/DisR5900.o ../Elfheader.o \
	   ../Hw.o ../Memory.o ../Misc.o ../GS.o ../FiFo.o ../Sif.o\
	   ../Plugins.o ../R5900.o ../InterTables.o ../Interpreter.o \
	   ../COP0.o ../FPU.o ../VU0.o ../VU0micro.o ../VU1micro.o ../VUflags.o \
	   ../MMI.o ../Vif.o ../SPR.o ../DebugTools/DisR5900asm.o ../IPU/IPU.o \
	   ../DebugTools/DisVU0Micro.o ../DebugTools/DisVU1Micro.o ../CDVDiso.o ../CDVDisodrv.o \
	   ../DebugTools/cpuopsDebug.o ../IPU/mpeg2lib/Idct.o ../IPU/mpeg2lib/Mpeg.o ../IPU/yuv2rgb.o \
	   ../Patch.o ../Stats.o ../VUops.o \
	   ../VifDma.o ../Cache.o
OBJS+= ../R3000A.o ../DebugTools/DisR3000A.o ../DebugTools/DisR3000asm.o ../PsxBios.o \
	   ../PsxMem.o ../PsxInterpreter.o ../PsxCounters.o ../PsxHw.o \
	   ../PsxDma.o ../Gte.o ../Sio.o ../CdRom.o ../Mdec.o \
	   ../Decode_XA.o ../PsxGPU.o ../PsxSio2.o ../CDVD.o

OBJS+= WinMain.o ini.o AboutDlg.o ConfigDlg.o McdsDlg.o PatchBrowser.o \
	   DebugMemory.o Debugreg.o RDebugger.o Debugger.o CpuDlg.o ${RESOBJ} \
	   ../RDebug/deci2.o ../RDebug/deci2_dbgp.o \
	   ../RDebug/deci2_dcmp.o \
	   ../RDebug/deci2_iloadp.o ../RDebug/deci2_netmp.o \
	   ../RDebug/deci2_ttyp.o
OBJS+= ../zlib/adler32.o ../zlib/compress.o ../zlib/crc32.o ../zlib/gzio.o ../zlib/uncompr.o ../zlib/deflate.o ../zlib/trees.o \
	../zlib/zutil.o ../zlib/inflate.o ../zlib/infback.o ../zlib/inftrees.o ../zlib/inffast.o
#../RDebug/deci2_drfp.o  

ifeq (${CPU}, ix86)
	OBJS+= ../x86/iR5900.o ../ix86-32/iR5900Arit.o ../ix86-32/iR5900AritImm.o \
		   ../ix86-32/iR5900Branch.o ../ix86-32/iR5900Jump.o ../ix86-32/iR5900LoadStore.o \
		   ../ix86-32/iR5900Move.o ../ix86-32/iR5900MultDiv.o ../ix86-32/iR5900Shift.o \
		   ../x86/recCOP2.o ../x86/iCP0.o \
		   ../x86/iFPU.o ../x86/iMMI.o ../x86/iVUmicro.o \
		   ../x86/iVU0micro.o ../x86/iVU1micro.o ../x86/iR3000A.o
	FLAGS+= -I../ix86-32 -I../x86
	OBJS+= ../x86/ix86/ix86.o ../x86/ix86/ix86_fpu.o \
		   ../x86/ix86/ix86_3dnow.o ../x86/ix86/ix86_mmx.o \
		   ../x86/ix86/ix86_cpudetect.o ../x86/ix86/ix86_sse.o
endif

DEPS:= $(OBJS:.o=.d)
ROBJS:= $(OBJS:.o=.ro)
RESROBJ:= $(RESOBJ:.o=.ro)
RSOBJS:= $(OBJS:.o=.rso)
RESRSOBJ:= $(RESOBJ:.o=.rso)

_CFLAGS = -Wall -mwindows -I. -I.. -I../zlib -I../IPU -I../DebugTools -Imingw ${FLAGS}
CFLAGS = ${_CFLAGS} ${OPTIMIZE}
CRFLAGS = ${_CFLAGS} ${ROPTIMIZE}
CRSFLAGS = ${_CFLAGS} ${RSOPTIMIZE}

pcsx2: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} -o ${PCSX2} ${LIBS}

release: ${ROBJS}
	${CC} ${CFLAGS} ${ROBJS} -o ${PCSX2} ${LIBS}
	${STRIP} ${PCSX2}

release-sse: ${RSOBJS}
	${CC} ${CSSEFLAGS} ${RSOBJS} -o ${PCSX2} ${LIBS}
	${STRIP} ${PCSX2}

.PHONY: clean pcsx2 release release-sse

clean:
	${RM} ${OBJS} ${DEPS} ${PCSX2} ${ROBJS} ${RSOBJS}

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

%.ro: %.c
	${CC} ${CRFLAGS} -D_RELEASE -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

%.rso: %.c
	${CC} ${CRSFLAGS} -D_RELEASE -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

${RESOBJ}: pcsx2.rc
	    ${RC} -D__MINGW32__ -J rc -Imingw -O coff -o $@ -i $<

${RESROBJ}: pcsx2.rc
	    ${RC} -D__MINGW32__ -J rc -Imingw -O coff -o $@ -i $<

${RESRSOBJ}: pcsx2.rc
	    ${RC} -D__MINGW32__ -J rc -Imingw -O coff -o $@ -i $<

-include ${DEPS}
