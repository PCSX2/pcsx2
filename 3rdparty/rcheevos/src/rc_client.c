#include "rc_client_internal.h"

#include "rc_api_info.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_consoles.h"
#include "rc_hash.h"
#include "rc_version.h"

#include "rapi/rc_api_common.h"

#include "rcheevos/rc_internal.h"

#include <stdarg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <profileapi.h>
#else
#include <time.h>
#endif

#define RC_CLIENT_UNKNOWN_GAME_ID (uint32_t)-1
#define RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS (10 * 60) /* ten minutes */

#define RC_MINIMUM_UNPAUSED_FRAMES 20
#define RC_PAUSE_DECAY_MULTIPLIER 4

enum {
  RC_CLIENT_ASYNC_NOT_ABORTED = 0,
  RC_CLIENT_ASYNC_ABORTED = 1,
  RC_CLIENT_ASYNC_DESTROYED = 2
};

typedef struct rc_client_generic_callback_data_t {
  rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;
  rc_client_async_handle_t async_handle;
} rc_client_generic_callback_data_t;

typedef struct rc_client_pending_media_t
{
#ifdef RC_CLIENT_SUPPORTS_HASH
  const char* file_path;
  uint8_t* data;
  size_t data_size;
#endif
  const char* hash;
  rc_client_callback_t callback;
  void* callback_userdata;
} rc_client_pending_media_t;

typedef struct rc_client_load_state_t
{
  rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;

  rc_client_game_info_t* game;
  rc_client_subset_info_t* subset;
  rc_client_game_hash_t* hash;

#ifdef RC_CLIENT_SUPPORTS_HASH
  rc_hash_iterator_t hash_iterator;
  rc_client_game_hash_t* tried_hashes[4];
#endif
  rc_client_pending_media_t* pending_media;

  rc_api_start_session_response_t *start_session_response;

  rc_client_async_handle_t async_handle;

  uint8_t progress;
  uint8_t outstanding_requests;
#ifdef RC_CLIENT_SUPPORTS_HASH
  uint8_t hash_console_id;
#endif
} rc_client_load_state_t;

static void rc_client_process_resolved_hash(rc_client_load_state_t* load_state);
static void rc_client_begin_fetch_game_sets(rc_client_load_state_t* callback_data);
static void rc_client_hide_progress_tracker(rc_client_t* client, rc_client_game_info_t* game);
static void rc_client_load_error(rc_client_load_state_t* load_state, int result, const char* error_message);
static rc_client_async_handle_t* rc_client_load_game(rc_client_load_state_t* load_state, const char* hash, const char* file_path);
static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);
static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset);
static void rc_client_raise_pending_events(rc_client_t* client, rc_client_game_info_t* game);
static void rc_client_reschedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* callback, rc_clock_t when);
static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);
static int rc_client_is_award_achievement_pending(const rc_client_t* client, uint32_t achievement_id);
static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);

/* ===== natvis extensions ===== */

typedef struct __rc_client_achievement_state_enum_t { uint8_t value; } __rc_client_achievement_state_enum_t;
typedef struct __rc_client_achievement_category_enum_t { uint8_t value; } __rc_client_achievement_category_enum_t;
typedef struct __rc_client_achievement_type_enum_t { uint8_t value; } __rc_client_achievement_type_enum_t;
typedef struct __rc_client_achievement_bucket_enum_t { uint8_t value; } __rc_client_achievement_bucket_enum_t;
typedef struct __rc_client_achievement_unlocked_enum_t { uint8_t value; } __rc_client_achievement_unlocked_enum_t;
typedef struct __rc_client_leaderboard_state_enum_t { uint8_t value; } __rc_client_leaderboard_state_enum_t;
typedef struct __rc_client_leaderboard_format_enum_t { uint8_t value; } __rc_client_leaderboard_format_enum_t;
typedef struct __rc_client_log_level_enum_t { uint8_t value; } __rc_client_log_level_enum_t;
typedef struct __rc_client_event_type_enum_t { uint8_t value; } __rc_client_event_type_enum_t;
typedef struct __rc_client_load_game_state_enum_t { uint8_t value; } __rc_client_load_game_state_enum_t;
typedef struct __rc_client_user_state_enum_t { uint8_t value; } __rc_client_user_state_enum_t;
typedef struct __rc_client_mastery_state_enum_t { uint8_t value; } __rc_client_mastery_state_enum_t;
typedef struct __rc_client_spectator_mode_enum_t { uint8_t value; } __rc_client_spectator_mode_enum_t;
typedef struct __rc_client_disconnect_enum_t { uint8_t value; } __rc_client_disconnect_enum_t;
typedef struct __rc_client_leaderboard_tracker_list_t { rc_client_leaderboard_tracker_info_t* first; } __rc_client_leaderboard_tracker_list_t;
typedef struct __rc_client_subset_info_list_t { rc_client_subset_info_t* first; } __rc_client_subset_info_list_t;
typedef struct __rc_client_media_hash_list_t { rc_client_media_hash_t* first; } __rc_client_media_hash_list_t;
typedef struct __rc_client_subset_info_achievements_list_t { rc_client_subset_info_t info; } __rc_client_subset_info_achievements_list_t;
typedef struct __rc_client_subset_info_leaderboards_list_t { rc_client_subset_info_t info; } __rc_client_subset_info_leaderboards_list_t;
typedef struct __rc_client_scheduled_callback_list_t { rc_client_state_t state; } __rc_client_scheduled_callback_list_t;
typedef struct __rc_client_game_hash_list_t { rc_client_t client; } __rc_client_game_hash_list_t;

static void rc_client_natvis_helper(const rc_client_event_t* event, rc_client_t* client)
{
  struct natvis_extensions {
    __rc_client_achievement_state_enum_t achievement_state;
    __rc_client_achievement_category_enum_t achievement_category;
    __rc_client_achievement_type_enum_t achievement_type;
    __rc_client_achievement_bucket_enum_t achievement_bucket;
    __rc_client_achievement_unlocked_enum_t achievement_unlocked;
    __rc_client_leaderboard_state_enum_t leaderboard_state;
    __rc_client_leaderboard_format_enum_t leaderboard_format;
    __rc_client_log_level_enum_t log_level;
    __rc_client_event_type_enum_t event_type;
    __rc_client_load_game_state_enum_t load_game_state;
    __rc_client_user_state_enum_t user_state;
    __rc_client_mastery_state_enum_t mastery_state;
    __rc_client_spectator_mode_enum_t spectator_mode;
    __rc_client_disconnect_enum_t disconnect;
    __rc_client_leaderboard_tracker_list_t leaderboard_tracker_list;
    __rc_client_subset_info_list_t subset_info_list;
    __rc_client_media_hash_list_t media_hash_list;
    __rc_client_subset_info_achievements_list_t subset_info_achievements_list;
    __rc_client_subset_info_leaderboards_list_t subset_info_leaderboards_list;
    __rc_client_scheduled_callback_list_t scheduled_callback_list;
    __rc_client_game_hash_list_t client_game_hash_list;
  } natvis;

  memset(&natvis, 0, sizeof(natvis));
  (void)event;
  (void)client;

  /* this code should never be executed. it just ensures these constants get defined for
   * the natvis VisualStudio extension as they're not used directly in the code. */
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_STANDARD;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_WIN;
  natvis.achievement_category.value = RC_CLIENT_ACHIEVEMENT_CATEGORY_NONE;
  natvis.event_type.value = RC_CLIENT_EVENT_TYPE_NONE;
}

/* ===== Construction/Destruction ===== */

static void rc_client_dummy_event_handler(const rc_client_event_t* event, rc_client_t* client)
{
  (void)event;
  (void)client;
}

rc_client_t* rc_client_create(rc_client_read_memory_func_t read_memory_function, rc_client_server_call_t server_call_function)
{
  rc_client_t* client = (rc_client_t*)calloc(1, sizeof(rc_client_t));
  if (!client)
    return NULL;

  client->state.hardcore = 1;
  client->state.required_unpaused_frames = RC_MINIMUM_UNPAUSED_FRAMES;
  client->state.allow_background_memory_reads = 1;

  client->callbacks.read_memory = read_memory_function;
  client->callbacks.server_call = server_call_function;
  client->callbacks.event_handler = rc_client_natvis_helper;
  client->callbacks.event_handler = rc_client_dummy_event_handler;
  rc_client_set_legacy_peek(client, RC_CLIENT_LEGACY_PEEK_AUTO);
  rc_client_set_get_time_millisecs_function(client, NULL);

  rc_mutex_init(&client->state.mutex);

  rc_buffer_init(&client->state.buffer);

  return client;
}

void rc_client_destroy(rc_client_t* client)
{
  if (!client)
    return;

  rc_mutex_lock(&client->state.mutex);
  {
    size_t i;
    for (i = 0; i < sizeof(client->state.async_handles) / sizeof(client->state.async_handles[0]); ++i) {
      if (client->state.async_handles[i])
        client->state.async_handles[i]->aborted = RC_CLIENT_ASYNC_DESTROYED;
    }

    if (client->state.load) {
      client->state.load->async_handle.aborted = RC_CLIENT_ASYNC_DESTROYED;
      client->state.load = NULL;
    }
  }
  rc_mutex_unlock(&client->state.mutex);

  rc_client_unload_game(client);

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  rc_client_unload_raintegration(client);
#endif

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->destroy)
    client->state.external_client->destroy();
#endif

  rc_buffer_destroy(&client->state.buffer);

  rc_mutex_destroy(&client->state.mutex);

  free(client);
}

/* ===== Logging ===== */

void rc_client_log_message(const rc_client_t* client, const char* message)
{
  if (client->callbacks.log_call)
    client->callbacks.log_call(message, client);
}

static void rc_client_log_message_va(const rc_client_t* client, const char* format, va_list args)
{
  if (client->callbacks.log_call) {
    char buffer[2048];

#ifdef __STDC_SECURE_LIB__
    vsprintf_s(buffer, sizeof(buffer), format, args);
#elif __STDC_VERSION__ >= 199901L /* vsnprintf requires c99 */
    vsnprintf(buffer, sizeof(buffer), format, args);
#else /* c89 doesn't have a size-limited vsprintf function - assume the buffer is large enough */
    vsprintf(buffer, format, args);
#endif

    client->callbacks.log_call(buffer, client);
  }
}

#ifdef RC_NO_VARIADIC_MACROS

void RC_CLIENT_LOG_ERR_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_WARN_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_INFO_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_VERBOSE_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

#else

void rc_client_log_message_formatted(const rc_client_t* client, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  rc_client_log_message_va(client, format, args);
  va_end(args);
}

#endif /* RC_NO_VARIADIC_MACROS */

void rc_client_enable_logging(rc_client_t* client, int level, rc_client_message_callback_t callback)
{
  client->callbacks.log_call = callback;
  client->state.log_level = callback ? level : RC_CLIENT_LOG_LEVEL_NONE;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->enable_logging)
    client->state.external_client->enable_logging(client, level, callback);
#endif
}

/* ===== Common ===== */

static rc_clock_t rc_client_clock_get_now_millisecs(const rc_client_t* client)
{
#if defined(CLOCK_MONOTONIC)
  struct timespec now;
  (void)client;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    return 0;

  /* round nanoseconds to nearest millisecond and add to seconds */
  return ((rc_clock_t)now.tv_sec * 1000 + ((rc_clock_t)now.tv_nsec / 1000000));
#elif defined(_WIN32)
  static LARGE_INTEGER freq;
  LARGE_INTEGER ticks;

  (void)client;

  /* Frequency is the number of ticks per second and is guaranteed to not change. */
  if (!freq.QuadPart) {
    if (!QueryPerformanceFrequency(&freq))
      return 0;

    /* convert to number of ticks per millisecond to simplify later calculations */
    freq.QuadPart /= 1000;
  }

  if (!QueryPerformanceCounter(&ticks))
    return 0;

  return (rc_clock_t)(ticks.QuadPart / freq.QuadPart);
#else
  const clock_t clock_now = clock();

  (void)client;

  if (sizeof(clock_t) == 4) {
    static uint32_t clock_wraps = 0;
    static clock_t last_clock = 0;
    static time_t last_timet = 0;
    const time_t time_now = time(NULL);

    if (last_timet != 0) {
      const time_t seconds_per_clock_t = (time_t)(((uint64_t)1 << 32) / CLOCKS_PER_SEC);
      if (clock_now < last_clock) {
        /* clock() has wrapped */
        ++clock_wraps;
      }
      else if (time_now - last_timet > seconds_per_clock_t) {
        /* it's been long enough that clock() has wrapped and is higher than the last time it was read */
        ++clock_wraps;
      }
    }

    last_timet = time_now;
    last_clock = clock_now;

    return (rc_clock_t)((((uint64_t)clock_wraps << 32) | clock_now) / (CLOCKS_PER_SEC / 1000));
  }
  else {
    return (rc_clock_t)(clock_now / (CLOCKS_PER_SEC / 1000));
  }
#endif
}

void rc_client_set_get_time_millisecs_function(rc_client_t* client, rc_get_time_millisecs_func_t handler)
{
  client->callbacks.get_time_millisecs = handler ? handler : rc_client_clock_get_now_millisecs;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_get_time_millisecs)
    client->state.external_client->set_get_time_millisecs(client, handler);
#endif
}

int rc_client_async_handle_aborted(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  int aborted;

  rc_mutex_lock(&client->state.mutex);
  aborted = async_handle->aborted;
  rc_mutex_unlock(&client->state.mutex);

  return aborted;
}

static void rc_client_begin_async(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  size_t i;

  rc_mutex_lock(&client->state.mutex);
  for (i = 0; i < sizeof(client->state.async_handles) / sizeof(client->state.async_handles[0]); ++i) {
    if (!client->state.async_handles[i]) {
      client->state.async_handles[i] = async_handle;
      break;
    }
  }
  rc_mutex_unlock(&client->state.mutex);
}

static int rc_client_end_async(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  int aborted = async_handle->aborted;

  /* if client was destroyed, mutex doesn't exist and we don't need to remove the handle from the collection */
  if (aborted != RC_CLIENT_ASYNC_DESTROYED) {
    size_t i;

    rc_mutex_lock(&client->state.mutex);
    for (i = 0; i < sizeof(client->state.async_handles) / sizeof(client->state.async_handles[0]); ++i) {
      if (client->state.async_handles[i] == async_handle) {
        client->state.async_handles[i] = NULL;
        break;
      }
    }
    aborted = async_handle->aborted;

    rc_mutex_unlock(&client->state.mutex);
  }

  return aborted;
}

void rc_client_abort_async(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  if (async_handle && client) {
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
    if (client->state.external_client && client->state.external_client->abort_async) {
      client->state.external_client->abort_async(async_handle);
      return;
    }
#endif

    rc_mutex_lock(&client->state.mutex);
    async_handle->aborted = RC_CLIENT_ASYNC_ABORTED;
    rc_mutex_unlock(&client->state.mutex);
  }
}

static int rc_client_async_handle_valid(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  int valid = 0;
  size_t i;

  /* there is a small window of opportunity where the client could have been destroyed before calling
   * this function, but this function assumes the possibility that the handle has been destroyed, so
   * we can't check it for RC_CLIENT_ASYNC_DESTROYED before attempting to scan the client data */
  rc_mutex_lock(&client->state.mutex);

  for (i = 0; i < sizeof(client->state.async_handles) / sizeof(client->state.async_handles[0]); ++i) {
    if (client->state.async_handles[i] == async_handle) {
      valid = 1;
      break;
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  return valid;
}

static const char* rc_client_server_error_message(int* result, int http_status_code, const rc_api_response_t* response)
{
  if (!response->succeeded) {
    if (*result == RC_OK) {
      *result = RC_API_FAILURE;
      if (!response->error_message)
        return "Unexpected API failure with no error message";
    }

    if (response->error_message)
      return response->error_message;
  }

  (void)http_status_code;

  if (*result != RC_OK)
    return rc_error_str(*result);

  return NULL;
}

static void rc_client_raise_server_error_event(rc_client_t* client,
    const char* api, uint32_t related_id, int result, const char* error_message)
{
  rc_client_server_error_t server_error;
  rc_client_event_t client_event;

  server_error.api = api;
  server_error.error_message = error_message;
  server_error.result = result;
  server_error.related_id = related_id;

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_SERVER_ERROR;
  client_event.server_error = &server_error;

  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_update_disconnect_state(rc_client_t* client)
{
  rc_client_scheduled_callback_data_t* scheduled_callback;
  uint8_t new_state = RC_CLIENT_DISCONNECT_HIDDEN;

  rc_mutex_lock(&client->state.mutex);

  scheduled_callback = client->state.scheduled_callbacks;
  for (; scheduled_callback; scheduled_callback = scheduled_callback->next) {
    if (scheduled_callback->callback == rc_client_award_achievement_retry ||
      scheduled_callback->callback == rc_client_submit_leaderboard_entry_retry) {
      new_state = RC_CLIENT_DISCONNECT_VISIBLE;
      break;
    }
  }

  if ((client->state.disconnect & RC_CLIENT_DISCONNECT_VISIBLE) != new_state) {
    if (new_state == RC_CLIENT_DISCONNECT_VISIBLE)
      client->state.disconnect = RC_CLIENT_DISCONNECT_HIDDEN | RC_CLIENT_DISCONNECT_SHOW_PENDING;
    else
      client->state.disconnect = RC_CLIENT_DISCONNECT_VISIBLE | RC_CLIENT_DISCONNECT_HIDE_PENDING;
  }
  else {
    client->state.disconnect = new_state;
  }

  rc_mutex_unlock(&client->state.mutex);
}

static void rc_client_raise_disconnect_events(rc_client_t* client)
{
  rc_client_event_t client_event;
  uint8_t new_state;

  rc_mutex_lock(&client->state.mutex);

  if (client->state.disconnect & RC_CLIENT_DISCONNECT_SHOW_PENDING)
    new_state = RC_CLIENT_DISCONNECT_VISIBLE;
  else
    new_state = RC_CLIENT_DISCONNECT_HIDDEN;
  client->state.disconnect = new_state;

  rc_mutex_unlock(&client->state.mutex);

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = (new_state == RC_CLIENT_DISCONNECT_VISIBLE) ?
    RC_CLIENT_EVENT_DISCONNECTED : RC_CLIENT_EVENT_RECONNECTED;
  client->callbacks.event_handler(&client_event, client);
}

static int rc_client_should_retry(const rc_api_server_response_t* server_response)
{
  switch (server_response->http_status_code) {
    case 502: /* 502 Bad Gateway */
      /* nginx connection pool full */
      return 1;

    case 503: /* 503 Service Temporarily Unavailable */
      /* site is in maintenance mode */
      return 1;

    case 504: /* 504 Gateway Timeout */
      /* timeout between web server and database server */
      return 1;

    case 429: /* 429 Too Many Requests */
      /* too many unlocks occurred at the same time */
      return 1;

    case 521: /* 521 Web Server is Down */
      /* cloudfare could not find the server */
      return 1;

    case 522: /* 522 Connection Timed Out */
      /* timeout connecting to server from cloudfare */
      return 1;

    case 523: /* 523 Origin is Unreachable */
      /* cloudfare cannot find server */
      return 1;

    case 524: /* 524 A Timeout Occurred */
      /* connection to server from cloudfare was dropped before request was completed */
      return 1;

    case 525: /* 525 SSL Handshake Failed */
      /* web server worker connection pool is exhausted */
      return 1;

    case RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR:
      /* client provided non-HTTP error (explicitly retryable) */
      return 1;

    case RC_API_SERVER_RESPONSE_CLIENT_ERROR:
      /* client provided non-HTTP error (implicitly non-retryable) */
      return 0;

    default:
      /* assume any error not handled above where no response was received should be retried */
      if (server_response->body_length == 0 || !server_response->body || !server_response->body[0])
        return 1;

      return 0;
  }
}

static int rc_client_get_image_url(char buffer[], size_t buffer_size, int image_type, const char* image_name)
{
  rc_api_fetch_image_request_t image_request;
  rc_api_request_t request;
  int result;

  if (!buffer)
    return RC_INVALID_STATE;

  memset(&image_request, 0, sizeof(image_request));
  image_request.image_type = image_type;
  image_request.image_name = image_name;
  result = rc_api_init_fetch_image_request_hosted(&request, &image_request, NULL);
  if (result == RC_OK)
  {
    const size_t len = strlen(request.url);
    if (len >= buffer_size)
      result = RC_INSUFFICIENT_BUFFER;
    else
      memcpy(buffer, request.url, len + 1);
  }

  rc_api_destroy_request(&request);
  return result;
}

/* ===== User ===== */

static void rc_client_login_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_generic_callback_data_t* login_callback_data = (rc_client_generic_callback_data_t*)callback_data;
  rc_client_t* client = login_callback_data->client;
  rc_api_login_response_t login_response;
  rc_client_load_state_t* load_state;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &login_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      rc_client_logout(client); /* logout will reset the user state and call the load game callback */

    free(login_callback_data);
    return;
  }

  if (client->state.user == RC_CLIENT_USER_STATE_NONE) {
    /* logout was called */
    if (login_callback_data->callback)
      login_callback_data->callback(RC_ABORTED, "Login aborted", client, login_callback_data->callback_userdata);

    free(login_callback_data);
    /* logout call will immediately abort load game before this callback gets called */
    return;
  }

  result = rc_api_process_login_server_response(&login_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &login_response.response);
  if (error_message) {
    rc_mutex_lock(&client->state.mutex);
    client->state.user = RC_CLIENT_USER_STATE_NONE;
    load_state = client->state.load;
    rc_mutex_unlock(&client->state.mutex);

    RC_CLIENT_LOG_ERR_FORMATTED(client, "Login failed: %s", error_message);
    if (login_callback_data->callback)
      login_callback_data->callback(result, error_message, client, login_callback_data->callback_userdata);

    if (load_state && load_state->progress == RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN)
      rc_client_begin_fetch_game_sets(load_state);
  }
  else {
    client->user.username = rc_buffer_strcpy(&client->state.buffer, login_response.username);

    if (strcmp(login_response.username, login_response.display_name) == 0)
      client->user.display_name = client->user.username;
    else
      client->user.display_name = rc_buffer_strcpy(&client->state.buffer, login_response.display_name);

    client->user.avatar_url = rc_buffer_strcpy(&client->state.buffer, login_response.avatar_url);
    client->user.token = rc_buffer_strcpy(&client->state.buffer, login_response.api_token);
    client->user.score = login_response.score;
    client->user.score_softcore = login_response.score_softcore;
    client->user.num_unread_messages = login_response.num_unread_messages;

    rc_mutex_lock(&client->state.mutex);
    client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;
    load_state = client->state.load;
    rc_mutex_unlock(&client->state.mutex);

    RC_CLIENT_LOG_INFO_FORMATTED(client, "%s logged in successfully", login_response.display_name);

    if (load_state && load_state->progress == RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN)
      rc_client_begin_fetch_game_sets(load_state);

    if (login_callback_data->callback)
      login_callback_data->callback(RC_OK, NULL, client, login_callback_data->callback_userdata);
  }

  rc_api_destroy_login_response(&login_response);
  free(login_callback_data);
}

static rc_client_async_handle_t* rc_client_begin_login(rc_client_t* client,
  const rc_api_login_request_t* login_request, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_generic_callback_data_t* callback_data;
  rc_api_request_t request;
  int result = rc_api_init_login_request_hosted(&request, login_request, &client->state.host);
  const char* error_message = rc_error_str(result);

  if (result == RC_OK) {
    rc_mutex_lock(&client->state.mutex);

    if (client->state.user == RC_CLIENT_USER_STATE_LOGIN_REQUESTED) {
      error_message = "Login already in progress";
      result = RC_INVALID_STATE;
    }
    client->state.user = RC_CLIENT_USER_STATE_LOGIN_REQUESTED;

    rc_mutex_unlock(&client->state.mutex);
  }

  if (result != RC_OK) {
    callback(result, error_message, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_generic_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;

  rc_client_begin_async(client, &callback_data->async_handle);
  client->callbacks.server_call(&request, rc_client_login_callback, callback_data, client);

  rc_api_destroy_request(&request);

  /* if the user state has changed, the async operation completed synchronously */
  rc_mutex_lock(&client->state.mutex);
  if (client->state.user != RC_CLIENT_USER_STATE_LOGIN_REQUESTED)
    callback_data = NULL;
  rc_mutex_unlock(&client->state.mutex);

  return callback_data ? &callback_data->async_handle : NULL;
}

rc_client_async_handle_t* rc_client_begin_login_with_password(rc_client_t* client,
  const char* username, const char* password, rc_client_callback_t callback, void* callback_userdata)
{
  rc_api_login_request_t login_request;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", client, callback_userdata);
    return NULL;
  }

  if (!password || !password[0]) {
    callback(RC_INVALID_STATE, "password is required", client, callback_userdata);
    return NULL;
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_login_with_password)
    return client->state.external_client->begin_login_with_password(client, username, password, callback, callback_userdata);
#endif

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.password = password;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Attempting to log in %s (with password)", username);
  return rc_client_begin_login(client, &login_request, callback, callback_userdata);
}

rc_client_async_handle_t* rc_client_begin_login_with_token(rc_client_t* client,
  const char* username, const char* token, rc_client_callback_t callback, void* callback_userdata)
{
  rc_api_login_request_t login_request;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", client, callback_userdata);
    return NULL;
  }

  if (!token || !token[0]) {
    callback(RC_INVALID_STATE, "token is required", client, callback_userdata);
    return NULL;
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_login_with_token)
    return client->state.external_client->begin_login_with_token(client, username, token, callback, callback_userdata);
#endif

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.api_token = token;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Attempting to log in %s (with token)", username);
  return rc_client_begin_login(client, &login_request, callback, callback_userdata);
}

void rc_client_logout(rc_client_t* client)
{
  rc_client_load_state_t* load_state;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->logout) {
    client->state.external_client->logout();
    return;
  }
#endif

  switch (client->state.user) {
    case RC_CLIENT_USER_STATE_LOGGED_IN:
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Logging %s out", client->user.display_name);
      break;

    case RC_CLIENT_USER_STATE_LOGIN_REQUESTED:
      RC_CLIENT_LOG_INFO(client, "Aborting login");
      break;
  }

  rc_mutex_lock(&client->state.mutex);

  client->state.user = RC_CLIENT_USER_STATE_NONE;
  memset(&client->user, 0, sizeof(client->user));

  load_state = client->state.load;

  rc_mutex_unlock(&client->state.mutex);

  rc_client_unload_game(client);

  if (load_state && load_state->progress == RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN)
    rc_client_load_error(load_state, RC_ABORTED, "Login aborted");
}

