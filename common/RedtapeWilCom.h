// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#ifdef _WIN32

#include "common/RedtapeWindows.h"

// warning : variable 's_hrErrorLast' set but not used [-Wunused-but-set-variable]
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif

#include <wil/com.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif