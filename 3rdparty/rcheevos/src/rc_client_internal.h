#ifndef RC_CLIENT_INTERNAL_H
#define RC_CLIENT_INTERNAL_H

#include "rc_client.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
 #include "rc_client_raintegration_internal.h"
#endif
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
 #include "rc_client_external.h"
#endif

#include "rc_compat.h"
#include "rc_runtime.h"
#include "rc_runtime_types.h"

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

struct rc_api_fetch_game_data_response_t;
typedef void (RC_CCONV *rc_client_post_process_game_data_response_t)(const rc_api_server_response_t* server_response,
              struct rc_api_fetch_game_data_response_t* game_data_response, rc_client_t* client, void* userdata);
typedef int (RC_CCONV *rc_client_can_submit_achievement_unlock_t)(uint32_t achievement_id, rc_client_t* client);
typedef int (RC_CCONV *rc_client_can_submit_leaderboard_entry_t)(uint32_t leaderboard_id, rc_client_t* client);
typedef int (RC_CCONV *rc_client_rich_presence_override_t)(rc_client_t* client, char buffer[], size_t buffersize);

typedef struct rc_client_callbacks_t {
  rc_client_read_memory_func_t read_memory;
  rc_client_event_handler_t event_handler;
  rc_client_server_call_t server_call;
  rc_client_message_callback_t log_call;
  rc_get_time_millisecs_func_t get_time_millisecs;
  rc_client_post_process_game_data_response_t post_process_game_data_response;
  rc_client_can_submit_achievement_unlock_t can_submit_achievement_unlock;
  rc_client_can_submit_leaderboard_entry_t can_submit_leaderboard_entry;
  rc_client_rich_presence_override_t rich_presence_override;

  void* client_data;
} rc_client_callbacks_t;

struct rc_client_scheduled_callback_data_t;
typedef void (RC_CCONV *rc_client_scheduled_callback_t)(struct rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);

typedef struct rc_client_scheduled_callback_data_t
{
  rc_clock_t when;
  uint32_t related_id;
  rc_client_scheduled_callback_t callback;
  void* data;
  struct rc_client_scheduled_callback_data_t* next;
} rc_client_scheduled_callback_data_t;

void rc_client_schedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* scheduled_callback);

struct rc_client_async_handle_t {
  uint8_t aborted;
};

int rc_client_async_handle_aborted(rc_client_t* client, rc_client_async_handle_t* async_handle);

/*****************************************************************************\
| Achievements                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE = 0,
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED = (1 << 1),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW = (1 << 2),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE = (1 << 3),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_UPDATE = (1 << 4) /* not a real event, just triggers update */
};

typedef struct rc_client_achievement_info_t {
  rc_client_achievement_t public_;

  rc_trigger_t* trigger;
  uint8_t md5[16];

  time_t unlock_time_hardcore;
  time_t unlock_time_softcore;

  uint8_t pending_events;

  const char* author;
  time_t created_time;
  time_t updated_time;
} rc_client_achievement_info_t;

struct rc_client_achievement_list_info_t;
typedef void (RC_CCONV *rc_client_destroy_achievement_list_func_t)(struct rc_client_achievement_list_info_t* list);

typedef struct rc_client_achievement_list_info_t {
  rc_client_achievement_list_t public_;
  rc_client_destroy_achievement_list_func_t destroy_func;
} rc_client_achievement_list_info_t;

enum {
  RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_UPDATE,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE
};

typedef struct rc_client_progress_tracker_t {
  rc_client_achievement_info_t* achievement;
  float progress;

  rc_client_scheduled_callback_data_t* hide_callback;
  uint8_t action;
} rc_client_progress_tracker_t;

/*****************************************************************************\
| Leaderboard Trackers                                                        |
\*****************************************************************************/

enum {
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE = (1 << 1),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW = (1 << 2),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE = (1 << 3)
};

typedef struct rc_client_leaderboard_tracker_info_t {
  rc_client_leaderboard_tracker_t public_;
  struct rc_client_leaderboard_tracker_info_t* next;
  int32_t raw_value;

  uint32_t value_djb2;

  uint8_t format;
  uint8_t pending_events;
  uint8_t reference_count;
  uint8_t value_from_hits;
} rc_client_leaderboard_tracker_info_t;

