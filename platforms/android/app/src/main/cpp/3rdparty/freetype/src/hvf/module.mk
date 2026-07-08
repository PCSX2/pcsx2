#
# FreeType 2 HVF module definition
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


FTMODULE_H_COMMANDS += HVF_DRIVER

define HVF_DRIVER
$(OPEN_DRIVER) FT_Driver_ClassRec, hvf_driver_class $(CLOSE_DRIVER)
$(ECHO_DRIVER)hvf       $(ECHO_DRIVER_DESC)Apple HVF fonts$(ECHO_DRIVER_DONE)
endef

# EOF