const rc_client_user_t* rc_client_get_user_info(const rc_client_t* client)
{
  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_user_info_v3)
      return client->state.external_client->get_user_info_v3();

    if (client->state.external_client->get_user_info)
      return rc_client_external_convert_v1_user(client, client->state.external_client->get_user_info());
  }
#endif

  return (client->state.user == RC_CLIENT_USER_STATE_LOGGED_IN) ? &client->user : NULL;
}

int rc_client_user_get_image_url(const rc_client_user_t* user, char buffer[], size_t buffer_size)
{
  if (!user)
    return RC_INVALID_STATE;

  if (user->avatar_url) {
    const size_t len = strlen(user->avatar_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, user->avatar_url, len + 1);
    return RC_OK;
  }

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_USER, user->display_name);
}

static void rc_client_subset_get_user_game_summary(const rc_client_t* client,
    const rc_client_subset_info_t* subset, rc_client_user_game_summary_t* summary)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  time_t last_unlock_time = 0;
  time_t last_progression_time = 0;
  time_t first_win_time = 0;
  int num_progression_achievements = 0;
  int num_win_achievements = 0;
  int num_unlocked_progression_achievements = 0;
  const uint8_t unlock_bit = (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_mutex_lock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */

  for (; achievement < stop; ++achievement) {
    switch (achievement->public_.category) {
      case RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE:
        ++summary->num_core_achievements;
        summary->points_core += achievement->public_.points;

        if (achievement->public_.unlocked & unlock_bit) {
          ++summary->num_unlocked_achievements;
          summary->points_unlocked += achievement->public_.points;

          if (achievement->public_.unlock_time > last_unlock_time)
            last_unlock_time = achievement->public_.unlock_time;

          if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION) {
            ++num_unlocked_progression_achievements;
            if (achievement->public_.unlock_time > last_progression_time)
              last_progression_time = achievement->public_.unlock_time;
          }
          else if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_WIN) {
            if (first_win_time == 0 || achievement->public_.unlock_time < first_win_time)
              first_win_time = achievement->public_.unlock_time;
          }
        }

        if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION)
          ++num_progression_achievements;
        else if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_WIN)
          ++num_win_achievements;

        if (achievement->public_.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
          ++summary->num_unsupported_achievements;

        break;

      case RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL:
        ++summary->num_unofficial_achievements;
        break;

      default:
        continue;
    }
  }

  rc_mutex_unlock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */

  if (summary->num_unlocked_achievements == summary->num_core_achievements)
    summary->completed_time = last_unlock_time;

  if ((first_win_time || num_win_achievements == 0) && num_unlocked_progression_achievements == num_progression_achievements)
    summary->beaten_time = (first_win_time > last_progression_time) ? first_win_time : last_progression_time;
}

void rc_client_get_user_game_summary(const rc_client_t* client, rc_client_user_game_summary_t* summary)
{
  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_user_game_summary_v5) {
      client->state.external_client->get_user_game_summary_v5(summary);
      return;
    }
    if (client->state.external_client->get_user_game_summary) {
      client->state.external_client->get_user_game_summary(summary);
      return;
    }
  }
#endif

  if (rc_client_is_game_loaded(client))
    rc_client_subset_get_user_game_summary(client, client->game->subsets, summary);
}

void rc_client_get_user_subset_summary(const rc_client_t* client, uint32_t subset_id, rc_client_user_game_summary_t* summary)
{
  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
  if (!client || !subset_id)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_user_subset_summary) {
    client->state.external_client->get_user_subset_summary(subset_id, summary);
    return;
  }
#endif

  if (rc_client_is_game_loaded(client)) {
    const rc_client_subset_info_t* subset = client->game->subsets;
    for (; subset; subset = subset->next) {
      if (subset->public_.id == subset_id) {
        rc_client_subset_get_user_game_summary(client, subset, summary);
        break;
      }
    }
  }
}

typedef struct rc_client_fetch_all_user_progress_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_all_user_progress_callback_t callback;
  void* callback_userdata;
  uint32_t console_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_all_user_progress_callback_data_t;

static void rc_client_fetch_all_user_progress_callback(const rc_api_server_response_t* server_response,
                                                       void* callback_data)
{
  rc_client_fetch_all_user_progress_callback_data_t* ap_callback_data =
    (rc_client_fetch_all_user_progress_callback_data_t*)callback_data;
  rc_client_t* client = ap_callback_data->client;
  rc_api_fetch_all_user_progress_response_t ap_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &ap_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch all progress aborted");

    free(ap_callback_data);
    return;
  }

  result = rc_api_process_fetch_all_user_progress_server_response(&ap_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &ap_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch all progress for console %u failed: %s", ap_callback_data->console_id,
                                error_message);
    ap_callback_data->callback(result, error_message, NULL, client, ap_callback_data->callback_userdata);
  } else {
    rc_client_all_user_progress_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_all_user_progress_entry_t) * ap_response.num_entries;

    list = (rc_client_all_user_progress_t*)malloc(list_size);
    if (!list) {
      ap_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                 ap_callback_data->callback_userdata);
    } else {
      rc_client_all_user_progress_entry_t* entry = list->entries =
        (rc_client_all_user_progress_entry_t*)((uint8_t*)list + sizeof(*list));
      const rc_api_all_user_progress_entry_t* hlentry = ap_response.entries;
      const rc_api_all_user_progress_entry_t* stop = hlentry + ap_response.num_entries;

      for (; hlentry < stop; ++hlentry, ++entry)
      {
        entry->game_id = hlentry->game_id;
        entry->num_achievements = hlentry->num_achievements;
        entry->num_unlocked_achievements = hlentry->num_unlocked_achievements;
        entry->num_unlocked_achievements_hardcore = hlentry->num_unlocked_achievements_hardcore;
      }

      list->num_entries = ap_response.num_entries;

      ap_callback_data->callback(RC_OK, NULL, list, client, ap_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_all_user_progress_response(&ap_response);
  free(ap_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_all_user_progress(rc_client_t* client, uint32_t console_id,
                                                                  rc_client_fetch_all_user_progress_callback_t callback,
                                                                  void* callback_userdata)
{
  rc_api_fetch_all_user_progress_request_t api_params;
  rc_client_fetch_all_user_progress_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  } else if (client->state.user != RC_CLIENT_USER_STATE_LOGGED_IN) {
    callback(RC_INVALID_STATE, "client must be logged in", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.username = client->user.username;
  api_params.api_token = client->user.token;
  api_params.console_id = console_id;

  result = rc_api_init_fetch_all_user_progress_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_all_user_progress_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->console_id = console_id;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_all_user_progress_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_all_user_progress(rc_client_all_user_progress_t* list)
{
  free(list);
}

/* ===== Game ===== */

static void rc_client_free_game(rc_client_game_info_t* game)
{
  rc_runtime_destroy(&game->runtime);

  rc_buffer_destroy(&game->buffer);

  free(game);
}

static void rc_client_free_load_state(rc_client_load_state_t* load_state)
{
  if (load_state->game)
    rc_client_free_game(load_state->game);

  if (load_state->start_session_response) {
    rc_api_destroy_start_session_response(load_state->start_session_response);
    free(load_state->start_session_response);
  }

#ifdef RC_CLIENT_SUPPORTS_HASH
  rc_hash_destroy_iterator(&load_state->hash_iterator);
#endif

  free(load_state);
}

static void rc_client_begin_load_state(rc_client_load_state_t* load_state, uint8_t state, uint8_t num_requests)
{
  rc_mutex_lock(&load_state->client->state.mutex);

  load_state->progress = state;
  load_state->outstanding_requests += num_requests;

  rc_mutex_unlock(&load_state->client->state.mutex);
}

static int rc_client_end_load_state(rc_client_load_state_t* load_state)
{
  int remaining_requests = 0;
  int aborted = 0;

  rc_mutex_lock(&load_state->client->state.mutex);

  if (load_state->outstanding_requests > 0)
    --load_state->outstanding_requests;
  remaining_requests = load_state->outstanding_requests;

  if (load_state->client->state.load != load_state)
    aborted = 1;

  rc_mutex_unlock(&load_state->client->state.mutex);

  if (aborted) {
    /* we can't actually free the load_state itself if there are any outstanding requests
     * or their callbacks will try to use the free'd memory. As they call end_load_state,
     * the outstanding_requests count will reach zero and the memory will be free'd then. */
    if (remaining_requests == 0) {
      /* if one of the callbacks called rc_client_load_error, progress will be set to
       * RC_CLIENT_LOAD_STATE_ABORTED. There's no need to call the callback with RC_ABORTED
       * in that case, as it will have already been called with something more appropriate. */
      if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_ABORTED && load_state->callback)
        load_state->callback(RC_ABORTED, "The requested game is no longer active", load_state->client, load_state->callback_userdata);

      rc_client_free_load_state(load_state);
    }

    return -1;
  }

  return remaining_requests;
}

static void rc_client_load_error(rc_client_load_state_t* load_state, int result, const char* error_message)
{
  int remaining_requests = 0;

  rc_mutex_lock(&load_state->client->state.mutex);

  load_state->progress = RC_CLIENT_LOAD_GAME_STATE_ABORTED;
  if (load_state->client->state.load == load_state)
    load_state->client->state.load = NULL;

  remaining_requests = load_state->outstanding_requests;

  rc_mutex_unlock(&load_state->client->state.mutex);

  RC_CLIENT_LOG_ERR_FORMATTED(load_state->client, "Load failed (%d): %s", result, error_message);

  if (load_state->callback)
    load_state->callback(result, error_message, load_state->client, load_state->callback_userdata);

  /* we can't actually free the load_state itself if there are any outstanding requests
   * or their callbacks will try to use the free'd memory. as they call end_load_state,
   * the outstanding_requests count will reach zero and the memory will be free'd then. */
  if (remaining_requests == 0)
    rc_client_free_load_state(load_state);
}

static void rc_client_load_aborted(rc_client_load_state_t* load_state)
{
  /* prevent callback from being called when manually aborted */
  load_state->callback = NULL;

  /* mark the game as no longer being loaded */
  rc_client_load_error(load_state, RC_ABORTED, NULL);

  /* decrement the async counter and potentially free the load_state object */
  rc_client_end_load_state(load_state);
}

static void rc_client_invalidate_memref_achievements(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_achievement_info_t* achievement = subset->achievements;
    rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
        continue;

      if (rc_trigger_contains_memref(achievement->trigger, memref)) {
        achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
        achievement->public_.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;

        if (achievement->trigger)
          achievement->trigger->state = RC_TRIGGER_STATE_DISABLED;

        RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled achievement %u. Invalid address %06X", achievement->public_.id, memref->address);
      }
    }
  }
}

static void rc_client_invalidate_memref_leaderboards(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
    rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
        continue;
      if (!leaderboard->lboard)
        continue;

      if (rc_trigger_contains_memref(&leaderboard->lboard->start, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->cancel, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->submit, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_value_contains_memref(&leaderboard->lboard->value, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else
        continue;

      leaderboard->lboard->state = RC_LBOARD_STATE_DISABLED;

      RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled leaderboard %u. Invalid address %06X", leaderboard->public_.id, memref->address);
    }
  }
}

static void rc_client_validate_addresses(rc_client_game_info_t* game, rc_client_t* client)
{
  const rc_memory_regions_t* regions = rc_console_memory_regions(game->public_.console_id);
  const uint32_t max_address = (regions && regions->num_regions > 0) ?
      regions->region[regions->num_regions - 1].end_address : 0xFFFFFFFF;
  uint8_t buffer[8];
  uint32_t total_count = 0;
  uint32_t invalid_count = 0;

  rc_memref_list_t* memref_list = &game->runtime.memrefs->memrefs;
  for (; memref_list; memref_list = memref_list->next) {
    rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_end = memref + memref_list->count;
    total_count += memref_list->count;

    for (; memref < memref_end; ++memref) {
      if (memref->address > max_address ||
          client->callbacks.read_memory(memref->address, buffer, 1, client) == 0) {
        memref->value.type = RC_VALUE_TYPE_NONE;

        rc_client_invalidate_memref_achievements(game, client, memref);
        rc_client_invalidate_memref_leaderboards(game, client, memref);

        invalid_count++;
      }
    }
  }

  game->max_valid_address = max_address;
  RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "%u/%u memory addresses valid", total_count - invalid_count, total_count);
}

static void rc_client_update_legacy_runtime_achievements(rc_client_game_info_t* game, uint32_t active_count)
{
  if (active_count > 0) {
    rc_client_achievement_info_t* achievement;
    rc_client_achievement_info_t* stop;
    rc_runtime_trigger_t* trigger;
    rc_client_subset_info_t* subset;

    if (active_count <= game->runtime.trigger_capacity) {
      if (active_count != 0)
        memset(game->runtime.triggers, 0, active_count * sizeof(rc_runtime_trigger_t));
    } else {
      if (game->runtime.triggers)
        free(game->runtime.triggers);

      game->runtime.trigger_capacity = active_count;
      game->runtime.triggers = (rc_runtime_trigger_t*)calloc(1, active_count * sizeof(rc_runtime_trigger_t));
    }

    trigger = game->runtime.triggers;
    if (!trigger) {
      /* malloc failed, no way to report error, just bail */
      game->runtime.trigger_count = 0;
      return;
    }

    for (subset = game->subsets; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      achievement = subset->achievements;
      stop = achievement + subset->public_.num_achievements;

      for (; achievement < stop; ++achievement) {
        if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
          trigger->id = achievement->public_.id;
          memcpy(trigger->md5, achievement->md5, 16);
          trigger->trigger = achievement->trigger;
          ++trigger;
        }
      }
    }
  }

  game->runtime.trigger_count = active_count;
}

static uint32_t rc_client_subset_count_active_achievements(const rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      ++active_count;
  }

  return active_count;
}

void rc_client_update_active_achievements(rc_client_game_info_t* game)
{
  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      active_count += rc_client_subset_count_active_achievements(subset);
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static uint32_t rc_client_subset_toggle_hardcore_achievements(rc_client_subset_info_t* subset, rc_client_t* client, uint8_t active_bit)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if ((achievement->public_.unlocked & active_bit) == 0) {
      switch (achievement->public_.state) {
        case RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED:
        case RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE:
          rc_reset_trigger(achievement->trigger);
          achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
          ++active_count;
          break;

        case RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE:
          ++active_count;
          break;
      }
    }
    else {
      achievement->public_.unlock_time = (active_bit == RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) ?
          achievement->unlock_time_hardcore : achievement->unlock_time_softcore;

      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE ||
          achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE) {
        /* if it's active despite being unlocked, and we're in encore mode, leave it active */
        if (client->state.encore_mode) {
          ++active_count;
          continue;
        }

        /* switch to inactive */
        achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;

        if (achievement->trigger && rc_trigger_state_active(achievement->trigger->state)) {
          /* hide any active challenge indicators */
          if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
            rc_client_event_t client_event;
            memset(&client_event, 0, sizeof(client_event));
            client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
            client_event.achievement = &achievement->public_;
            client->callbacks.event_handler(&client_event, client);
          }

          achievement->trigger->state = RC_TRIGGER_STATE_TRIGGERED;
        }
      }
    }
  }

  return active_count;
}

static void rc_client_toggle_hardcore_achievements(rc_client_game_info_t* game, rc_client_t* client, uint8_t active_bit)
{
  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      active_count += rc_client_subset_toggle_hardcore_achievements(subset, client, active_bit);
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static void rc_client_activate_achievements(rc_client_game_info_t* game, rc_client_t* client)
{
  const uint8_t active_bit = (client->state.encore_mode) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE : (client->state.hardcore) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_client_toggle_hardcore_achievements(game, client, active_bit);
}

static void rc_client_update_legacy_runtime_leaderboards(rc_client_game_info_t* game, uint32_t active_count)
{
  if (active_count > 0) {
    rc_client_leaderboard_info_t* leaderboard;
    rc_client_leaderboard_info_t* stop;
    rc_client_subset_info_t* subset;
    rc_runtime_lboard_t* lboard;

    if (active_count <= game->runtime.lboard_capacity) {
      if (active_count != 0)
        memset(game->runtime.lboards, 0, active_count * sizeof(rc_runtime_lboard_t));
    } else {
      if (game->runtime.lboards)
        free(game->runtime.lboards);

      game->runtime.lboard_capacity = active_count;
      game->runtime.lboards = (rc_runtime_lboard_t*)calloc(1, active_count * sizeof(rc_runtime_lboard_t));
    }

    lboard = game->runtime.lboards;
    if (!lboard) {
      /* malloc failed. no way to report error, just bail */
      game->runtime.lboard_count = 0;
      return;
    }

    for (subset = game->subsets; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public_.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE ||
            leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
          lboard->id = leaderboard->public_.id;
          memcpy(lboard->md5, leaderboard->md5, 16);
          lboard->lboard = leaderboard->lboard;
          ++lboard;
        }
      }
    }
  }

  game->runtime.lboard_count = active_count;
}

void rc_client_update_active_leaderboards(rc_client_game_info_t* game)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard)
    {
      switch (leaderboard->public_.state)
      {
        case RC_CLIENT_LEADERBOARD_STATE_ACTIVE:
        case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
          ++active_count;
          break;
      }
    }
  }

  rc_client_update_legacy_runtime_leaderboards(game, active_count);
}

static void rc_client_activate_leaderboards(rc_client_game_info_t* game, rc_client_t* client)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;
  const uint8_t leaderboards_allowed =
      client->state.hardcore || client->state.allow_leaderboards_in_softcore;

  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public_.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          if (leaderboards_allowed) {
            rc_reset_lboard(leaderboard->lboard);
            leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
            ++active_count;
          }
          break;

        default:
          if (leaderboards_allowed)
            ++active_count;
          else
            leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  rc_client_update_legacy_runtime_leaderboards(game, active_count);
}

static void rc_client_deactivate_leaderboards(rc_client_game_info_t* game, rc_client_t* client)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public_.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
          /* fallthrough */ /* to default */
        default:
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  game->runtime.lboard_count = 0;
}

static void rc_client_apply_unlocks(rc_client_subset_info_t* subset, rc_api_unlock_entry_t* unlocks, uint32_t num_unlocks, uint8_t mode)
{
  rc_client_achievement_info_t* start = subset->achievements;
  rc_client_achievement_info_t* stop = start + subset->public_.num_achievements;
  rc_client_achievement_info_t* scan;
  rc_api_unlock_entry_t* unlock = unlocks;
  rc_api_unlock_entry_t* unlock_stop = unlocks + num_unlocks;

  for (; unlock < unlock_stop; ++unlock) {
    for (scan = start; scan < stop; ++scan) {
      if (scan->public_.id == unlock->achievement_id) {
        scan->public_.unlocked |= mode;

        if (mode & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE)
          scan->unlock_time_hardcore = unlock->when;
        if (mode & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE)
          scan->unlock_time_softcore = unlock->when;

        if (scan == start)
          ++start;
        else if (scan + 1 == stop)
          --stop;
        break;
      }
    }
  }

  if (subset->next)
    rc_client_apply_unlocks(subset->next, unlocks, num_unlocks, mode);
}

static void rc_client_free_pending_media(rc_client_pending_media_t* pending_media)
{
  if (pending_media->hash)
    free((void*)pending_media->hash);
#ifdef RC_CLIENT_SUPPORTS_HASH
  if (pending_media->data)
    free(pending_media->data);
  free((void*)pending_media->file_path);
#endif
  free(pending_media);
}

