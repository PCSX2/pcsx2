#ifndef GLIB_H
#define GLIB_H
#include <cstddef>
#include <cstdint>
#include "gmem-size.h"

#define G_MAXUINT64    0xffffffffffffffffUL
#define G_MAXUINT32    ((uint32_t)0xffffffff)

void* my_g_malloc0 (size_t n_bytes);
void* my_g_malloc (size_t n_bytes);
void* my_g_malloc_n (size_t n_blocks,
	size_t n_block_bytes);
void* my_g_realloc_n (void* mem,
	size_t    n_blocks,
	size_t    n_block_bytes);

#define my_g_free free

#define G_LIKELY(expr) (expr)
#define G_UNLIKELY(expr) (expr)

#define my_G_NEW(struct_type, n_structs, func) \
        ((struct_type *) my_g_##func##_n ((n_structs), sizeof (struct_type)))
#define my_G_RENEW(struct_type, mem, n_structs, func) \
        ((struct_type *) my_g_##func##_n (mem, (n_structs), sizeof (struct_type)))

#define my_g_new(struct_type, n_structs)			my_G_NEW (struct_type, n_structs, malloc)
#define my_g_renew(struct_type, mem, n_structs)		my_G_RENEW (struct_type, mem, n_structs, realloc)

#endif