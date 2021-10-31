/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once


#define CPUINFO_COUNT_OF(array) (sizeof(array) / sizeof(0[array]))

#if defined(__GNUC__)
	#define CPUINFO_LIKELY(condition) (__builtin_expect(!!(condition), 1))
	#define CPUINFO_UNLIKELY(condition) (__builtin_expect(!!(condition), 0))
#else
	#define CPUINFO_LIKELY(condition) (!!(condition))
	#define CPUINFO_UNLIKELY(condition) (!!(condition))
#endif

#ifndef CPUINFO_INTERNAL
	#if defined(__ELF__)
		#define CPUINFO_INTERNAL __attribute__((__visibility__("internal")))
	#elif defined(__MACH__)
		#define CPUINFO_INTERNAL __attribute__((__visibility__("hidden")))
	#else
		#define CPUINFO_INTERNAL
	#endif
#endif

#ifndef CPUINFO_PRIVATE
	#if defined(__ELF__)
		#define CPUINFO_PRIVATE __attribute__((__visibility__("hidden")))
	#elif defined(__MACH__)
		#define CPUINFO_PRIVATE __attribute__((__visibility__("hidden")))
	#else
		#define CPUINFO_PRIVATE
	#endif
#endif
