# ##########################################################################
# LZ4 programs - Makefile
# Copyright (C) Yann Collet 2011-2023
#
# This Makefile is validated for Linux, macOS, *BSD, Hurd, Solaris, MSYS2 targets
#
# GPL v2 License
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# You can contact the author at :
#  - LZ4 homepage : http://www.lz4.org
#  - LZ4 source repository : https://github.com/lz4/lz4
# ##########################################################################
# lz4 : Command Line Utility, supporting gzip-like arguments
# lz4c  : CLU, supporting also legacy lz4demo arguments
# lz4c32: Same as lz4c, but forced to compile in 32-bits mode
# ##########################################################################
SED ?= sed

# Version numbers
LIBLZ4DIR   := ../lib
LIBVER_SRC := $(LIBLZ4DIR)/lz4.h
LIBVER_MAJOR_SCRIPT:=`$(SED) -n '/define[[:blank:]][[:blank:]]*LZ4_VERSION_MAJOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_MINOR_SCRIPT:=`$(SED) -n '/define[[:blank:]][[:blank:]]*LZ4_VERSION_MINOR/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_PATCH_SCRIPT:=`$(SED) -n '/define[[:blank:]][[:blank:]]*LZ4_VERSION_RELEASE/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < $(LIBVER_SRC)`
LIBVER_SCRIPT:= $(LIBVER_MAJOR_SCRIPT).$(LIBVER_MINOR_SCRIPT).$(LIBVER_PATCH_SCRIPT)
LIBVER_MAJOR := $(shell echo $(LIBVER_MAJOR_SCRIPT))
LIBVER_MINOR := $(shell echo $(LIBVER_MINOR_SCRIPT))
LIBVER_PATCH := $(shell echo $(LIBVER_PATCH_SCRIPT))
LIBVER   := $(shell echo $(LIBVER_SCRIPT))

LIBFILES  = $(wildcard $(LIBLZ4DIR)/*.c)
SRCFILES  = $(sort $(LIBFILES) $(wildcard *.c))
OBJFILES  = $(SRCFILES:.c=.o)

DEBUGFLAGS= -Wall -Wextra -Wundef -Wcast-qual -Wcast-align -Wshadow \
            -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes \
            -Wpointer-arith -Wstrict-aliasing=1
USERCFLAGS:= -O3 $(CFLAGS) # -O3 can be overruled by user-provided -Ox level
CFLAGS    = $(DEBUGFLAGS) $(USERCFLAGS)
CPPFLAGS += -I$(LIBLZ4DIR) -DXXH_NAMESPACE=LZ4_

include ../build/make/lz4defs.make

OS_VERSION ?= $(UNAME) -r
ifeq ($(TARGET_OS)$(shell $(OS_VERSION)),SunOS5.10)
LDFLAGS  += -lrt
endif

ALLFLAGS  = $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

.PHONY:default
default: lz4-release

C_SRCDIRS = $(LIBLZ4DIR) .
include ../build/make/multiconf.make

# thread detection
NUM_SYMBOL   := \#
NO_THREAD_MSG:= ==> no multithread support: built for single-thread mode only
HAVE_PTHREAD := $(shell printf '$(NUM_SYMBOL)include <pthread.h>\nint main(void) { return 0; }' > have_pthread.c && $(CC) $(ALLFLAGS) -o have_pthread$(EXT) have_pthread.c -pthread 2> $(VOID) && rm -f have_pthread$(EXT) && echo 1 || echo 0; rm have_pthread.c)
HAVE_MULTITHREAD  := $(shell [ "$(HAVE_PTHREAD)" -eq "1" -o -n "$(filter Windows%,$(OS))" ] && echo 1 || echo 0)
ifeq ($(HAVE_MULTITHREAD), 1)
  THREAD_MSG := ==> built with multithreading support
  THREAD_CPP := -DLZ4IO_MULTITHREAD
  ifeq ($(HAVE_PTHREAD), 1)
    THREAD_LD  := -pthread
  endif
else
  THREAD_MSG := $(NO_THREAD_MSG)
endif

LZ4_VERSION=$(LIBVER)
MD2ROFF    = ronn
MD2ROFF_FLAGS = --roff --warnings --manual="User Commands" --organization="lz4 $(LZ4_VERSION)"

# silent mode by default; verbose can be triggered by V=1 or VERBOSE=1
$(V)$(VERBOSE).SILENT:

.PHONY: all
all: lz4 lz4-nomt lz4c unlz4 lz4cat

.PHONY: all32
all32: CFLAGS+=-m32
all32: all

LZ4_OBJS := $(notdir $(OBJFILES))

ifeq ($(WINBASED),yes)
lz4-exe.rc: lz4-exe.rc.in
	@echo creating executable resource
	$(SED) -e 's|@PROGNAME@|lz4|' \
           -e 's|@LIBVER_MAJOR@|$(LIBVER_MAJOR)|g' \
           -e 's|@LIBVER_MINOR@|$(LIBVER_MINOR)|g' \
           -e 's|@LIBVER_PATCH@|$(LIBVER_PATCH)|g' \
           -e 's|@EXT@|.exe|g' \
           $< >$@

lz4-exe.o: lz4-exe.rc
	$(WINDRES) -i lz4-exe.rc -o lz4-exe.o

lz4: CPPFLAGS += $(THREAD_CPP)
lz4: LDFLAGS += $(THREAD_LD)
$(eval $(call c_program,lz4,$(LZ4_OBJS),lz4-exe.o,echo "$(THREAD_MSG)"))

else # not WINBASED

lz4: CPPFLAGS += $(THREAD_CPP)
lz4: LDFLAGS += $(THREAD_LD)
$(eval $(call c_program,lz4,$(LZ4_OBJS),,echo "$(THREAD_MSG)"))
endif

.PHONY: lz4-release
lz4-release: DEBUGFLAGS=
lz4-release: CPPFLAGS+=-DNDEBUG
lz4-release: lz4

lz4-nomt:
$(eval $(call c_program,lz4-nomt,$(LZ4_OBJS)))

CLEAN += lz4-wlib
lz4-wlib: LIBFILES =
lz4-wlib: SRCFILES+= $(LIBLZ4DIR)/xxhash.c  # benchmark unit needs XXH64()
lz4-wlib: LDFLAGS += -L $(LIBLZ4DIR)
lz4-wlib: LDLIBS   = -llz4
lz4-wlib: liblz4 $(OBJFILES)
	@echo WARNING: $@ must link to an extended variant of the dynamic library which also exposes unstable symbols
	$(CC) $(ALLFLAGS) $(OBJFILES) -o $@$(EXT) $(LDLIBS)

.PHONY:liblz4
liblz4: MAKEFLAGS=
liblz4:
	CPPFLAGS="-DLZ4F_PUBLISH_STATIC_FUNCTIONS -DLZ4_PUBLISH_STATIC_FUNCTIONS" $(MAKE) -C $(LIBLZ4DIR) liblz4

CLEAN += lz4c
lz4c: lz4
	$(LN_SF) lz4$(EXT) lz4c$(EXT)

lz4c32: CFLAGS += -m32
$(eval $(call c_program,lz4c32,$(LZ4_OBJS)))

CLEAN += unlz4
unlz4: lz4
	$(LN_SF) lz4$(EXT) unlz4$(EXT)

CLEAN += lz4cat
lz4cat: lz4
	$(LN_SF) lz4$(EXT) lz4cat$(EXT)

lz4.1: lz4.1.md $(LIBVER_SRC)
	cat $< | $(MD2ROFF) $(MD2ROFF_FLAGS) | $(SED) -n '/^\.\\\".*/!p' > $@

.PHONY:man
man: lz4.1

.PHONY:clean-man
clean-man:
	$(RM) lz4.1

.PHONY:preview-man
preview-man: clean-man man
	man ./lz4.1

.PHONY:clean
clean: clean_cache
ifeq ($(WINBASED),yes)
	$(RM) *.rc
endif
	$(MAKE) -C $(LIBLZ4DIR) $@ > $(VOID)
	$(RM) $(CLEAN) *.o *.test core
	$(RM) -r tmp*
	@echo Cleaning completed


#-----------------------------------------------------------------------------
# make install is validated only for Linux, OSX, BSD, Hurd and Solaris targets
#-----------------------------------------------------------------------------
ifeq ($(POSIX_ENV),Yes)

DESTDIR     ?=
# directory variables : GNU conventions prefer lowercase
# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html
# support both lower and uppercase (BSD), use lowercase in script
PREFIX      ?= /usr/local
prefix      ?= $(PREFIX)
EXEC_PREFIX ?= $(prefix)
exec_prefix ?= $(EXEC_PREFIX)
BINDIR      ?= $(exec_prefix)/bin
bindir      ?= $(BINDIR)
DATAROOTDIR ?= $(prefix)/share
datarootdir ?= $(DATAROOTDIR)
MANDIR      ?= $(datarootdir)/man
mandir      ?= $(MANDIR)
MAN1DIR     ?= $(mandir)/man1
man1dir     ?= $(MAN1DIR)

.PHONY: install
install: lz4
	@echo Installing binaries in $(DESTDIR)$(bindir)
	$(MAKE_DIR) $(DESTDIR)$(bindir)/ $(DESTDIR)$(man1dir)/
	$(INSTALL_PROGRAM) lz4$(EXT) $(DESTDIR)$(bindir)/lz4$(EXT)
	$(LN_SF) lz4$(EXT) $(DESTDIR)$(bindir)/lz4c$(EXT)
	$(LN_SF) lz4$(EXT) $(DESTDIR)$(bindir)/lz4cat$(EXT)
	$(LN_SF) lz4$(EXT) $(DESTDIR)$(bindir)/unlz4$(EXT)
	@echo Installing man pages in $(DESTDIR)$(man1dir)
	$(INSTALL_DATA) lz4.1 $(DESTDIR)$(man1dir)/lz4.1
	$(LN_SF) lz4.1 $(DESTDIR)$(man1dir)/lz4c.1
	$(LN_SF) lz4.1 $(DESTDIR)$(man1dir)/lz4cat.1
	$(LN_SF) lz4.1 $(DESTDIR)$(man1dir)/unlz4.1
	@echo lz4 installation completed

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(bindir)/lz4cat$(EXT)
	$(RM) $(DESTDIR)$(bindir)/unlz4$(EXT)
	$(RM) $(DESTDIR)$(bindir)/lz4$(EXT)
	$(RM) $(DESTDIR)$(bindir)/lz4c$(EXT)
	$(RM) $(DESTDIR)$(man1dir)/lz4.1
	$(RM) $(DESTDIR)$(man1dir)/lz4c.1
	$(RM) $(DESTDIR)$(man1dir)/lz4cat.1
	$(RM) $(DESTDIR)$(man1dir)/unlz4.1
	@echo lz4 programs successfully uninstalled

endif