static void rc_client_log_active_assets(rc_client_t* client)
{
  uint32_t num_achievements;
  uint32_t num_active_achievements;
  uint32_t num_unsupported_achievements;
  uint32_t num_leaderboards;
  uint32_t num_unsupported_leaderboards;
  const rc_client_achievement_info_t* ach;
  const rc_client_achievement_info_t* ach_stop;
  const rc_client_leaderboard_info_t* lbd;
  const rc_client_leaderboard_info_t* lbd_stop;

  const rc_client_subset_info_t* subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    num_achievements = 0;
    num_active_achievements = 0;
    num_unsupported_achievements = 0;
    num_leaderboards = 0;
    num_unsupported_leaderboards = 0;

    ach = subset->achievements;
    ach_stop = ach + subset->public_.num_achievements;
    for (; ach < ach_stop; ++ach) {
      if (ach->public_.category == RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE) {
        ++num_achievements;
        if (ach->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
          ++num_active_achievements;
        else if (ach->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
          ++num_unsupported_achievements;
      }
    }

    lbd = subset->leaderboards;
    lbd_stop = lbd + subset->public_.num_leaderboards;
    for (; lbd < lbd_stop; ++lbd) {
      ++num_leaderboards;
      if (lbd->public_.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
        ++num_unsupported_leaderboards;
    }

    if (num_unsupported_achievements) {
      if (num_unsupported_leaderboards) {
        RC_CLIENT_LOG_INFO_FORMATTED(client, "Set %u: %u/%u achievements active (%u unsupported), %u leaderboards (%u unsupported)",
          subset->public_.id, num_active_achievements, num_achievements, num_unsupported_achievements, num_leaderboards, num_unsupported_leaderboards);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(client, "Set %u: %u/%u achievements active (%u unsupported), %u leaderboards",
          subset->public_.id, num_active_achievements, num_achievements, num_unsupported_achievements, num_leaderboards);
      }
    }
    else if (num_unsupported_leaderboards) {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Set %u: %u/%u achievements active, %u leaderboards (%u unsupported)",
        subset->public_.id, num_active_achievements, num_achievements, num_leaderboards, num_unsupported_leaderboards);
    }
    else {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Set %u: %u/%u achievements active, %u leaderboards",
        subset->public_.id, num_active_achievements, num_achievements, num_leaderboards);
    }
  }
}

/* NOTE: address validation uses the read_memory callback to make sure the client
 *       will return data for the requested address. As such, this function must
 *       respect the `client->state.allow_background_memory_reads setting. Use
 *       rc_client_queue_activate_game to dispatch this function to the do_frame loop/
 */
static void rc_client_activate_game(rc_client_load_state_t* load_state, rc_api_start_session_response_t *start_session_response)
{
  rc_client_t* client = load_state->client;

  rc_mutex_lock(&client->state.mutex);
  load_state->progress = (client->state.load == load_state) ?
      RC_CLIENT_LOAD_GAME_STATE_DONE : RC_CLIENT_LOAD_GAME_STATE_ABORTED;
  rc_mutex_unlock(&client->state.mutex);

  if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_DONE) {
    /* previous load state was aborted */
    if (load_state->callback)
      load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
  }
  else if (!start_session_response && client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) {
    /* unlocks not available - assume malloc failed */
    if (load_state->callback)
      load_state->callback(RC_INVALID_STATE, "Unlock arrays were not allocated", client, load_state->callback_userdata);
  }
  else {
    if (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) {
      rc_client_apply_unlocks(load_state->subset, start_session_response->hardcore_unlocks,
          start_session_response->num_hardcore_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
      rc_client_apply_unlocks(load_state->subset, start_session_response->unlocks,
          start_session_response->num_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    }

    /* make the loaded game active if another game is not aleady being loaded. */
    rc_mutex_lock(&client->state.mutex);
    if (client->state.load == load_state) {
      client->game = load_state->game;
      client->state.frames_processed = client->state.frames_at_last_ping = 0;
    }
    else {
      load_state->progress = RC_CLIENT_LOAD_GAME_STATE_ABORTED;
    }
    rc_mutex_unlock(&client->state.mutex);

    if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_ABORTED) {
      /* if a change media request is pending, kick it off */
      rc_client_pending_media_t* pending_media;

      rc_mutex_lock(&load_state->client->state.mutex);
      pending_media = load_state->pending_media;
      load_state->pending_media = NULL;
      rc_mutex_unlock(&load_state->client->state.mutex);

      if (pending_media) {
        /* rc_client_check_pending_media will fail if it can't find the game in client->game or
         * client->state.load->game. since we've detached the load_state, this has to occur after
         * we've made the game active. */
        if (pending_media->hash) {
          rc_client_begin_change_media(client, pending_media->hash,
            pending_media->callback, pending_media->callback_userdata);
        } else {
#ifdef RC_CLIENT_SUPPORTS_HASH
          rc_client_begin_identify_and_change_media(client, pending_media->file_path,
            pending_media->data, pending_media->data_size,
            pending_media->callback, pending_media->callback_userdata);
#endif
        }
        rc_client_free_pending_media(pending_media);
      }

      rc_mutex_lock(&client->state.mutex);
      if (client->state.load != load_state)
        load_state->progress = RC_CLIENT_LOAD_GAME_STATE_ABORTED;
      rc_mutex_unlock(&client->state.mutex);
    }

    /* if the game is still being loaded, make sure all the required memory addresses are accessible
     * so we can mark achievements as unsupported before loading them into the runtime. */
    if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_ABORTED) {
      /* ASSERT: client->game must be set before calling this function so the read_memory callback can query the console_id */
      rc_client_validate_addresses(load_state->game, client);

      rc_mutex_lock(&client->state.mutex);
      if (client->state.load != load_state)
        load_state->progress = RC_CLIENT_LOAD_GAME_STATE_ABORTED;
      rc_mutex_unlock(&client->state.mutex);
    }

    /* if the game is still being loaded, load any active acheivements/leaderboards into the runtime */
    if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_ABORTED) {
      rc_client_activate_achievements(load_state->game, client);
      rc_client_activate_leaderboards(load_state->game, client);

      /* detach the load state to indicate that loading is fully complete */
      rc_mutex_lock(&client->state.mutex);
      if (client->state.load == load_state)
        client->state.load = NULL;
      else
        load_state->progress = RC_CLIENT_LOAD_GAME_STATE_ABORTED;
      rc_mutex_unlock(&client->state.mutex);
    }

    /* one last sanity check to make sure the game is still being loaded. */
    if (load_state->progress == RC_CLIENT_LOAD_GAME_STATE_ABORTED) {
      /* game has been unloaded, or another game is being loaded over the top of this game */
      if (load_state->callback)
        load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
    }
    else {
      if (load_state->hash->hash[0] != '[') {
        if (load_state->client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_LOCKED) {
          /* schedule the periodic ping */
          rc_client_scheduled_callback_data_t* callback_data = (rc_client_scheduled_callback_data_t*)
            rc_buffer_alloc(&load_state->game->buffer, sizeof(rc_client_scheduled_callback_data_t));

          memset(callback_data, 0, sizeof(*callback_data));
          callback_data->callback = rc_client_ping;
          callback_data->related_id = load_state->game->public_.id;
          callback_data->when = client->callbacks.get_time_millisecs(client) + 30 * 1000;
          rc_client_schedule_callback(client, callback_data);
        }

        RC_CLIENT_LOG_INFO_FORMATTED(client, "Game %u loaded, hardcore %s%s", load_state->game->public_.id,
            client->state.hardcore ? "enabled" : "disabled",
            (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) ? ", spectating" : "");

        if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO)
          rc_client_log_active_assets(client);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(client, "Subset %u loaded", load_state->subset->public_.id);
      }

      if (load_state->callback)
        load_state->callback(RC_OK, NULL, client, load_state->callback_userdata);

      /* detach the game object so it doesn't get freed by free_load_state */
      load_state->game = NULL;
    }
  }

  rc_client_free_load_state(load_state);
}

static void rc_client_dispatch_activate_game(struct rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data->data;
  free(callback_data);

  (void)client;
  (void)now;

  rc_client_activate_game(load_state, load_state->start_session_response);
}

static void rc_client_queue_activate_game(rc_client_load_state_t* load_state)
{
  rc_client_scheduled_callback_data_t* scheduled_callback_data =
    (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(rc_client_scheduled_callback_data_t));

  if (!scheduled_callback_data) {
    rc_client_load_error(load_state, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }

  scheduled_callback_data->callback = rc_client_dispatch_activate_game;
  scheduled_callback_data->data = load_state;

  rc_client_schedule_callback(load_state->client, scheduled_callback_data);
}

static void rc_client_start_session_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_api_start_session_response_t start_session_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  result = rc_client_end_async(load_state->client, &load_state->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED) {
      rc_client_t* client = load_state->client;
      rc_client_load_aborted(load_state);
      RC_CLIENT_LOG_VERBOSE(client, "Load aborted while starting session");
    } else {
      rc_client_free_load_state(load_state);
    }
    return;
  }

  result = rc_api_process_start_session_server_response(&start_session_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &start_session_response.response);
  outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(load_state, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, load_state was free'd */
  }
  else if (outstanding_requests == 0 && load_state->client->state.allow_background_memory_reads) {
    rc_client_activate_game(load_state, &start_session_response);
  }
  else {
    load_state->start_session_response =
        (rc_api_start_session_response_t*)malloc(sizeof(rc_api_start_session_response_t));

    if (!load_state->start_session_response) {
      rc_client_load_error(load_state, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    }
    else {
      /* safer to parse the response again than to try to copy it */
      rc_api_process_start_session_response(load_state->start_session_response, server_response->body);
    }

    if (outstanding_requests == 0) {
      if (load_state->client->state.allow_background_memory_reads)
        rc_client_activate_game(load_state, load_state->start_session_response);
      else
        rc_client_queue_activate_game(load_state);
    }
  }

  rc_api_destroy_start_session_response(&start_session_response);
}

static void rc_client_begin_start_session(rc_client_load_state_t* load_state)
{
  rc_api_start_session_request_t start_session_params;
  rc_client_t* client = load_state->client;
  rc_api_request_t start_session_request;
  int result;

  memset(&start_session_params, 0, sizeof(start_session_params));
  start_session_params.username = client->user.username;
  start_session_params.api_token = client->user.token;
  start_session_params.game_id = load_state->hash->game_id;
  start_session_params.game_hash = load_state->hash->hash;
  start_session_params.hardcore = client->state.hardcore;

  result = rc_api_init_start_session_request_hosted(&start_session_request, &start_session_params, &client->state.host);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
  }
  else {
    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION, 1);
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Starting session for game %u", start_session_params.game_id);
    rc_client_begin_async(client, &load_state->async_handle);
    client->callbacks.server_call(&start_session_request, rc_client_start_session_callback, load_state, client);
    rc_api_destroy_request(&start_session_request);
  }
}

static void rc_client_copy_achievements(rc_client_load_state_t* load_state,
    rc_client_subset_info_t* subset,
    const rc_api_achievement_definition_t* achievement_definitions, uint32_t num_achievements)
{
  const rc_api_achievement_definition_t* read;
  const rc_api_achievement_definition_t* stop;
  rc_client_achievement_info_t* achievements;
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* scan;
  rc_buffer_t* buffer;
  rc_preparse_state_t preparse;
  const char* memaddr;
  size_t size;
  rc_trigger_t* trigger;
  int trigger_size;

  subset->achievements = NULL;
  subset->public_.num_achievements = num_achievements;

  if (num_achievements == 0)
    return;

  stop = achievement_definitions + num_achievements;

  /* if not testing unofficial, filter them out */
  if (!load_state->client->state.unofficial_enabled) {
    for (read = achievement_definitions; read < stop; ++read) {
      if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE)
        --num_achievements;
    }

    subset->public_.num_achievements = num_achievements;

    if (num_achievements == 0)
      return;
  }

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2 /* trigger container */
      + sizeof(rc_condition_t) * 8 /* assume average trigger length of 8 conditions */
      + sizeof(rc_client_achievement_info_t);
  buffer = &load_state->game->buffer;
  rc_buffer_reserve(buffer, size * num_achievements);

  /* allocate the achievement array */
  size = sizeof(rc_client_achievement_info_t) * num_achievements;
  achievement = achievements = (rc_client_achievement_info_t*)rc_buffer_alloc(buffer, size);
  memset(achievements, 0, size);

  /* copy the achievement data */
  for (read = achievement_definitions; read < stop; ++read) {
    if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE && !load_state->client->state.unofficial_enabled)
      continue;

    achievement->public_.title = rc_buffer_strcpy(buffer, read->title);
    achievement->public_.description = rc_buffer_strcpy(buffer, read->description);
    snprintf(achievement->public_.badge_name, sizeof(achievement->public_.badge_name), "%s", read->badge_name);
    achievement->public_.id = read->id;
    achievement->public_.points = read->points;
    achievement->public_.category = (read->category != RC_ACHIEVEMENT_CATEGORY_CORE) ?
      RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL : RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE;
    achievement->public_.rarity = read->rarity;
    achievement->public_.rarity_hardcore = read->rarity_hardcore;
    achievement->public_.type = read->type; /* assert: mapping is 1:1 */
    achievement->public_.badge_url = rc_buffer_strcpy(buffer, read->badge_url);
    achievement->public_.badge_locked_url = rc_buffer_strcpy(buffer, read->badge_locked_url);

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, achievement->md5);

    rc_init_preparse_state(&preparse);
    preparse.parse.existing_memrefs = load_state->game->runtime.memrefs;
    trigger = RC_ALLOC(rc_trigger_t, &preparse.parse);
    rc_parse_trigger_internal(trigger, &memaddr, &preparse.parse);

    trigger_size = preparse.parse.offset;
    if (trigger_size < 0) {
      RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing achievement %u", trigger_size, read->id);
      achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
      achievement->public_.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
    }
    else {
      /* populate the item, using the communal memrefs pool */
      rc_reset_parse_state(&preparse.parse, rc_buffer_reserve(buffer, trigger_size));
      rc_preparse_reserve_memrefs(&preparse, load_state->game->runtime.memrefs);
      achievement->trigger = RC_ALLOC(rc_trigger_t, &preparse.parse);
      memaddr = read->definition;
      rc_parse_trigger_internal(achievement->trigger, &memaddr, &preparse.parse);

      if (preparse.parse.offset < 0) {
        RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing achievement %u", preparse.parse.offset, read->id);
        achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
        achievement->public_.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
      }
      else {
        rc_buffer_consume(buffer, (const uint8_t*)preparse.parse.buffer, (uint8_t*)preparse.parse.buffer + preparse.parse.offset);
      }

      rc_destroy_preparse_state(&preparse);
    }

    achievement->created_time = read->created;
    achievement->updated_time = read->updated;

    scan = achievement;
    while (scan > achievements) {
      --scan;
      if (strcmp(scan->author, read->author) == 0) {
        achievement->author = scan->author;
        break;
      }
    }
    if (!achievement->author)
      achievement->author = rc_buffer_strcpy(buffer, read->author);

    ++achievement;
  }

  subset->achievements = achievements;
}

uint8_t rc_client_map_leaderboard_format(int format)
{
  switch (format) {
    case RC_FORMAT_SECONDS:
    case RC_FORMAT_CENTISECS:
    case RC_FORMAT_MINUTES:
    case RC_FORMAT_SECONDS_AS_MINUTES:
    case RC_FORMAT_FRAMES:
      return RC_CLIENT_LEADERBOARD_FORMAT_TIME;

    case RC_FORMAT_SCORE:
      return RC_CLIENT_LEADERBOARD_FORMAT_SCORE;

    case RC_FORMAT_VALUE:
    case RC_FORMAT_FLOAT1:
    case RC_FORMAT_FLOAT2:
    case RC_FORMAT_FLOAT3:
    case RC_FORMAT_FLOAT4:
    case RC_FORMAT_FLOAT5:
    case RC_FORMAT_FLOAT6:
    case RC_FORMAT_FIXED1:
    case RC_FORMAT_FIXED2:
    case RC_FORMAT_FIXED3:
    case RC_FORMAT_TENS:
    case RC_FORMAT_HUNDREDS:
    case RC_FORMAT_THOUSANDS:
    case RC_FORMAT_UNSIGNED_VALUE:
    default:
      return RC_CLIENT_LEADERBOARD_FORMAT_VALUE;
  }
}

static void rc_client_copy_leaderboards(rc_client_load_state_t* load_state,
    rc_client_subset_info_t* subset,
    const rc_api_leaderboard_definition_t* leaderboard_definitions, uint32_t num_leaderboards)
{
  const rc_api_leaderboard_definition_t* read;
  const rc_api_leaderboard_definition_t* stop;
  rc_client_leaderboard_info_t* leaderboards;
  rc_client_leaderboard_info_t* leaderboard;
  rc_buffer_t* buffer;
  rc_preparse_state_t preparse;
  const char* memaddr;
  const char* ptr;
  size_t size;
  rc_lboard_t* lboard;
  int lboard_size;

  subset->leaderboards = NULL;
  subset->public_.num_leaderboards = num_leaderboards;

  if (num_leaderboards == 0)
    return;

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_lboard_t) /* lboard container */
      + (sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2) * 3 /* start/submit/cancel */
      + (sizeof(rc_value_t) + sizeof(rc_condset_t)) /* value */
      + sizeof(rc_condition_t) * 4 * 4 /* assume average of 4 conditions in each start/submit/cancel/value */
      + sizeof(rc_client_leaderboard_info_t);
  rc_buffer_reserve(&load_state->game->buffer, size * num_leaderboards);

  /* allocate the achievement array */
  size = sizeof(rc_client_leaderboard_info_t) * num_leaderboards;
  buffer = &load_state->game->buffer;
  leaderboard = leaderboards = (rc_client_leaderboard_info_t*)rc_buffer_alloc(buffer, size);
  memset(leaderboards, 0, size);

  /* copy the achievement data */
  read = leaderboard_definitions;
  stop = read + num_leaderboards;
  do {
    leaderboard->public_.title = rc_buffer_strcpy(buffer, read->title);
    leaderboard->public_.description = rc_buffer_strcpy(buffer, read->description);
    leaderboard->public_.id = read->id;
    leaderboard->public_.format = rc_client_map_leaderboard_format(read->format);
    leaderboard->public_.lower_is_better = read->lower_is_better;
    leaderboard->format = (uint8_t)read->format;
    leaderboard->hidden = (uint8_t)read->hidden;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, leaderboard->md5);

    ptr = strstr(memaddr, "VAL:");
    if (ptr != NULL) {
      /* calculate the DJB2 hash of the VAL portion of the string*/
      uint32_t hash = 5381;
      ptr += 4; /* skip 'VAL:' */
      while (*ptr && (ptr[0] != ':' || ptr[1] != ':'))
         hash = (hash << 5) + hash + *ptr++;
      leaderboard->value_djb2 = hash;
    }

    rc_init_preparse_state(&preparse);
    preparse.parse.existing_memrefs = load_state->game->runtime.memrefs;
    lboard = RC_ALLOC(rc_lboard_t, &preparse.parse);
    rc_parse_lboard_internal(lboard, memaddr, &preparse.parse);

    lboard_size = preparse.parse.offset;
    if (lboard_size < 0) {
      RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing leaderboard %u", lboard_size, read->id);
      leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
    }
    else {
      /* populate the item, using the communal memrefs pool */
      rc_reset_parse_state(&preparse.parse, rc_buffer_reserve(buffer, lboard_size));
      rc_preparse_reserve_memrefs(&preparse, load_state->game->runtime.memrefs);
      leaderboard->lboard = RC_ALLOC(rc_lboard_t, &preparse.parse);
      rc_parse_lboard_internal(leaderboard->lboard, memaddr, &preparse.parse);

      if (preparse.parse.offset < 0) {
        RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing leaderboard %u", preparse.parse.offset, read->id);
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      }
      else {
        rc_buffer_consume(buffer, (const uint8_t*)preparse.parse.buffer, (uint8_t*)preparse.parse.buffer + preparse.parse.offset);
      }

      rc_destroy_preparse_state(&preparse);
    }

    ++leaderboard;
    ++read;
  } while (read < stop);

  subset->leaderboards = leaderboards;
}

static void rc_client_fetch_game_sets_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_api_fetch_game_sets_response_t fetch_game_sets_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  result = rc_client_end_async(load_state->client, &load_state->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED) {
      rc_client_t* client = load_state->client;
      rc_client_load_aborted(load_state);
      RC_CLIENT_LOG_VERBOSE(client, "Load aborted while fetching game data");
    } else {
      rc_client_free_load_state(load_state);
    }
    return;
  }

  result = rc_api_process_fetch_game_sets_server_response(&fetch_game_sets_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &fetch_game_sets_response.response);

  outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message && result != RC_NOT_FOUND) {
    rc_client_load_error(load_state, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, load_state was free'd */
  }
  else if (fetch_game_sets_response.id == 0) {
    load_state->hash->game_id = 0;
    rc_client_process_resolved_hash(load_state);
  }
  else {
    rc_client_subset_info_t** next_subset;
    rc_client_subset_info_t* first_subset = NULL;
    uint32_t set_index;

    /* hash exists outside the load state - always update it */
    load_state->hash->game_id = fetch_game_sets_response.id;
    RC_CLIENT_LOG_INFO_FORMATTED(load_state->client, "Identified game: %u \"%s\" (%s)", load_state->hash->game_id, fetch_game_sets_response.title, load_state->hash->hash);

    if (load_state->hash->hash[0] != '[') {
      /* not [NO HASH] or [SUBSETxx] */
      load_state->game->public_.id = load_state->hash->game_id;
      load_state->game->public_.hash = load_state->hash->hash;
    }

    if (load_state->game->public_.console_id != RC_CONSOLE_UNKNOWN &&
        fetch_game_sets_response.console_id != load_state->game->public_.console_id) {
      RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Data for game %u is for console %u, expecting console %u",
        fetch_game_sets_response.id, fetch_game_sets_response.console_id, load_state->game->public_.console_id);
    }

    /* kick off the start session request while we process the game data */
    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION, 1);
    if (load_state->client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
      /* we can't unlock achievements without a session, lock spectator mode for the game */
      load_state->client->state.spectator_mode = RC_CLIENT_SPECTATOR_MODE_LOCKED;
    }
    else {
      rc_client_begin_start_session(load_state);
    }

    /* process the game data */
    next_subset = &first_subset;
    for (set_index = 0; set_index < fetch_game_sets_response.num_sets; ++set_index) {
      rc_api_achievement_set_definition_t* set = &fetch_game_sets_response.sets[set_index];
      rc_client_subset_info_t* subset;

      subset = (rc_client_subset_info_t*)rc_buffer_alloc(&load_state->game->buffer, sizeof(rc_client_subset_info_t));
      memset(subset, 0, sizeof(*subset));
      subset->public_.id = set->id;
      subset->active = 1;
      snprintf(subset->public_.badge_name, sizeof(subset->public_.badge_name), "%s", set->image_name);
      subset->public_.badge_url = rc_buffer_strcpy(&load_state->game->buffer, set->image_url);
      subset->public_.title = rc_buffer_strcpy(&load_state->game->buffer, set->title);

      rc_client_copy_achievements(load_state, subset, set->achievements, set->num_achievements);
      rc_client_copy_leaderboards(load_state, subset, set->leaderboards, set->num_leaderboards);

      if (set->type == RC_ACHIEVEMENT_SET_TYPE_CORE) {
        if (!first_subset)
          next_subset = &subset->next;
        subset->next = first_subset;
        first_subset = subset;
      }
      else {
        *next_subset = subset;
        next_subset = &subset->next;
      }
    }

    if (!first_subset) {
      rc_client_load_error(load_state, RC_NOT_FOUND, "Response contained no sets");
    } else {
      load_state->subset = first_subset;

      /* core set */
      rc_mutex_lock(&load_state->client->state.mutex);
      load_state->game->public_.title = rc_buffer_strcpy(&load_state->game->buffer, fetch_game_sets_response.title);
      load_state->game->subsets = first_subset;
      load_state->game->public_.badge_name = first_subset->public_.badge_name;
      load_state->game->public_.badge_url = first_subset->public_.badge_url;
      load_state->game->public_.console_id = fetch_game_sets_response.console_id;
      rc_mutex_unlock(&load_state->client->state.mutex);

      if (fetch_game_sets_response.rich_presence_script && fetch_game_sets_response.rich_presence_script[0]) {
        result = rc_runtime_activate_richpresence(&load_state->game->runtime, fetch_game_sets_response.rich_presence_script, NULL, 0);
        if (result != RC_OK) {
          RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing rich presence", result);
        }
      }

      if (load_state->client->callbacks.post_process_game_sets_response) {
        load_state->client->callbacks.post_process_game_sets_response(server_response,
          &fetch_game_sets_response, load_state->client, load_state->callback_userdata);
      }
    }

    outstanding_requests = rc_client_end_load_state(load_state);
    if (outstanding_requests < 0) {
      /* previous load state was aborted, load_state was free'd */
    }
    else if (outstanding_requests == 0) {
      if (load_state->client->state.allow_background_memory_reads)
        rc_client_activate_game(load_state, load_state->start_session_response);
      else
        rc_client_queue_activate_game(load_state);
    }
  }

  rc_api_destroy_fetch_game_sets_response(&fetch_game_sets_response);
}

