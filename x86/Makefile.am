INCLUDES = -I../
noinst_LIBRARIES = libx86recomp.a

libx86recomp_a_SOURCES = \
iCore.cpp  iFPU.c  iR3000A.cpp        iR5900.h        iVU1micro.c  iVUops.h \
iCore.h    iFPU.h  iR3000A.h          ir5900tables.c  iVU1micro.h  iVUzerorec.cpp \
iCP0.c     iMMI.c  iR3000Atables.cpp  iVU0micro.c     iVUmicro.c   iVUzerorec.h \
iCP0.h     iMMI.h  iR5900.c           iVU0micro.h     iVUmicro.h   recCOP2.c



if X86_64
tempdir = ix86-64
else
tempdir = ix86-32
endif

SUBDIRS = ix86 $(tempdir)