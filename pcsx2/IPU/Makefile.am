INCLUDES = -I@srcdir@/../ -I@srcdir@/../x86   -I@srcdir@/../../common/
noinst_LIBRARIES = libIPU.a

libIPU_a_SOURCES = IPU.c yuv2rgb.cpp coroutine.cpp acoroutine.S

SUBDIRS = mpeg2lib

#libIPU_a_LIBADD = mpeg2lib/libmpeg2IPU.a