static rc_client_game_info_t* rc_client_allocate_game(void)
{
  rc_client_game_info_t* game = (rc_client_game_info_t*)calloc(1, sizeof(*game));
  if (!game)
    return NULL;

  rc_buffer_init(&game->buffer);
  rc_runtime_init(&game->runtime);

  return game;
}

static int rc_client_attach_load_state(rc_client_t* client, rc_client_load_state_t* load_state)
{
  if (client->state.load == NULL) {
    rc_client_unload_game(client);

    if (load_state->game == NULL) {
      load_state->game = rc_client_allocate_game();
      if (!load_state->game) {
        if (load_state->callback)
          load_state->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, load_state->callback_userdata);

        return 0;
      }
    }

    rc_mutex_lock(&client->state.mutex);
    client->state.load = load_state;
    client->state.frames_processed = client->state.frames_at_last_ping = 0;
    rc_mutex_unlock(&client->state.mutex);
  }
  else if (client->state.load != load_state) {
    /* previous load was aborted */
    if (load_state->callback)
      load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);

    return 0;
  }

  return 1;
}

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

static void rc_client_external_load_state_callback(int result, const char* error_message, rc_client_t* client, void* userdata)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)userdata;
  int async_aborted;

  client = load_state->client;
  async_aborted = rc_client_end_async(client, &load_state->async_handle);
  if (async_aborted) {
    if (async_aborted != RC_CLIENT_ASYNC_DESTROYED) {
      RC_CLIENT_LOG_VERBOSE(client, "Load aborted during external loading");
    }

    rc_client_unload_game(client); /* unload the game from the external client */
    rc_client_free_load_state(load_state);
    return;
  }

  if (result != RC_OK) {
    rc_client_load_error(load_state, result, error_message);
    return;
  }

  rc_mutex_lock(&client->state.mutex);
  load_state->progress = (client->state.load == load_state) ?
    RC_CLIENT_LOAD_GAME_STATE_DONE : RC_CLIENT_LOAD_GAME_STATE_ABORTED;
  client->state.load = NULL;
  rc_mutex_unlock(&client->state.mutex);

  if (load_state->progress != RC_CLIENT_LOAD_GAME_STATE_DONE) {
    /* previous load state was aborted */
    if (load_state->callback)
      load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
  }
  else {
    /* keep partial game object for media_hash management */
    if (client->state.external_client) {
      const rc_client_game_t* info = rc_client_get_game_info(client);
      load_state->game->public_.console_id = info->console_id;
      client->game = load_state->game;
      load_state->game = NULL;
    }

    if (load_state->callback)
      load_state->callback(RC_OK, NULL, client, load_state->callback_userdata);
  }

  rc_client_free_load_state(load_state);
}

#endif

static void rc_client_initialize_unknown_game(rc_client_game_info_t* game)
{
  rc_client_subset_info_t* subset;
  char buffer[64];

  subset = (rc_client_subset_info_t*)rc_buffer_alloc(&game->buffer, sizeof(rc_client_subset_info_t));
  memset(subset, 0, sizeof(*subset));
  subset->public_.title = "";
  game->subsets = subset;

  game->public_.title = "Unknown Game";
  game->public_.badge_name = "";

  rc_client_get_image_url(buffer, sizeof(buffer), RC_IMAGE_TYPE_GAME, "000001");
  game->public_.badge_url = rc_buffer_strcpy(&game->buffer, buffer);
}

static void rc_client_process_resolved_hash(rc_client_load_state_t* load_state)
{
  rc_client_t* client = load_state->client;

  if (load_state->hash->game_id == 0) {
#ifdef RC_CLIENT_SUPPORTS_HASH
    char hash[33];

    if (rc_hash_iterate(hash, &load_state->hash_iterator)) {
      /* found another hash to try */
      load_state->hash_console_id = load_state->hash_iterator.consoles[load_state->hash_iterator.index - 1];
      rc_client_load_game(load_state, hash, NULL);
      return;
    }

    if (load_state->tried_hashes[1]) {
      /* multiple hashes were tried, create a CSV */
      size_t i;
      size_t count = 0;
      char* ptr;
      size_t size = 0;

      for (i = 0; i < sizeof(load_state->tried_hashes) / sizeof(load_state->tried_hashes[0]); ++i) {
        if (!load_state->tried_hashes[i])
          break;

        size += strlen(load_state->tried_hashes[i]->hash) + 1;
        count++;
      }

      ptr = (char*)rc_buffer_alloc(&load_state->game->buffer, size);
      load_state->game->public_.hash = ptr;
      for (i = 0; i < count; i++) {
        const size_t hash_len = strlen(load_state->tried_hashes[i]->hash);
        memcpy(ptr, load_state->tried_hashes[i]->hash, hash_len);
        ptr += hash_len;
        *ptr++ = ',';
      }
      *(ptr - 1) = '\0';

      load_state->game->public_.console_id = RC_CONSOLE_UNKNOWN;
    } else {
      /* only a single hash was tried, capture it */
      load_state->game->public_.console_id = load_state->hash_console_id;
      load_state->game->public_.hash = load_state->hash->hash;

      if (client->callbacks.identify_unknown_hash) {
        load_state->hash->game_id = client->callbacks.identify_unknown_hash(
            load_state->hash_console_id, load_state->hash->hash, client, load_state->callback_userdata);

        if (load_state->hash->game_id != 0) {
          load_state->hash->is_unknown = 1;
          RC_CLIENT_LOG_INFO_FORMATTED(load_state->client, "Client says to load game %u for unidentified hash %s",
            load_state->hash->game_id, load_state->hash->hash);
        }
      }
    }

    rc_hash_destroy_iterator(&load_state->hash_iterator); /* done with this now */
#else
    load_state->game->public_.console_id = RC_CONSOLE_UNKNOWN;
    load_state->game->public_.hash = load_state->hash->hash;
#endif /* RC_CLIENT_SUPPORTS_HASH */

    if (load_state->hash->game_id == 0) {
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
 #ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
      if (client->state.raintegration && client->state.raintegration->set_console_id) {
        if (load_state->game->public_.console_id != RC_CONSOLE_UNKNOWN)
          client->state.raintegration->set_console_id(load_state->game->public_.console_id);
      }
 #endif
      if (client->state.external_client) {
        if (client->state.external_client->load_unknown_game) {
          client->state.external_client->load_unknown_game(load_state->game->public_.hash);
          rc_client_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
          return;
        }
        /* no external method specifically for unknown game, just pass the hash through to begin_load_game below */
      }
      else {
#endif
        rc_client_initialize_unknown_game(load_state->game);

        client->game = load_state->game;
        load_state->game = NULL;

        rc_client_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
        return;
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
      }
#endif
    }
  }

  if (load_state->hash->hash[0] != '[') { /* not [NO HASH] or [SUBSETxx] */
    load_state->game->public_.id = load_state->hash->game_id;
    load_state->game->public_.hash = load_state->hash->hash;
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->add_game_hash)
      client->state.external_client->add_game_hash(load_state->hash->hash, load_state->hash->game_id);

    if (client->state.external_client->begin_load_game) {
      rc_client_begin_async(client, &load_state->async_handle);
      client->state.external_client->begin_load_game(client, load_state->hash->hash, rc_client_external_load_state_callback, load_state);
    }
    return;
  }
#endif

  rc_client_begin_fetch_game_sets(load_state);
}

void rc_client_load_unknown_game(rc_client_t* client, const char* tried_hashes)
{
  rc_client_game_info_t* game;

  game = rc_client_allocate_game();
  if (!game)
    return;

  rc_client_initialize_unknown_game(game);
  game->public_.console_id = RC_CONSOLE_UNKNOWN;

  if (strlen(tried_hashes) == 32) { /* only one hash tried, add it to the list */
    rc_client_game_hash_t* game_hash = rc_client_find_game_hash(client, tried_hashes);
    game_hash->game_id = 0;
    game->public_.hash = game_hash->hash;
  }
  else {
    game->public_.hash = rc_buffer_strcpy(&game->buffer, tried_hashes);
  }

  rc_client_unload_game(client);
  client->game = game;
}

static void rc_client_begin_fetch_game_sets(rc_client_load_state_t* load_state)
{
  rc_api_fetch_game_sets_request_t fetch_game_sets_request;
  rc_client_t* client = load_state->client;
  rc_api_request_t request;
  int result;

  rc_mutex_lock(&client->state.mutex);
  result = client->state.user;
  if (result == RC_CLIENT_USER_STATE_LOGIN_REQUESTED)
    load_state->progress = RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN;
  else
    load_state->progress = RC_CLIENT_LOAD_GAME_STATE_FETCHING_GAME_DATA;
  rc_mutex_unlock(&client->state.mutex);

  switch (result) {
    case RC_CLIENT_USER_STATE_LOGGED_IN:
      break;

    case RC_CLIENT_USER_STATE_LOGIN_REQUESTED:
      /* do nothing, this function will be called again after login completes */
      return;

    default:
      rc_client_load_error(load_state, RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED));
      return;
  }

  memset(&fetch_game_sets_request, 0, sizeof(fetch_game_sets_request));
  fetch_game_sets_request.username = client->user.username;
  fetch_game_sets_request.api_token = client->user.token;

  if (load_state->hash->is_unknown) /* lookup failed, but client provided a mapping */
    fetch_game_sets_request.game_id = load_state->hash->game_id;
  else
    fetch_game_sets_request.game_hash = load_state->hash->hash;

  result = rc_api_init_fetch_game_sets_request_hosted(&request, &fetch_game_sets_request, &client->state.host);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
    return;
  }

  rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME, 1);
  RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Fetching data for hash %s", fetch_game_sets_request.game_hash);

  rc_client_begin_async(client, &load_state->async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_game_sets_callback, load_state, client);

  rc_api_destroy_request(&request);
}

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
static void rc_client_identify_game_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* client = load_state->client;
  rc_api_resolve_hash_response_t resolve_hash_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &load_state->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED) {
      rc_client_load_aborted(load_state);
      RC_CLIENT_LOG_VERBOSE(client, "Load aborted during game identification");
    } else {
      rc_client_free_load_state(load_state);
    }
    return;
  }

  result = rc_api_process_resolve_hash_server_response(&resolve_hash_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &resolve_hash_response.response);

  if (error_message) {
    rc_client_end_load_state(load_state);
    rc_client_load_error(load_state, result, error_message);
  }
  else {
    /* hash exists outside the load state - always update it */
    load_state->hash->game_id = resolve_hash_response.game_id;
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    /* have to call end_load_state after updating hash in case the load_state gets free'd */
    outstanding_requests = rc_client_end_load_state(load_state);
    if (outstanding_requests < 0) {
      /* previous load state was aborted, load_state was free'd */
    }
    else {
      rc_client_process_resolved_hash(load_state);
    }
  }

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}
#endif

rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* client, const char* hash)
{
  rc_client_game_hash_t* game_hash;

  rc_mutex_lock(&client->state.mutex);
  game_hash = client->hashes;
  while (game_hash) {
    if (strcasecmp(game_hash->hash, hash) == 0)
      break;

    game_hash = game_hash->next;
  }

  if (!game_hash) {
    game_hash = (rc_client_game_hash_t*)rc_buffer_alloc(&client->state.buffer, sizeof(rc_client_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    snprintf(game_hash->hash, sizeof(game_hash->hash), "%s", hash);
    game_hash->game_id = RC_CLIENT_UNKNOWN_GAME_ID;
    game_hash->next = client->hashes;
    client->hashes = game_hash;
  }
  rc_mutex_unlock(&client->state.mutex);

  return game_hash;
}

void rc_client_add_game_hash(rc_client_t* client, const char* hash, uint32_t game_id)
{
  /* store locally, even if passing to external client */
  rc_client_game_hash_t* game_hash = rc_client_find_game_hash(client, hash);
  game_hash->game_id = game_id;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->add_game_hash)
    client->state.external_client->add_game_hash(hash, game_id);
#endif
}

static rc_client_async_handle_t* rc_client_load_game(rc_client_load_state_t* load_state,
  const char* hash, const char* file_path)
{
  rc_client_t* client = load_state->client;
  rc_client_game_hash_t* old_hash;
#ifdef RC_CLIENT_SUPPORTS_HASH
  size_t i;
#endif

  if (!rc_client_attach_load_state(client, load_state)) {
    rc_client_free_load_state(load_state);
    return NULL;
  }

  old_hash = load_state->hash;
  load_state->hash = rc_client_find_game_hash(client, hash);

#ifdef RC_CLIENT_SUPPORTS_HASH
  i = 0;
  do {
    if (!load_state->tried_hashes[i]) {
      load_state->tried_hashes[i] = load_state->hash;
      break;
    }

    if (load_state->tried_hashes[i] == load_state->hash)
      break;

    if (++i == sizeof(load_state->tried_hashes) / sizeof(load_state->tried_hashes[0])) {
      RC_CLIENT_LOG_VERBOSE(client, "tried_hashes buffer is full");
      break;
    }
  } while (1);
#endif

  if (file_path) {
    rc_client_media_hash_t* media_hash =
        (rc_client_media_hash_t*)rc_buffer_alloc(&load_state->game->buffer, sizeof(*media_hash));
    media_hash->game_hash = load_state->hash;
    media_hash->path_djb2 = rc_djb2(file_path);
    media_hash->next = load_state->game->media_hash;
    load_state->game->media_hash = media_hash;
  }
  else if (load_state->game->media_hash && load_state->game->media_hash->game_hash == old_hash) {
    load_state->game->media_hash->game_hash = load_state->hash;
  }

  if (load_state->hash->game_id == 0) {
    rc_client_process_resolved_hash(load_state);
  }
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  else if (load_state->hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID &&
           client->state.external_client && client->state.external_client->add_game_hash) {
    /* if an add_game_hash external handler exists, do the identification locally, then
     * pass the resulting game_id/hash to the external client */
    rc_api_resolve_hash_request_t resolve_hash_request;
    rc_api_request_t request;
    int result;

    memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
    resolve_hash_request.game_hash = hash;

    result = rc_api_init_resolve_hash_request_hosted(&request, &resolve_hash_request, &client->state.host);
    if (result != RC_OK) {
      rc_client_load_error(load_state, result, rc_error_str(result));
      return NULL;
    }

    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME, 1);

    rc_client_begin_async(client, &load_state->async_handle);
    client->callbacks.server_call(&request, rc_client_identify_game_callback, load_state, client);

    rc_api_destroy_request(&request);
  }
  else if (load_state->hash->game_id != RC_CLIENT_UNKNOWN_GAME_ID &&
           client->state.external_client && client->state.external_client->begin_load_game) {
    rc_client_begin_async(client, &load_state->async_handle);
    client->state.external_client->begin_load_game(client, load_state->hash->hash, rc_client_external_load_state_callback, load_state);
  }
#endif
  else {
    rc_client_begin_fetch_game_sets(load_state);
  }

  return (client->state.load == load_state) ? &load_state->async_handle : NULL;
}

static void rc_client_abort_load_in_progress(rc_client_t* client)
{
  rc_client_load_state_t* load_state;

  rc_mutex_lock(&client->state.mutex);

  load_state = client->state.load;
  if (load_state) {
    /* this mimics rc_client_abort_async without nesting the lock */
    load_state->async_handle.aborted = RC_CLIENT_ASYNC_ABORTED;

    client->state.load = NULL;
  }

  rc_mutex_unlock(&client->state.mutex);

  if (load_state && load_state->callback)
    load_state->callback(RC_ABORTED, "The requested game is no longer active", load_state->client, load_state->callback_userdata);
}

rc_client_async_handle_t* rc_client_begin_load_game(rc_client_t* client, const char* hash, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_load_state_t* load_state;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!hash || !hash[0]) {
    callback(RC_INVALID_STATE, "hash is required", client, callback_userdata);
    return NULL;
  }

  rc_client_abort_load_in_progress(client);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_load_game)
    return client->state.external_client->begin_load_game(client, hash, callback, callback_userdata);
#endif

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  if (!load_state) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  load_state->client = client;
  load_state->callback = callback;
  load_state->callback_userdata = callback_userdata;

  return rc_client_load_game(load_state, hash, NULL);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

rc_hash_iterator_t* rc_client_get_load_state_hash_iterator(rc_client_t* client)
{
  if (client && client->state.load)
    return &client->state.load->hash_iterator;

  return NULL;
}

static void rc_client_log_hash_message_verbose(const char* message, const rc_hash_iterator_t* iterator)
{
  const rc_client_load_state_t* load_state = (const rc_client_load_state_t*)iterator->userdata;
  if (load_state->client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO)
    rc_client_log_message(load_state->client, message);
}

static void rc_client_log_hash_message_error(const char* message, const rc_hash_iterator_t* iterator)
{
  const rc_client_load_state_t* load_state = (const rc_client_load_state_t*)iterator->userdata;
  if (load_state->client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR)
    rc_client_log_message(load_state->client, message);
}

void rc_client_set_hash_callbacks(rc_client_t* client, const struct rc_hash_callbacks* callbacks)
{
  memcpy(&client->callbacks.hash, callbacks, sizeof(*callbacks));

  if (!callbacks->verbose_message)
    client->callbacks.hash.verbose_message = rc_client_log_hash_message_verbose;
  if (!callbacks->error_message)
    client->callbacks.hash.error_message = rc_client_log_hash_message_error;
}

rc_client_async_handle_t* rc_client_begin_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path,
    const uint8_t* data, size_t data_size,
    rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_load_state_t* load_state;
  char hash[33];

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  rc_client_abort_load_in_progress(client);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  /* if a add_game_hash handler exists, do the identification locally, then pass the
   * resulting game_id/hash to the external client */
  if (client->state.external_client && !client->state.external_client->add_game_hash) {
    if (client->state.external_client->begin_identify_and_load_game)
      return client->state.external_client->begin_identify_and_load_game(client, console_id, file_path, data, data_size, callback, callback_userdata);
  }
#endif

  if (data) {
    if (file_path) {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %zu bytes at %p (%s)", data_size, data, file_path);
    }
    else {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %zu bytes at %p", data_size, data);
    }
  }
  else if (file_path) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %s", file_path);
  }
  else {
    callback(RC_INVALID_STATE, "either data or file_path is required", client, callback_userdata);
    return NULL;
  }

  if (!file_path)
    file_path = "?";

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  if (!load_state) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }
  load_state->client = client;
  load_state->callback = callback;
  load_state->callback_userdata = callback_userdata;

  /* initialize the iterator */
  rc_hash_initialize_iterator(&load_state->hash_iterator, file_path, data, data_size);
  rc_hash_merge_callbacks(&load_state->hash_iterator, &client->callbacks.hash);
  load_state->hash_iterator.userdata = load_state;

  if (!load_state->hash_iterator.callbacks.verbose_message)
    load_state->hash_iterator.callbacks.verbose_message = rc_client_log_hash_message_verbose;
  if (!load_state->hash_iterator.callbacks.error_message)
    load_state->hash_iterator.callbacks.error_message = rc_client_log_hash_message_error;

  /* calculate the hash */
  if (console_id == RC_CONSOLE_UNKNOWN) {
    if (!rc_hash_iterate(hash, &load_state->hash_iterator)) {
      rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
      return NULL;
    }

    load_state->hash_console_id = load_state->hash_iterator.consoles[load_state->hash_iterator.index - 1];
  }
  else {
    /* ASSERT: hash_iterator->index and hash_iterator->consoles[0] will be 0 from calloc */
    load_state->hash_console_id = console_id;

    /* prevent initializing the iterator so it won't try other consoles in rc_client_process_resolved_hash */
    load_state->hash_iterator.index = 0;

    if (!rc_hash_generate(hash, console_id, &load_state->hash_iterator)) {
      rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
      return NULL;
    }
  }

  return rc_client_load_game(load_state, hash, file_path);
}

#endif /* RC_CLIENT_SUPPORTS_HASH */

int rc_client_get_load_game_state(const rc_client_t* client)
{
  int state = RC_CLIENT_LOAD_GAME_STATE_NONE;
  if (client) {
    const rc_client_load_state_t* load_state = client->state.load;
    if (load_state)
      state = load_state->progress;
    else if (client->game)
      state = RC_CLIENT_LOAD_GAME_STATE_DONE;
  }

  return state;
}

int rc_client_is_game_loaded(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    const rc_client_game_t* game = rc_client_get_game_info(client);
    return (game && game->id != 0);
  }
#endif

  return (client->game && client->game->public_.id != 0);
}

static void rc_client_game_mark_ui_to_be_hidden(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;
  rc_client_subset_info_t* subset;

  for (subset = game->subsets; subset; subset = subset->next) {
    achievement = subset->achievements;
    achievement_stop = achievement + subset->public_.num_achievements;
    for (; achievement < achievement_stop; ++achievement) {
      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE &&
          achievement->trigger && achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
      }
    }

    leaderboard = subset->leaderboards;
    leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < leaderboard_stop; ++leaderboard) {
      if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING)
        rc_client_release_leaderboard_tracker(game, leaderboard);
    }
  }

  rc_client_hide_progress_tracker(client, game);
}

void rc_client_unload_game(rc_client_t* client)
{
  rc_client_game_info_t* game;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->unload_game) {
    client->state.external_client->unload_game();

    /* a game object may have been allocated to manage hashes */
    game = client->game;
    client->game = NULL;
    if (game != NULL)
      rc_client_free_game(game);

    return;
  }
#endif

  rc_mutex_lock(&client->state.mutex);

  game = client->game;
  client->game = NULL;

  if (client->state.load) {
    /* this mimics rc_client_abort_async without nesting the lock */
    client->state.load->async_handle.aborted = RC_CLIENT_ASYNC_ABORTED;

    /* if the game is still being loaded, let the load process clean it up */
    if (client->state.load->game == game)
      game = NULL;

    client->state.load = NULL;
  }

  if (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_LOCKED)
    client->state.spectator_mode = RC_CLIENT_SPECTATOR_MODE_ON;

  if (game != NULL) {
    rc_client_scheduled_callback_data_t** last;
    rc_client_scheduled_callback_data_t* next;

    rc_client_game_mark_ui_to_be_hidden(client, game);

    last = &client->state.scheduled_callbacks;
    do {
      next = *last;
      if (!next)
        break;

      /* remove rich presence ping scheduled event for game */
      if (next->callback == rc_client_ping && next->related_id == game->public_.id) {
        *last = next->next;
        continue;
      }

      last = &next->next;
    } while (1);
  }

  rc_mutex_unlock(&client->state.mutex);

  if (game != NULL) {
    rc_client_raise_pending_events(client, game);

    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unloading game %u", game->public_.id);
    rc_client_free_game(game);
  }
}

