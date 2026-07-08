#include "rc_internal.h"

#include "../rc_compat.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

int rc_parse_format(const char* format_str) {
  switch (*format_str++) {
    case 'F':
      if (!strcmp(format_str, "RAMES")) {
        return RC_FORMAT_FRAMES;
      }
      if (!strncmp(format_str, "LOAT", 4) && format_str[4] >= '1' && format_str[4] <= '6' && format_str[5] == '\0') {
        return RC_FORMAT_FLOAT1 + (format_str[4] - '1');
      }
      if (!strncmp(format_str, "IXED", 4) && format_str[4] >= '1' && format_str[4] <= '3' && format_str[5] == '\0') {
        return RC_FORMAT_FIXED1 + (format_str[4] - '1');
      }

      break;

    case 'T':
      if (!strcmp(format_str, "IME")) {
        return RC_FORMAT_FRAMES;
      }
      if (!strcmp(format_str, "IMESECS")) {
        return RC_FORMAT_SECONDS;
      }
      if (!strcmp(format_str, "HOUSANDS")) {
        return RC_FORMAT_THOUSANDS;
      }
      if (!strcmp(format_str, "ENS")) {
        return RC_FORMAT_TENS;
      }

      break;

    case 'S':
      if (!strcmp(format_str, "ECS")) {
        return RC_FORMAT_SECONDS;
      }
      if (!strcmp(format_str, "CORE")) {
        return RC_FORMAT_SCORE;
      }
      if (!strcmp(format_str, "ECS_AS_MINS")) {
        return RC_FORMAT_SECONDS_AS_MINUTES;
      }

      break;

    case 'M':
      if (!strcmp(format_str, "ILLISECS")) {
        return RC_FORMAT_CENTISECS;
      }
      if (!strcmp(format_str, "INUTES")) {
        return RC_FORMAT_MINUTES;
      }

      break;

    case 'P':
      if (!strcmp(format_str, "OINTS")) {
        return RC_FORMAT_SCORE;
      }

      break;

    case 'V':
      if (!strcmp(format_str, "ALUE")) {
        return RC_FORMAT_VALUE;
      }

      break;

    case 'U':
      if (!strcmp(format_str, "NSIGNED")) {
        return RC_FORMAT_UNSIGNED_VALUE;
      }

      break;

    case 'O':
      if (!strcmp(format_str, "THER")) {
        return RC_FORMAT_SCORE;
      }

      break;

    case 'H':
      if (!strcmp(format_str, "UNDREDS")) {
        return RC_FORMAT_HUNDREDS;
      }

      break;
  }

  return RC_FORMAT_VALUE;
}

static int rc_format_value_minutes(char* buffer, size_t size, uint32_t minutes) {
  uint32_t hours;

    hours = minutes / 60;
    minutes -= hours * 60;
    return snprintf(buffer, size, "%uh%02u", hours, minutes);
}

static int rc_format_value_seconds(char* buffer, size_t size, uint32_t seconds) {
  uint32_t hours, minutes;

  /* apply modulus math to split the seconds into hours/minutes/seconds */
  minutes = seconds / 60;
  seconds -= minutes * 60;
  if (minutes < 60) {
    return snprintf(buffer, size, "%u:%02u", minutes, seconds);
  }

  hours = minutes / 60;
  minutes -= hours * 60;
  return snprintf(buffer, size, "%uh%02u:%02u", hours, minutes, seconds);
}

static int rc_format_value_centiseconds(char* buffer, size_t size, uint32_t centiseconds) {
  uint32_t seconds;
  int chars, chars2;

  /* modulus off the centiseconds */
  seconds = centiseconds / 100;
  centiseconds -= seconds * 100;

  chars = rc_format_value_seconds(buffer, size, seconds);
  if (chars > 0) {
    chars2 = snprintf(buffer + chars, size - chars, ".%02u", centiseconds);
    if (chars2 > 0) {
      chars += chars2;
    } else {
      chars = chars2;
    }
  }

  return chars;
}

