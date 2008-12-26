INCLUDES = -I@srcdir@/.. -I@srcdir@/../../
noinst_LIBRARIES = libix86.a

libix86_a_SOURCES = ix86_3dnow.cpp  ix86.cpp  ix86_cpudetect.cpp  ix86_fpu.cpp  ix86.h ix86_sse.cpp

if X86_64
else
libix86_a_SOURCES += ix86_mmx.cpp
endif
