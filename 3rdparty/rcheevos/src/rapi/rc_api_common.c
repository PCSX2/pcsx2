#include "rc_api_common.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"

#include "../rc_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETROACHIEVEMENTS_HOST "https://retroachievements.org"
#define RETROACHIEVEMENTS_IMAGE_HOST "https://media.retroachievements.org"
#define RETROACHIEVEMENTS_HOST_NONSSL "http://retroachievements.org"
#define RETROACHIEVEMENTS_IMAGE_HOST_NONSSL "http://media.retroachievements.org"
static char* g_host = NULL;
static char* g_imagehost = NULL;

/* --- rc_json --- */

static int rc_json_parse_object(rc_json_iterator_t* iterator, rc_json_field_t* fields, size_t field_count, uint32_t* fields_seen);
static int rc_json_parse_array(rc_json_iterator_t* iterator, rc_json_field_t* field);

static int rc_json_match_char(rc_json_iterator_t* iterator, char c)
{
  if (iterator->json < iterator->end && *iterator->json == c) {
    ++iterator->json;
    return 1;
  }

  return 0;
}

static void rc_json_skip_whitespace(rc_json_iterator_t* iterator)
{
  while (iterator->json < iterator->end && isspace((unsigned char)*iterator->json))
    ++iterator->json;
}

static int rc_json_find_closing_quote(rc_json_iterator_t* iterator)
{
  while (iterator->json < iterator->end) {
    if (*iterator->json == '"')
      return 1;

    if (*iterator->json == '\\') {
      ++iterator->json;
      if (iterator->json == iterator->end)
        return 0;
    }

    if (*iterator->json == '\0')
      return 0;

    ++iterator->json;
  }

  return 0;
}

static int rc_json_parse_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  int result;

  if (iterator->json >= iterator->end)
    return RC_INVALID_JSON;

  field->value_start = iterator->json;

  switch (*iterator->json)
  {
    case '"': /* quoted string */
      ++iterator->json;
      if (!rc_json_find_closing_quote(iterator))
        return RC_INVALID_JSON;
      ++iterator->json;
      break;

    case '-':
    case '+': /* signed number */
      ++iterator->json;
      /* fallthrough to number */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': /* number */
      while (iterator->json < iterator->end && *iterator->json >= '0' && *iterator->json <= '9')
        ++iterator->json;

      if (rc_json_match_char(iterator, '.')) {
        while (iterator->json < iterator->end && *iterator->json >= '0' && *iterator->json <= '9')
          ++iterator->json;
      }
      break;

    case '[': /* array */
      result = rc_json_parse_array(iterator, field);
      if (result != RC_OK)
        return result;

      break;

    case '{': /* object */
      result = rc_json_parse_object(iterator, NULL, 0, &field->array_size);
      if (result != RC_OK)
        return result;

      break;

    default: /* non-quoted text [true,false,null] */
      if (!isalpha((unsigned char)*iterator->json))
        return RC_INVALID_JSON;

      while (iterator->json < iterator->end && isalnum((unsigned char)*iterator->json))
        ++iterator->json;
      break;
  }

  field->value_end = iterator->json;
  return RC_OK;
}

static int rc_json_parse_array(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  rc_json_field_t unused_field;
  int result;

  if (!rc_json_match_char(iterator, '['))
    return RC_INVALID_JSON;

  field->array_size = 0;

  if (rc_json_match_char(iterator, ']')) /* empty array */
    return RC_OK;

  do
  {
    rc_json_skip_whitespace(iterator);

    result = rc_json_parse_field(iterator, &unused_field);
    if (result != RC_OK)
      return result;

    ++field->array_size;

    rc_json_skip_whitespace(iterator);
  } while (rc_json_match_char(iterator, ','));

  if (!rc_json_match_char(iterator, ']'))
    return RC_INVALID_JSON;

  return RC_OK;
}