static void rc_client_change_media_internal(rc_client_t* client, const rc_client_game_hash_t* game_hash, rc_client_callback_t callback, void* callback_userdata)
{
  client->game->public_.hash = game_hash->hash;

  if (game_hash->game_id == client->game->public_.id) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to valid media for game %u: %s", game_hash->game_id, game_hash->hash);
  }
  else if (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) {
    RC_CLIENT_LOG_INFO(client, "Switching to unknown media");
  }
  else if (game_hash->game_id == 0) {
    if (client->state.hardcore) {
      RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabling hardcore for unidentified media: %s", game_hash->hash);
      rc_client_set_hardcore_enabled(client, 0);
      callback(RC_HARDCORE_DISABLED, "Hardcore disabled. Unidentified media inserted.", client, callback_userdata);
      return;
    }

    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to unrecognized media: %s", game_hash->hash);
  }
  else {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to known media for game %u: %s", game_hash->game_id, game_hash->hash);
  }

  callback(RC_OK, NULL, client, callback_userdata);
}

static void rc_client_identify_changed_media_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* client = load_state->client;
  rc_api_resolve_hash_response_t resolve_hash_response;

  int result = rc_api_process_resolve_hash_server_response(&resolve_hash_response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &resolve_hash_response.response);

  const int async_aborted = rc_client_end_async(client, &load_state->async_handle);
  if (async_aborted) {
    if (async_aborted != RC_CLIENT_ASYNC_DESTROYED) {
      RC_CLIENT_LOG_VERBOSE(client, "Media change aborted");
      /* if lookup succeeded, still capture the new hash */
      if (result == RC_OK)
        load_state->hash->game_id = resolve_hash_response.game_id;
    }
  }
  else if (client->game != load_state->game) {
    /* loaded game changed. return success regardless of result */
    load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
  }
  else if (error_message) {
    load_state->callback(result, error_message, client, load_state->callback_userdata);
  }
  else {
    load_state->hash->game_id = resolve_hash_response.game_id;

    if (resolve_hash_response.game_id != 0) {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);
    }

    rc_client_change_media_internal(client, load_state->hash, load_state->callback, load_state->callback_userdata);
  }

  free(load_state);
  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static rc_client_async_handle_t* rc_client_begin_change_media_internal(rc_client_t* client,
    rc_client_game_info_t* game, rc_client_game_hash_t* game_hash, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_load_state_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_api_request_t request;
  int result;

  if (game_hash->game_id != RC_CLIENT_UNKNOWN_GAME_ID || /* previously looked up */
      game_hash->hash[0] == '[') { /* internal use - don't try to look up */
    rc_client_change_media_internal(client, game_hash, callback, callback_userdata);
    return NULL;
  }

  /* call the server to make sure the hash is valid for the loaded game */
  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = game_hash->hash;

  result = rc_api_init_resolve_hash_request_hosted(&request, &resolve_hash_request, &client->state.host);
  if (result != RC_OK) {
    callback(result, rc_error_str(result), client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_load_state_t*)calloc(1, sizeof(rc_client_load_state_t));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->client = client;
  callback_data->hash = game_hash;
  callback_data->game = game;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_identify_changed_media_callback, callback_data, client);

  rc_api_destroy_request(&request);

  /* if handle is no longer valid, the async operation completed synchronously */
  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

static rc_client_game_info_t* rc_client_check_pending_media(rc_client_t* client, const rc_client_pending_media_t* media)
{
  rc_client_game_info_t* game;
  rc_client_pending_media_t* pending_media = NULL;

  rc_mutex_lock(&client->state.mutex);
  if (client->state.load) {
    game = client->state.load->game;
    if (!game || game->public_.console_id == 0) {
      /* still waiting for game data */
      pending_media = client->state.load->pending_media;
      if (pending_media)
        rc_client_free_pending_media(pending_media);

      pending_media = (rc_client_pending_media_t*)malloc(sizeof(*pending_media));
      if (!pending_media) {
        rc_mutex_unlock(&client->state.mutex);
        media->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, media->callback_userdata);
        return NULL;
      }

      memcpy(pending_media, media, sizeof(*pending_media));
      if (media->hash)
        pending_media->hash = strdup(media->hash);

#ifdef RC_CLIENT_SUPPORTS_HASH
      if (media->file_path)
        pending_media->file_path = strdup(media->file_path);

      if (media->data && media->data_size) {
        pending_media->data = (uint8_t*)malloc(media->data_size);
        if (!pending_media->data) {
          rc_mutex_unlock(&client->state.mutex);
          media->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, media->callback_userdata);
          return NULL;
        }
        memcpy(pending_media->data, media->data, media->data_size);
      } else {
        pending_media->data = NULL;
      }
#endif

      client->state.load->pending_media = pending_media;
    }
  }
  else {
    game = client->game;
  }
  rc_mutex_unlock(&client->state.mutex);

  if (!game) {
    media->callback(RC_NO_GAME_LOADED, rc_error_str(RC_NO_GAME_LOADED), client, media->callback_userdata);
    return NULL;
  }

  /* still waiting for game data - don't call callback - it's queued */
  if (pending_media)
    return NULL;

  return game;
}

#ifdef RC_CLIENT_SUPPORTS_HASH

rc_client_async_handle_t* rc_client_begin_identify_and_change_media(rc_client_t* client, const char* file_path,
    const uint8_t* data, size_t data_size, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_pending_media_t media;
  rc_client_game_hash_t* game_hash = NULL;
  rc_client_game_info_t* game;
  rc_client_media_hash_t* media_hash;
  uint32_t path_djb2;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!data && !file_path) {
    callback(RC_INVALID_STATE, "either data or file_path is required", client, callback_userdata);
    return NULL;
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && !client->state.external_client->begin_change_media) {
    if (client->state.external_client->begin_identify_and_change_media)
      return client->state.external_client->begin_identify_and_change_media(client, file_path, data, data_size, callback, callback_userdata);
  }
#endif

  memset(&media, 0, sizeof(media));
  media.file_path = file_path;
  media.data = (uint8_t*)data;
  media.data_size = data_size;
  media.callback = callback;
  media.callback_userdata = callback_userdata;

  game = rc_client_check_pending_media(client, &media);
  if (game == NULL)
    return NULL;

  /* check to see if we've already hashed this file */
  path_djb2 = rc_djb2(file_path);
  rc_mutex_lock(&client->state.mutex);
  for (media_hash = game->media_hash; media_hash; media_hash = media_hash->next) {
    if (media_hash->path_djb2 == path_djb2) {
      game_hash = media_hash->game_hash;
      break;
    }
  }
  rc_mutex_unlock(&client->state.mutex);

  if (!game_hash) {
    char hash[33];
    int result;

    if (data != NULL)
      result = rc_hash_generate_from_buffer(hash, game->public_.console_id, data, data_size);
    else
      result = rc_hash_generate_from_file(hash, game->public_.console_id, file_path);

    if (!result) {
      /* when changing discs, if the disc is not supported by the system, allow it. this is
       * primarily for games that support user-provided audio CDs, but does allow using discs
       * from other systems for games that leverage user-provided discs. */
      strcpy_s(hash, sizeof(hash), "[NO HASH]");
    }

    game_hash = rc_client_find_game_hash(client, hash);

    media_hash = (rc_client_media_hash_t*)rc_buffer_alloc(&game->buffer, sizeof(*media_hash));
    media_hash->game_hash = game_hash;
    media_hash->path_djb2 = path_djb2;

    rc_mutex_lock(&client->state.mutex);
    media_hash->next = game->media_hash;
    game->media_hash = media_hash;
    rc_mutex_unlock(&client->state.mutex);

    if (!result) {
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
      if (client->state.external_client && client->state.external_client->begin_change_media)
        return client->state.external_client->begin_change_media(client, game_hash->hash, callback, callback_userdata);
#endif

      rc_client_change_media_internal(client, game_hash, callback, callback_userdata);
      return NULL;
    }
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->add_game_hash)
      client->state.external_client->add_game_hash(game_hash->hash, game_hash->game_id);
    if (client->state.external_client->begin_change_media)
      return client->state.external_client->begin_change_media(client, game_hash->hash, callback, callback_userdata);
  }
#endif

  return rc_client_begin_change_media_internal(client, game, game_hash, callback, callback_userdata);
}

#endif /* RC_CLIENT_SUPPORTS_HASH */

rc_client_async_handle_t* rc_client_begin_change_media(rc_client_t* client, const char* hash,
    rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_pending_media_t media;
  rc_client_game_hash_t* game_hash;
  rc_client_game_info_t* game;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!hash || !hash[0]) {
    callback(RC_INVALID_STATE, "hash is required", client, callback_userdata);
    return NULL;
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_change_media) {
    return client->state.external_client->begin_change_media(client, hash, callback, callback_userdata);
  }
#endif

  memset(&media, 0, sizeof(media));
  media.hash = hash;
  media.callback = callback;
  media.callback_userdata = callback_userdata;

  game = rc_client_check_pending_media(client, &media);
  if (game == NULL)
    return NULL;

  /* check to see if we've already hashed this file. */
  game_hash = rc_client_find_game_hash(client, hash);
  return rc_client_begin_change_media_internal(client, game, game_hash, callback, callback_userdata);
}

const rc_client_game_t* rc_client_get_game_info(const rc_client_t* client)
{
  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_game_info_v3)
      return client->state.external_client->get_game_info_v3();

    if (client->state.external_client->get_game_info)
      return rc_client_external_convert_v1_game(client, client->state.external_client->get_game_info());
  }
#endif

  return client->game ? &client->game->public_ : NULL;
}

int rc_client_game_get_image_url(const rc_client_game_t* game, char buffer[], size_t buffer_size)
{
  if (!game)
    return RC_INVALID_STATE;

  if (game->badge_url) {
    const size_t len = strlen(game->badge_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, game->badge_url, len + 1);
    return RC_OK;
  }

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_GAME, game->badge_name);
}

/* ===== Subsets ===== */

const rc_client_subset_t* rc_client_get_subset_info(rc_client_t* client, uint32_t subset_id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_subset_info_v3)
      return client->state.external_client->get_subset_info_v3(subset_id);

    if (client->state.external_client->get_subset_info)
      return rc_client_external_convert_v1_subset(client, client->state.external_client->get_subset_info(subset_id));
  }
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (subset->public_.id == subset_id)
      return &subset->public_;
  }

  return NULL;
}

rc_client_subset_list_t* rc_client_create_subset_list(rc_client_t* client)
{
  rc_client_subset_list_info_t* list;
  const rc_client_subset_info_t* subset;
  const rc_client_subset_t** subset_ptr;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t num_subsets = 0;

  if (!client)
    return (rc_client_subset_list_t*)calloc(1, list_size);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->create_subset_list)
    return (rc_client_subset_list_t*)client->state.external_client->create_subset_list();
#endif

  if (!client->game)
    return (rc_client_subset_list_t*)calloc(1, list_size);

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      num_subsets++;
  }

  list = (rc_client_subset_list_info_t*)malloc(list_size + num_subsets * sizeof(rc_client_subset_t*));
  list->public_.subsets = subset_ptr = (const rc_client_subset_t**)((uint8_t*)list + list_size);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      *subset_ptr++ = &subset->public_;
  }

  rc_mutex_unlock(&client->state.mutex);

  list->destroy_func = NULL;
  list->public_.num_subsets = (uint32_t)(subset_ptr - list->public_.subsets);
  return &list->public_;
}

void rc_client_destroy_subset_list(rc_client_subset_list_t* list)
{
  rc_client_subset_list_info_t* info = (rc_client_subset_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

/* ===== Fetch Game Hashes ===== */

typedef struct rc_client_fetch_hash_library_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_hash_library_callback_t callback;
  void* callback_userdata;
  uint32_t console_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_hash_library_callback_data_t;

static void rc_client_fetch_hash_library_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_hash_library_callback_data_t* hashlib_callback_data =
    (rc_client_fetch_hash_library_callback_data_t*)callback_data;
  rc_client_t* client = hashlib_callback_data->client;
  rc_api_fetch_hash_library_response_t hashlib_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &hashlib_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch hash library aborted");

    free(hashlib_callback_data);
    return;
  }

  result = rc_api_process_fetch_hash_library_server_response(&hashlib_response, server_response);
  error_message =
    rc_client_server_error_message(&result, server_response->http_status_code, &hashlib_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch hash library for console %u failed: %s",
                                hashlib_callback_data->console_id, error_message);
    hashlib_callback_data->callback(result, error_message, NULL, client, hashlib_callback_data->callback_userdata);
  } else {
    rc_client_hash_library_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_hash_library_entry_t) * hashlib_response.num_entries;
    list = (rc_client_hash_library_t*)malloc(list_size);
    if (!list) {
      hashlib_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                      hashlib_callback_data->callback_userdata);
    } else {
      rc_client_hash_library_entry_t* entry = list->entries =
        (rc_client_hash_library_entry_t*)((uint8_t*)list + sizeof(*list));
      const rc_api_hash_library_entry_t* hlentry = hashlib_response.entries;
      const rc_api_hash_library_entry_t* stop = hlentry + hashlib_response.num_entries;

      for (; hlentry < stop; ++hlentry, ++entry) {
        snprintf(entry->hash, sizeof(entry->hash), "%s", hlentry->hash);
        entry->game_id = hlentry->game_id;
      }

      list->num_entries = hashlib_response.num_entries;

      hashlib_callback_data->callback(RC_OK, NULL, list, client, hashlib_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_hash_library_response(&hashlib_response);
  free(hashlib_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_hash_library(rc_client_t* client, uint32_t console_id,
                                                             rc_client_fetch_hash_library_callback_t callback,
                                                             void* callback_userdata)
{
  rc_api_fetch_hash_library_request_t api_params;
  rc_client_fetch_hash_library_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.console_id = console_id;
  result = rc_api_init_fetch_hash_library_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_hash_library_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->console_id = console_id;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_hash_library_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_hash_library(rc_client_hash_library_t* list)
{
  free(list);
}

/* ===== Fetch Game Titles ===== */

typedef struct rc_client_fetch_game_titles_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_game_titles_callback_t callback;
  void* callback_userdata;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_game_titles_callback_data_t;

static void rc_client_fetch_game_titles_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_game_titles_callback_data_t* titles_callback_data =
    (rc_client_fetch_game_titles_callback_data_t*)callback_data;
  rc_client_t* client = titles_callback_data->client;
  rc_api_fetch_game_titles_response_t titles_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &titles_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch game titles aborted");

    free(titles_callback_data);
    return;
  }

  result = rc_api_process_fetch_game_titles_server_response(&titles_response, server_response);
  error_message =
    rc_client_server_error_message(&result, server_response->http_status_code, &titles_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch game titles failed: %s", error_message);
    titles_callback_data->callback(result, error_message, NULL, client, titles_callback_data->callback_userdata);
  } else {
    rc_client_game_title_list_t* list;
    size_t strings_size = 0;
    const rc_api_game_title_entry_t* src;
    const rc_api_game_title_entry_t* stop;
    size_t list_size;

    /* calculate string buffer size */
    for (src = titles_response.entries, stop = src + titles_response.num_entries; src < stop; ++src) {
      if (src->title)
        strings_size += strlen(src->title) + 1;
      if (src->image_url)
        strings_size += strlen(src->image_url) + 1;
    }

    list_size = sizeof(*list) + sizeof(rc_client_game_title_entry_t) * titles_response.num_entries + strings_size;
    list = (rc_client_game_title_list_t*)malloc(list_size);
    if (!list) {
      titles_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                     titles_callback_data->callback_userdata);
    } else {
      rc_client_game_title_entry_t* entry = list->entries =
        (rc_client_game_title_entry_t*)((uint8_t*)list + sizeof(*list));
      char* strings = (char*)((uint8_t*)list + sizeof(*list) +
                              sizeof(rc_client_game_title_entry_t) * titles_response.num_entries);

      for (src = titles_response.entries, stop = src + titles_response.num_entries; src < stop; ++src, ++entry) {
        entry->game_id = src->id;

        if (src->title) {
          const size_t len = strlen(src->title) + 1;
          entry->title = strings;
          memcpy(strings, src->title, len);
          strings += len;
        } else {
          entry->title = NULL;
        }

        if (src->image_name)
          snprintf(entry->badge_name, sizeof(entry->badge_name), "%s", src->image_name);
        else
          entry->badge_name[0] = '\0';

        if (src->image_url) {
          const size_t len = strlen(src->image_url) + 1;
          entry->badge_url = strings;
          memcpy(strings, src->image_url, len);
          strings += len;
        }
        else {
          entry->badge_url = NULL;
        }
      }

      list->num_entries = titles_response.num_entries;

      titles_callback_data->callback(RC_OK, NULL, list, client, titles_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_game_titles_response(&titles_response);
  free(titles_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_game_titles(rc_client_t* client, const uint32_t* game_ids,
                                                            uint32_t num_game_ids,
                                                            rc_client_fetch_game_titles_callback_t callback,
                                                            void* callback_userdata)
{
  rc_api_fetch_game_titles_request_t api_params;
  rc_client_fetch_game_titles_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }

  if (!game_ids || num_game_ids == 0) {
    callback(RC_INVALID_STATE, "game_ids is required", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.game_ids = game_ids;
  api_params.num_game_ids = num_game_ids;
  result = rc_api_init_fetch_game_titles_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_game_titles_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_game_titles_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_game_title_list(rc_client_game_title_list_t* list)
{
  free(list);
}

/* ===== Achievements ===== */

static void rc_client_update_achievement_display_information(rc_client_t* client, rc_client_achievement_info_t* achievement, time_t recent_unlock_time)
{
  uint8_t new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN;
  uint32_t new_measured_value = 0;

  if (achievement->public_.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
    return;

  achievement->public_.measured_progress[0] = '\0';

  if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) {
    /* achievement unlocked */
    if (achievement->public_.unlock_time >= recent_unlock_time) {
      new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED;
    } else {
      new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;

      if (client->state.disconnect && rc_client_is_award_achievement_pending(client, achievement->public_.id))
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED;
    }
  }
  else {
    /* active achievement */
    new_bucket = (achievement->public_.category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL) ?
        RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL : RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;

    if (achievement->trigger) {
      if (achievement->trigger->measured_target) {
        if (achievement->trigger->measured_value == RC_MEASURED_UNKNOWN) {
          /* value hasn't been initialized yet, leave progress string empty */
        }
        else if (achievement->trigger->measured_value == 0) {
          /* value is 0, leave progress string empty. update progress to 0.0 */
          achievement->public_.measured_percent = 0.0;
        }
        else {
          /* clamp measured value at target (can't get more than 100%) */
          new_measured_value = (achievement->trigger->measured_value > achievement->trigger->measured_target) ?
              achievement->trigger->measured_target : achievement->trigger->measured_value;

          achievement->public_.measured_percent = ((float)new_measured_value * 100) / (float)achievement->trigger->measured_target;

          if (!achievement->trigger->measured_as_percent) {
            char* ptr = achievement->public_.measured_progress;
            const int buffer_size = (int)sizeof(achievement->public_.measured_progress);
            const int chars = rc_format_value(ptr, buffer_size, (int32_t)new_measured_value, RC_FORMAT_UNSIGNED_VALUE);
            ptr[chars] = '/';
            rc_format_value(ptr + chars + 1, buffer_size - chars - 1, (int32_t)achievement->trigger->measured_target, RC_FORMAT_UNSIGNED_VALUE);
          }
          else if (achievement->public_.measured_percent >= 1.0) {
            snprintf(achievement->public_.measured_progress, sizeof(achievement->public_.measured_progress),
                "%lu%%", (unsigned long)achievement->public_.measured_percent);
          }
        }
      }

      if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE;
      else if (achievement->public_.measured_percent >= 80.0)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE;
    }
  }

  achievement->public_.bucket = new_bucket;
}

static const char* rc_client_get_achievement_bucket_label(uint8_t bucket_type)
{
  switch (bucket_type) {
    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED: return "Locked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED: return "Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED: return "Unsupported";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL: return "Unofficial";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED: return "Recently Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE: return "Active Challenges";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE: return "Almost There";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED: return "Unlocks Not Synced to Server";
    default: return "Unknown";
  }
}

static const char* rc_client_get_subset_achievement_bucket_label(uint8_t bucket_type, rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  const char** ptr;
  const char* label;
  char* new_label;
  size_t new_label_len;

  switch (bucket_type) {
    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED: ptr = &subset->locked_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED: ptr = &subset->unlocked_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED: ptr = &subset->unsupported_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL: ptr = &subset->unofficial_label; break;
    default: return rc_client_get_achievement_bucket_label(bucket_type);
  }

  if (*ptr)
    return *ptr;

  label = rc_client_get_achievement_bucket_label(bucket_type);
  new_label_len = strlen(subset->public_.title) + strlen(label) + 4;
  new_label = (char*)rc_buffer_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public_.title, label);

  *ptr = new_label;
  return new_label;
}

static int rc_client_compare_achievement_unlock_times(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = *(const rc_client_achievement_t**)a;
  const rc_client_achievement_t* unlock_b = *(const rc_client_achievement_t**)b;
  if (unlock_b->unlock_time == unlock_a->unlock_time)
    return 0;
  return (unlock_b->unlock_time < unlock_a->unlock_time) ? -1 : 1;
}

static int rc_client_compare_achievement_progress(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = *(const rc_client_achievement_t**)a;
  const rc_client_achievement_t* unlock_b = *(const rc_client_achievement_t**)b;
  if (unlock_b->measured_percent == unlock_a->measured_percent) {
    if (unlock_a->id == unlock_b->id)
      return 0;
    return (unlock_a->id < unlock_b->id) ? -1 : 1;
  }
  return (unlock_b->measured_percent < unlock_a->measured_percent) ? -1 : 1;
}

static uint8_t rc_client_map_bucket(uint8_t bucket, int grouping)
{
  if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE) {
    switch (bucket) {
      case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
      case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED:
        return RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;

      case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
      case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
        return RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;

      default:
        return bucket;
    }
  }

  return bucket;
}

rc_client_achievement_list_t* rc_client_create_achievement_list(rc_client_t* client, int category, int grouping)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;
  const rc_client_achievement_t** bucket_achievements;
  const rc_client_achievement_t** achievement_ptr;
  rc_client_achievement_bucket_t* bucket_ptr;
  rc_client_achievement_list_info_t* list;
  rc_client_subset_info_t* subset;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[NUM_RC_CLIENT_ACHIEVEMENT_BUCKETS];
  uint32_t num_buckets;
  uint32_t num_achievements;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t num_subsets = 0;
  uint32_t i, j;
  const uint8_t shared_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED,
  };
  const uint8_t subset_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED
  };
  const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;

  if (!client)
    return (rc_client_achievement_list_t*)calloc(1, sizeof(rc_client_achievement_list_info_t));

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->create_achievement_list_v3)
      return (rc_client_achievement_list_t*)client->state.external_client->create_achievement_list_v3(category, grouping);

    if (client->state.external_client->create_achievement_list)
      return rc_client_external_convert_v1_achievement_list(client,
        (rc_client_achievement_list_t*)client->state.external_client->create_achievement_list(category, grouping));
  }
#endif

  if (!client->game)
    return (rc_client_achievement_list_t*)calloc(1, sizeof(rc_client_achievement_list_info_t));

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    num_subsets++;
    achievement = subset->achievements;
    stop = achievement + subset->public_.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public_.category & category) {
        rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
        bucket_counts[rc_client_map_bucket(achievement->public_.bucket, grouping)]++;
      }
    }
  }

  num_buckets = 0;
  num_achievements = 0;
  for (i = 0; i < sizeof(bucket_counts) / sizeof(bucket_counts[0]); ++i) {
    if (bucket_counts[i]) {
      int needs_split = 0;

      num_achievements += bucket_counts[i];

      if (num_subsets > 1) {
        for (j = 0; j < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++j) {
          if (subset_bucket_order[j] == i) {
            needs_split = 1;
            break;
          }
        }
      }

      if (!needs_split) {
        ++num_buckets;
        continue;
      }

      subset = client->game->subsets;
      for (; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public_.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (achievement->public_.category & category) {
            if (rc_client_map_bucket(achievement->public_.bucket, grouping) == i) {
              ++num_buckets;
              break;
            }
          }
        }
      }
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_achievement_bucket_t));

  list = (rc_client_achievement_list_info_t*)malloc(list_size + buckets_size + num_achievements * sizeof(rc_client_achievement_t*));
  list->public_.buckets = bucket_ptr = (rc_client_achievement_bucket_t*)((uint8_t*)list + list_size);
  achievement_ptr = (const rc_client_achievement_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public_.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (achievement->public_.category & category &&
              rc_client_map_bucket(achievement->public_.bucket, grouping) == bucket_type) {
            *achievement_ptr++ = &achievement->public_;
          }
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = 0;
        bucket_ptr->label = rc_client_get_achievement_bucket_label(bucket_type);
        bucket_ptr->bucket_type = bucket_type;

        if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED)
          qsort((void*)bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_unlock_times);
        else if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE)
          qsort((void*)bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_progress);

        ++bucket_ptr;
      }
    }
  }

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;

      achievement = subset->achievements;
      stop = achievement + subset->public_.num_achievements;
      for (; achievement < stop; ++achievement) {
        if (achievement->public_.category & category &&
            rc_client_map_bucket(achievement->public_.bucket, grouping) == bucket_type) {
          *achievement_ptr++ = &achievement->public_;
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public_.id : 0;
        bucket_ptr->bucket_type = bucket_type;

        if (num_subsets > 1)
          bucket_ptr->label = rc_client_get_subset_achievement_bucket_label(bucket_type, client->game, subset);
        else
          bucket_ptr->label = rc_client_get_achievement_bucket_label(bucket_type);

        ++bucket_ptr;
      }
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  list->destroy_func = NULL;
  list->public_.num_buckets = (uint32_t)(bucket_ptr - list->public_.buckets);
  return &list->public_;
}