static int rc_format_value_fixed(char* buffer, size_t size, const char* format, int32_t value, int32_t factor)
{
  if (value >= 0)
    return snprintf(buffer, size, format, value / factor, value % factor);

  return snprintf(buffer, size, format, value / factor, (-value) % factor);
}

static int rc_format_value_padded(char* buffer, size_t size, const char* format, int32_t value)
{
  if (value == 0)
    return snprintf(buffer, size, "0");

  return snprintf(buffer, size, format, value);
}

static int rc_format_insert_commas(int chars, char* buffer, size_t size)
{
  int to_insert;
  char* src = buffer;
  char* ptr;
  char* dst = &buffer[chars];
  if (chars == 0)
    return 0;

  /* ignore leading negative sign */
  if (*src == '-')
    src++;

  /* determine how many digits are present in the leading number */
  ptr = src;
  while (ptr < dst && isdigit((int)*ptr))
    ++ptr;

  /* determine how many commas are needed */
  to_insert = (int)((ptr - src - 1) / 3);
  if (to_insert == 0) /* no commas needed */
    return chars;

  /* if there's not enough room to insert the commas, leave string as-is, but return wanted space */
  chars += to_insert;
  if (chars >= (int)size)
    return chars;

  /* move the trailing part of the string */
  memmove(ptr + to_insert, ptr, dst - ptr + 1);

  /* shift blocks of three digits at a time, inserting commas in front of them */
  src = ptr - 1;
  dst = src + to_insert;
  while (to_insert > 0) {
    *dst-- = *src--;
    *dst-- = *src--;
    *dst-- = *src--;
    *dst-- = ',';

    --to_insert;
  }

  return chars;
}

int rc_format_typed_value(char* buffer, size_t size, const rc_typed_value_t* value, int format) {
  int chars;
  rc_typed_value_t converted_value;

  memcpy(&converted_value, value, sizeof(converted_value));

  switch (format) {
    default:
    case RC_FORMAT_VALUE:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = snprintf(buffer, size, "%d", converted_value.value.i32);
      break;

    case RC_FORMAT_FRAMES:
      /* 60 frames per second = 100 centiseconds / 60 frames; multiply frames by 100 / 60 */
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_centiseconds(buffer, size, converted_value.value.u32 * 10 / 6);
      break;

    case RC_FORMAT_CENTISECS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_centiseconds(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SECONDS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_seconds(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SECONDS_AS_MINUTES:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_minutes(buffer, size, converted_value.value.u32 / 60);
      break;

    case RC_FORMAT_MINUTES:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_minutes(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SCORE:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      return snprintf(buffer, size, "%06d", converted_value.value.i32);

    case RC_FORMAT_FLOAT1:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.1f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT2:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.2f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT3:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.3f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT4:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.4f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT5:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.5f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT6:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.6f", converted_value.value.f32);
      break;

    case RC_FORMAT_FIXED1:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_fixed(buffer, size, "%d.%u", converted_value.value.i32, 10);
      break;

    case RC_FORMAT_FIXED2:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_fixed(buffer, size, "%d.%02u", converted_value.value.i32, 100);
      break;

    case RC_FORMAT_FIXED3:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_fixed(buffer, size, "%d.%03u", converted_value.value.i32, 1000);
      break;

    case RC_FORMAT_TENS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_padded(buffer, size, "%d0", converted_value.value.i32);
      break;

    case RC_FORMAT_HUNDREDS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_padded(buffer, size, "%d00", converted_value.value.i32);
      break;

    case RC_FORMAT_THOUSANDS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = rc_format_value_padded(buffer, size, "%d000", converted_value.value.i32);
      break;

    case RC_FORMAT_UNSIGNED_VALUE:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = snprintf(buffer, size, "%u", converted_value.value.u32);
      break;

    case RC_FORMAT_UNFORMATTED:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      return snprintf(buffer, size, "%u", converted_value.value.u32);
  }

  return rc_format_insert_commas(chars, buffer, size);
}

int rc_format_value(char* buffer, int size, int32_t value, int format) {
  rc_typed_value_t typed_value;

  typed_value.value.i32 = value;
  typed_value.type = RC_VALUE_TYPE_SIGNED;
  return rc_format_typed_value(buffer, size, &typed_value, format);
}
