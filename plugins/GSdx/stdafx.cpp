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

std::string format(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(nullptr, 0, fmt, args) + 1;
	va_end(args);

	assert(size > 0);
	std::vector<char> buffer(std::max(1, size));

	va_start(args, fmt);
	vsnprintf(buffer.data(), size, fmt, args);
	va_end(args);

	return {buffer.data()};
}

// Helper path to dump texture
#ifdef _WIN32
const std::string root_sw("c:\\temp1\\_");
const std::string root_hw("c:\\temp2\\_");
#else
#ifdef _M_AMD64
const std::string root_sw("/tmp/GS_SW_dump64/");
const std::string root_hw("/tmp/GS_HW_dump64/");
#else
const std::string root_sw("/tmp/GS_SW_dump32/");
const std::string root_hw("/tmp/GS_HW_dump32/");
#endif
#endif

#ifdef _WIN32

void* vmalloc(size_t size, bool code)
{
	return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
}

void vmfree(void* ptr, size_t size)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

static HANDLE s_fh = NULL;
static uint8* s_Next[8];

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_fh == NULL);

	if (repeat >= countof(s_Next)) {
		fprintf(stderr, "Memory mapping overflow (%zu >= %u)\n", repeat, countof(s_Next));
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	s_fh = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
	DWORD errorID = ::GetLastError();
	if (s_fh == NULL) {
		fprintf(stderr, "Failed to reserve memory. WIN API ERROR:%u\n", errorID);
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	int mmap_segment_failed = 0;
	void* fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
	for (size_t i = 1; i < repeat; i++) {
		void* base = (uint8*)fifo + size * i;
		s_Next[i] = (uint8*)MapViewOfFileEx(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size, base);
		errorID = ::GetLastError();
		if (s_Next[i] != base) {
			mmap_segment_failed++;
			if (mmap_segment_failed > 4) {
				fprintf(stderr, "Memory mapping failed after %d attempts, aborting. WIN API ERROR:%u\n", mmap_segment_failed, errorID);
				fifo_free(fifo, size, repeat);
				return vmalloc(size * repeat, false); // Fallback to default vmalloc
			}
			do {
				UnmapViewOfFile(s_Next[i]);
				s_Next[i] = 0;
			} while (--i > 0);

			fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
		}
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_fh != NULL);

	if (s_fh == NULL) {
		if (ptr != NULL)
			vmfree(ptr, size);
		return;
	}

	UnmapViewOfFile(ptr);

	for (size_t i = 1; i < countof(s_Next); i++) {
		if (s_Next[i] != 0) {
			UnmapViewOfFile(s_Next[i]);
			s_Next[i] = 0;
		}
	}

	CloseHandle(s_fh);
	s_fh = NULL;
}

#else

#include <sys/mman.h>
#include <unistd.h>

void* vmalloc(size_t size, bool code)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if(code) {
		prot |= PROT_EXEC;
#if defined(_M_AMD64) && !defined(__APPLE__)
		// macOS doesn't allow any mappings in the first 4GB of address space
		flags |= MAP_32BIT;
#endif
	}

	return mmap(NULL, size, prot, flags, -1, 0);
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
	ASSERT(s_shm_fd == -1);

	const char* file_name = "/GSDX.mem";
	s_shm_fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (s_shm_fd != -1) {
		shm_unlink(file_name); // file is deleted but descriptor is still open
	} else {
		fprintf(stderr, "Failed to open %s due to %s\n", file_name, strerror(errno));
		return nullptr;
	}

	if (ftruncate(s_shm_fd, repeat * size) < 0)
		fprintf(stderr, "Failed to reserve memory due to %s\n", strerror(errno));

	void* fifo = mmap(nullptr, size * repeat, PROT_READ | PROT_WRITE, MAP_SHARED, s_shm_fd, 0);

	for (size_t i = 1; i < repeat; i++) {
		void* base = (uint8*)fifo + size * i;
		uint8* next = (uint8*)mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, s_shm_fd, 0);
		if (next != base)
			fprintf(stderr, "Fail to mmap contiguous segment\n");
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
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