static int rc_json_get_next_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, '"'))
    return RC_INVALID_JSON;

  field->name = iterator->json;
  while (iterator->json < iterator->end && *iterator->json != '"') {
    if (!*iterator->json)
      return RC_INVALID_JSON;
    ++iterator->json;
  }

  if (iterator->json == iterator->end)
    return RC_INVALID_JSON;

  field->name_len = iterator->json - field->name;
  ++iterator->json;

  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, ':'))
    return RC_INVALID_JSON;

  rc_json_skip_whitespace(iterator);

  if (rc_json_parse_field(iterator, field) < 0)
    return RC_INVALID_JSON;

  rc_json_skip_whitespace(iterator);

  return RC_OK;
}

static int rc_json_parse_object(rc_json_iterator_t* iterator, rc_json_field_t* fields, size_t field_count, uint32_t* fields_seen) {
  size_t i;
  uint32_t num_fields = 0;
  rc_json_field_t field;
  int result;

  if (fields_seen)
    *fields_seen = 0;

  for (i = 0; i < field_count; ++i)
    fields[i].value_start = fields[i].value_end = NULL;

  if (!rc_json_match_char(iterator, '{'))
    return RC_INVALID_JSON;

  if (rc_json_match_char(iterator, '}')) /* empty object */
    return RC_OK;

  do
  {
    result = rc_json_get_next_field(iterator, &field);
    if (result != RC_OK)
      return result;

    for (i = 0; i < field_count; ++i) {
      if (!fields[i].value_start && fields[i].name_len == field.name_len &&
          memcmp(fields[i].name, field.name, field.name_len) == 0) {
        fields[i].value_start = field.value_start;
        fields[i].value_end = field.value_end;
        fields[i].array_size = field.array_size;
        break;
      }
    }

    ++num_fields;

  } while (rc_json_match_char(iterator, ','));

  if (!rc_json_match_char(iterator, '}'))
    return RC_INVALID_JSON;

  if (fields_seen)
    *fields_seen = num_fields;

  return RC_OK;
}

int rc_json_get_next_object_field(rc_json_iterator_t* iterator, rc_json_field_t* field) {
  if (!rc_json_match_char(iterator, ',') && !rc_json_match_char(iterator, '{'))
    return 0;

  return (rc_json_get_next_field(iterator, field) == RC_OK);
}

int rc_json_get_object_string_length(const char* json) {
  const char* json_start = json;

  rc_json_iterator_t iterator;
  memset(&iterator, 0, sizeof(iterator));
  iterator.json = json;
  iterator.end = json + (1024 * 1024 * 1024); /* arbitrary 1GB limit on JSON response */

  rc_json_parse_object(&iterator, NULL, 0, NULL);

  return (int)(iterator.json - json_start);
}

static int rc_json_extract_html_error(rc_api_response_t* response, const rc_api_server_response_t* server_response) {
  const char* json = server_response->body;
  const char* end = json;

  const char* title_start = strstr(json, "<title>");
  if (title_start) {
    title_start += 7;
    if (isdigit((int)*title_start)) {
      const char* title_end = strstr(title_start + 7, "</title>");
      if (title_end) {
        response->error_message = rc_buffer_strncpy(&response->buffer, title_start, title_end - title_start);
        response->succeeded = 0;
        return RC_INVALID_JSON;
      }
    }
  }

  while (*end && *end != '\n' && end - json < 200)
    ++end;

  if (end > json && end[-1] == '\r')
    --end;

  if (end > json)
    response->error_message = rc_buffer_strncpy(&response->buffer, json, end - json);

  response->succeeded = 0;
  return RC_INVALID_JSON;
}

static int rc_json_convert_error_code(const char* server_error_code)
{
  switch (server_error_code[0]) {
    case 'a':
      if (strcmp(server_error_code, "access_denied") == 0)
        return RC_ACCESS_DENIED;
      break;

    case 'e':
      if (strcmp(server_error_code, "expired_token") == 0)
        return RC_EXPIRED_TOKEN;
      break;

    case 'i':
      if (strcmp(server_error_code, "invalid_credentials") == 0)
        return RC_INVALID_CREDENTIALS;
      break;

    default:
      break;
  }

  return RC_API_FAILURE;
}