void rc_client_destroy_achievement_list(rc_client_achievement_list_t* list)
{
  rc_client_achievement_list_info_t* info = (rc_client_achievement_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

int rc_client_has_achievements(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  int result;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_achievements)
    return client->state.external_client->has_achievements();
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  result = 0;
  for (; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    if (subset->public_.num_achievements > 0) {
      result = 1;
      break;
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  return result;
}

static const rc_client_achievement_t* rc_client_subset_get_achievement_info(
    rc_client_t* client, rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    if (achievement->public_.id == id) {
      const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
      rc_mutex_lock((rc_mutex_t*)(&client->state.mutex));
      rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
      rc_mutex_unlock((rc_mutex_t*)(&client->state.mutex));
      return &achievement->public_;
    }
  }

  return NULL;
}

const rc_client_achievement_t* rc_client_get_achievement_info(rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_achievement_info_v3)
      return client->state.external_client->get_achievement_info_v3(id);

    if (client->state.external_client->get_achievement_info)
      return rc_client_external_convert_v1_achievement(client, client->state.external_client->get_achievement_info(id));
  }
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_achievement_t* achievement = rc_client_subset_get_achievement_info(client, subset, id);
    if (achievement != NULL)
      return achievement;
  }

  return NULL;
}

const rc_client_achievement_t* rc_client_get_next_achievement_info(rc_client_t* client,
    const rc_client_achievement_t* achievement, int bucket)
{
  const rc_client_achievement_info_t* after = (const rc_client_achievement_info_t*)achievement;
  rc_client_achievement_info_t* achievement_info;
  time_t recent_unlock_time;
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_next_achievement_info)
    return client->state.external_client->get_next_achievement_info(achievement ? achievement->id : 0, bucket);
#endif

  if (!client->game)
    return NULL;

  recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (subset->active && subset->public_.num_achievements > 0) {
      const rc_client_achievement_info_t* start = subset->achievements;
      const rc_client_achievement_info_t* stop = start + subset->public_.num_achievements;
      if (after == NULL || (after >= start && after <= stop)) {
        /* found a subset containing the provided achievement. look for the next
         * achievement matching the requested bucket */
        uint32_t index = after ? (uint32_t)(after - start) + 1 : 0;
        do {
          if (index >= subset->public_.num_achievements) {
            /* done with this subset. find the next active subset with achievements */
            do {
              subset = subset->next;
              if (!subset)
                return NULL;
            } while (!subset->active || subset->public_.num_achievements == 0);

            index = 0;
          }

          /* found an achievement. check to see if it matches the requested bucket. */
          achievement_info = &subset->achievements[index];
          rc_client_update_achievement_display_information(client, achievement_info, recent_unlock_time);
          if (achievement_info->public_.bucket == bucket)
            return &achievement_info->public_;

          ++index;
        } while (1);
      }
    }
  }

  return NULL;
}


int rc_client_achievement_get_image_url(const rc_client_achievement_t* achievement, int state, char buffer[], size_t buffer_size)
{
  const int image_type = (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ?
      RC_IMAGE_TYPE_ACHIEVEMENT : RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  if (!achievement || !achievement->badge_name[0])
    return rc_client_get_image_url(buffer, buffer_size, image_type, "00000");

  if (image_type == RC_IMAGE_TYPE_ACHIEVEMENT && achievement->badge_url) {
    const size_t len = strlen(achievement->badge_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, achievement->badge_url, len + 1);
    return RC_OK;
  }

  if (image_type == RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED && achievement->badge_locked_url) {
    const size_t len = strlen(achievement->badge_locked_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, achievement->badge_locked_url, len + 1);
    return RC_OK;
  }

  return rc_client_get_image_url(buffer, buffer_size, image_type, achievement->badge_name);
}

typedef struct rc_client_award_achievement_callback_data_t
{
  uint32_t id;
  uint32_t retry_count;
  uint8_t hardcore;
  const char* game_hash;
  rc_clock_t unlock_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_award_achievement_callback_data_t;

static int rc_client_is_award_achievement_pending(const rc_client_t* client, uint32_t achievement_id)
{
  /* assume lock already held */
  rc_client_scheduled_callback_data_t* scheduled_callback = client->state.scheduled_callbacks;
  for (; scheduled_callback; scheduled_callback = scheduled_callback->next)
  {
    if (scheduled_callback->callback == rc_client_award_achievement_retry)
    {
      rc_client_award_achievement_callback_data_t* ach_data =
        (rc_client_award_achievement_callback_data_t*)scheduled_callback->data;
      if (ach_data->id == achievement_id)
        return 1;
    }
  }

  return 0;
}

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data);

static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_award_achievement_callback_data_t* ach_data =
    (rc_client_award_achievement_callback_data_t*)callback_data->data;

  (void)client;
  (void)now;

  rc_client_award_achievement_server_call(ach_data);
}

static void rc_client_award_achievement_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_award_achievement_callback_data_t* ach_data =
      (rc_client_award_achievement_callback_data_t*)callback_data;
  rc_api_award_achievement_response_t award_achievement_response;

  int result = rc_api_process_award_achievement_server_response(&award_achievement_response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &award_achievement_response.response);

  if (error_message) {
    if (award_achievement_response.response.error_message && !rc_client_should_retry(server_response)) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s", ach_data->id, error_message);
      rc_client_raise_server_error_event(ach_data->client, "award_achievement", ach_data->id, result, award_achievement_response.response.error_message);
    }
    else if (ach_data->retry_count++ == 0) {
      /* first retry is immediate */
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s, retrying immediately", ach_data->id, error_message);
      rc_client_award_achievement_server_call(ach_data);
      return;
    }
    else {
      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      const uint32_t delay = (ach_data->retry_count > 8) ? 120 : (1 << (ach_data->retry_count - 2));
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s, retrying in %u seconds", ach_data->id, error_message, delay);

      if (!ach_data->scheduled_callback_data) {
        ach_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*ach_data->scheduled_callback_data));
        if (!ach_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Failed to allocate scheduled callback data for reattempt to unlock achievement %u", ach_data->id);
          rc_client_raise_server_error_event(ach_data->client, "award_achievement", ach_data->id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        ach_data->scheduled_callback_data->callback = rc_client_award_achievement_retry;
        ach_data->scheduled_callback_data->data = ach_data;
        ach_data->scheduled_callback_data->related_id = ach_data->id;
      }

      ach_data->scheduled_callback_data->when =
          ach_data->client->callbacks.get_time_millisecs(ach_data->client) + delay * 1000;

      rc_client_schedule_callback(ach_data->client, ach_data->scheduled_callback_data);

      rc_client_update_disconnect_state(ach_data->client);
      return;
    }
  }
  else {
    ach_data->client->user.score = award_achievement_response.new_player_score;
    ach_data->client->user.score_softcore = award_achievement_response.new_player_score_softcore;

    if (award_achievement_response.awarded_achievement_id != ach_data->id) {
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Awarded achievement %u instead of %u", award_achievement_response.awarded_achievement_id, error_message);
    }
    else {
      if (award_achievement_response.response.error_message) {
        /* previously unlocked achievements are returned as a success with an error message */
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u: %s", ach_data->id, award_achievement_response.response.error_message);
      }
      else if (ach_data->retry_count) {
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u awarded after %u attempts, new score: %u",
            ach_data->id, ach_data->retry_count + 1,
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_softcore);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u awarded, new score: %u",
            ach_data->id,
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_softcore);
      }

      if (award_achievement_response.achievements_remaining == 0) {
        rc_client_subset_info_t* subset;
        for (subset = ach_data->client->game->subsets; subset; subset = subset->next) {
          if (subset->mastery == RC_CLIENT_MASTERY_STATE_NONE &&
              rc_client_subset_get_achievement_info(ach_data->client, subset, ach_data->id)) {
            if (subset->public_.id == ach_data->client->game->public_.id) {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Game %u %s", ach_data->client->game->public_.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");
              subset->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
            }
            else {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Subset %u %s", ach_data->client->game->public_.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");

              subset->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
            }
          }
        }
      }
    }
  }

  if (ach_data->retry_count)
    rc_client_update_disconnect_state(ach_data->client);

  if (ach_data->scheduled_callback_data)
    free(ach_data->scheduled_callback_data);
  free(ach_data);
}

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data)
{
  rc_api_award_achievement_request_t api_params;
  rc_api_request_t request;
  int result;

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = ach_data->client->user.username;
  api_params.api_token = ach_data->client->user.token;
  api_params.achievement_id = ach_data->id;
  api_params.hardcore = ach_data->hardcore;
  api_params.game_hash = ach_data->game_hash;

  if (ach_data->retry_count) {
    const rc_clock_t now = ach_data->client->callbacks.get_time_millisecs(ach_data->client);
    api_params.seconds_since_unlock = (uint32_t)((now - ach_data->unlock_time) / 1000);
  }

  result = rc_api_init_award_achievement_request_hosted(&request, &api_params, &ach_data->client->state.host);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error constructing unlock request for achievement %u: %s", ach_data->id, rc_error_str(result));
    free(ach_data);
    return;
  }

  ach_data->client->callbacks.server_call(&request, rc_client_award_achievement_callback, ach_data, ach_data->client);

  rc_api_destroy_request(&request);
}

static void rc_client_award_achievement(rc_client_t* client, rc_client_achievement_info_t* achievement)
{
  rc_client_award_achievement_callback_data_t* callback_data;

  rc_mutex_lock(&client->state.mutex);

  if (client->state.hardcore) {
    achievement->public_.unlock_time = achievement->unlock_time_hardcore = time(NULL);
    if (achievement->unlock_time_softcore == 0)
      achievement->unlock_time_softcore = achievement->unlock_time_hardcore;

    /* adjust score now - will get accurate score back from server */
    client->user.score += achievement->public_.points;
  }
  else {
    achievement->public_.unlock_time = achievement->unlock_time_softcore = time(NULL);

    /* adjust score now - will get accurate score back from server */
    client->user.score_softcore += achievement->public_.points;
  }

  achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  achievement->public_.unlocked |= (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_mutex_unlock(&client->state.mutex);

  if (client->callbacks.can_submit_achievement_unlock &&
      !client->callbacks.can_submit_achievement_unlock(achievement->public_.id, client)) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Achievement %u unlock blocked by client", achievement->public_.id);
    return;
  }

  /* can't unlock unofficial achievements on the server */
  if (achievement->public_.category != RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unlocked unofficial achievement %u: %s", achievement->public_.id, achievement->public_.title);
    return;
  }

  /* don't actually unlock achievements when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated achievement %u: %s", achievement->public_.id, achievement->public_.title);
    return;
  }

  callback_data = (rc_client_award_achievement_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for unlocking achievement %u", achievement->public_.id);
    rc_client_raise_server_error_event(client, "award_achievement", achievement->public_.id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = achievement->public_.id;
  callback_data->hardcore = client->state.hardcore;
  callback_data->game_hash = client->game->public_.hash;
  callback_data->unlock_time = client->callbacks.get_time_millisecs(client);

  if (client->game) /* may be NULL if this gets called while unloading the game (from another thread - events are raised outside the lock) */
    callback_data->game_hash = client->game->public_.hash;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Awarding achievement %u: %s", achievement->public_.id, achievement->public_.title);
  rc_client_award_achievement_server_call(callback_data);
}

static void rc_client_subset_reset_achievements(rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }

    rc_reset_trigger(trigger);
  }
}

static void rc_client_reset_achievements(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_reset_achievements(subset);
}

/* ===== Leaderboards ===== */

static rc_client_leaderboard_info_t* rc_client_subset_get_leaderboard_info(const rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    if (leaderboard->public_.id == id)
      return leaderboard;
  }

  return NULL;
}

const rc_client_leaderboard_t* rc_client_get_leaderboard_info(const rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_leaderboard_info)
    return client->state.external_client->get_leaderboard_info(id);
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_leaderboard_info_t* leaderboard = rc_client_subset_get_leaderboard_info(subset, id);
    if (leaderboard != NULL)
      return &leaderboard->public_;
  }

  return NULL;
}

static const char* rc_client_get_leaderboard_bucket_label(uint8_t bucket_type)
{
  switch (bucket_type) {
    case RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE: return "Inactive";
    case RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE: return "Active";
    case RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED: return "Unsupported";
    case RC_CLIENT_LEADERBOARD_BUCKET_ALL: return "All";
    default: return "Unknown";
  }
}

static const char* rc_client_get_subset_leaderboard_bucket_label(uint8_t bucket_type, rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  const char** ptr;
  const char* label;
  char* new_label;
  size_t new_label_len;

  switch (bucket_type) {
    case RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE: ptr = &subset->inactive_label; break;
    case RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED: ptr = &subset->unsupported_label; break;
    case RC_CLIENT_LEADERBOARD_BUCKET_ALL: ptr = &subset->all_label; break;
    default: return rc_client_get_achievement_bucket_label(bucket_type);
  }

  if (*ptr)
    return *ptr;

  label = rc_client_get_leaderboard_bucket_label(bucket_type);
  new_label_len = strlen(subset->public_.title) + strlen(label) + 4;
  new_label = (char*)rc_buffer_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public_.title, label);

  *ptr = new_label;
  return new_label;
}

static uint8_t rc_client_get_leaderboard_bucket(const rc_client_leaderboard_info_t* leaderboard, int grouping)
{
  switch (leaderboard->public_.state) {
    case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
      return (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE) ?
        RC_CLIENT_LEADERBOARD_BUCKET_ALL : RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE;

    case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
      return RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED;

    default:
      return (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE) ?
        RC_CLIENT_LEADERBOARD_BUCKET_ALL : RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE;
  }
}

rc_client_leaderboard_list_t* rc_client_create_leaderboard_list(rc_client_t* client, int grouping)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;
  const rc_client_leaderboard_t** bucket_leaderboards;
  const rc_client_leaderboard_t** leaderboard_ptr;
  rc_client_leaderboard_bucket_t* bucket_ptr;
  rc_client_leaderboard_list_info_t* list;
  rc_client_subset_info_t* subset;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[8];
  uint32_t num_buckets;
  uint32_t num_leaderboards;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t num_subsets = 0;
  uint32_t i, j;
  const uint8_t shared_bucket_order[] = {
    RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE
  };
  const uint8_t subset_bucket_order[] = {
    RC_CLIENT_LEADERBOARD_BUCKET_ALL,
    RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE,
    RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED
  };

  if (!client)
    return (rc_client_leaderboard_list_t*)calloc(1, list_size);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->create_leaderboard_list)
    return (rc_client_leaderboard_list_t*)client->state.external_client->create_leaderboard_list(grouping);
#endif

  if (!client->game)
    return (rc_client_leaderboard_list_t*)calloc(1, list_size);

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    num_subsets++;
    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      if (leaderboard->hidden)
        continue;

      leaderboard->bucket = rc_client_get_leaderboard_bucket(leaderboard, grouping);
      bucket_counts[leaderboard->bucket]++;
    }
  }

  num_buckets = 0;
  num_leaderboards = 0;
  for (i = 0; i < sizeof(bucket_counts) / sizeof(bucket_counts[0]); ++i) {
    if (bucket_counts[i]) {
      int needs_split = 0;

      num_leaderboards += bucket_counts[i];

      if (num_subsets > 1) {
        for (j = 0; j < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++j) {
          if (subset_bucket_order[j] == i) {
            needs_split = 1;
            break;
          }
        }
      }

      if (!needs_split) {
        ++num_buckets;
        continue;
      }

      subset = client->game->subsets;
      for (; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public_.num_leaderboards;
        for (; leaderboard < stop; ++leaderboard) {
          if (leaderboard->bucket == i) {
            ++num_buckets;
            break;
          }
        }
      }
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_leaderboard_bucket_t));

  list = (rc_client_leaderboard_list_info_t*)malloc(list_size + buckets_size + num_leaderboards * sizeof(rc_client_leaderboard_t*));
  list->public_.buckets = bucket_ptr = (rc_client_leaderboard_bucket_t*)((uint8_t*)list + list_size);
  leaderboard_ptr = (const rc_client_leaderboard_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public_.num_leaderboards;
        for (; leaderboard < stop; ++leaderboard) {
          if (leaderboard->bucket == bucket_type && !leaderboard->hidden)
            *leaderboard_ptr++ = &leaderboard->public_;
        }
      }

      if (leaderboard_ptr > bucket_leaderboards) {
        bucket_ptr->leaderboards = bucket_leaderboards;
        bucket_ptr->num_leaderboards = (uint32_t)(leaderboard_ptr - bucket_leaderboards);
        bucket_ptr->subset_id = 0;
        bucket_ptr->label = rc_client_get_leaderboard_bucket_label(bucket_type);
        bucket_ptr->bucket_type = bucket_type;
        ++bucket_ptr;
      }
    }
  }

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public_.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->bucket == bucket_type && !leaderboard->hidden)
          *leaderboard_ptr++ = &leaderboard->public_;
      }

      if (leaderboard_ptr > bucket_leaderboards) {
        bucket_ptr->leaderboards = bucket_leaderboards;
        bucket_ptr->num_leaderboards = (uint32_t)(leaderboard_ptr - bucket_leaderboards);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public_.id : 0;
        bucket_ptr->bucket_type = bucket_type;

        if (num_subsets > 1)
          bucket_ptr->label = rc_client_get_subset_leaderboard_bucket_label(bucket_type, client->game, subset);
        else
          bucket_ptr->label = rc_client_get_leaderboard_bucket_label(bucket_type);

        ++bucket_ptr;
      }
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  list->destroy_func = NULL;
  list->public_.num_buckets = (uint32_t)(bucket_ptr - list->public_.buckets);
  return &list->public_;
}

void rc_client_destroy_leaderboard_list(rc_client_leaderboard_list_t* list)
{
  rc_client_leaderboard_list_info_t* info = (rc_client_leaderboard_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

int rc_client_has_leaderboards(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  int result;
  uint32_t i;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_leaderboards)
    return client->state.external_client->has_leaderboards();
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  result = 0;
  for (subset = client->game->subsets; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    for (i = 0; i < subset->public_.num_leaderboards; ++i) {
      if (!subset->leaderboards[i].hidden) {
        result = 1;
        break;
      }
    }

    if (result)
      break;
  }

  rc_mutex_unlock(&client->state.mutex);

  return result;
}

void rc_client_allocate_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker;
  rc_client_leaderboard_tracker_info_t* available_tracker = NULL;

  for (tracker = game->leaderboard_trackers; tracker; tracker = tracker->next) {
    if (tracker->reference_count == 0) {
      if (available_tracker == NULL && tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
        available_tracker = tracker;

      continue;
    }

    if (tracker->value_djb2 != leaderboard->value_djb2 || tracker->format != leaderboard->format)
      continue;

    if (tracker->raw_value != leaderboard->value) {
      /* if the value comes from tracking hits, we can't assume the trackers started in the
       * same frame, so we can't share the tracker */
      if (tracker->value_from_hits)
        continue;

      /* value has changed. prepare an update event */
      tracker->raw_value = leaderboard->value;
      tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE;
      game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
    }

    /* attach to the existing tracker */
    ++tracker->reference_count;
    tracker->pending_events &= ~RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
    leaderboard->tracker = tracker;
    leaderboard->public_.tracker_value = tracker->public_.display;
    return;
  }

  if (!available_tracker) {
    rc_client_leaderboard_tracker_info_t** next = &game->leaderboard_trackers;

    available_tracker = (rc_client_leaderboard_tracker_info_t*)rc_buffer_alloc(&game->buffer, sizeof(*available_tracker));
    memset(available_tracker, 0, sizeof(*available_tracker));
    available_tracker->public_.id = 1;

    for (tracker = *next; tracker; next = &tracker->next, tracker = *next)
      available_tracker->public_.id++;

    *next = available_tracker;
  }

  /* update the claimed tracker */
  available_tracker->reference_count = 1;
  available_tracker->value_djb2 = leaderboard->value_djb2;
  available_tracker->format = leaderboard->format;
  available_tracker->raw_value = leaderboard->value;
  available_tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW;
  available_tracker->value_from_hits = rc_value_from_hits(&leaderboard->lboard->value);
  leaderboard->tracker = available_tracker;
  leaderboard->public_.tracker_value = available_tracker->public_.display;
  game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
}

void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = leaderboard->tracker;
  leaderboard->tracker = NULL;

  if (tracker && --tracker->reference_count == 0) {
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
  }
}

static void rc_client_update_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = leaderboard->tracker;
  if (tracker && tracker->raw_value != leaderboard->value) {
    tracker->raw_value = leaderboard->value;
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
  }
}

typedef struct rc_client_submit_leaderboard_entry_callback_data_t
{
  uint32_t id;
  int32_t score;
  uint32_t retry_count;
  const char* game_hash;
  rc_clock_t submit_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_submit_leaderboard_entry_callback_data_t;

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data);

static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data->data;

  (void)client;
  (void)now;

  rc_client_submit_leaderboard_entry_server_call(lboard_data);
}

