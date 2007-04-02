LIBS = "-L/usr/lib/ $(QTDIR)/lib/ -L/usr/X11R6/lib/ -lqt-mt -lXext -lX11 -lpthread"
INCLUDES = "-I$(QTDIR)/include"

noinst_PROGRAMS = pcsx2

# the application source, library search path, and link libraries
pcsx2_SOURCES = LnxMain.c
pcsx2_LDFLAGS =

LDADD = ../libpcsx2.a ../DebugTools/libDebugTools.a ../IPU/libIPU.a ../x86/ix86/libix86.a ../pcl/libpcl.a ../RDebug/libRDebug.a ../zlib/libpcsx2zlib.a

if X86_64
LDADD += ../x86/ix86-64/libx86-64recomp.a
else
LDADD += ../x86/ix86-32/libx86-32recomp.a
endif
