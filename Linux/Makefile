#
# PCSX2 Makefile for Linux
#


all: pcsx2

CPU = i386
#CPU = x86_64

PCSX2 = pcsx2

CC = gcc
RM = rm -f
AR = ar
STRIP = strip

OPTIMIZE = -O2 -fomit-frame-pointer -finline-functions -ffast-math -fno-strict-aliasing -m128bit-long-double
SSEOPTIMIZE = -O4 -fomit-frame-pointer -finline-functions -ffast-math -fno-strict-aliasing -m128bit-long-double -mfpmath=sse -march=athlon64

FLAGS = -D__LINUX__ -DENABLE_NLS -DPACKAGE=\"pcsx2\"
# this includes the option -rdynamic and we don't want that
LIBST = $(shell gtk-config --libs)
LIBS = $(subst -rdynamic,,${LIBST}) -lz -lpthread

OBJS = ../Counters.o ../DebugTools/DisR5900.o ../Elfheader.o \
	   ../Hw.o ../Memory.o ../Misc.o ../GS.o ../FiFo.o ../Sif.o\
	   ../Plugins.o ../R5900.o ../InterTables.o ../Interpreter.o \
	   ../COP0.o ../FPU.o ../VU0.o ../VU0micro.o ../VU1micro.o ../VUflags.o \
	   ../MMI.o ../Vif.o ../SPR.o ../DebugTools/DisR5900asm.o ../IPU/IPU.o \
	   ../DebugTools/DisVU0Micro.o ../DebugTools/DisVU1Micro.o ../CDVDiso.o ../CDVDisodrv.o \
	   ../DebugTools/cpuopsDebug.o ../IPU/yuv2rgb.o \
	   ../IPU/mpeg2lib/Idct.o ../IPU/mpeg2lib/Mpeg.o \
	   ../Patch.o ../Stats.o ../VUops.o ../Cache.o ../VifDma.o ../FPU2.o
OBJS+= ../R3000A.o ../DebugTools/DisR3000A.o ../PsxBios.o \
	   ../PsxMem.o ../PsxInterpreter.o ../PsxCounters.o ../PsxHw.o \
	   ../PsxDma.o ../Gte.o ../Sio.o ../CdRom.o ../Mdec.o \
	   ../Decode_XA.o ../PsxGPU.o ../PsxSio2.o ../CDVD.o

OBJS+= LnxMain.o Config.o GtkGui.o
OBJS+= GladeGui.o GladeFuncs.o

ifeq (${CPU}, i386)
	OBJS+= ../x86/iR5900.o ../ix86-32/iR5900Arit.o ../ix86-32/iR5900AritImm.o \
		   ../ix86-32/iR5900Branch.o ../ix86-32/iR5900Jump.o ../ix86-32/iR5900LoadStore.o \
		   ../ix86-32/iR5900Move.o ../ix86-32/iR5900MultDiv.o ../ix86-32/iR5900Shift.o \
		   ../x86/recCOP2.o ../x86/iCP0.o \
		   ../x86/iFPU.o ../x86/iMMI.o ../x86/iVUmicro.o \
		   ../x86/iVU0micro.o ../x86/iVU1micro.o ../x86/iR3000A.o
	OBJS+= ../x86/ix86/ix86.o ../x86/ix86/ix86_fpu.o \
		   ../x86/ix86/ix86_3dnow.o ../x86/ix86/ix86_mmx.o \
		   ../x86/ix86/ix86_cpudetect.o ../x86/ix86/ix86_sse.o
	FLAGS+= -I../ix86-32 -I../x86
endif

ifeq (${CPU}, x86_64)
	OBJS+= ../x86/iR5900.o ../x86-64/iR5900Arit.o ../x86-64/iR5900AritImm.o \
		   ../x86-64/iR5900Branch.o ../x86-64/iR5900Jump.o ../x86-64/iR5900LoadStore.o \
		   ../x86-64/iR5900Move.o ../x86-64/iR5900MultDiv.o ../x86-64/iR5900Shift.o \
		   ../x86/recCOP2.o ../x86/iCP0.o \
		   ../x86/iFPU.o ../x86/iMMI.o ../x86/iVUmicro.o \
		   ../x86/iVU0micro.o ../x86/iVU1micro.o ../x86/iR3000A.o
	OBJS+= ../x86/ix86/ix86.o ../x86/ix86/ix86_fpu.o \
		   ../x86/ix86/ix86_3dnow.o ../x86/ix86/ix86_mmx.o \
		   ../x86/ix86/ix86_cpudetect.o ../x86/ix86/ix86_sse.o
	FLAGS+= -I../x86-64 -I../x86
endif

DEPS:= $(OBJS:.o=.d)
ROBJS:= $(OBJS:.o=.ro)
RSOBJS:= $(OBJS:.o=.rso)

CFLAGS = -Wall ${OPTIMIZE} -I. -I.. -I../IPU -I../DebugTools ${FLAGS}
CSSEFLAGS = -Wall ${SSEOPTIMIZE} -I. -I.. -I../IPU -I../DebugTools ${FLAGS}
CFLAGS+= $(shell gtk-config --cflags)
CSSEFLAGS+= $(shell gtk-config --cflags)

pcsx2: ${OBJS}
	${CC} ${CFLAGS} ${OBJS} -o pcsx2 ${LIBS}
#	${STRIP} pcsx2

release: ${ROBJS}
	${CC} ${CFLAGS} ${ROBJS} -o pcsx2 ${LIBS}
#	${STRIP} pcsx2

release-sse: ${RSOBJS}
	${CC} ${CSSEFLAGS} ${RSOBJS} -o pcsx2 ${LIBS}
#	${STRIP} pcsx2

.PHONY: clean pcsx2 release release-sse

clean:
	${RM} ${OBJS} ${DEPS} ${PCSX2} ${ROBJS} ${RSOBJS}

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

%.ro: %.c
	${CC} ${CFLAGS} -D_RELEASE -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

%.rso: %.c
	${CC} ${CSSEFLAGS} -D_RELEASE -c -o $@ $< -MD -MF $(patsubst %.o,%.d,$@)

-include ${DEPS}