static void rc_client_raise_scoreboard_event(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data,
    const rc_api_submit_lboard_entry_response_t* response)
{
  rc_client_leaderboard_scoreboard_t sboard;
  rc_client_event_t client_event;
  rc_client_subset_info_t* subset;
  rc_client_t* client = lboard_data->client;
  rc_client_leaderboard_info_t* leaderboard = NULL;

  if (!client || !client->game)
    return;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    leaderboard = rc_client_subset_get_leaderboard_info(subset, lboard_data->id);
    if (leaderboard != NULL)
      break;
  }
  if (leaderboard == NULL) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Trying to raise scoreboard for unknown leaderboard %u", lboard_data->id);
    return;
  }

  memset(&sboard, 0, sizeof(sboard));
  sboard.leaderboard_id = lboard_data->id;
  rc_format_value(sboard.submitted_score, sizeof(sboard.submitted_score), response->submitted_score, leaderboard->format);
  rc_format_value(sboard.best_score, sizeof(sboard.best_score), response->best_score, leaderboard->format);
  sboard.new_rank = response->new_rank;
  sboard.num_entries = response->num_entries;
  sboard.num_top_entries = response->num_top_entries;
  if (sboard.num_top_entries > 0) {
    sboard.top_entries = (rc_client_leaderboard_scoreboard_entry_t*)calloc(
      response->num_top_entries, sizeof(rc_client_leaderboard_scoreboard_entry_t));
    if (sboard.top_entries != NULL) {
      uint32_t i;
      for (i = 0; i < response->num_top_entries; i++) {
        sboard.top_entries[i].username = response->top_entries[i].username;
        sboard.top_entries[i].rank = response->top_entries[i].rank;
        rc_format_value(sboard.top_entries[i].score, sizeof(sboard.top_entries[i].score), response->top_entries[i].score,
            leaderboard->format);
      }
    }
  }

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD;
  client_event.leaderboard = &leaderboard->public_;
  client_event.leaderboard_scoreboard = &sboard;

  lboard_data->client->callbacks.event_handler(&client_event, lboard_data->client);

  if (sboard.top_entries != NULL) {
    free(sboard.top_entries);
  }
}

static void rc_client_submit_leaderboard_entry_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data;
  rc_api_submit_lboard_entry_response_t submit_lboard_entry_response;

  int result = rc_api_process_submit_lboard_entry_server_response(&submit_lboard_entry_response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &submit_lboard_entry_response.response);

  if (error_message) {
    if (submit_lboard_entry_response.response.error_message && !rc_client_should_retry(server_response)) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s", lboard_data->id, error_message);
      rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", lboard_data->id, result, submit_lboard_entry_response.response.error_message);
    }
    else if (lboard_data->retry_count++ == 0) {
      /* first retry is immediate */
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s, retrying immediately", lboard_data->id, error_message);
      rc_client_submit_leaderboard_entry_server_call(lboard_data);
      return;
    }
    else {
      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      const uint32_t delay = (lboard_data->retry_count > 8) ? 120 : (1 << (lboard_data->retry_count - 2));
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s, retrying in %u seconds", lboard_data->id, error_message, delay);

      if (!lboard_data->scheduled_callback_data) {
        lboard_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*lboard_data->scheduled_callback_data));
        if (!lboard_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Failed to allocate scheduled callback data for reattempt to submit entry for leaderboard %u", lboard_data->id);
          rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", lboard_data->id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        lboard_data->scheduled_callback_data->callback = rc_client_submit_leaderboard_entry_retry;
        lboard_data->scheduled_callback_data->data = lboard_data;
        lboard_data->scheduled_callback_data->related_id = lboard_data->id;
      }

      lboard_data->scheduled_callback_data->when =
          lboard_data->client->callbacks.get_time_millisecs(lboard_data->client) + delay * 1000;

      rc_client_schedule_callback(lboard_data->client, lboard_data->scheduled_callback_data);

      rc_client_update_disconnect_state(lboard_data->client);
      return;
    }
  }
  else {
    /* raise event for scoreboard */
    if (lboard_data->retry_count < 2) {
      rc_client_raise_scoreboard_event(lboard_data, &submit_lboard_entry_response);
    }

    /* not currently doing anything with the response */
    if (lboard_data->retry_count) {
      RC_CLIENT_LOG_INFO_FORMATTED(lboard_data->client, "Leaderboard %u submission %d completed after %u attempts",
          lboard_data->id, lboard_data->score, lboard_data->retry_count);
    }
  }

  if (lboard_data->retry_count)
    rc_client_update_disconnect_state(lboard_data->client);

  if (lboard_data->scheduled_callback_data)
    free(lboard_data->scheduled_callback_data);
  free(lboard_data);
}

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data)
{
  rc_api_submit_lboard_entry_request_t api_params;
  rc_api_request_t request;
  int result;

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = lboard_data->client->user.username;
  api_params.api_token = lboard_data->client->user.token;
  api_params.leaderboard_id = lboard_data->id;
  api_params.score = lboard_data->score;
  api_params.game_hash = lboard_data->game_hash;

  if (lboard_data->retry_count) {
    const rc_clock_t now = lboard_data->client->callbacks.get_time_millisecs(lboard_data->client);
    api_params.seconds_since_completion = (uint32_t)((now - lboard_data->submit_time) / 1000);
  }

  result = rc_api_init_submit_lboard_entry_request_hosted(&request, &api_params, &lboard_data->client->state.host);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error constructing submit leaderboard entry for leaderboard %u: %s", lboard_data->id, rc_error_str(result));
    return;
  }

  lboard_data->client->callbacks.server_call(&request, rc_client_submit_leaderboard_entry_callback, lboard_data, lboard_data->client);

  rc_api_destroy_request(&request);
}

static void rc_client_submit_leaderboard_entry(rc_client_t* client, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_submit_leaderboard_entry_callback_data_t* callback_data;

  if (!client->state.hardcore) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Leaderboard %u entry submission not allowed in softcore", leaderboard->public_.id);
    return;
  }

  if (client->callbacks.can_submit_leaderboard_entry &&
      !client->callbacks.can_submit_leaderboard_entry(leaderboard->public_.id, client)) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Leaderboard %u entry submission blocked by client", leaderboard->public_.id);
    return;
  }

  /* don't actually submit leaderboard entries when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated %s (%d) for leaderboard %u: %s",
        leaderboard->public_.tracker_value, leaderboard->value, leaderboard->public_.id, leaderboard->public_.title);
    return;
  }

  callback_data = (rc_client_submit_leaderboard_entry_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for submitting entry for leaderboard %u", leaderboard->public_.id);
    rc_client_raise_server_error_event(client, "submit_lboard_entry", leaderboard->public_.id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = leaderboard->public_.id;
  callback_data->score = leaderboard->value;
  callback_data->game_hash = client->game->public_.hash;
  callback_data->submit_time = client->callbacks.get_time_millisecs(client);

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Submitting %s (%d) for leaderboard %u: %s",
      leaderboard->public_.tracker_value, leaderboard->value, leaderboard->public_.id, leaderboard->public_.title);
  rc_client_submit_leaderboard_entry_server_call(callback_data);
}

static void rc_client_subset_reset_leaderboards(rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard)
      continue;

    switch (leaderboard->public_.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
        rc_client_release_leaderboard_tracker(game, leaderboard);
        /* fallthrough */ /* to default */
      default:
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
        rc_reset_lboard(lboard);
        break;
    }
  }
}

static void rc_client_reset_leaderboards(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_reset_leaderboards(client->game, subset);
}

typedef struct rc_client_fetch_leaderboard_entries_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_leaderboard_entries_callback_t callback;
  void* callback_userdata;
  uint32_t leaderboard_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_leaderboard_entries_callback_data_t;

static void rc_client_fetch_leaderboard_entries_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_leaderboard_entries_callback_data_t* lbinfo_callback_data = (rc_client_fetch_leaderboard_entries_callback_data_t*)callback_data;
  rc_client_t* client = lbinfo_callback_data->client;
  rc_api_fetch_leaderboard_info_response_t lbinfo_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &lbinfo_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED) {
      RC_CLIENT_LOG_VERBOSE(client, "Fetch leaderbord entries aborted");
    }
    free(lbinfo_callback_data);
    return;
  }

  result = rc_api_process_fetch_leaderboard_info_server_response(&lbinfo_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &lbinfo_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch leaderboard %u info failed: %s", lbinfo_callback_data->leaderboard_id, error_message);
    lbinfo_callback_data->callback(result, error_message, NULL, client, lbinfo_callback_data->callback_userdata);
  }
  else {
    rc_client_leaderboard_entry_list_info_t* info;
    const size_t list_size = sizeof(*info) + sizeof(rc_client_leaderboard_entry_t) * lbinfo_response.num_entries;
    size_t needed_size = list_size;
    uint32_t i;

    for (i = 0; i < lbinfo_response.num_entries; i++)
      needed_size += strlen(lbinfo_response.entries[i].username) + 1;

    info = (rc_client_leaderboard_entry_list_info_t*)malloc(needed_size);
    if (!info) {
      lbinfo_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, lbinfo_callback_data->callback_userdata);
    }
    else {
      rc_client_leaderboard_entry_list_t* list = &info->public_;
      rc_client_leaderboard_entry_t* entry = list->entries = (rc_client_leaderboard_entry_t*)((uint8_t*)info + sizeof(*info));
      char* user = (char*)((uint8_t*)list + list_size);
      const rc_api_lboard_info_entry_t* lbentry = lbinfo_response.entries;
      const rc_api_lboard_info_entry_t* stop = lbentry + lbinfo_response.num_entries;
      const size_t logged_in_user_len = strlen(client->user.display_name) + 1;
      info->destroy_func = NULL;
      list->user_index = -1;

      for (; lbentry < stop; ++lbentry, ++entry) {
        const size_t len = strlen(lbentry->username) + 1;
        entry->user = user;
        memcpy(user, lbentry->username, len);
        user += len;

        if (len == logged_in_user_len && memcmp(entry->user, client->user.display_name, len) == 0)
          list->user_index = (int)(entry - list->entries);

        entry->index = lbentry->index;
        entry->rank = lbentry->rank;
        entry->submitted = lbentry->submitted;

        rc_format_value(entry->display, sizeof(entry->display), lbentry->score, lbinfo_response.format);
      }

      list->num_entries = lbinfo_response.num_entries;
      list->total_entries = lbinfo_response.total_entries;

      lbinfo_callback_data->callback(RC_OK, NULL, list, client, lbinfo_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_leaderboard_info_response(&lbinfo_response);
  free(lbinfo_callback_data);
}

static rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_info(rc_client_t* client,
    const rc_api_fetch_leaderboard_info_request_t* lbinfo_request,
    rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_client_fetch_leaderboard_entries_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  result = rc_api_init_fetch_leaderboard_info_request_hosted(&request, lbinfo_request, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_leaderboard_entries_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->leaderboard_id = lbinfo_request->leaderboard_id;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_leaderboard_entries_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_entries(rc_client_t* client, uint32_t leaderboard_id,
    uint32_t first_entry, uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_api_fetch_leaderboard_info_request_t lbinfo_request;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_fetch_leaderboard_entries)
    return client->state.external_client->begin_fetch_leaderboard_entries(client, leaderboard_id, first_entry, count, callback, callback_userdata);
#endif

  memset(&lbinfo_request, 0, sizeof(lbinfo_request));
  lbinfo_request.leaderboard_id = leaderboard_id;
  lbinfo_request.first_entry = first_entry;
  lbinfo_request.count = count;

  return rc_client_begin_fetch_leaderboard_info(client, &lbinfo_request, callback, callback_userdata);
}

rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_entries_around_user(rc_client_t* client, uint32_t leaderboard_id,
  uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_api_fetch_leaderboard_info_request_t lbinfo_request;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_fetch_leaderboard_entries_around_user)
    return client->state.external_client->begin_fetch_leaderboard_entries_around_user(client, leaderboard_id, count, callback, callback_userdata);
#endif

  memset(&lbinfo_request, 0, sizeof(lbinfo_request));
  lbinfo_request.leaderboard_id = leaderboard_id;
  lbinfo_request.username = client->user.username;
  lbinfo_request.count = count;

  if (!lbinfo_request.username) {
    callback(RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED), NULL, client, callback_userdata);
    return NULL;
  }

  return rc_client_begin_fetch_leaderboard_info(client, &lbinfo_request, callback, callback_userdata);
}

void rc_client_destroy_leaderboard_entry_list(rc_client_leaderboard_entry_list_t* list)
{
  rc_client_leaderboard_entry_list_info_t* info = (rc_client_leaderboard_entry_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

int rc_client_leaderboard_entry_get_user_image_url(const rc_client_leaderboard_entry_t* entry, char buffer[], size_t buffer_size)
{
  if (!entry)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_USER, entry->user);
}

/* ===== Rich Presence ===== */

static void rc_client_ping_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_t* client = (rc_client_t*)callback_data;
  rc_api_ping_response_t response;

  int result = rc_api_process_ping_server_response(&response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &response.response);
  if (error_message) {
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Ping response error: %s", error_message);
  }

  rc_api_destroy_ping_response(&response);
}

static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_api_ping_request_t api_params;
  rc_api_request_t request;
  char buffer[256];
  int result;

  /* if no frames have been processed since the last ping, the emulator is idle. let the
   * server session expire. it will be resumed/restarted once frames start getting
   * processed again. */
  if (client->state.frames_processed != client->state.frames_at_last_ping) {
    client->state.frames_at_last_ping = client->state.frames_processed;

    memset(&api_params, 0, sizeof(api_params));
    api_params.username = client->user.username;
    api_params.api_token = client->user.token;
    api_params.game_id = client->game->public_.id;
    api_params.rich_presence = buffer;
    api_params.game_hash = client->game->public_.hash;
    api_params.hardcore = client->state.hardcore;

    if (!client->callbacks.rich_presence_override ||
        !client->callbacks.rich_presence_override(client, buffer, sizeof(buffer))) {
      rc_mutex_lock(&client->state.mutex);

      rc_runtime_get_richpresence(&client->game->runtime, buffer, sizeof(buffer),
          client->state.legacy_peek, client, NULL);

      rc_mutex_unlock(&client->state.mutex);
    }

    /* there's a miniscule chance the game will be changed out while we're waiting for the lock.
     * if that happens, discard this ping. the new game will have scheduled its own ping.
     * don't reschedule this one. */
    if (!client->game || client->game->public_.id != api_params.game_id) {
      return;
    }

    result = rc_api_init_ping_request_hosted(&request, &api_params, &client->state.host);
    if (result != RC_OK) {
      RC_CLIENT_LOG_WARN_FORMATTED(client, "Error generating ping request: %s", rc_error_str(result));
    }
    else {
      client->callbacks.server_call(&request, rc_client_ping_callback, client, client);
    }
  }

  callback_data->when = now + 120 * 1000;
  rc_client_schedule_callback(client, callback_data);
}

int rc_client_has_rich_presence(rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_rich_presence)
    return client->state.external_client->has_rich_presence();
#endif

  if (!client->game || !client->game->runtime.richpresence || !client->game->runtime.richpresence->richpresence)
    return 0;

  return 1;
}

size_t rc_client_get_rich_presence_message(rc_client_t* client, char buffer[], size_t buffer_size)
{
  int result;

  if (!client || !buffer)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_rich_presence_message)
    return client->state.external_client->get_rich_presence_message(buffer, buffer_size);
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  result = rc_runtime_get_richpresence(&client->game->runtime, buffer, (unsigned)buffer_size,
      client->state.legacy_peek, client, NULL);

  rc_mutex_unlock(&client->state.mutex);

  if (result == 0) {
    result = snprintf(buffer, buffer_size, "Playing %s", client->game->public_.title);
    /* snprintf will return the amount of space needed, we want to return the number of chars written */
    if ((size_t)result >= buffer_size)
      return (buffer_size - 1);
  }

  return result;
}

/* ===== Processing ===== */

void rc_client_set_event_handler(rc_client_t* client, rc_client_event_handler_t handler)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_event_handler)
    client->state.external_client->set_event_handler(client, handler);
#endif

  client->callbacks.event_handler = handler;
}

void rc_client_set_read_memory_function(rc_client_t* client, rc_client_read_memory_func_t handler)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_read_memory)
    client->state.external_client->set_read_memory(client, handler);
#endif

  client->callbacks.read_memory = handler;
}

void rc_client_set_allow_background_memory_reads(rc_client_t* client, int allowed)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_allow_background_memory_reads)
    client->state.external_client->set_allow_background_memory_reads(allowed);
#endif

  client->state.allow_background_memory_reads = allowed;
}

static void rc_client_invalidate_processing_memref(rc_client_t* client)
{
  /* if processing_memref is not set, this occurred following a pointer chain. ignore it. */
  if (!client->state.processing_memref)
    return;

  client->state.processing_memref->value.type = RC_VALUE_TYPE_NONE;

  rc_client_invalidate_memref_achievements(client->game, client, client->state.processing_memref);
  rc_client_invalidate_memref_leaderboards(client->game, client, client->state.processing_memref);

  client->state.processing_memref = NULL;
}

static uint32_t rc_client_peek_le(uint32_t address, uint32_t num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;
  uint32_t value = 0;
  uint32_t num_read = 0;

  /* if we know the address is out of range, and it's part of a pointer chain
   * (processing_memref is null), don't bother processing it. */
  if (address > client->game->max_valid_address && !client->state.processing_memref)
    return 0;

  if (num_bytes <= sizeof(value)) {
    num_read = client->callbacks.read_memory(address, (uint8_t*)&value, num_bytes, client);
    if (num_read == num_bytes)
      return value;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(client);

  return 0;
}

static uint32_t rc_client_peek(uint32_t address, uint32_t num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;
  uint8_t buffer[4];
  uint32_t num_read = 0;

  /* if we know the address is out of range, and it's part of a pointer chain
   * (processing_memref is null), don't bother processing it. */
  if (address > client->game->max_valid_address && !client->state.processing_memref)
    return 0;

  switch (num_bytes) {
    case 1:
      num_read = client->callbacks.read_memory(address, buffer, 1, client);
      if (num_read == 1)
        return buffer[0];
      break;
    case 2:
      num_read = client->callbacks.read_memory(address, buffer, 2, client);
      if (num_read == 2)
        return buffer[0] | (buffer[1] << 8);
      break;
    case 3:
      num_read = client->callbacks.read_memory(address, buffer, 3, client);
      if (num_read == 3)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
      break;
    case 4:
      num_read = client->callbacks.read_memory(address, buffer, 4, client);
      if (num_read == 4)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
      break;
    default:
      break;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(client);

  return 0;
}

void rc_client_set_legacy_peek(rc_client_t* client, int method)
{
  if (method == RC_CLIENT_LEGACY_PEEK_AUTO) {
    union {
      uint32_t whole;
      uint8_t parts[4];
    } u;
    u.whole = 1;
    method = (u.parts[0] == 1) ?
        RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS : RC_CLIENT_LEGACY_PEEK_CONSTRUCTED;
  }

  client->state.legacy_peek = (method == RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS) ?
      rc_client_peek_le : rc_client_peek;
}

int rc_client_is_processing_required(rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->is_processing_required)
    return client->state.external_client->is_processing_required();
#endif

  if (!client->game)
    return 0;

  if (client->game->runtime.trigger_count || client->game->runtime.lboard_count)
    return 1;

  return (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence);
}

static void rc_client_update_memref_values(rc_client_t* client) {
  rc_memrefs_t* memrefs = client->game->runtime.memrefs;
  rc_memref_list_t* memref_list;
  rc_modified_memref_list_t* modified_memref_list;
  int invalidated_memref = 0;

  memref_list = &memrefs->memrefs;
  do {
    rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_stop = memref + memref_list->count;
    uint32_t value;

    for (; memref < memref_stop; ++memref) {
      if (memref->value.type == RC_VALUE_TYPE_NONE)
        continue;

      /* if processing_memref is set, and the memory read fails, all dependent achievements will be disabled */
      client->state.processing_memref = memref;

      value = rc_peek_value(memref->address, memref->value.size, client->state.legacy_peek, client);

      if (client->state.processing_memref) {
        rc_update_memref_value(&memref->value, value);
      }
      else {
        /* if the peek function cleared the processing_memref, the memref was invalidated */
        invalidated_memref = 1;
      }
    }

    memref_list = memref_list->next;
  } while (memref_list);

  client->state.processing_memref = NULL;

  modified_memref_list = &memrefs->modified_memrefs;
  if (modified_memref_list->count) {
    do {
      rc_modified_memref_t* modified_memref = modified_memref_list->items;
      const rc_modified_memref_t* modified_memref_stop = modified_memref + modified_memref_list->count;

      for (; modified_memref < modified_memref_stop; ++modified_memref)
        rc_update_memref_value(&modified_memref->memref.value, rc_get_modified_memref_value(modified_memref, client->state.legacy_peek, client));

      modified_memref_list = modified_memref_list->next;
    } while (modified_memref_list);
  }

  if (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence)
    rc_update_values(client->game->runtime.richpresence->richpresence->values, client->state.legacy_peek, client);

  if (invalidated_memref)
    rc_client_update_active_achievements(client->game);
}

static void rc_client_do_frame_process_achievements(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    int old_state, new_state;
    uint32_t old_measured_value;

    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    old_measured_value = trigger->measured_value;
    old_state = trigger->state;
    new_state = rc_evaluate_trigger(trigger, client->state.legacy_peek, client, NULL);

    /* trigger->state doesn't actually change to RESET - RESET just serves as a notification.
     * we don't care about that particular notification, so look at the actual state. */
    if (new_state == RC_TRIGGER_STATE_RESET)
      new_state = trigger->state;

    /* if the measured value changed and the achievement hasn't triggered, show a progress indicator */
    if (trigger->measured_value != old_measured_value && old_measured_value != RC_MEASURED_UNKNOWN &&
        trigger->measured_value <= trigger->measured_target &&
        rc_trigger_state_active(new_state) && new_state != RC_TRIGGER_STATE_WAITING) {

      /* only show a popup for the achievement closest to triggering */
      float progress = (float)trigger->measured_value / (float)trigger->measured_target;

      if (trigger->measured_as_percent) {
        /* if reporting the measured value as a percentage, only show the popup if the percentage changes */
        const uint32_t old_percent = (uint32_t)(((unsigned long long)old_measured_value * 100) / trigger->measured_target);
        const uint32_t new_percent = (uint32_t)(((unsigned long long)trigger->measured_value * 100) / trigger->measured_target);
        if (old_percent == new_percent)
          progress = -1.0;
      }

      if (progress > client->game->progress_tracker.progress) {
        client->game->progress_tracker.progress = progress;
        client->game->progress_tracker.achievement = achievement;
        client->game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_UPDATE;
      }
    }

    /* if the state hasn't changed, there won't be any events raised */
    if (new_state == old_state)
      continue;

    /* raise a CHALLENGE_INDICATOR_HIDE event when changing from PRIMED to anything else */
    if (old_state == RC_TRIGGER_STATE_PRIMED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;

    /* raise events for each of the possible new states */
    if (new_state == RC_TRIGGER_STATE_TRIGGERED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED;
    else if (new_state == RC_TRIGGER_STATE_PRIMED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW;

    subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
  }
}

static void rc_client_hide_progress_tracker(rc_client_t* client, rc_client_game_info_t* game)
{
  /* ASSERT: this should only be called if the mutex is held */

  if (game->progress_tracker.hide_callback &&
      game->progress_tracker.hide_callback->when &&
      game->progress_tracker.action == RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE) {
    rc_client_reschedule_callback(client, game->progress_tracker.hide_callback, 0);
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER;
  }
}

static void rc_client_progress_tracker_timer_elapsed(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_event_t client_event;
  memset(&client_event, 0, sizeof(client_event));

  (void)callback_data;
  (void)now;

  rc_mutex_lock(&client->state.mutex);
  if (client->game->progress_tracker.action == RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE) {
    client->game->progress_tracker.hide_callback->when = 0;
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE;
  }
  rc_mutex_unlock(&client->state.mutex);

  if (client_event.type)
    client->callbacks.event_handler(&client_event, client);
}

static void rc_client_do_frame_update_progress_tracker(rc_client_t* client, rc_client_game_info_t* game)
{
  /* ASSERT: this should only be called if the mutex is held */

  if (!game->progress_tracker.hide_callback) {
    game->progress_tracker.hide_callback = (rc_client_scheduled_callback_data_t*)
      rc_buffer_alloc(&game->buffer, sizeof(rc_client_scheduled_callback_data_t));
    memset(game->progress_tracker.hide_callback, 0, sizeof(rc_client_scheduled_callback_data_t));
    game->progress_tracker.hide_callback->callback = rc_client_progress_tracker_timer_elapsed;
  }

  if (game->progress_tracker.hide_callback->when == 0)
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW;
  else
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_UPDATE;

  rc_client_reschedule_callback(client, game->progress_tracker.hide_callback,
      client->callbacks.get_time_millisecs(client) + 2 * 1000);
}

static void rc_client_raise_progress_tracker_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  switch (game->progress_tracker.action) {
  case RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW;
    break;
  case RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE;
    break;
  default:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE;
    break;
  }
  game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE;

  client_event.achievement = &game->progress_tracker.achievement->public_;
  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_raise_achievement_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  rc_client_event_t client_event;
  time_t recent_unlock_time = 0;

  memset(&client_event, 0, sizeof(client_event));

  for (; achievement < stop; ++achievement) {
    if (achievement->pending_events == RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE)
      continue;

    /* kick off award achievement request first */
    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      rc_client_award_achievement(client, achievement);
      client->game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS;
    }

    /* update display state */
    if (recent_unlock_time == 0)
      recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
    rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);

    /* raise events */
    client_event.achievement = &achievement->public_;

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW;
      client->callbacks.event_handler(&client_event, client);
    }

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED;
      client->callbacks.event_handler(&client_event, client);
    }

    /* clear pending flags */
    achievement->pending_events = RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE;
  }
}

