/*
 * QEMU System Emulator header
 * 
 * Copyright (c) 2003 Fabrice Bellard
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef VL_H
#define VL_H

/* we put basic includes here to avoid repeating them in device drivers */
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>

#if !defined(_MSC_VER)
#define inline __inline
#endif

#include "qusb.h"

#ifndef glue
#define xglue(x, y) x##y
#define glue(x, y) xglue(x, y)
#define stringify(s) tostring(s)
#define tostring(s) #s
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* vl.c */
uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c);
int cpu_physical_memory_rw(uint32_t addr, uint8_t* buf,
						   size_t len, int is_write);

inline int cpu_physical_memory_read(uint32_t addr,
									uint8_t* buf, size_t len)
{
	return cpu_physical_memory_rw(addr, buf, len, 0);
}

inline int cpu_physical_memory_write(uint32_t addr,
									 const uint8_t* buf, size_t len)
{
	return cpu_physical_memory_rw(addr, (uint8_t*)buf, len, 1);
}

#endif /* VL_H */