int rc_json_parse_server_response(rc_api_response_t* response, const rc_api_server_response_t* server_response, rc_json_field_t* fields, size_t field_count) {
  int result;

#ifndef NDEBUG
  if (field_count < 2)
    return RC_INVALID_STATE;
  if (strcmp(fields[0].name, "Success") != 0)
    return RC_INVALID_STATE;
  if (strcmp(fields[1].name, "Error") != 0)
    return RC_INVALID_STATE;
#endif

  response->error_message = NULL;

  if (!server_response) {
    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (server_response->http_status_code == RC_API_SERVER_RESPONSE_CLIENT_ERROR ||
      server_response->http_status_code == RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR) {
    /* client provided error message is passed as the response body */
    response->error_message = server_response->body;
    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (!server_response->body || !*server_response->body) {
    /* expect valid HTTP status codes to have bodies that we can extract the message from,
     * but provide some default messages in case they don't. */
    switch (server_response->http_status_code) {
      case 504: /* 504 Gateway Timeout */
      case 522: /* 522 Connection Timed Out */
      case 524: /* 524 A Timeout Occurred */
        response->error_message = "Request has timed out.";
        break;

      case 521: /* 521 Web Server is Down */
      case 523: /* 523 Origin is Unreachable */
        response->error_message = "Could not connect to server.";
        break;

      default:
        break;
    }

    response->succeeded = 0;
    return RC_NO_RESPONSE;
  }

  if (*server_response->body != '{') {
    result = rc_json_extract_html_error(response, server_response);
  }
  else {
    rc_json_iterator_t iterator;
    memset(&iterator, 0, sizeof(iterator));
    iterator.json = server_response->body;
    iterator.end = server_response->body + server_response->body_length;
    result = rc_json_parse_object(&iterator, fields, field_count, NULL);

    rc_json_get_optional_string(&response->error_message, response, &fields[1], "Error", NULL);
    rc_json_get_optional_bool(&response->succeeded, &fields[0], "Success", 1);

    /* Code will be the third field in the fields array, but may not always be present */
    if (field_count > 2 && strcmp(fields[2].name, "Code") == 0) {
      rc_json_get_optional_string(&response->error_code, response, &fields[2], "Code", NULL);
      if (response->error_code != NULL)
        result = rc_json_convert_error_code(response->error_code);
    }
  }

  return result;
}

static int rc_json_missing_field(rc_api_response_t* response, const rc_json_field_t* field) {
  const char* not_found = " not found in response";
  const size_t not_found_len = strlen(not_found);
  const size_t field_len = strlen(field->name);

  uint8_t* write = rc_buffer_reserve(&response->buffer, field_len + not_found_len + 1);
  if (write) {
    response->error_message = (char*)write;
    memcpy(write, field->name, field_len);
    write += field_len;
    memcpy(write, not_found, not_found_len + 1);
    write += not_found_len + 1;
    rc_buffer_consume(&response->buffer, (uint8_t*)response->error_message, write);
  }

  response->succeeded = 0;
  return 0;
}

int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name) {
  rc_json_iterator_t iterator;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!field->value_start)
    return rc_json_missing_field(response, field);

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = field->value_start;
  iterator.end = field->value_end;
  return (rc_json_parse_object(&iterator, fields, field_count, &field->array_size) == RC_OK);
}

static int rc_json_get_array_entry_value(rc_json_field_t* field, rc_json_iterator_t* iterator) {
  rc_json_skip_whitespace(iterator);

  if (iterator->json >= iterator->end)
    return 0;

  if (rc_json_parse_field(iterator, field) != RC_OK)
    return 0;

  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, ','))
    rc_json_match_char(iterator, ']');

  return 1;
}