static void rc_client_raise_mastery_event(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));
  client_event.subset = &subset->public_;

  if (subset == client->game->subsets)
    client_event.type = RC_CLIENT_EVENT_GAME_COMPLETED;
  else
    client_event.type = RC_CLIENT_EVENT_SUBSET_COMPLETED;

  subset->mastery = RC_CLIENT_MASTERY_STATE_SHOWN;

  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_do_frame_process_leaderboards(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    int old_state, new_state;

    switch (leaderboard->public_.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      default:
        if (!lboard)
          continue;

        break;
    }

    old_state = lboard->state;
    new_state = rc_evaluate_lboard(lboard, &leaderboard->value, client->state.legacy_peek, client, NULL);

    switch (new_state) {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (old_state != RC_LBOARD_STATE_STARTED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED;
          rc_client_allocate_leaderboard_tracker(client->game, leaderboard);
        }
        else {
          rc_client_update_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (old_state != RC_LBOARD_STATE_CANCELED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (old_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED;

          if (old_state != RC_LBOARD_STATE_STARTED)
            rc_client_allocate_leaderboard_tracker(client->game, leaderboard);
          else
            rc_client_update_leaderboard_tracker(client->game, leaderboard);

          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;
    }

    if (leaderboard->pending_events)
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD;
  }
}

static void rc_client_raise_leaderboard_tracker_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_leaderboard_tracker_info_t* tracker = game->leaderboard_trackers;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  tracker = game->leaderboard_trackers;
  for (; tracker; tracker = tracker->next) {
    if (tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard_tracker = &tracker->public_;

    /* update display text for new trackers or updated trackers */
    if (tracker->pending_events & (RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW | RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE))
      rc_format_value(tracker->public_.display, sizeof(tracker->public_.display), tracker->raw_value, tracker->format);

    if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE) {
      if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
        /* request to show and hide in the same frame - ignore the event */
      }
      else {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE;
        client->callbacks.event_handler(&client_event, client);
      }
    }
    else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE;
      client->callbacks.event_handler(&client_event, client);
    }

    tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE;
  }
}

static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    if (leaderboard->pending_events == RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard = &leaderboard->public_;

    if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u canceled: %s", leaderboard->public_.id, leaderboard->public_.title);
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_FAILED;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED) {
      /* kick off submission request before raising event */
      rc_client_submit_leaderboard_entry(client, leaderboard);

      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED) {
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u started: %s", leaderboard->public_.id, leaderboard->public_.title);
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_STARTED;
      client->callbacks.event_handler(&client_event, client);
    }

    leaderboard->pending_events = RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE;
  }
}

static void rc_client_reset_pending_events(rc_client_t* client)
{
  rc_client_subset_info_t* subset;

  client->game->pending_events = RC_CLIENT_GAME_PENDING_EVENT_NONE;

  for (subset = client->game->subsets; subset; subset = subset->next)
    subset->pending_events = RC_CLIENT_SUBSET_PENDING_EVENT_NONE;
}

static void rc_client_subset_raise_pending_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  /* raise any pending achievement events */
  if (subset->pending_events & RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT)
    rc_client_raise_achievement_events(client, subset);

  /* raise any pending leaderboard events */
  if (subset->pending_events & RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD)
    rc_client_raise_leaderboard_events(client, subset);

  /* raise mastery event if pending */
  if (subset->mastery == RC_CLIENT_MASTERY_STATE_PENDING)
    rc_client_raise_mastery_event(client, subset);
}

static void rc_client_raise_pending_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_subset_info_t* subset;

  /* raise tracker events before leaderboard events so formatted values are updated for leaderboard events */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER)
    rc_client_raise_leaderboard_tracker_events(client, game);

  for (subset = game->subsets; subset; subset = subset->next)
    rc_client_subset_raise_pending_events(client, subset);

  /* raise progress tracker events after achievement events so formatted values are updated for tracker event */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER)
    rc_client_raise_progress_tracker_events(client, game);

  /* if any achievements were unlocked, resync the active achievements list */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS) {
    rc_mutex_lock(&client->state.mutex);
    rc_client_update_active_achievements(game);
    rc_mutex_unlock(&client->state.mutex);
  }

  game->pending_events = RC_CLIENT_GAME_PENDING_EVENT_NONE;
}

void rc_client_do_frame(rc_client_t* client)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->do_frame) {
    client->state.external_client->do_frame();
    return;
  }
#endif

  if (client->game && !client->game->waiting_for_reset) {
    rc_runtime_richpresence_t* richpresence;
    rc_client_subset_info_t* subset;

    rc_mutex_lock(&client->state.mutex);

    rc_client_reset_pending_events(client);

    rc_client_update_memref_values(client);

    client->game->progress_tracker.progress = 0.0;
    for (subset = client->game->subsets; subset; subset = subset->next) {
      if (subset->active)
        rc_client_do_frame_process_achievements(client, subset);
    }
    if (client->game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER)
      rc_client_do_frame_update_progress_tracker(client, client->game);

    if (client->state.hardcore || client->state.allow_leaderboards_in_softcore) {
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (subset->active)
          rc_client_do_frame_process_leaderboards(client, subset);
      }
    }

    richpresence = client->game->runtime.richpresence;
    if (richpresence && richpresence->richpresence)
      rc_update_richpresence_internal(richpresence->richpresence, client->state.legacy_peek, client);

    rc_mutex_unlock(&client->state.mutex);

    rc_client_raise_pending_events(client, client->game);

    ++client->state.frames_processed;
  }

  /* we've processed a frame. if there's a pause delay in effect, process it */
  if (client->state.unpaused_frame_decay > 0) {
    client->state.unpaused_frame_decay--;

    if (client->state.unpaused_frame_decay == 0 &&
        client->state.required_unpaused_frames > RC_MINIMUM_UNPAUSED_FRAMES) {
      /* the full decay has elapsed and a penalty still exists.
       * lower the penalty and reset the decay counter */
      client->state.required_unpaused_frames >>= 1;

      if (client->state.required_unpaused_frames <= RC_MINIMUM_UNPAUSED_FRAMES)
        client->state.required_unpaused_frames = RC_MINIMUM_UNPAUSED_FRAMES;

      client->state.unpaused_frame_decay =
        client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1) - 1;
    }
  }

  rc_client_idle(client);
}

void rc_client_idle(rc_client_t* client)
{
  rc_client_scheduled_callback_data_t* scheduled_callback;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->idle) {
    client->state.external_client->idle();
    return;
  }
#endif

  scheduled_callback = client->state.scheduled_callbacks;
  if (scheduled_callback) {
    const rc_clock_t now = client->callbacks.get_time_millisecs(client);

    do {
      rc_mutex_lock(&client->state.mutex);
      scheduled_callback = client->state.scheduled_callbacks;
      if (scheduled_callback) {
        if (scheduled_callback->when > now) {
          /* not time for next callback yet, ignore it */
          scheduled_callback = NULL;
        }
        else {
          /* remove the callback from the queue while we process it. callback can requeue if desired */
          client->state.scheduled_callbacks = scheduled_callback->next;
          scheduled_callback->next = NULL;
        }
      }
      rc_mutex_unlock(&client->state.mutex);

      if (!scheduled_callback)
        break;

      scheduled_callback->callback(scheduled_callback, client, now);
    } while (1);
  }

  if (client->state.disconnect & ~RC_CLIENT_DISCONNECT_VISIBLE)
    rc_client_raise_disconnect_events(client);
}

void rc_client_schedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* scheduled_callback)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  rc_mutex_lock(&client->state.mutex);

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;
    if (!next || scheduled_callback->when < next->when) {
      scheduled_callback->next = next;
      *last = scheduled_callback;
      break;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&client->state.mutex);
}

static void rc_client_reschedule_callback(rc_client_t* client,
  rc_client_scheduled_callback_data_t* callback, rc_clock_t when)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  /* ASSERT: this should only be called if the mutex is held */

  callback->when = when;

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;

    if (next == callback) {
      if (when == 0) {
        /* request to unschedule the callback */
        *last = next->next;
        next->next = NULL;
        break;
      }

      if (!next->next) {
         /* end of list, just append it */
         break;
      }

      if (when < next->next->when) {
        /* already in the correct place */
        break;
      }

      /* remove from current position - will insert later */
      *last = next->next;
      next->next = NULL;
      continue;
    }

    if (!next || (when < next->when && when != 0)) {
      /* insert here */
      callback->next = next;
      *last = callback;
      break;
    }

    last = &next->next;
  } while (1);
}

static void rc_client_reset_richpresence(rc_client_t* client)
{
  rc_runtime_richpresence_t* richpresence = client->game->runtime.richpresence;
  if (richpresence && richpresence->richpresence)
    rc_reset_richpresence(richpresence->richpresence);
}

static void rc_client_reset_variables(rc_client_t* client)
{
  if (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence)
    rc_reset_values(client->game->runtime.richpresence->richpresence->values);
}

static void rc_client_reset_all(rc_client_t* client)
{
  rc_client_reset_achievements(client);
  rc_client_reset_leaderboards(client);
  rc_client_reset_richpresence(client);
  rc_client_reset_variables(client);
}

void rc_client_reset(rc_client_t* client)
{
  rc_client_game_hash_t* game_hash;
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->reset) {
    client->state.external_client->reset();
    return;
  }
#endif

  if (!client->game)
    return;

  game_hash = rc_client_find_game_hash(client, client->game->public_.hash);
  if (game_hash && game_hash->game_id != client->game->public_.id) {
    /* current media is not for loaded game. unload game */
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabling runtime. Reset with non-game media loaded: %u (%s)",
        (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) ? 0 : game_hash->game_id, game_hash->hash);
    rc_client_unload_game(client);
    return;
  }

  RC_CLIENT_LOG_INFO(client, "Resetting runtime");

  rc_mutex_lock(&client->state.mutex);

  client->game->waiting_for_reset = 0;
  rc_client_reset_pending_events(client);

  rc_client_hide_progress_tracker(client, client->game);
  rc_client_reset_all(client);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client, client->game);
}

int rc_client_can_pause(rc_client_t* client, uint32_t* frames_remaining)
{
  if (!client)
    return 1;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->can_pause)
    return client->state.external_client->can_pause(frames_remaining);
#endif

  if (frames_remaining)
    *frames_remaining = 0;

  /* pause is always allowed in softcore */
  if (!rc_client_get_hardcore_enabled(client))
    return 1;

  /* a full decay means we haven't processed any frames since the last time this was called. */
  if (client->state.unpaused_frame_decay == client->state.required_unpaused_frames * RC_PAUSE_DECAY_MULTIPLIER)
    return 1;

  /* if less than RC_MINIMUM_UNPAUSED_FRAMES have been processed, don't allow the pause */
  if (client->state.unpaused_frame_decay > client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1)) {
    if (frames_remaining) {
      *frames_remaining = client->state.unpaused_frame_decay -
                          client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1);
    }
    return 0;
  }

  /* we're going to allow the emulator to pause. calculate how many frames are needed before the next
   * pause will be allowed. */

  if (client->state.unpaused_frame_decay > 0) {
    /* The user has paused within the decay window. Require a longer
     * run of unpaused frames before allowing the next pause */
    if (client->state.required_unpaused_frames < 5 * 60) /* don't make delay longer then 5 seconds */
      client->state.required_unpaused_frames += RC_MINIMUM_UNPAUSED_FRAMES;
  }

  /* require multiple unpaused_frames windows to decay the penalty */
  client->state.unpaused_frame_decay = client->state.required_unpaused_frames * RC_PAUSE_DECAY_MULTIPLIER;

  return 1;
}

size_t rc_client_progress_size(rc_client_t* client)
{
  size_t result;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->progress_size)
    return client->state.external_client->progress_size();
#endif

  if (!rc_client_is_game_loaded(client))
    return 0;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_progress_size(&client->game->runtime, NULL);
  rc_mutex_unlock(&client->state.mutex);

  return result;
}

int rc_client_serialize_progress(rc_client_t* client, uint8_t* buffer)
{
  return rc_client_serialize_progress_sized(client, buffer, 0xFFFFFFFF);
}

int rc_client_serialize_progress_sized(rc_client_t* client, uint8_t* buffer, size_t buffer_size)
{
  int result;

  if (!client)
    return RC_NO_GAME_LOADED;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->serialize_progress)
    return client->state.external_client->serialize_progress(buffer, buffer_size);
#endif

  if (!rc_client_is_game_loaded(client))
    return RC_NO_GAME_LOADED;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_serialize_progress_sized(buffer, (uint32_t)buffer_size, &client->game->runtime, NULL);
  rc_mutex_unlock(&client->state.mutex);

  return result;
}

static void rc_client_subset_before_deserialize_progress(rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;

  /* flag any visible challenge indicators to be hidden */
  achievement = subset->achievements;
  achievement_stop = achievement + subset->public_.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (trigger && trigger->state == RC_TRIGGER_STATE_PRIMED &&
        achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }
  }

  /* flag any visible trackers to be hidden */
  leaderboard = subset->leaderboards;
  leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (lboard && lboard->state == RC_LBOARD_STATE_STARTED &&
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
      leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD;
    }
  }
}

static void rc_client_subset_after_deserialize_progress(rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;

  /* flag any challenge indicators that should be shown */
  achievement = subset->achievements;
  achievement_stop = achievement + subset->public_.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED) {
      /* if it's already shown, just keep it. otherwise flag it to be shown */
      if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
        achievement->pending_events &= ~RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      }
      else {
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
      }
    }
    /* ASSERT: only active achievements are serialized, so we don't have to worry about
     *         deserialization deactiving them. */
  }

  /* flag any trackers that need to be shown */
  leaderboard = subset->leaderboards;
  leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard ||
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_INACTIVE ||
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
      continue;

    if (lboard->state == RC_LBOARD_STATE_STARTED) {
      leaderboard->value = (int)lboard->value.value.value;
      leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;

      /* if it's already being tracked, just update tracker. otherwise, allocate one */
      if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
        leaderboard->pending_events &= ~RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
        rc_client_update_leaderboard_tracker(game, leaderboard);
      }
      else {
        rc_client_allocate_leaderboard_tracker(game, leaderboard);
      }
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      /* deallocate the tracker (don't actually raise the failed event) */
      leaderboard->pending_events &= ~RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
      rc_client_release_leaderboard_tracker(game, leaderboard);
    }
  }
}

int rc_client_deserialize_progress(rc_client_t* client, const uint8_t* serialized)
{
  return rc_client_deserialize_progress_sized(client, serialized, 0xFFFFFFFF);
}

int rc_client_deserialize_progress_sized(rc_client_t* client, const uint8_t* serialized, size_t serialized_size)
{
  rc_client_subset_info_t* subset;
  int result;

  if (!client)
    return RC_NO_GAME_LOADED;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->deserialize_progress)
    return client->state.external_client->deserialize_progress(serialized, serialized_size);
#endif

  if (!rc_client_is_game_loaded(client))
    return RC_NO_GAME_LOADED;

  rc_mutex_lock(&client->state.mutex);

  rc_client_reset_pending_events(client);

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_before_deserialize_progress(subset);

  rc_client_hide_progress_tracker(client, client->game);

  if (!serialized) {
    rc_client_reset_all(client);
    result = RC_OK;
  }
  else {
    result = rc_runtime_deserialize_progress_sized(&client->game->runtime, serialized, (uint32_t)serialized_size, NULL);
  }

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_after_deserialize_progress(client->game, subset);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client, client->game);

  return result;
}

/* ===== Toggles ===== */

static void rc_client_enable_hardcore(rc_client_t* client)
{
  client->state.hardcore = 1;

  if (client->game) {
    rc_client_toggle_hardcore_achievements(client->game, client, RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE);
    rc_client_activate_leaderboards(client->game, client);

    /* disable processing until the client acknowledges the reset event by calling rc_runtime_reset() */
    RC_CLIENT_LOG_INFO(client, "Hardcore enabled, waiting for reset");
    client->game->waiting_for_reset = 1;
  }
  else {
    RC_CLIENT_LOG_INFO(client, "Hardcore enabled");
  }
}

static void rc_client_disable_hardcore(rc_client_t* client)
{
  client->state.hardcore = 0;
  RC_CLIENT_LOG_INFO(client, "Hardcore disabled");

  if (client->game) {
    rc_client_toggle_hardcore_achievements(client->game, client, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    if (!client->state.allow_leaderboards_in_softcore)
      rc_client_deactivate_leaderboards(client->game, client);
  }
}

void rc_client_set_hardcore_enabled(rc_client_t* client, int enabled)
{
  int changed = 0;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_hardcore_enabled) {
    client->state.external_client->set_hardcore_enabled(enabled);
    return;
  }
#endif

  rc_mutex_lock(&client->state.mutex);

  enabled = enabled ? 1 : 0;
  if (client->state.hardcore != enabled) {
    if (enabled)
      rc_client_enable_hardcore(client);
    else
      rc_client_disable_hardcore(client);

    changed = 1;
  }

  rc_mutex_unlock(&client->state.mutex);

  /* events must be raised outside of lock */
  if (changed && client->game) {
    if (enabled) {
      /* if enabling hardcore, notify client that a reset is requested */
      if (client->game->waiting_for_reset) {
        rc_client_event_t client_event;
        memset(&client_event, 0, sizeof(client_event));
        client_event.type = RC_CLIENT_EVENT_RESET;
        client->callbacks.event_handler(&client_event, client);
      }
    }
    else {
      /* if disabling hardcore, leaderboards will be deactivated. raise events for hiding trackers */
      rc_client_raise_pending_events(client, client->game);
    }
  }
}

int rc_client_get_hardcore_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_hardcore_enabled)
    return client->state.external_client->get_hardcore_enabled();
#endif

  return client->state.hardcore;
}

void rc_client_set_unofficial_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_unofficial_enabled) {
    client->state.external_client->set_unofficial_enabled(enabled);
    return;
  }
#endif

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Unofficial %s", enabled ? "enabled" : "disabled");
  client->state.unofficial_enabled = enabled ? 1 : 0;
}

int rc_client_get_unofficial_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_unofficial_enabled)
    return client->state.external_client->get_unofficial_enabled();
#endif

  return client->state.unofficial_enabled;
}

void rc_client_set_encore_mode_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_encore_mode_enabled) {
    client->state.external_client->set_encore_mode_enabled(enabled);
    return;
  }
#endif

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Encore mode %s", enabled ? "enabled" : "disabled");
  client->state.encore_mode = enabled ? 1 : 0;
}

int rc_client_get_encore_mode_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_encore_mode_enabled)
    return client->state.external_client->get_encore_mode_enabled();
#endif

  return client->state.encore_mode;
}

void rc_client_set_spectator_mode_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_spectator_mode_enabled) {
    client->state.external_client->set_spectator_mode_enabled(enabled);
    return;
  }
#endif

  if (!enabled && client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_LOCKED) {
    RC_CLIENT_LOG_WARN(client, "Spectator mode cannot be disabled if it was enabled prior to loading game.");
    return;
  }

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectator mode %s", enabled ? "enabled" : "disabled");
  client->state.spectator_mode = enabled ? RC_CLIENT_SPECTATOR_MODE_ON : RC_CLIENT_SPECTATOR_MODE_OFF;
}

int rc_client_get_spectator_mode_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_spectator_mode_enabled)
    return client->state.external_client->get_spectator_mode_enabled();
#endif

  return (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) ? 0 : 1;
}

void rc_client_set_userdata(rc_client_t* client, void* userdata)
{
  if (client)
    client->callbacks.client_data = userdata;
}

void* rc_client_get_userdata(const rc_client_t* client)
{
  return client ? client->callbacks.client_data : NULL;
}

void rc_client_set_host(rc_client_t* client, const char* hostname)
{
  if (!client)
    return;

  if (client->state.host.host && hostname && strcmp(hostname, client->state.host.host) == 0)
    return;

  /* clear out any previously specified host information */
  memset(&client->state.host, 0, sizeof(client->state.host));

  if (hostname && (!hostname[0] || strcmp(hostname, rc_api_default_host()) == 0)) {
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Using host: %s", rc_api_default_host());
    hostname = rc_api_default_host();
  }
  else {
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Using host: %s", hostname);
    client->state.host.host = rc_buffer_strcpy(&client->state.buffer, hostname);
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_host)
    client->state.external_client->set_host(hostname);
#endif
}

size_t rc_client_get_user_agent_clause(rc_client_t* client, char buffer[], size_t buffer_size)
{
  size_t result;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client && client->state.external_client && client->state.external_client->get_user_agent_clause) {
    result = client->state.external_client->get_user_agent_clause(buffer, buffer_size);
    if (result > 0) {
      result += snprintf(buffer + result, buffer_size - result, " rc_client/" RCHEEVOS_VERSION_STRING);
      buffer[buffer_size - 1] = '\0';
      return result;
    }
  }
#else
  (void)client;
#endif

  result = snprintf(buffer, buffer_size, "rcheevos/" RCHEEVOS_VERSION_STRING);

  /* some implementations of snprintf will fill the buffer without null terminating.
   * make sure the buffer is null terminated */
  buffer[buffer_size - 1] = '\0';
  return result;
}
