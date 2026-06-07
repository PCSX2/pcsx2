#ifndef RC_UTIL_H
#define RC_UTIL_H

#include "rc_export.h"

#include <stddef.h>
#include <stdint.h>

RC_BEGIN_C_DECLS

/**
 * A block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_buffer_chunk_t {
  /* The current location where data is being written */
  uint8_t* write;
  /* The first byte past the end of data where writing cannot occur */
  uint8_t* end;
  /* The first byte of the data */
  uint8_t* start;
  /* The next block in the allocated memory chain */
  struct rc_buffer_chunk_t* next;
}
rc_buffer_chunk_t;

/**
 * A preallocated block of memory for variable length data (like strings and arrays).
 */
typedef struct rc_buffer_t {
  /* The chunk data (will point at the local data member) */
  struct rc_buffer_chunk_t chunk;
  /* Small chunk of memory pre-allocated for the chunk */
  uint8_t data[256];
}
rc_buffer_t;

void rc_buffer_init(rc_buffer_t* buffer);
void rc_buffer_destroy(rc_buffer_t* buffer);
uint8_t* rc_buffer_reserve(rc_buffer_t* buffer, size_t amount);
void rc_buffer_consume(rc_buffer_t* buffer, const uint8_t* start, uint8_t* end);
void* rc_buffer_alloc(rc_buffer_t* buffer, size_t amount);
char* rc_buffer_strcpy(rc_buffer_t* buffer, const char* src);
char* rc_buffer_strncpy(rc_buffer_t* buffer, const char* src, size_t len);

uint32_t rc_djb2(const char* input);

void rc_format_md5(char checksum[33], const uint8_t digest[16]);

RC_END_C_DECLS

#endif /* RC_UTIL_H */
