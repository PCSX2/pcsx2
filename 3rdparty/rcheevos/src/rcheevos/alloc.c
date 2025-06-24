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
    ptr = (void*)((uint8_t*)pointer + *offset);
  }
  else if (scratch != 0 && scratch_object_pointer_offset < sizeof(scratch->objs)) {
    /* only allocate one instance of each object type (indentified by scratch_object_pointer_offset) */
    void** scratch_object_pointer = (void**)((uint8_t*)&scratch->objs + scratch_object_pointer_offset);
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

void rc_init_preparse_state(rc_preparse_state_t* preparse)
{
  rc_init_parse_state(&preparse->parse, NULL);
  rc_init_parse_state_memrefs(&preparse->parse, &preparse->memrefs);
}

void rc_destroy_preparse_state(rc_preparse_state_t* preparse)
{
  rc_destroy_parse_state(&preparse->parse);
}

void rc_preparse_alloc_memrefs(rc_memrefs_t* memrefs, rc_preparse_state_t* preparse)
{
  const uint32_t num_memrefs = rc_memrefs_count_memrefs(&preparse->memrefs);
  const uint32_t num_modified_memrefs = rc_memrefs_count_modified_memrefs(&preparse->memrefs);

  if (preparse->parse.offset < 0)
    return;

  if (memrefs) {
    memset(memrefs, 0, sizeof(*memrefs));
    preparse->parse.memrefs = memrefs;
  }

  if (num_memrefs) {
    rc_memref_t* memref_items = RC_ALLOC_ARRAY(rc_memref_t, num_memrefs, &preparse->parse);

    if (memrefs) {
      memrefs->memrefs.capacity = num_memrefs;
      memrefs->memrefs.items = memref_items;
    }
  }

  if (num_modified_memrefs) {
    rc_modified_memref_t* modified_memref_items =
      RC_ALLOC_ARRAY(rc_modified_memref_t, num_modified_memrefs, &preparse->parse);

    if (memrefs) {
      memrefs->modified_memrefs.capacity = num_modified_memrefs;
      memrefs->modified_memrefs.items = modified_memref_items;
    }
  }

  /* when preparsing, this structure will be allocated at the end. when it's allocated earlier
   * in the buffer, it could be followed by something aligned at 8 bytes. force the offset to
   * an 8-byte boundary */
  if (!memrefs) {
    rc_alloc(preparse->parse.buffer, &preparse->parse.offset, 0, 8, &preparse->parse.scratch, 0);
  }
}

static uint32_t rc_preparse_array_size(uint32_t needed, uint32_t minimum)
{
  while (minimum < needed)
    minimum <<= 1;

  return minimum;
}

void rc_preparse_reserve_memrefs(rc_preparse_state_t* preparse, rc_memrefs_t* memrefs)
{
  uint32_t num_memrefs = rc_memrefs_count_memrefs(&preparse->memrefs);
  uint32_t num_modified_memrefs = rc_memrefs_count_modified_memrefs(&preparse->memrefs);
  uint32_t available;

  if (preparse->parse.offset < 0)
    return;

  if (num_memrefs) {
    rc_memref_list_t* memref_list = &memrefs->memrefs;
    while (memref_list->count == memref_list->capacity) {
      if (!memref_list->next)
        break;

      memref_list = memref_list->next;
    }

    available = memref_list->capacity - memref_list->count;
    if (available < num_memrefs) {
      rc_memref_list_t* new_memref_list = (rc_memref_list_t*)calloc(1, sizeof(rc_memref_list_t));
      if (!new_memref_list)
        return;

      new_memref_list->capacity = rc_preparse_array_size(num_memrefs - available, 16);
      new_memref_list->items = (rc_memref_t*)malloc(new_memref_list->capacity * sizeof(rc_memref_t));
      new_memref_list->allocated = 1;
      memref_list->next = new_memref_list;
    }
  }

  if (num_modified_memrefs) {
    rc_modified_memref_list_t* modified_memref_list = &memrefs->modified_memrefs;
    while (modified_memref_list->count == modified_memref_list->capacity) {
      if (!modified_memref_list->next)
        break;

      modified_memref_list = modified_memref_list->next;
    }

    available = modified_memref_list->capacity - modified_memref_list->count;
    if (available < num_modified_memrefs) {
      rc_modified_memref_list_t* new_modified_memref_list = (rc_modified_memref_list_t*)calloc(1, sizeof(rc_modified_memref_list_t));
      if (!new_modified_memref_list)
        return;

      new_modified_memref_list->capacity = rc_preparse_array_size(num_modified_memrefs - available, 8);
      new_modified_memref_list->items = (rc_modified_memref_t*)malloc(new_modified_memref_list->capacity * sizeof(rc_modified_memref_t));
      new_modified_memref_list->allocated = 1;
      modified_memref_list->next = new_modified_memref_list;
    }
  }

  preparse->parse.memrefs = memrefs;
}

static void rc_preparse_sync_operand(rc_operand_t* operand, rc_parse_state_t* parse, const rc_memrefs_t* memrefs)
{
  if (rc_operand_is_memref(operand) || rc_operand_is_recall(operand)) {
    const rc_memref_t* src_memref = operand->value.memref;

    if (src_memref->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
      const rc_modified_memref_list_t* modified_memref_list = &memrefs->modified_memrefs;
      for (; modified_memref_list; modified_memref_list = modified_memref_list->next) {
        const rc_modified_memref_t* modified_memref = modified_memref_list->items;
        const rc_modified_memref_t* modified_memref_end = modified_memref + modified_memref_list->count;

        for (; modified_memref < modified_memref_end; ++modified_memref) {
          if ((const rc_modified_memref_t*)src_memref == modified_memref) {
            rc_modified_memref_t* dst_modified_memref = rc_alloc_modified_memref(parse, modified_memref->memref.value.size,
              &modified_memref->parent, modified_memref->modifier_type, &modified_memref->modifier);

            operand->value.memref = &dst_modified_memref->memref;
            return;
          }
        }
      }
    }
    else {
      const rc_memref_list_t* memref_list = &memrefs->memrefs;
      for (; memref_list; memref_list = memref_list->next) {
        const rc_memref_t* memref = memref_list->items;
        const rc_memref_t* memref_end = memref + memref_list->count;

        for (; memref < memref_end; ++memref) {
          if (src_memref == memref) {
            operand->value.memref = rc_alloc_memref(parse, memref->address, memref->value.size);
            return;
          }
        }
      }
    }
  }
}

void rc_preparse_copy_memrefs(rc_parse_state_t* parse, rc_memrefs_t* memrefs)
{
  const rc_memref_list_t* memref_list = &memrefs->memrefs;
  const rc_modified_memref_list_t* modified_memref_list = &memrefs->modified_memrefs;

  for (; memref_list; memref_list = memref_list->next) {
    const rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_end = memref + memref_list->count;

    for (; memref < memref_end; ++memref)
      rc_alloc_memref(parse, memref->address, memref->value.size);
  }

  for (; modified_memref_list; modified_memref_list = modified_memref_list->next) {
    rc_modified_memref_t* modified_memref = modified_memref_list->items;
    const rc_modified_memref_t* modified_memref_end = modified_memref + modified_memref_list->count;

    for (; modified_memref < modified_memref_end; ++modified_memref) {
      rc_preparse_sync_operand(&modified_memref->parent, parse, memrefs);
      rc_preparse_sync_operand(&modified_memref->modifier, parse, memrefs);

      rc_alloc_modified_memref(parse, modified_memref->memref.value.size,
        &modified_memref->parent, modified_memref->modifier_type, &modified_memref->modifier);
    }
  }
}

void rc_reset_parse_state(rc_parse_state_t* parse, void* buffer)
{
  parse->buffer = buffer;

  parse->offset = 0;
  parse->memrefs = NULL;
  parse->existing_memrefs = NULL;
  parse->variables = NULL;
  parse->measured_target = 0;
  parse->lines_read = 0;
  parse->addsource_oper = RC_OPERATOR_NONE;
  parse->addsource_parent.type = RC_OPERAND_NONE;
  parse->indirect_parent.type = RC_OPERAND_NONE;
  parse->remember.type = RC_OPERAND_NONE;
  parse->is_value = 0;
  parse->has_required_hits = 0;
  parse->measured_as_percent = 0;
  parse->ignore_non_parse_errors = 0;

  parse->scratch.strings = NULL;
}

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer)
{
  /* could use memset here, but rc_parse_state_t contains a 512 byte buffer that doesn't need to be initialized */
  rc_buffer_init(&parse->scratch.buffer);
  memset(&parse->scratch.objs, 0, sizeof(parse->scratch.objs));

  rc_reset_parse_state(parse, buffer);
}

void rc_destroy_parse_state(rc_parse_state_t* parse)
{
  rc_buffer_destroy(&parse->scratch.buffer);
}
