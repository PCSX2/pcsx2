#include "rc_util.h"

#include "rc_compat.h"
#include "rc_error.h"

#include <stdlib.h>
#include <string.h>

#undef DEBUG_BUFFERS

/* --- rc_buffer --- */

void rc_buffer_init(rc_buffer_t* buffer)
{
  buffer->chunk.write = buffer->chunk.start = &buffer->data[0];
  buffer->chunk.end = &buffer->data[sizeof(buffer->data)];
  buffer->chunk.next = NULL;
  /* leave buffer->data uninitialized */
}

void rc_buffer_destroy(rc_buffer_t* buffer)
{
  rc_buffer_chunk_t* chunk;
#ifdef DEBUG_BUFFERS
  int count = 0;
  int wasted = 0;
  int total = 0;
#endif

  /* first chunk is not allocated. skip it. */
  chunk = buffer->chunk.next;

  /* deallocate any additional buffers */
  while (chunk)
  {
    rc_buffer_chunk_t* next = chunk->next;
#ifdef DEBUG_BUFFERS
    total += (int)(chunk->end - chunk->start);
    wasted += (int)(chunk->end - chunk->write);
    ++count;
#endif
    free(chunk);
    chunk = next;
  }

#ifdef DEBUG_BUFFERS
  printf("-- %d allocated buffers (%d/%d used, %d wasted, %0.2f%% efficiency)\n", count,
    total - wasted, total, wasted, (float)(100.0 - (wasted * 100.0) / total));
#endif
}

uint8_t* rc_buffer_reserve(rc_buffer_t* buffer, size_t amount)
{
  rc_buffer_chunk_t* chunk = &buffer->chunk;
  size_t remaining;
  while (chunk)
  {
    remaining = chunk->end - chunk->write;
    if (remaining >= amount)
      return chunk->write;

    if (!chunk->next)
    {
      /* allocate a chunk of memory that is a multiple of 256-bytes. the first 32 bytes will be associated
       * to the chunk header, and the remaining will be used for data.
       */
      const size_t chunk_header_size = sizeof(rc_buffer_chunk_t);
      const size_t alloc_size = (chunk_header_size + amount + 0xFF) & ~0xFF;
      chunk->next = (rc_buffer_chunk_t*)malloc(alloc_size);
      if (!chunk->next)
        break;

      chunk->next->start = (uint8_t*)chunk->next + chunk_header_size;
      chunk->next->write = chunk->next->start;
      chunk->next->end = (uint8_t*)chunk->next + alloc_size;
      chunk->next->next = NULL;
    }

    chunk = chunk->next;
  }

  return NULL;
}

void rc_buffer_consume(rc_buffer_t* buffer, const uint8_t* start, uint8_t* end)
{
  rc_buffer_chunk_t* chunk = &buffer->chunk;
  do
  {
    if (chunk->write == start)
    {
      size_t offset = (end - chunk->start);
      offset = (offset + 7) & ~7;
      chunk->write = &chunk->start[offset];

      if (chunk->write > chunk->end)
        chunk->write = chunk->end;
      break;
    }

    chunk = chunk->next;
  } while (chunk);
}

void* rc_buffer_alloc(rc_buffer_t* buffer, size_t amount)
{
  uint8_t* ptr = rc_buffer_reserve(buffer, amount);
  rc_buffer_consume(buffer, ptr, ptr + amount);
  return (void*)ptr;
}

char* rc_buffer_strncpy(rc_buffer_t* buffer, const char* src, size_t len)
{
  uint8_t* dst = rc_buffer_reserve(buffer, len + 1);
  memcpy(dst, src, len);
  dst[len] = '\0';
  rc_buffer_consume(buffer, dst, dst + len + 2);
  return (char*)dst;
}

char* rc_buffer_strcpy(rc_buffer_t* buffer, const char* src)
{
  return rc_buffer_strncpy(buffer, src, strlen(src));
}

/* --- other --- */

void rc_format_md5(char checksum[33], const uint8_t digest[16])
{
  snprintf(checksum, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );
}

uint32_t rc_djb2(const char* input)
{
  uint32_t result = 5381;
  char c;

  while ((c = *input++) != '\0')
    result = ((result << 5) + result) + c; /* result = result * 33 + c */

  return result;
}

const char* rc_error_str(int ret)
{
  switch (ret) {
    case RC_OK: return "OK";
    case RC_INVALID_LUA_OPERAND: return "Invalid Lua operand";
    case RC_INVALID_MEMORY_OPERAND: return "Invalid memory operand";
    case RC_INVALID_CONST_OPERAND: return "Invalid constant operand";
    case RC_INVALID_FP_OPERAND: return "Invalid floating-point operand";
    case RC_INVALID_CONDITION_TYPE: return "Invalid condition type";
    case RC_INVALID_OPERATOR: return "Invalid operator";
    case RC_INVALID_REQUIRED_HITS: return "Invalid required hits";
    case RC_DUPLICATED_START: return "Duplicated start condition";
    case RC_DUPLICATED_CANCEL: return "Duplicated cancel condition";
    case RC_DUPLICATED_SUBMIT: return "Duplicated submit condition";
    case RC_DUPLICATED_VALUE: return "Duplicated value expression";
    case RC_DUPLICATED_PROGRESS: return "Duplicated progress expression";
    case RC_MISSING_START: return "Missing start condition";
    case RC_MISSING_CANCEL: return "Missing cancel condition";
    case RC_MISSING_SUBMIT: return "Missing submit condition";
    case RC_MISSING_VALUE: return "Missing value expression";
    case RC_INVALID_LBOARD_FIELD: return "Invalid field in leaderboard";
    case RC_MISSING_DISPLAY_STRING: return "Missing display string";
    case RC_OUT_OF_MEMORY: return "Out of memory";
    case RC_INVALID_VALUE_FLAG: return "Invalid flag in value expression";
    case RC_MISSING_VALUE_MEASURED: return "Missing measured flag in value expression";
    case RC_MULTIPLE_MEASURED: return "Multiple measured targets";
    case RC_INVALID_MEASURED_TARGET: return "Invalid measured target";
    case RC_INVALID_COMPARISON: return "Invalid comparison";
    case RC_INVALID_STATE: return "Invalid state";
    case RC_INVALID_JSON: return "Invalid JSON";
    case RC_API_FAILURE: return "API call failed";
    case RC_LOGIN_REQUIRED: return "Login required";
    case RC_NO_GAME_LOADED: return "No game loaded";
    case RC_HARDCORE_DISABLED: return "Hardcore disabled";
    case RC_ABORTED: return "Aborted";
    case RC_NO_RESPONSE: return "No response";
    case RC_ACCESS_DENIED: return "Access denied";
    case RC_INVALID_CREDENTIALS: return "Invalid credentials";
    case RC_EXPIRED_TOKEN: return "Expired token";
    default: return "Unknown error";
  }
}
