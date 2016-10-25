/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

// stdafx.cpp : source file that includes just the standard includes
// GSdx.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

string format(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	int result = -1, length = 256;

	char* buffer = NULL;

	while(result == -1)
	{
		if(buffer) delete [] buffer;

		buffer = new char[length + 1];

		memset(buffer, 0, length + 1);

		result = vsnprintf(buffer, length, fmt, args);

		length *= 2;
	}

	va_end(args);

	string s(buffer);

	delete [] buffer;

	return s;
}

#ifdef _WIN32

void* vmalloc(size_t size, bool code)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
}

void vmfree(void* ptr, size_t size)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

void* fifo_alloc(size_t size, size_t repeat)
{
	// FIXME check linux code
	return vmalloc(size * repeat, false);
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	// FIXME check linux code
	return vmfree(ptr, size * repeat);
}

#else

#include <sys/mman.h>
#include <unistd.h>

void* vmalloc(size_t size, bool code)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	int flags = PROT_READ | PROT_WRITE;

	if(code)
	{
		flags |= PROT_EXEC;
	}

	return mmap(NULL, size, flags, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void vmfree(void* ptr, size_t size)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	munmap(ptr, size);
}

static int s_shm_fd = -1;

void* fifo_alloc(size_t size, size_t repeat)
{
	fprintf(stderr, "FIFO ALLOC\n");
	ASSERT(s_shm_fd == -1);

	const char* file_name = "/GSDX.mem";
	s_shm_fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (s_shm_fd != -1)
		shm_unlink(file_name); // file is deleted but descriptor is still open
	else
		fprintf(stderr, "Failed to open %s due to %s\n", file_name, strerror(errno));

	if (ftruncate(s_shm_fd, repeat * size) < 0)
		fprintf(stderr, "Failed to reserve memory due to %s\n", strerror(errno));

	void* fifo = mmap(nullptr, size * repeat, PROT_READ | PROT_WRITE, MAP_SHARED, s_shm_fd, 0);

	for (size_t i = 1; i < repeat; i++) {
		void* base = (uint8*)fifo + size * i;
		uint8* next = (uint8*)mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, s_shm_fd, 0);
		if (next != base)
			fprintf(stderr, "Fail to mmap contiguous segment\n");
		else
			fprintf(stderr, "MMAP next %x\n", (uintptr_t)base);
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	fprintf(stderr, "FIFO FREE\n");

	ASSERT(s_shm_fd >= 0);

	if (s_shm_fd < 0)
		return;

	munmap(ptr, size * repeat);

	close(s_shm_fd);
	s_shm_fd = -1;
}

#endif

#if !defined(_MSC_VER)

// declare linux equivalents (alignment must be power of 2 (1,2,4...2^15)

#if !defined(__USE_ISOC11) || defined(ASAN_WORKAROUND)

void* _aligned_malloc(size_t size, size_t alignment)
{
	void *ret = 0;
	posix_memalign(&ret, alignment, size);
	return ret;
}

#endif

#endif
