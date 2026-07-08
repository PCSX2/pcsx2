/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "cubeb_strings.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CUBEB_STRINGS_INLINE_COUNT 4

struct cubeb_strings {
  uint32_t size;
  uint32_t count;
  char ** data;
  char * small_store[CUBEB_STRINGS_INLINE_COUNT];
};

int
cubeb_strings_init(cubeb_strings ** strings)
{
  cubeb_strings * strs = NULL;

  if (!strings) {
    return CUBEB_ERROR;
  }

  strs = calloc(1, sizeof(cubeb_strings));
  assert(strs);

  if (!strs) {
    return CUBEB_ERROR;
  }

  strs->size = sizeof(strs->small_store) / sizeof(strs->small_store[0]);
  strs->count = 0;
  strs->data = strs->small_store;

  *strings = strs;

  return CUBEB_OK;
}

void
cubeb_strings_destroy(cubeb_strings * strings)
{
  char ** sp = NULL;
  char ** se = NULL;

  if (!strings) {
    return;
  }

  sp = strings->data;
  se = sp + strings->count;

  for (; sp != se; sp++) {
    if (*sp) {
      free(*sp);
    }
  }

  if (strings->data != strings->small_store) {
    free(strings->data);
  }

  free(strings);
}

/** Look for string in string storage.
    @param strings Opaque pointer to interned string storage.
    @param s String to look up.
    @retval Read-only string or NULL if not found. */
static char const *
cubeb_strings_lookup(cubeb_strings * strings, char const * s)
{
  char ** sp = NULL;
  char ** se = NULL;

  if (!strings || !s) {
    return NULL;
  }

  sp = strings->data;
  se = sp + strings->count;

  for (; sp != se; sp++) {
    if (*sp && strcmp(*sp, s) == 0) {
      return *sp;
    }
  }

  return NULL;
}

static char const *
cubeb_strings_push(cubeb_strings * strings, char const * s)
{
  char * is = NULL;

  if (strings->count == strings->size) {
    char ** new_data;
    uint32_t value_size = sizeof(char const *);
    uint32_t new_size = strings->size * 2;
    if (!new_size || value_size > (uint32_t)-1 / new_size) {
      // overflow
      return NULL;
    }

    if (strings->small_store == strings->data) {
      // First time heap allocation.
      new_data = malloc(new_size * value_size);
      if (new_data) {
        memcpy(new_data, strings->small_store, sizeof(strings->small_store));
      }
    } else {
      new_data = realloc(strings->data, new_size * value_size);
    }

    if (!new_data) {
      // out of memory
      return NULL;
    }

    strings->size = new_size;
    strings->data = new_data;
  }

  is = strdup(s);
  strings->data[strings->count++] = is;

  return is;
}

char const *
cubeb_strings_intern(cubeb_strings * strings, char const * s)
{
  char const * is = NULL;

  if (!strings || !s) {
    return NULL;
  }

  is = cubeb_strings_lookup(strings, s);
  if (is) {
    return is;
  }

  return cubeb_strings_push(strings, s);
}