int rc_json_get_required_unum_array(uint32_t** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_iterator_t iterator;
  rc_json_field_t array;
  rc_json_field_t value;
  uint32_t* entry;

  memset(&array, 0, sizeof(array));
  if (!rc_json_get_required_array(num_entries, &array, response, field, field_name))
    return RC_MISSING_VALUE;

  if (*num_entries) {
    *entries = (uint32_t*)rc_buffer_alloc(&response->buffer, *num_entries * sizeof(uint32_t));
    if (!*entries)
      return RC_OUT_OF_MEMORY;

    value.name = field_name;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array.value_start;
    iterator.end = array.value_end;

    entry = *entries;
    while (rc_json_get_array_entry_value(&value, &iterator)) {
      if (!rc_json_get_unum(entry, &value, field_name))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }
  else {
    *entries = NULL;
  }

  return RC_OK;
}

int rc_json_get_required_array(uint32_t* num_entries, rc_json_field_t* array_field, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!rc_json_get_optional_array(num_entries, array_field, field, field_name))
    return rc_json_missing_field(response, field);

  return 1;
}

int rc_json_get_optional_array(uint32_t* num_entries, rc_json_field_t* array_field, const rc_json_field_t* field, const char* field_name) {
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!field->value_start || *field->value_start != '[') {
    *num_entries = 0;
    return 0;
  }

  memcpy(array_field, field, sizeof(*array_field));
  ++array_field->value_start; /* skip [ */

  *num_entries = field->array_size;
  return 1;
}

int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_iterator_t* iterator) {
  rc_json_skip_whitespace(iterator);

  if (iterator->json >= iterator->end)
    return 0;

  if (rc_json_parse_object(iterator, fields, field_count, NULL) != RC_OK)
    return 0;

  rc_json_skip_whitespace(iterator);

  if (!rc_json_match_char(iterator, ','))
    rc_json_match_char(iterator, ']');

  return 1;
}

static uint32_t rc_json_decode_hex4(const char* input) {
  char hex[5];

  memcpy(hex, input, 4);
  hex[4] = '\0';

  return (uint32_t)strtoul(hex, NULL, 16);
}

static int rc_json_ucs32_to_utf8(uint8_t* dst, uint32_t ucs32_char) {
  if (ucs32_char < 0x80) {
    dst[0] = (ucs32_char & 0x7F);
    return 1;
  }

  if (ucs32_char < 0x0800) {
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xC0 | (ucs32_char & 0x1F);
    return 2;
  }

  if (ucs32_char < 0x010000) {
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xE0 | (ucs32_char & 0x0F);
    return 3;
  }

  if (ucs32_char < 0x200000) {
    dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xF0 | (ucs32_char & 0x07);
    return 4;
  }

  if (ucs32_char < 0x04000000) {
    dst[4] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xF8 | (ucs32_char & 0x03);
    return 5;
  }

  dst[5] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[4] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[0] = 0xFC | (ucs32_char & 0x01);
  return 6;
}

int rc_json_get_string(const char** out, rc_buffer_t* buffer, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  size_t len = field->value_end - field->value_start;
  char* dst;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = NULL;
    return 0;
  }

  if (len == 4 && memcmp(field->value_start, "null", 4) == 0) {
    *out = NULL;
    return 1;
  }

  if (*src == '\"') {
    ++src;

    if (*src == '\"') {
      /* simple optimization for empty string - don't allocate space */
      *out = "";
      return 1;
    }

    *out = dst = (char*)rc_buffer_reserve(buffer, len - 1); /* -2 for quotes, +1 for null terminator */

    do {
      if (*src == '\\') {
        ++src;
        if (*src == 'n') {
          /* newline */
          ++src;
          *dst++ = '\n';
          continue;
        }

        if (*src == 'r') {
          /* carriage return */
          ++src;
          *dst++ = '\r';
          continue;
        }

        if (*src == 'u') {
          /* unicode character */
          uint32_t ucs32_char = rc_json_decode_hex4(src + 1);
          src += 5;

          if (ucs32_char >= 0xD800 && ucs32_char < 0xE000) {
            /* surrogate lead - look for surrogate tail */
            if (ucs32_char < 0xDC00 && src[0] == '\\' && src[1] == 'u') {
              const uint32_t surrogate = rc_json_decode_hex4(src + 2);
              src += 6;

              if (surrogate >= 0xDC00 && surrogate < 0xE000) {
                /* found a surrogate tail, merge them */
                ucs32_char = (((ucs32_char - 0xD800) << 10) | (surrogate - 0xDC00)) + 0x10000;
              }
            }

            if (!(ucs32_char & 0xFFFF0000)) {
              /* invalid surrogate pair, fallback to replacement char */
              ucs32_char = 0xFFFD;
            }
          }

          dst += rc_json_ucs32_to_utf8((unsigned char*)dst, ucs32_char);
          continue;
        }

        if (*src == 't') {
          /* tab */
          ++src;
          *dst++ = '\t';
          continue;
        }

        /* just an escaped character, fallthrough to normal copy */
      }

      *dst++ = *src++;
    } while (*src != '\"');

  } else {
    *out = dst = (char*)rc_buffer_reserve(buffer, len + 1); /* +1 for null terminator */
    memcpy(dst, src, len);
    dst += len;
  }

  *dst++ = '\0';
  rc_buffer_consume(buffer, (uint8_t*)(*out), (uint8_t*)dst);
  return 1;
}