/*****************************************************************************\
| Leaderboards                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED = (1 << 1),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED = (1 << 2),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED = (1 << 3)
};

typedef struct rc_client_leaderboard_info_t {
  rc_client_leaderboard_t public_;

  rc_lboard_t* lboard;
  uint8_t md5[16];

  rc_client_leaderboard_tracker_info_t* tracker;

  uint32_t value_djb2;
  int32_t value;

  uint8_t format;
  uint8_t pending_events;
  uint8_t bucket;
  uint8_t hidden;
} rc_client_leaderboard_info_t;

struct rc_client_leaderboard_list_info_t;
typedef void (RC_CCONV *rc_client_destroy_leaderboard_list_func_t)(struct rc_client_leaderboard_list_info_t* list);

typedef struct rc_client_leaderboard_list_info_t {
  rc_client_leaderboard_list_t public_;
  rc_client_destroy_leaderboard_list_func_t destroy_func;
} rc_client_leaderboard_list_info_t;

struct rc_client_leaderboard_entry_list_info_t;
typedef void (RC_CCONV *rc_client_destroy_leaderboard_entry_list_func_t)(struct rc_client_leaderboard_entry_list_info_t* list);

typedef struct rc_client_leaderboard_entry_list_info_t {
  rc_client_leaderboard_entry_list_t public_;
  rc_client_destroy_leaderboard_entry_list_func_t destroy_func;
} rc_client_leaderboard_entry_list_info_t;

uint8_t rc_client_map_leaderboard_format(int format);

/*****************************************************************************\
| Subsets                                                                     |
\*****************************************************************************/

enum {
  RC_CLIENT_SUBSET_PENDING_EVENT_NONE = 0,
  RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT = (1 << 1),
  RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD = (1 << 2)
};

typedef struct rc_client_subset_info_t {
  rc_client_subset_t public_;

  rc_client_achievement_info_t* achievements;
  rc_client_leaderboard_info_t* leaderboards;

  struct rc_client_subset_info_t* next;

  const char* all_label;
  const char* inactive_label;
  const char* locked_label;
  const char* unlocked_label;
  const char* unofficial_label;
  const char* unsupported_label;

  uint8_t active;
  uint8_t mastery;
  uint8_t pending_events;
} rc_client_subset_info_t;

rc_client_async_handle_t* rc_client_begin_load_subset(rc_client_t* client, uint32_t subset_id, rc_client_callback_t callback, void* callback_userdata);

/*****************************************************************************\
| Game                                                                        |
\*****************************************************************************/

typedef struct rc_client_game_hash_t {
  char hash[33];
  uint32_t game_id;
  struct rc_client_game_hash_t* next;
} rc_client_game_hash_t;

rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* client, const char* hash);

typedef struct rc_client_media_hash_t {
  rc_client_game_hash_t* game_hash;
  struct rc_client_media_hash_t* next;
  uint32_t path_djb2;
} rc_client_media_hash_t;

enum {
  RC_CLIENT_GAME_PENDING_EVENT_NONE = 0,
  RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER = (1 << 1),
  RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS = (1 << 2),
  RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER = (1 << 3)
};

typedef struct rc_client_game_info_t {
  rc_client_game_t public_;
  rc_client_leaderboard_tracker_info_t* leaderboard_trackers;
  rc_client_progress_tracker_t progress_tracker;

  rc_client_subset_info_t* subsets;

  rc_client_media_hash_t* media_hash;

  rc_runtime_t runtime;

  uint32_t max_valid_address;

  uint8_t waiting_for_reset;
  uint8_t pending_events;

  rc_buffer_t buffer;
} rc_client_game_info_t;

void rc_client_update_active_achievements(rc_client_game_info_t* game);
void rc_client_update_active_leaderboards(rc_client_game_info_t* game);

/*****************************************************************************\
| Client                                                                      |
\*****************************************************************************/

