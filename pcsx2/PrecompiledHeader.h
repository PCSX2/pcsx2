// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// Disable some pointless warnings...
#ifdef _MSC_VER
#	pragma warning(disable:4250) //'class' inherits 'method' via dominance
#endif

#include "common/Pcsx2Defs.h"
#include "common/VectorIntrin.h"

//////////////////////////////////////////////////////////////////////////////////////////
// Include the STL that's actually handy.

#include <algorithm>
#include <charconv>
#include <cinttypes>	// Printf format
#include <condition_variable>
#include <climits>
#include <cstring>		// string.h under c++
#include <cstdio>		// stdio.h under c++
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <list>
#include <locale>
#include <memory>
#include <mutex>
#include <functional>
#include <optional>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

// ... and include some ANSI/POSIX C libs that are useful too, just for good measure.
// (these compile lightning fast with or without PCH, but they never change so
// might as well add them here)

#include <stddef.h>
#include <sys/stat.h>

// We use fmt a fair bit now.
// fmt pch breaks GCC in debug builds: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114370
#if !defined(__GNUC__) || defined(__clang__)
#include "fmt/format.h"
#endif

// StringUtil.h is included by the vast majority of translation units and is very
// stable, so parse it once here instead of in every TU. It also pulls in the
// heavy <sstream>/<iomanip>/<locale>/<charconv> stream stack.
#include "common/StringUtil.h"
