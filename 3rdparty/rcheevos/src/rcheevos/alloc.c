#include "rc_internal.h"

#include <stdlib.h>
#include <string.h>

void* rc_alloc_scratch(void* pointer, int32_t* offset, uint32_t size, uint32_t alignment, rc_scratch_t* scratch, uint32_t scratch_object_pointer_offset)
{
  void* data;

  /* if we have a real buffer, then allocate the data there */
  if (pointer)
    return rc_alloc(pointer, offset, size, alignment, NULL, scratch_object_pointer_offset);

  /* update how much space will be required in the real buffer */
  {
    const int32_t aligned_offset = (*offset + alignment - 1) & ~(alignment - 1);
    *offset += (aligned_offset - *offset);
    *offset += size;
  }

  /* find a scratch buffer to hold the temporary data */
  data = rc_buffer_alloc(&scratch->buffer, size);
  if (!data) {
    *offset = RC_OUT_OF_MEMORY;
    return NULL;
  }

  return data;
}

void* rc_alloc(void* pointer, int32_t* offset, uint32_t size, uint32_t alignment, rc_scratch_t* scratch, uint32_t scratch_object_pointer_offset) {
  void* ptr;

  *offset = (*offset + alignment - 1) & ~(alignment - 1);

  if (pointer != 0) {
    /* valid buffer, grab the next chunk */
    ptr = (void*)((char*)pointer + *offset);
  }
  else if (scratch != 0 && scratch_object_pointer_offset < sizeof(scratch->objs)) {
    /* only allocate one instance of each object type (indentified by scratch_object_pointer_offset) */
    void** scratch_object_pointer = (void**)((char*)&scratch->objs + scratch_object_pointer_offset);
    ptr = *scratch_object_pointer;
    if (!ptr) {
      int32_t used;
      ptr = *scratch_object_pointer = rc_alloc_scratch(NULL, &used, size, alignment, scratch, -1);
    }
  }
  else {
    /* nowhere to get memory from, return NULL */
    ptr = NULL;
  }

  *offset += size;
  return ptr;
}

char* rc_alloc_str(rc_parse_state_t* parse, const char* text, size_t length) {
  int32_t used = 0;
  char* ptr;

  rc_scratch_string_t** next = &parse->scratch.strings;
  while (*next) {
    int diff = strncmp(text, (*next)->value, length);
    if (diff == 0) {
      diff = (*next)->value[length];
      if (diff == 0)
        return (*next)->value;
    }

    if (diff < 0)
      next = &(*next)->left;
    else
      next = &(*next)->right;
  }

  *next = (rc_scratch_string_t*)rc_alloc_scratch(NULL, &used, sizeof(rc_scratch_string_t), RC_ALIGNOF(rc_scratch_string_t), &parse->scratch, RC_OFFSETOF(parse->scratch.objs, __rc_scratch_string_t));
  ptr = (char*)rc_alloc_scratch(parse->buffer, &parse->offset, (uint32_t)length + 1, RC_ALIGNOF(char), &parse->scratch, -1);

  if (!ptr || !*next) {
    if (parse->offset >= 0)
      parse->offset = RC_OUT_OF_MEMORY;

    return NULL;
  }

  memcpy(ptr, text, length);
  ptr[length] = '\0';

  (*next)->left = NULL;
  (*next)->right = NULL;
  (*next)->value = ptr;

  return ptr;
}

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer, lua_State* L, int funcs_ndx)
{
  /* could use memset here, but rc_parse_state_t contains a 512 byte buffer that doesn't need to be initialized */
  parse->offset = 0;
  parse->L = L;
  parse->funcs_ndx = funcs_ndx;
  parse->buffer = buffer;
  parse->scratch.strings = NULL;
  rc_buffer_init(&parse->scratch.buffer);
  memset(&parse->scratch.objs, 0, sizeof(parse->scratch.objs));
  parse->first_memref = 0;
  parse->variables = 0;
  parse->measured_target = 0;
  parse->lines_read = 0;
  parse->has_required_hits = 0;
  parse->measured_as_percent = 0;
}

void rc_destroy_parse_state(rc_parse_state_t* parse)
{
  rc_buffer_destroy(&parse->scratch.buffer);
}
