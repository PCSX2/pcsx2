/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "glib.h"
#include <cstdlib>
#include <cstring>

#define SIZE_OVERFLOWS(a, b) (G_UNLIKELY((b) > 0 && (a) > G_MAXSIZE / (b)))

/**
 * g_malloc:
 * @n_bytes: the number of bytes to allocate
 * 
 * Allocates @n_bytes bytes of memory.
 * If @n_bytes is 0 it returns %NULL.
 * 
 * Returns: a pointer to the allocated memory
 */
void* my_g_malloc(size_t n_bytes)
{
	if (G_LIKELY(n_bytes))
	{
		void* mem;

		mem = malloc(n_bytes);
		//TRACE (GLIB_MEM_ALLOC((void*) mem, (unsigned int) n_bytes, 0, 0));
		if (mem)
			return mem;

		//g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
		//         G_STRLOC, n_bytes);
	}

	//TRACE(GLIB_MEM_ALLOC((void*) NULL, (int) n_bytes, 0, 0));

	return NULL;
}
/**
 * g_malloc0:
 * @n_bytes: the number of bytes to allocate
 * 
 * Allocates @n_bytes bytes of memory, initialized to 0's.
 * If @n_bytes is 0 it returns %NULL.
 * 
 * Returns: a pointer to the allocated memory
 */
void* my_g_malloc0(size_t n_bytes)
{
	if (G_LIKELY(n_bytes))
	{
		void* mem;

		mem = calloc(1, n_bytes);
		//TRACE (GLIB_MEM_ALLOC((void*) mem, (unsigned int) n_bytes, 1, 0));
		if (mem)
			return mem;

		//g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
		//         G_STRLOC, n_bytes);
	}

	//TRACE(GLIB_MEM_ALLOC((void*) NULL, (int) n_bytes, 1, 0));

	return NULL;
}
/**
 * g_malloc_n:
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_malloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: a pointer to the allocated memory
 */
void* my_g_malloc_n(size_t n_blocks,
					size_t n_block_bytes)
{
	if (SIZE_OVERFLOWS(n_blocks, n_block_bytes))
	{
		//g_error ("%s: overflow allocating %"G_GSIZE_FORMAT"*%"G_GSIZE_FORMAT" bytes",
		//         G_STRLOC, n_blocks, n_block_bytes);
	}

	return my_g_malloc(n_blocks * n_block_bytes);
}

/**
 * g_realloc:
 * @mem: (nullable): the memory to reallocate
 * @n_bytes: new size of the memory in bytes
 * 
 * Reallocates the memory pointed to by @mem, so that it now has space for
 * @n_bytes bytes of memory. It returns the new address of the memory, which may
 * have been moved. @mem may be %NULL, in which case it's considered to
 * have zero-length. @n_bytes may be 0, in which case %NULL will be returned
 * and @mem will be freed unless it is %NULL.
 * 
 * Returns: the new address of the allocated memory
 */
void* my_g_realloc(void* mem,
				   size_t n_bytes)
{
	void* newmem;

	if (G_LIKELY(n_bytes))
	{
		newmem = realloc(mem, n_bytes);
		//TRACE (GLIB_MEM_REALLOC((void*) newmem, (void*)mem, (unsigned int) n_bytes, 0));
		if (newmem)
			return newmem;

		//g_error ("%s: failed to allocate %"G_GSIZE_FORMAT" bytes",
		//         G_STRLOC, n_bytes);
	}

	if (mem)
		free(mem);

	//TRACE (GLIB_MEM_REALLOC((void*) NULL, (void*)mem, 0, 0));

	return NULL;
}

/**
 * g_realloc_n:
 * @mem: (nullable): the memory to reallocate
 * @n_blocks: the number of blocks to allocate
 * @n_block_bytes: the size of each block in bytes
 * 
 * This function is similar to g_realloc(), allocating (@n_blocks * @n_block_bytes) bytes,
 * but care is taken to detect possible overflow during multiplication.
 * 
 * Since: 2.24
 * Returns: the new address of the allocated memory
 */
void* my_g_realloc_n(void* mem,
					 size_t n_blocks,
					 size_t n_block_bytes)
{
	if (SIZE_OVERFLOWS(n_blocks, n_block_bytes))
	{
		//g_error ("%s: overflow allocating %"G_GSIZE_FORMAT"*%"G_GSIZE_FORMAT" bytes",
		//         G_STRLOC, n_blocks, n_block_bytes);
	}

	return my_g_realloc(mem, n_blocks * n_block_bytes);
}