void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value) {
  if (!rc_json_get_string(out, &response->buffer, field, field_name))
    *out = default_value;
}

int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_string(out, &response->buffer, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_num(int32_t* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  int32_t value = 0;
  int negative = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals and an optional sign per rc_json_parse_field */
  if (*src == '-') {
    negative = 1;
    ++src;
  } else if (*src == '+') {
    ++src;
  } else if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  if (negative)
    *out = -value;
  else
    *out = value;

  return 1;
}

void rc_json_get_optional_num(int32_t* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_num(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_num(int32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_num(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  uint32_t value = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals per rc_json_parse_field */
  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  *out = value;
  return 1;
}

void rc_json_get_optional_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name, uint32_t default_value) {
  if (!rc_json_get_unum(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_unum(uint32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_unum(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_float(float* out, const rc_json_field_t* field, const char* field_name) {
  int32_t whole, fraction, fraction_denominator;
  const char* decimal = field->value_start;

  if (!decimal) {
    *out = 0.0f;
    return 0;
  }

  if (!rc_json_get_num(&whole, field, field_name))
    return 0;

  while (decimal < field->value_end && *decimal != '.')
    ++decimal;

  fraction = 0;
  fraction_denominator = 1;
  if (decimal) {
    ++decimal;
    while (decimal < field->value_end && *decimal >= '0' && *decimal <= '9') {
      fraction *= 10;
      fraction += *decimal - '0';
      fraction_denominator *= 10;
      ++decimal;
    }
  }

  if (whole < 0)
    fraction = -fraction;

  *out = (float)whole + ((float)fraction / (float)fraction_denominator);
  return 1;
}

void rc_json_get_optional_float(float* out, const rc_json_field_t* field, const char* field_name, float default_value) {
  if (!rc_json_get_float(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_float(float* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_float(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_datetime(time_t* out, const rc_json_field_t* field, const char* field_name) {
  struct tm tm;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (*field->value_start == '\"') {
    memset(&tm, 0, sizeof(tm));
    if (sscanf_s(field->value_start + 1, "%d-%d-%d %d:%d:%d",
        &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_mon--; /* 0-based */
      tm.tm_year -= 1900; /* 1900 based */

      /* mktime converts a struct tm to a time_t using the local timezone.
       * the input string is UTC. since timegm is not universally cross-platform,
       * figure out the offset between UTC and local time by applying the
       * timezone conversion twice and manually removing the difference */
      {
         time_t local_timet = mktime(&tm);
         time_t skewed_timet, tz_offset;
         struct tm gmt_tm;
         gmtime_s(&gmt_tm, &local_timet);
         skewed_timet = mktime(&gmt_tm); /* applies local time adjustment second time */
         tz_offset = skewed_timet - local_timet;
         *out = local_timet - tz_offset;
      }

      return 1;
    }
  }

  *out = 0;
  return 0;
}

int rc_json_get_required_datetime(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_datetime(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#else
  (void)field_name;
#endif

  if (src) {
    const size_t len = field->value_end - field->value_start;
    if (len == 4 && strncasecmp(src, "true", 4) == 0) {
      *out = 1;
      return 1;
    } else if (len == 5 && strncasecmp(src, "false", 5) == 0) {
      *out = 0;
      return 1;
    } else if (len == 1) {
      *out = (*src != '0');
      return 1;
    }
  }

  *out = 0;
  return 0;
}

void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_bool(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_bool(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

/* --- rc_api_request --- */

void rc_api_destroy_request(rc_api_request_t* request)
{
  rc_buffer_destroy(&request->buffer);
}

/* --- rc_url_builder --- */

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_buffer_t* buffer, size_t estimated_size) {
  rc_buffer_chunk_t* used_buffer;

  memset(builder, 0, sizeof(*builder));
  builder->buffer = buffer;
  builder->write = builder->start = (char*)rc_buffer_reserve(buffer, estimated_size);

  used_buffer = &buffer->chunk;
  while (used_buffer && used_buffer->write != (uint8_t*)builder->write)
    used_buffer = used_buffer->next;

  builder->end = (used_buffer) ? (char*)used_buffer->end : builder->start + estimated_size;
}

const char* rc_url_builder_finalize(rc_api_url_builder_t* builder) {
  rc_url_builder_append(builder, "", 1);

  if (builder->result != RC_OK)
    return NULL;

  rc_buffer_consume(builder->buffer, (uint8_t*)builder->start, (uint8_t*)builder->write);
  return builder->start;
}

static int rc_url_builder_reserve(rc_api_url_builder_t* builder, size_t amount) {
  if (builder->result == RC_OK) {
    size_t remaining = builder->end - builder->write;
    if (remaining < amount) {
      const size_t used = builder->write - builder->start;
      const size_t current_size = builder->end - builder->start;
      const size_t buffer_prefix_size = sizeof(rc_buffer_chunk_t);
      char* new_start;
      size_t new_size = (current_size < 256) ? 256 : current_size * 2;
      do {
        remaining = new_size - used;
        if (remaining >= amount)
          break;

        new_size *= 2;
      } while (1);

      /* rc_buffer_reserve will align to 256 bytes after including the buffer prefix. attempt to account for that */
      if ((remaining - amount) > buffer_prefix_size)
        new_size -= buffer_prefix_size;

      new_start = (char*)rc_buffer_reserve(builder->buffer, new_size);
      if (!new_start) {
        builder->result = RC_OUT_OF_MEMORY;
        return RC_OUT_OF_MEMORY;
      }

      if (new_start != builder->start) {
        memcpy(new_start, builder->start, used);
        builder->start = new_start;
        builder->write = new_start + used;
      }

      builder->end = builder->start + new_size;
    }
  }

  return builder->result;
}

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str) {
  static const char hex[] = "0123456789abcdef";
  const char* start = str;
  size_t len = 0;
  for (;;) {
    const char c = *str++;
    switch (c) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        len++;
        continue;

      case '\0':
        if (len)
          rc_url_builder_append(builder, start, len);

        return;

      default:
        if (rc_url_builder_reserve(builder, len + 3) != RC_OK)
          return;

        if (len) {
          memcpy(builder->write, start, len);
          builder->write += len;
        }

        if (c == ' ') {
          *builder->write++ = '+';
        } else {
          *builder->write++ = '%';
          *builder->write++ = hex[((unsigned char)c) >> 4];
          *builder->write++ = hex[c & 0x0F];
        }
        break;
    }

    start = str;
    len = 0;
  }
}

void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len) {
  if (rc_url_builder_reserve(builder, len) == RC_OK) {
    memcpy(builder->write, data, len);
    builder->write += len;
  }
}

static int rc_url_builder_append_param_equals(rc_api_url_builder_t* builder, const char* param) {
  size_t param_len = strlen(param);

  if (rc_url_builder_reserve(builder, param_len + 2) == RC_OK) {
    if (builder->write > builder->start) {
      if (builder->write[-1] != '?')
        *builder->write++ = '&';
    }

    memcpy(builder->write, param, param_len);
    builder->write += param_len;
    *builder->write++ = '=';
  }

  return builder->result;
}

void rc_url_builder_append_unum_param(rc_api_url_builder_t* builder, const char* param, uint32_t value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%u", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, int32_t value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%d", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value) {
  rc_url_builder_append_param_equals(builder, param);
  rc_url_builder_append_encoded_str(builder, value);
}

void rc_api_url_build_dorequest_url(rc_api_request_t* request) {
  #define DOREQUEST_ENDPOINT "/dorequest.php"
  rc_buffer_init(&request->buffer);

  if (!g_host) {
    request->url = RETROACHIEVEMENTS_HOST DOREQUEST_ENDPOINT;
  }
  else {
    const size_t endpoint_len = sizeof(DOREQUEST_ENDPOINT);
    const size_t host_len = strlen(g_host);
    const size_t url_len = host_len + endpoint_len;
    uint8_t* url = rc_buffer_reserve(&request->buffer, url_len);

    memcpy(url, g_host, host_len);
    memcpy(url + host_len, DOREQUEST_ENDPOINT, endpoint_len);
    rc_buffer_consume(&request->buffer, url, url + url_len);

    request->url = (char*)url;
  }
  #undef DOREQUEST_ENDPOINT
}

int rc_api_url_build_dorequest(rc_api_url_builder_t* builder, const char* api, const char* username, const char* api_token) {
  if (!username || !*username || !api_token || !*api_token) {
    builder->result = RC_INVALID_STATE;
    return 0;
  }

  rc_url_builder_append_str_param(builder, "r", api);
  rc_url_builder_append_str_param(builder, "u", username);
  rc_url_builder_append_str_param(builder, "t", api_token);

  return (builder->result == RC_OK);
}

/* --- Set Host --- */

static void rc_api_update_host(char** host, const char* hostname) {
  if (*host != NULL)
    free(*host);

  if (hostname != NULL) {
    if (strstr(hostname, "://")) {
      *host = strdup(hostname);
    }
    else {
      const size_t hostname_len = strlen(hostname);
      if (hostname_len == 0) {
        *host = NULL;
      }
      else {
        char* newhost = (char*)malloc(hostname_len + 7 + 1);
        if (newhost) {
          memcpy(newhost, "http://", 7);
          memcpy(&newhost[7], hostname, hostname_len + 1);
          *host = newhost;
        }
        else {
          *host = NULL;
        }
      }
    }
  }
  else {
    *host = NULL;
  }
}

void rc_api_set_host(const char* hostname) {
  rc_api_update_host(&g_host, hostname);

  if (!hostname) {
    /* also clear out the image hostname */
    rc_api_set_image_host(NULL);
  }
  else if (strcmp(hostname, RETROACHIEVEMENTS_HOST_NONSSL) == 0) {
    /* if just pointing at the non-HTTPS host, explicitly use the default image host
     * so it doesn't try to use the web host directly */
    rc_api_set_image_host(RETROACHIEVEMENTS_IMAGE_HOST_NONSSL);
  }
}

void rc_api_set_image_host(const char* hostname) {
  rc_api_update_host(&g_imagehost, hostname);
}

/* --- Fetch Image --- */

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_buffer_init(&request->buffer);
  rc_url_builder_init(&builder, &request->buffer, 64);

  if (g_imagehost) {
    rc_url_builder_append(&builder, g_imagehost, strlen(g_imagehost));
  }
  else if (g_host) {
    rc_url_builder_append(&builder, g_host, strlen(g_host));
  }
  else {
    rc_url_builder_append(&builder, RETROACHIEVEMENTS_IMAGE_HOST, sizeof(RETROACHIEVEMENTS_IMAGE_HOST) - 1);
  }

  switch (api_params->image_type)
  {
    case RC_IMAGE_TYPE_GAME:
      rc_url_builder_append(&builder, "/Images/", 8);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, "_lock.png", 9);
      break;

    case RC_IMAGE_TYPE_USER:
      rc_url_builder_append(&builder, "/UserPic/", 9);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    default:
      return RC_INVALID_STATE;
  }

  request->url = rc_url_builder_finalize(&builder);
  request->post_data = NULL;

  return builder.result;
}