enum {
  RC_CLIENT_LOAD_STATE_NONE,
  RC_CLIENT_LOAD_STATE_IDENTIFYING_GAME,
  RC_CLIENT_LOAD_STATE_AWAIT_LOGIN,
  RC_CLIENT_LOAD_STATE_FETCHING_GAME_DATA,
  RC_CLIENT_LOAD_STATE_STARTING_SESSION,
  RC_CLIENT_LOAD_STATE_DONE,
  RC_CLIENT_LOAD_STATE_UNKNOWN_GAME
};

enum {
  RC_CLIENT_USER_STATE_NONE,
  RC_CLIENT_USER_STATE_LOGIN_REQUESTED,
  RC_CLIENT_USER_STATE_LOGGED_IN
};

enum {
  RC_CLIENT_MASTERY_STATE_NONE,
  RC_CLIENT_MASTERY_STATE_PENDING,
  RC_CLIENT_MASTERY_STATE_SHOWN
};

enum {
  RC_CLIENT_SPECTATOR_MODE_OFF,
  RC_CLIENT_SPECTATOR_MODE_ON,
  RC_CLIENT_SPECTATOR_MODE_LOCKED
};

enum {
  RC_CLIENT_DISCONNECT_HIDDEN = 0,
  RC_CLIENT_DISCONNECT_VISIBLE = (1 << 0),
  RC_CLIENT_DISCONNECT_SHOW_PENDING = (1 << 1),
  RC_CLIENT_DISCONNECT_HIDE_PENDING = (1 << 2)
};

struct rc_client_load_state_t;

typedef struct rc_client_state_t {
  rc_mutex_t mutex;
  rc_buffer_t buffer;

  rc_client_scheduled_callback_data_t* scheduled_callbacks;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  rc_client_external_t* external_client;
#endif
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  rc_client_raintegration_t* raintegration;
#endif

  uint16_t unpaused_frame_decay;
  uint16_t required_unpaused_frames;

  uint8_t hardcore;
  uint8_t encore_mode;
  uint8_t spectator_mode;
  uint8_t unofficial_enabled;
  uint8_t log_level;
  uint8_t user;
  uint8_t disconnect;
  uint8_t allow_leaderboards_in_softcore;

  struct rc_client_load_state_t* load;
  struct rc_client_async_handle_t* async_handles[4];
  rc_memref_t* processing_memref;

  rc_peek_t legacy_peek;
} rc_client_state_t;

struct rc_client_t {
  rc_client_game_info_t* game;
  rc_client_game_hash_t* hashes;

  rc_client_user_t user;

  rc_client_callbacks_t callbacks;

  rc_client_state_t state;
};

/*****************************************************************************\
| Helpers                                                                     |
\*****************************************************************************/

#ifdef RC_NO_VARIADIC_MACROS
 void RC_CLIENT_LOG_ERR_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_WARN_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_INFO_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_VERBOSE_FORMATTED(const rc_client_t* client, const char* format, ...);
#else
 void rc_client_log_message_formatted(const rc_client_t* client, const char* format, ...);
 #define RC_CLIENT_LOG_ERR_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_WARN_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_INFO_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_VERBOSE_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
#endif

void rc_client_log_message(const rc_client_t* client, const char* message);
#define RC_CLIENT_LOG_ERR(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_WARN(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_INFO(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_VERBOSE(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) rc_client_log_message(client, message); }

/* internals pulled from runtime.c */
void rc_runtime_checksum(const char* memaddr, uint8_t* md5);
int rc_trigger_contains_memref(const rc_trigger_t* trigger, const rc_memref_t* memref);
int rc_value_contains_memref(const rc_value_t* value, const rc_memref_t* memref);
/* end runtime.c internals */

/* helper functions for unit tests */
struct rc_hash_iterator;
struct rc_hash_iterator* rc_client_get_load_state_hash_iterator(rc_client_t* client);
/* end helper functions for unit tests */

enum {
  RC_CLIENT_LEGACY_PEEK_AUTO,
  RC_CLIENT_LEGACY_PEEK_CONSTRUCTED,
  RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS
};

void rc_client_set_legacy_peek(rc_client_t* client, int method);

void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard);

RC_END_C_DECLS

#endif /* RC_CLIENT_INTERNAL_H */
