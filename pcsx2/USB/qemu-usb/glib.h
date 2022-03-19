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

#ifndef GLIB_H
#define GLIB_H
#include <cstddef>
#include <cstdint>

#define G_MAXSIZE G_MAXUINT64

#define G_MAXUINT64 0xffffffffffffffffUL
#define G_MAXUINT32 ((uint32_t)0xffffffff)

void* my_g_malloc0(size_t n_bytes);
void* my_g_malloc(size_t n_bytes);
void* my_g_malloc_n(size_t n_blocks,
					size_t n_block_bytes);
void* my_g_realloc_n(void* mem,
					 size_t n_blocks,
					 size_t n_block_bytes);

#define my_g_free free

#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)

#define my_G_NEW(struct_type, n_structs, func) \
	((struct_type*)my_g_##func##_n((n_structs), sizeof(struct_type)))
#define my_G_RENEW(struct_type, mem, n_structs, func) \
	((struct_type*)my_g_##func##_n(mem, (n_structs), sizeof(struct_type)))

#define my_g_new(struct_type, n_structs) my_G_NEW(struct_type, n_structs, malloc)
#define my_g_renew(struct_type, mem, n_structs) my_G_RENEW(struct_type, mem, n_structs, realloc)

#endif
