# ################################################################
# LZ4 - Makefile common definitions
# Copyright (C) Yann Collet 2020
# All rights reserved.
#
# BSD license
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice, this
#   list of conditions and the following disclaimer in the documentation and/or
#   other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# You can contact the author at :
#  - LZ4 source repository : https://github.com/lz4/lz4
#  - LZ4 forum froup : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

UNAME ?= uname

TARGET_OS ?= $(shell $(UNAME))
ifeq ($(TARGET_OS),)
  TARGET_OS ?= $(OS)
endif

# Default to parallel builds unless a -j flag is already provided.
ifeq (,$(filter -j%,$(MAKEFLAGS)))
NPROC ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
MAKEFLAGS += -j$(NPROC)
endif

ifneq (,$(filter Windows%,$(TARGET_OS)))
LIBLZ4_NAME = liblz4-$(LIBVER_MAJOR)
LIBLZ4_EXP  = liblz4.lib
WINBASED    = yes
else
LIBLZ4_EXP  = liblz4.dll.a
  ifneq (,$(filter MINGW%,$(TARGET_OS)))
LIBLZ4_NAME = liblz4
WINBASED    = yes
  else
    ifneq (,$(filter MSYS%,$(TARGET_OS)))
LIBLZ4_NAME = msys-lz4-$(LIBVER_MAJOR)
WINBASED    = yes
    else
      ifneq (,$(filter CYGWIN%,$(TARGET_OS)))
LIBLZ4_NAME = cyglz4-$(LIBVER_MAJOR)
WINBASED    = yes
      else
LIBLZ4_NAME = liblz4
WINBASED    = no
EXT         =
      endif
    endif
  endif
endif

ifeq ($(WINBASED),yes)
EXT        = .exe
WINDRES ?= windres
LDFLAGS += -Wl,--force-exe-suffix
endif

LIBLZ4      = $(LIBLZ4_NAME).$(SHARED_EXT_VER)

#determine if dev/nul based on host environment
ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(shell $(UNAME))))
VOID := /dev/null
else
  ifneq (,$(filter Windows%,$(OS)))
VOID := nul
  else
VOID  := /dev/null
  endif
endif

ifneq (,$(filter Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS Haiku MidnightBSD MINGW% CYGWIN% MSYS% AIX,$(shell $(UNAME))))
POSIX_ENV = Yes
else
POSIX_ENV = No
endif

# Avoid symlinks when targeting Windows or building on a Windows host
ifeq ($(WINBASED),yes)
LN_SF = cp -p
else
  ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(shell $(UNAME))))
LN_SF = cp -p
  else
    ifneq (,$(filter Windows%,$(OS)))
LN_SF = cp -p
    else
LN_SF  = ln -sf
    endif
  endif
endif

ifneq (,$(filter $(shell $(UNAME)),SunOS))
INSTALL ?= ginstall
else
INSTALL ?= install
endif

INSTALL_PROGRAM ?= $(INSTALL) -m 755
INSTALL_DATA    ?= $(INSTALL) -m 644
MAKE_DIR        ?= $(INSTALL) -d -m 755
