#include "rc_url.h"

#include "../rc_compat.h"
#include "../rhash/md5.h"

#include <stdio.h>
#include <string.h>

#if RCHEEVOS_URL_SSL
#define RCHEEVOS_URL_PROTOCOL "https"
#else
#define RCHEEVOS_URL_PROTOCOL "http"
#endif

static int rc_url_encode(char* encoded, size_t len, const char* str) {
  for (;;) {
    switch (*str) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        if (len < 2)
          return -1;

        *encoded++ = *str++;
        --len;
        break;

      case ' ':
        if (len < 2)
          return -1;

        *encoded++ = '+';
        ++str;
        --len;
        break;

      default:
        if (len < 4)
          return -1;

        snprintf(encoded, len, "%%%02x", (unsigned char)*str);
        encoded += 3;
        ++str;
        len -= 3;
        break;

      case '\0':
        *encoded = 0;
        return 0;
    }
  }
}

int rc_url_award_cheevo(char* buffer, size_t size, const char* user_name, const char* login_token,
                        unsigned cheevo_id, int hardcore, const char* game_hash) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=awardachievement&u=%s&t=%s&a=%u&h=%d",
    urle_user_name,
    urle_login_token,
    cheevo_id,
    hardcore ? 1 : 0
  );

  if (game_hash && strlen(game_hash) == 32 && (size - (size_t)written) >= 35) {
     written += snprintf(buffer + written, size - (size_t)written, "&m=%s", game_hash);
  }

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_submit_lboard(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned lboard_id, int value) {
  char urle_user_name[64];
  char urle_login_token[64];
  char signature[64];
  unsigned char hash[16];
  md5_state_t state;
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  /* Evaluate the signature. */
  snprintf(signature, sizeof(signature), "%u%s%d", lboard_id, user_name, value);
  md5_init(&state);
  md5_append(&state, (unsigned char*)signature, (int)strlen(signature));
  md5_finish(&state, hash);

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=submitlbentry&u=%s&t=%s&i=%u&s=%d&v=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    urle_user_name,
    urle_login_token,
    lboard_id,
    value,
    hash[ 0], hash[ 1], hash[ 2], hash[ 3], hash[ 4], hash[ 5], hash[ 6], hash[ 7],
    hash[ 8], hash[ 9], hash[10], hash[11],hash[12], hash[13], hash[14], hash[15]
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_gameid(char* buffer, size_t size, const char* hash) {
  int written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=gameid&m=%s",
    hash
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_patch(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=patch&u=%s&t=%s&g=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_badge_image(char* buffer, size_t size, const char* badge_name) {
  int written = snprintf(
    buffer,
    size,
    "http://i.retroachievements.org/Badge/%s",
    badge_name
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_password(char* buffer, size_t size, const char* user_name, const char* password) {
  char urle_user_name[64];
  char urle_password[256];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_password, sizeof(urle_password), password) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=login&u=%s&p=%s",
    urle_user_name,
    urle_password
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_login_with_token(char* buffer, size_t size, const char* user_name, const char* login_token) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=login&u=%s&t=%s",
    urle_user_name,
    urle_login_token
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_get_unlock_list(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid, int hardcore) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=unlocks&u=%s&t=%s&g=%u&h=%d",
    urle_user_name,
    urle_login_token,
    gameid,
    hardcore ? 1 : 0
  );

  return (size_t)written >= size ? -1 : 0;
}

int rc_url_post_playing(char* buffer, size_t size, const char* user_name, const char* login_token, unsigned gameid) {
  char urle_user_name[64];
  char urle_login_token[64];
  int written;

  if (rc_url_encode(urle_user_name, sizeof(urle_user_name), user_name) != 0) {
    return -1;
  }

  if (rc_url_encode(urle_login_token, sizeof(urle_login_token), login_token) != 0) {
    return -1;
  }

  written = snprintf(
    buffer,
    size,
    RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php?r=postactivity&u=%s&t=%s&a=3&m=%u",
    urle_user_name,
    urle_login_token,
    gameid
  );

  return (size_t)written >= size ? -1 : 0;
}

static int rc_url_append_param_equals(char* buffer, size_t buffer_size, size_t buffer_offset, const char* param)
{
  int written = 0;
  size_t param_len;

  if (buffer_offset >= buffer_size)
    return -1;

  if (buffer_offset) {
    buffer += buffer_offset;
    buffer_size -= buffer_offset;

    if (buffer[-1] != '?') {
      *buffer++ = '&';
      buffer_size--;
      written = 1;
    }
  }

  param_len = strlen(param);
  if (param_len + 1 >= buffer_size)
    return -1;
  memcpy(buffer, param, param_len);
  buffer[param_len] = '=';

  written += (int)param_len + 1;
  return written + (int)buffer_offset;
}

static int rc_url_append_unum(char* buffer, size_t buffer_size, size_t* buffer_offset, const char* param, unsigned value)
{
  int written = rc_url_append_param_equals(buffer, buffer_size, *buffer_offset, param);
  if (written > 0) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%u", value);

    if (chars + written < (int)buffer_size) {
      memcpy(&buffer[written], num, chars + 1);
      *buffer_offset = written + chars;
      return 0;
    }
  }

  return -1;
}

static int rc_url_append_str(char* buffer, size_t buffer_size, size_t* buffer_offset, const char* param, const char* value)
{
  int written = rc_url_append_param_equals(buffer, buffer_size, *buffer_offset, param);
  if (written > 0) {
    buffer += written;
    buffer_size -= written;

    if (rc_url_encode(buffer, buffer_size, value) == 0) {
      written += (int)strlen(buffer);
      *buffer_offset = written;
      return 0;
    }
  }

  return -1;
}

static int rc_url_build_dorequest(char* url_buffer, size_t url_buffer_size, size_t* buffer_offset,
   const char* api, const char* user_name)
{
  const char* base_url = RCHEEVOS_URL_PROTOCOL"://retroachievements.org/dorequest.php";
  size_t written = strlen(base_url);
  int failure = 0;

  if (url_buffer_size < written + 1)
    return -1;
  memcpy(url_buffer, base_url, written);
  url_buffer[written++] = '?';

  failure |= rc_url_append_str(url_buffer, url_buffer_size, &written, "r", api);
  if (user_name)
    failure |= rc_url_append_str(url_buffer, url_buffer_size, &written, "u", user_name);

  *buffer_offset += written;
  return failure;
}

int rc_url_ping(char* url_buffer, size_t url_buffer_size, char* post_buffer, size_t post_buffer_size,
                const char* user_name, const char* login_token, unsigned gameid, const char* rich_presence) 
{
  size_t written = 0;
  int failure = rc_url_build_dorequest(url_buffer, url_buffer_size, &written, "ping", user_name);
  failure |= rc_url_append_unum(url_buffer, url_buffer_size, &written, "g", gameid);

  written = 0;
  failure |= rc_url_append_str(post_buffer, post_buffer_size, &written, "t", login_token);

  if (rich_presence && *rich_presence)
    failure |= rc_url_append_str(post_buffer, post_buffer_size, &written, "m", rich_presence);

  if (failure) {
    if (url_buffer_size)
      url_buffer[0] = '\0';
    if (post_buffer_size)
      post_buffer[0] = '\0';
  }

  return failure;
}

int rc_url_get_lboard_entries(char* buffer, size_t size, unsigned lboard_id, unsigned first_index, unsigned count)
{
  size_t written = 0;
  int failure = rc_url_build_dorequest(buffer, size, &written, "lbinfo", NULL);
  failure |= rc_url_append_unum(buffer, size, &written, "i", lboard_id);
  if (first_index > 1)
    failure |= rc_url_append_unum(buffer, size, &written, "o", first_index - 1);
  failure |= rc_url_append_unum(buffer, size, &written, "c", count);

  return failure;
}

int rc_url_get_lboard_entries_near_user(char* buffer, size_t size, unsigned lboard_id, const char* user_name, unsigned count)
{
  size_t written = 0;
  int failure = rc_url_build_dorequest(buffer, size, &written, "lbinfo", NULL);
  failure |= rc_url_append_unum(buffer, size, &written, "i", lboard_id);
  failure |= rc_url_append_str(buffer, size, &written, "u", user_name);
  failure |= rc_url_append_unum(buffer, size, &written, "c", count);

  return failure;
}

#undef RCHEEVOS_URL_PROTOCOL
