#
# FreeType 2 HVF driver configuration rules
#


# Copyright (C) 2025-2026 by
# Apple Inc.
# written by Deborah Goldsmith <goldsmit@apple.com>
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


# HVF driver directory.
#
HVF_DIR := $(SRC_DIR)/hvf


# Compilation flags for the driver.
#
HVF_COMPILE := $(CC) $(ANSIFLAGS)                            \
                     $I$(subst /,$(COMPILER_SEP),$(HVF_DIR)) \
                     $(INCLUDE_FLAGS)                        \
                     $(FT_CFLAGS)

# HVF driver sources (i.e., C files).
#
HVF_DRV_SRC := $(HVF_DIR)/hvfdrv.c  \
               $(HVF_DIR)/hvfload.c \
               $(HVF_DIR)/hvfobjs.c

# HVF driver headers.
#
HVF_DRV_H := $(HVF_DRV_SRC:%.c=%.h) \
             $(HVF_DIR)/hvferror.h


# HVF driver object(s).
#
#   HVF_DRV_OBJ_M is used during `multi' builds.
#   HVF_DRV_OBJ_S is used during `single' builds.
#
HVF_DRV_OBJ_M := $(HVF_DRV_SRC:$(HVF_DIR)/%.c=$(OBJ_DIR)/%.$O)
HVF_DRV_OBJ_S := $(OBJ_DIR)/hvf.$O


# HVF driver source file for single build.
#
HVF_DRV_SRC_S := $(HVF_DIR)/hvf.c


# HVF driver - single object.
#
$(HVF_DRV_OBJ_S): $(HVF_DRV_SRC_S) $(HVF_DRV_SRC) $(FREETYPE_H) $(HVF_DRV_H)
	$(HVF_COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $(HVF_DRV_SRC_S))


# HVF driver - multiple objects
#
$(OBJ_DIR)/%.$O: $(HVF_DIR)/%.c $(FREETYPE_H) $(HVF_DRV_H)
	$(HVF_COMPILE) $T$(subst /,$(COMPILER_SEP),$@ $<)


# Update main driver object lists.
#
DRV_OBJS_S += $(HVF_DRV_OBJ_S)
DRV_OBJS_M += $(HVF_DRV_OBJ_M)


# EOF
