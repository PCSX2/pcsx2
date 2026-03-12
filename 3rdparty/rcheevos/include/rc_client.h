#ifndef RC_CLIENT_H
#define RC_CLIENT_H

#include "rc_api_request.h"
#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

RC_BEGIN_C_DECLS

/* implementation abstracted in rc_client_internal.h */
typedef struct rc_client_t rc_client_t;
typedef struct rc_client_async_handle_t rc_client_async_handle_t;

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

/**
 * Callback used to read num_bytes bytes from memory starting at address into buffer.
 * Returns the number of bytes read. A return value of 0 indicates the address was invalid.
 */
typedef uint32_t (RC_CCONV *rc_client_read_memory_func_t)(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client);

/**
 * Internal method passed to rc_client_server_call_t to process the server response.
 */
typedef void (RC_CCONV *rc_client_server_callback_t)(const rc_api_server_response_t* server_response, void* callback_data);

/**
 * Callback used to issue a request to the server.
 */
typedef void (RC_CCONV *rc_client_server_call_t)(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);

/**
 * Generic callback for asynchronous eventing.
 */
typedef void (RC_CCONV *rc_client_callback_t)(int result, const char* error_message, rc_client_t* client, void* userdata);

/**
 * Callback for logging or displaying a message.
 */
typedef void (RC_CCONV *rc_client_message_callback_t)(const char* message, const rc_client_t* client);

/*****************************************************************************\
| Runtime                                                                     |
\*****************************************************************************/

/**
 * Creates a new rc_client_t object.
 */
RC_EXPORT rc_client_t* RC_CCONV rc_client_create(rc_client_read_memory_func_t read_memory_function, rc_client_server_call_t server_call_function);

/**
 * Releases resources associated to a rc_client_t object.
 * Pointer will no longer be valid after making this call.
 */
RC_EXPORT void RC_CCONV rc_client_destroy(rc_client_t* client);

/**
 * Sets whether hardcore is enabled (on by default).
 * Can be called with a game loaded.
 * Enabling hardcore with a game loaded will raise an RC_CLIENT_EVENT_RESET
 * event. Processing will be disabled until rc_client_reset is called.
 */
RC_EXPORT void RC_CCONV rc_client_set_hardcore_enabled(rc_client_t* client, int enabled);

/**
 * Gets whether hardcore is enabled (on by default).
 */
RC_EXPORT int RC_CCONV rc_client_get_hardcore_enabled(const rc_client_t* client);

/**
 * Sets whether encore mode is enabled (off by default).
 * Evaluated when loading a game. Has no effect while a game is loaded.
 */
RC_EXPORT void RC_CCONV rc_client_set_encore_mode_enabled(rc_client_t* client, int enabled);

/**
 * Gets whether encore mode is enabled (off by default).
 */
RC_EXPORT int RC_CCONV rc_client_get_encore_mode_enabled(const rc_client_t* client);

/**
 * Sets whether unofficial achievements should be loaded.
 * Evaluated when loading a game. Has no effect while a game is loaded.
 */
RC_EXPORT void RC_CCONV rc_client_set_unofficial_enabled(rc_client_t* client, int enabled);

/**
 * Gets whether unofficial achievements should be loaded.
 */
RC_EXPORT int RC_CCONV rc_client_get_unofficial_enabled(const rc_client_t* client);

/**
 * Sets whether spectator mode is enabled (off by default).
 * If enabled, events for achievement unlocks and leaderboard submissions will be
 * raised, but server calls to actually perform the unlock/submit will not occur.
 * Can be modified while a game is loaded. Evaluated at unlock/submit time.
 * Cannot be modified if disabled before a game is loaded.
 */
RC_EXPORT void RC_CCONV rc_client_set_spectator_mode_enabled(rc_client_t* client, int enabled);

/**
 * Gets whether spectator mode is enabled (off by default).
 */
RC_EXPORT int RC_CCONV rc_client_get_spectator_mode_enabled(const rc_client_t* client);

/**
 * Attaches client-specific data to the runtime.
 */
RC_EXPORT void RC_CCONV rc_client_set_userdata(rc_client_t* client, void* userdata);

/**
 * Gets the client-specific data attached to the runtime.
 */
RC_EXPORT void* RC_CCONV rc_client_get_userdata(const rc_client_t* client);

/**
 * Sets the name of the server to use.
 */
RC_EXPORT void RC_CCONV rc_client_set_host(rc_client_t* client, const char* hostname);

typedef uint64_t rc_clock_t;
typedef rc_clock_t (RC_CCONV *rc_get_time_millisecs_func_t)(const rc_client_t* client);

/**
 * Specifies a function that returns a value that increases once per millisecond.
 */
RC_EXPORT void RC_CCONV rc_client_set_get_time_millisecs_function(rc_client_t* client, rc_get_time_millisecs_func_t handler);

/**
 * Marks an async process as aborted. The associated callback will not be called.
 */
RC_EXPORT void RC_CCONV rc_client_abort_async(rc_client_t* client, rc_client_async_handle_t* async_handle);

/**
 * Gets a clause that can be added to the User-Agent to identify the version of rcheevos being used.
 */
RC_EXPORT size_t RC_CCONV rc_client_get_user_agent_clause(rc_client_t* client, char buffer[], size_t buffer_size);

/*****************************************************************************\
| Logging                                                                     |
\*****************************************************************************/

/**
 * Sets the logging level and provides a callback to be called to do the logging.
 */
RC_EXPORT void RC_CCONV rc_client_enable_logging(rc_client_t* client, int level, rc_client_message_callback_t callback);
enum {
  RC_CLIENT_LOG_LEVEL_NONE = 0,
  RC_CLIENT_LOG_LEVEL_ERROR = 1,
  RC_CLIENT_LOG_LEVEL_WARN = 2,
  RC_CLIENT_LOG_LEVEL_INFO = 3,
  RC_CLIENT_LOG_LEVEL_VERBOSE = 4,
  NUM_RC_CLIENT_LOG_LEVELS = 5
};

/*****************************************************************************\
| User                                                                        |
\*****************************************************************************/

/**
 * Attempt to login a user.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_login_with_password(rc_client_t* client,
    const char* username, const char* password,
    rc_client_callback_t callback, void* callback_userdata);

/**
 * Attempt to login a user.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_login_with_token(rc_client_t* client,
    const char* username, const char* token,
    rc_client_callback_t callback, void* callback_userdata);

/**
 * Logout the user.
 */
RC_EXPORT void RC_CCONV rc_client_logout(rc_client_t* client);

typedef struct rc_client_user_t {
  const char* display_name;
  const char* username;
  const char* token;
  uint32_t score;
  uint32_t score_softcore;
  uint32_t num_unread_messages;
  /* minimum version: 12.0 */
  const char* avatar_url;
} rc_client_user_t;

/**
 * Gets information about the logged in user. Will return NULL if the user is not logged in.
 */
RC_EXPORT const rc_client_user_t* RC_CCONV rc_client_get_user_info(const rc_client_t* client);

/**
 * Gets the URL for the user's profile picture.
 * Returns RC_OK on success.
 */
RC_EXPORT int RC_CCONV rc_client_user_get_image_url(const rc_client_user_t* user, char buffer[], size_t buffer_size);

typedef struct rc_client_user_game_summary_t {
  uint32_t num_core_achievements;
  uint32_t num_unofficial_achievements;
  uint32_t num_unlocked_achievements;
  uint32_t num_unsupported_achievements;

  uint32_t points_core;
  uint32_t points_unlocked;

  /* minimum version: 12.1 */
  time_t beaten_time;
  time_t completed_time;
} rc_client_user_game_summary_t;

/**
 * Gets a breakdown of the number of achievements in the game, and how many the user has unlocked.
 * Used for the "You have unlocked X of Y achievements" message shown when the game starts.
 */
RC_EXPORT void RC_CCONV rc_client_get_user_game_summary(const rc_client_t* client, rc_client_user_game_summary_t* summary);

typedef struct rc_client_all_user_progress_entry_t {
  uint32_t game_id;
  uint32_t num_achievements;
  uint32_t num_unlocked_achievements;
  uint32_t num_unlocked_achievements_hardcore;
} rc_client_all_user_progress_entry_t;

typedef struct rc_client_all_user_progress_t {
  rc_client_all_user_progress_entry_t* entries;
  uint32_t num_entries;
} rc_client_all_user_progress_t;

/**
 * Callback that is fired when an all progress query completes. list may be null if the query failed.
 */
typedef void(RC_CCONV* rc_client_fetch_all_user_progress_callback_t)(int result, const char* error_message,
                                                                     rc_client_all_user_progress_t* list,
                                                                     rc_client_t* client, void* callback_userdata);

/**
 * Starts an asynchronous request for all progress for the given console.
 * This query returns the total number of achievements for all games tracked by this console, as well as
 * the user's achievement unlock count for both softcore and hardcore modes.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV
rc_client_begin_fetch_all_user_progress(rc_client_t* client, uint32_t console_id,
                                        rc_client_fetch_all_user_progress_callback_t callback, void* callback_userdata);

/**
 * Destroys a previously-allocated result from the rc_client_begin_fetch_all_progress_list() callback.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_all_user_progress(rc_client_all_user_progress_t* list);

/*****************************************************************************\
| Game                                                                        |
\*****************************************************************************/

#ifdef RC_CLIENT_SUPPORTS_HASH
/**
 * Start loading an unidentified game.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path,
    const uint8_t* data, size_t data_size,
    rc_client_callback_t callback, void* callback_userdata);

struct rc_hash_callbacks;
/**
 * Provide callback functions for interacting with the file system and processing disc-based files when generating hashes.
 */
RC_EXPORT void rc_client_set_hash_callbacks(rc_client_t* client, const struct rc_hash_callbacks* callbacks);
#endif

/**
 * Start loading a game.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_load_game(rc_client_t* client, const char* hash,
    rc_client_callback_t callback, void* callback_userdata);

/**
 * Gets the current progress of the asynchronous load game process.
 */
RC_EXPORT int RC_CCONV rc_client_get_load_game_state(const rc_client_t* client);
enum {
  RC_CLIENT_LOAD_GAME_STATE_NONE,
  RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN,
  RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME,
  RC_CLIENT_LOAD_GAME_STATE_FETCHING_GAME_DATA, /* [deprecated] - game data is now returned by identify call */
  RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION,
  RC_CLIENT_LOAD_GAME_STATE_DONE,
  RC_CLIENT_LOAD_GAME_STATE_ABORTED
};

/**
 * Determines if a game was successfully identified and loaded.
 */
RC_EXPORT int RC_CCONV rc_client_is_game_loaded(const rc_client_t* client);

/**
 * Unloads the current game.
 */
RC_EXPORT void RC_CCONV rc_client_unload_game(rc_client_t* client);

typedef struct rc_client_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  const char* badge_name;
  /* minimum version: 12.0 */
  const char* badge_url;
} rc_client_game_t;

/**
 * Get information about the current game. Returns NULL if no game is loaded.
 * NOTE: returns a dummy game record if an unidentified game is loaded.
 */
RC_EXPORT const rc_client_game_t* RC_CCONV rc_client_get_game_info(const rc_client_t* client);

/**
 * Gets the URL for the game image.
 * Returns RC_OK on success.
 */
RC_EXPORT int RC_CCONV rc_client_game_get_image_url(const rc_client_game_t* game, char buffer[], size_t buffer_size);

#ifdef RC_CLIENT_SUPPORTS_HASH
/**
 * Changes the active disc in a multi-disc game.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_identify_and_change_media(rc_client_t* client, const char* file_path,
    const uint8_t* data, size_t data_size, rc_client_callback_t callback, void* callback_userdata);
#endif

/**
 * Changes the active disc in a multi-disc game.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_change_media(rc_client_t* client, const char* hash,
    rc_client_callback_t callback, void* callback_userdata);
/* this function was renamed in rcheevos 12.0 */
#define rc_client_begin_change_media_from_hash rc_client_begin_change_media

/*****************************************************************************\
| Subsets                                                                     |
\*****************************************************************************/

typedef struct rc_client_subset_t {
  uint32_t id;
  const char* title;
  char badge_name[16];

  uint32_t num_achievements;
  uint32_t num_leaderboards;

  /* minimum version: 12.0 */
  const char* badge_url;
} rc_client_subset_t;

RC_EXPORT const rc_client_subset_t* RC_CCONV rc_client_get_subset_info(rc_client_t* client, uint32_t subset_id);

RC_EXPORT void RC_CCONV rc_client_get_user_subset_summary(const rc_client_t* client, uint32_t subset_id, rc_client_user_game_summary_t* summary);

typedef struct rc_client_subset_list_t {
  const rc_client_subset_t** subsets;
  uint32_t num_subsets;
} rc_client_subset_list_t;

/**
 * Creates a list of subsets for the currently loaded game.
 * Returns an allocated list that must be free'd by calling rc_client_destroy_subset_list.
 */
RC_EXPORT rc_client_subset_list_t* RC_CCONV rc_client_create_subset_list(rc_client_t* client);

/**
 * Destroys a list allocated by rc_client_create_subset_list_list.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_subset_list(rc_client_subset_list_t* list);

/*****************************************************************************\
| Fetch Game Hashes                                                           |
\*****************************************************************************/

typedef struct rc_client_hash_library_entry_t {
  char hash[33];
  uint32_t game_id;
} rc_client_hash_library_entry_t;

typedef struct rc_client_hash_library_t {
  rc_client_hash_library_entry_t* entries;
  uint32_t num_entries;
} rc_client_hash_library_t;

/**
 * Callback that is fired when a hash library request completes. list may be null if the query failed.
 */
typedef void(RC_CCONV* rc_client_fetch_hash_library_callback_t)(int result, const char* error_message,
                                                                rc_client_hash_library_t* list, rc_client_t* client,
                                                                void* callback_userdata);

/**
 * Starts an asynchronous request for all hashes for the given console.
 * This request returns a mapping from hashes to the game's unique identifier. A single game may have multiple
 * hashes in the case of multi-disc games, or variants that are still compatible with the same achievement set.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_fetch_hash_library(
  rc_client_t* client, uint32_t console_id, rc_client_fetch_hash_library_callback_t callback, void* callback_userdata);

/**
 * Destroys a previously-allocated result from the rc_client_destroy_hash_library() callback.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_hash_library(rc_client_hash_library_t* list);

/*****************************************************************************\
| Fetch Game Titles                                                           |
\*****************************************************************************/

typedef struct rc_client_game_title_entry_t {
  uint32_t game_id;
  const char* title;
  char badge_name[16];
  const char* badge_url;
} rc_client_game_title_entry_t;

typedef struct rc_client_game_title_list_t {
  rc_client_game_title_entry_t* entries;
  uint32_t num_entries;
} rc_client_game_title_list_t;

/**
 * Callback that is fired when a game titles request completes. list may be null if the query failed.
 */
typedef void(RC_CCONV* rc_client_fetch_game_titles_callback_t)(int result, const char* error_message,
                                                               rc_client_game_title_list_t* list, rc_client_t* client,
                                                               void* callback_userdata);

/**
 * Starts an asynchronous request for titles and badge names for the specified games.
 * The caller must provide an array of game IDs and the number of IDs in the array.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_fetch_game_titles(
  rc_client_t* client, const uint32_t* game_ids, uint32_t num_game_ids,
  rc_client_fetch_game_titles_callback_t callback, void* callback_userdata);

/**
 * Destroys a previously-allocated result from the rc_client_begin_fetch_game_titles() callback.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_game_title_list(rc_client_game_title_list_t* list);

/*****************************************************************************\
| Achievements                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE = 0, /* unprocessed */
  RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE = 1,   /* eligible to trigger */
  RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED = 2, /* earned by user */
  RC_CLIENT_ACHIEVEMENT_STATE_DISABLED = 3, /* not supported by this version of the runtime */
  NUM_RC_CLIENT_ACHIEVEMENT_STATES = 4
};

enum {
  RC_CLIENT_ACHIEVEMENT_CATEGORY_NONE = 0,
  RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE = (1 << 0),
  RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL = (1 << 1),
  RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL = RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE | RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL
};

enum {
  RC_CLIENT_ACHIEVEMENT_TYPE_STANDARD = 0,
  RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE = 1,
  RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION = 2,
  RC_CLIENT_ACHIEVEMENT_TYPE_WIN = 3
};

enum {
  RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN = 0,
  RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED = 1,
  RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED = 2,
  RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED = 3,
  RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL = 4,
  RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED = 5,
  RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE = 6,
  RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE = 7,
  RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED = 8,
  NUM_RC_CLIENT_ACHIEVEMENT_BUCKETS = 9
};

enum {
  RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE = 0,
  RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE = (1 << 0),
  RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE = (1 << 1),
  RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH = RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE | RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE
};

typedef struct rc_client_achievement_t {
  const char* title;
  const char* description;
  char badge_name[8];
  char measured_progress[24];
  float measured_percent;
  uint32_t id;
  uint32_t points;
  time_t unlock_time;
  uint8_t state;
  uint8_t category;
  uint8_t bucket;
  uint8_t unlocked;
  /* minimum version: 11.1 */
  float rarity;
  float rarity_hardcore;
  uint8_t type;
  /* minimum version: 12.0 */
  const char* badge_url;
  const char* badge_locked_url;
} rc_client_achievement_t;

/**
 * Get information about an achievement. Returns NULL if not found.
 */
RC_EXPORT const rc_client_achievement_t* RC_CCONV rc_client_get_achievement_info(rc_client_t* client, uint32_t id);

/**
 * Gets the next achievement after a provided achievement that fits in the specified bucket. Returns NULL if none found.
 */
RC_EXPORT const rc_client_achievement_t * RC_CCONV rc_client_get_next_achievement_info(rc_client_t * client, const rc_client_achievement_t * achievement, int bucket);

/**
 * Gets the URL for the achievement image.
 * Returns RC_OK on success.
 */
RC_EXPORT int RC_CCONV rc_client_achievement_get_image_url(const rc_client_achievement_t* achievement, int state, char buffer[], size_t buffer_size);

typedef struct rc_client_achievement_bucket_t {
  const rc_client_achievement_t** achievements;
  uint32_t num_achievements;

  const char* label;
  uint32_t subset_id;
  uint8_t bucket_type;
} rc_client_achievement_bucket_t;

typedef struct rc_client_achievement_list_t {
  const rc_client_achievement_bucket_t* buckets;
  uint32_t num_buckets;
} rc_client_achievement_list_t;

enum {
  RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE = 0,
  RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS = 1
};

/**
 * Creates a list of achievements matching the specified category and grouping.
 * Returns an allocated list that must be free'd by calling rc_client_destroy_achievement_list.
 */
RC_EXPORT rc_client_achievement_list_t* RC_CCONV rc_client_create_achievement_list(rc_client_t* client, int category, int grouping);

/**
 * Destroys a list allocated by rc_client_create_achievement_list.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_achievement_list(rc_client_achievement_list_t* list);

/**
 * Returns non-zero if there are any achievements that can be queried through rc_client_create_achievement_list().
 */
RC_EXPORT int RC_CCONV rc_client_has_achievements(rc_client_t* client);

/*****************************************************************************\
| Leaderboards                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_LEADERBOARD_STATE_INACTIVE = 0,
  RC_CLIENT_LEADERBOARD_STATE_ACTIVE = 1,
  RC_CLIENT_LEADERBOARD_STATE_TRACKING = 2,
  RC_CLIENT_LEADERBOARD_STATE_DISABLED = 3,
  NUM_RC_CLIENT_LEADERBOARD_STATES = 4
};

enum {
  RC_CLIENT_LEADERBOARD_FORMAT_TIME = 0,
  RC_CLIENT_LEADERBOARD_FORMAT_SCORE = 1,
  RC_CLIENT_LEADERBOARD_FORMAT_VALUE = 2,
  NUM_RC_CLIENT_LEADERBOARD_FORMATS = 3
};

#define RC_CLIENT_LEADERBOARD_DISPLAY_SIZE 24

typedef struct rc_client_leaderboard_t {
  const char* title;
  const char* description;
  const char* tracker_value;
  uint32_t id;
  uint8_t state;
  uint8_t format;
  uint8_t lower_is_better;
} rc_client_leaderboard_t;

/**
 * Get information about a leaderboard. Returns NULL if not found.
 */
RC_EXPORT const rc_client_leaderboard_t* RC_CCONV rc_client_get_leaderboard_info(const rc_client_t* client, uint32_t id);

typedef struct rc_client_leaderboard_tracker_t {
  char display[RC_CLIENT_LEADERBOARD_DISPLAY_SIZE];
  uint32_t id;
} rc_client_leaderboard_tracker_t;

typedef struct rc_client_leaderboard_bucket_t {
  const rc_client_leaderboard_t** leaderboards;
  uint32_t num_leaderboards;

  const char* label;
  uint32_t subset_id;
  uint8_t bucket_type;
} rc_client_leaderboard_bucket_t;

typedef struct rc_client_leaderboard_list_t {
  const rc_client_leaderboard_bucket_t* buckets;
  uint32_t num_buckets;
} rc_client_leaderboard_list_t;

enum {
  RC_CLIENT_LEADERBOARD_BUCKET_UNKNOWN = 0,
  RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE = 1,
  RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE = 2,
  RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED = 3,
  RC_CLIENT_LEADERBOARD_BUCKET_ALL = 4,
  NUM_RC_CLIENT_LEADERBOARD_BUCKETS = 5
};

enum {
  RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE = 0,
  RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING = 1
};

/**
 * Creates a list of leaderboards matching the specified grouping.
 * Returns an allocated list that must be free'd by calling rc_client_destroy_leaderboard_list.
 */
RC_EXPORT rc_client_leaderboard_list_t* RC_CCONV rc_client_create_leaderboard_list(rc_client_t* client, int grouping);

/**
 * Destroys a list allocated by rc_client_create_leaderboard_list.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_leaderboard_list(rc_client_leaderboard_list_t* list);

/**
 * Returns non-zero if the current game has any leaderboards.
 */
RC_EXPORT int RC_CCONV rc_client_has_leaderboards(rc_client_t* client);

typedef struct rc_client_leaderboard_entry_t {
  const char* user;
  char display[RC_CLIENT_LEADERBOARD_DISPLAY_SIZE];
  time_t submitted;
  uint32_t rank;
  uint32_t index;
} rc_client_leaderboard_entry_t;

typedef struct rc_client_leaderboard_entry_list_t {
  rc_client_leaderboard_entry_t* entries;
  uint32_t num_entries;
  uint32_t total_entries;
  int32_t user_index;
} rc_client_leaderboard_entry_list_t;

typedef void (RC_CCONV *rc_client_fetch_leaderboard_entries_callback_t)(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata);

/**
 * Fetches a list of leaderboard entries from the server.
 * Callback receives an allocated list that must be free'd by calling rc_client_destroy_leaderboard_entry_list.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_fetch_leaderboard_entries(rc_client_t* client, uint32_t leaderboard_id,
    uint32_t first_entry, uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata);

/**
 * Fetches a list of leaderboard entries from the server containing the logged-in user.
 * Callback receives an allocated list that must be free'd by calling rc_client_destroy_leaderboard_entry_list.
 */
RC_EXPORT rc_client_async_handle_t* RC_CCONV rc_client_begin_fetch_leaderboard_entries_around_user(rc_client_t* client, uint32_t leaderboard_id,
    uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata);

/**
 * Gets the URL for the profile picture of the user associated to a leaderboard entry.
 * Returns RC_OK on success.
 */
RC_EXPORT int RC_CCONV rc_client_leaderboard_entry_get_user_image_url(const rc_client_leaderboard_entry_t* entry, char buffer[], size_t buffer_size);

/**
 * Destroys a list allocated by rc_client_begin_fetch_leaderboard_entries or rc_client_begin_fetch_leaderboard_entries_around_user.
 */
RC_EXPORT void RC_CCONV rc_client_destroy_leaderboard_entry_list(rc_client_leaderboard_entry_list_t* list);

/**
 * Used for scoreboard events. Contains the response from the server when a leaderboard entry is submitted.
 * NOTE: This structure is only valid within the event callback. If you want to make use of the data outside
 * of the callback, you should create copies of both the top entries and usernames within.
 */
typedef struct rc_client_leaderboard_scoreboard_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  uint32_t rank;
  /* The value of the entry */
  char score[RC_CLIENT_LEADERBOARD_DISPLAY_SIZE];
} rc_client_leaderboard_scoreboard_entry_t;

typedef struct rc_client_leaderboard_scoreboard_t {
  /* The ID of the leaderboard which was submitted */
  uint32_t leaderboard_id;
  /* The value that was submitted */
  char submitted_score[RC_CLIENT_LEADERBOARD_DISPLAY_SIZE];
  /* The player's best submitted value */
  char best_score[RC_CLIENT_LEADERBOARD_DISPLAY_SIZE];
  /* The player's new rank within the leaderboard */
  uint32_t new_rank;
  /* The total number of entries in the leaderboard */
  uint32_t num_entries;

  /* An array of the top entries for the leaderboard */
  rc_client_leaderboard_scoreboard_entry_t* top_entries;
  /* The number of items in the top_entries array */
  uint32_t num_top_entries;
} rc_client_leaderboard_scoreboard_t;

/*****************************************************************************\
| Rich Presence                                                               |
\*****************************************************************************/

/**
 * Returns non-zero if the current game supports rich presence.
 */
RC_EXPORT int RC_CCONV rc_client_has_rich_presence(rc_client_t* client);

/**
 * Gets the current rich presence message.
 * Returns the number of characters written to buffer.
 */
RC_EXPORT size_t RC_CCONV rc_client_get_rich_presence_message(rc_client_t* client, char buffer[], size_t buffer_size);

/*****************************************************************************\
| Processing                                                                  |
\*****************************************************************************/

enum {
  RC_CLIENT_EVENT_TYPE_NONE = 0,
  RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED = 1, /* [achievement] was earned by the player */
  RC_CLIENT_EVENT_LEADERBOARD_STARTED = 2, /* [leaderboard] attempt has started */
  RC_CLIENT_EVENT_LEADERBOARD_FAILED = 3, /* [leaderboard] attempt failed */
  RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED = 4, /* [leaderboard] attempt submitted */
  RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW = 5, /* [achievement] challenge indicator should be shown */
  RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE = 6, /* [achievement] challenge indicator should be hidden */
  RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW = 7, /* progress indicator should be shown for [achievement] */
  RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE = 8, /* progress indicator should be hidden */
  RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE = 9, /* progress indicator should be updated to reflect new badge/progress for [achievement] */
  RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW = 10, /* [leaderboard_tracker] should be shown */
  RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE = 11, /* [leaderboard_tracker] should be hidden */
  RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE = 12, /* [leaderboard_tracker] updated */
  RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD = 13, /* [leaderboard_scoreboard] possibly-new ranking received for [leaderboard] */
  RC_CLIENT_EVENT_RESET = 14, /* emulated system should be reset (as the result of enabling hardcore) */
  RC_CLIENT_EVENT_GAME_COMPLETED = 15, /* all achievements for the game have been earned */
  RC_CLIENT_EVENT_SERVER_ERROR = 16, /* an API response returned a [server_error] and will not be retried */
  RC_CLIENT_EVENT_DISCONNECTED = 17, /* an unlock request could not be completed and is pending */
  RC_CLIENT_EVENT_RECONNECTED = 18, /* all pending unlocks have been completed */
  RC_CLIENT_EVENT_SUBSET_COMPLETED = 19 /* all achievements for the subset have been earned */
};

typedef struct rc_client_server_error_t {
  const char* error_message;
  const char* api;
  int result;
  uint32_t related_id;
} rc_client_server_error_t;

typedef struct rc_client_event_t {
  uint32_t type;

  rc_client_achievement_t* achievement;
  rc_client_leaderboard_t* leaderboard;
  rc_client_leaderboard_tracker_t* leaderboard_tracker;
  rc_client_leaderboard_scoreboard_t* leaderboard_scoreboard;
  rc_client_server_error_t* server_error;
  rc_client_subset_t* subset;

} rc_client_event_t;

/**
 * Callback used to notify the client when certain events occur.
 */
typedef void (RC_CCONV *rc_client_event_handler_t)(const rc_client_event_t* event, rc_client_t* client);

/**
 * Provides a callback for event handling.
 */
RC_EXPORT void RC_CCONV rc_client_set_event_handler(rc_client_t* client, rc_client_event_handler_t handler);

/**
 * Provides a callback for reading memory.
 */
RC_EXPORT void RC_CCONV rc_client_set_read_memory_function(rc_client_t* client, rc_client_read_memory_func_t handler);

/**
 * Specifies whether rc_client is allowed to read memory outside of rc_client_do_frame/rc_client_idle.
 */
RC_EXPORT void RC_CCONV rc_client_set_allow_background_memory_reads(rc_client_t* client, int allowed);

/**
 * Determines if there are any active achievements/leaderboards/rich presence that need processing.
 */
RC_EXPORT int RC_CCONV rc_client_is_processing_required(rc_client_t* client);

/**
 * Processes achievements for the current frame.
 */
RC_EXPORT void RC_CCONV rc_client_do_frame(rc_client_t* client);

/**
 * Processes the periodic queue.
 * Called internally by rc_client_do_frame.
 * Should be explicitly called if rc_client_do_frame is not being called because emulation is paused.
 */
RC_EXPORT void RC_CCONV rc_client_idle(rc_client_t* client);

/**
 * Determines if a sufficient amount of frames have been processed since the last call to rc_client_can_pause.
 * Should not be called unless the client is trying to pause.
 * If false is returned, and frames_remaining is not NULL, frames_remaining will be set to the number of frames
 * still required before pause is allowed, which can be converted to a time in seconds for displaying to the user.
 */
RC_EXPORT int RC_CCONV rc_client_can_pause(rc_client_t* client, uint32_t* frames_remaining);

/**
 * Informs the runtime that the emulator has been reset. Will reset all achievements and leaderboards
 * to their initial state (includes hiding indicators/trackers).
 */
RC_EXPORT void RC_CCONV rc_client_reset(rc_client_t* client);

/**
 * Gets the number of bytes needed to serialized the runtime state.
 */
RC_EXPORT size_t RC_CCONV rc_client_progress_size(rc_client_t* client);

/**
 * Serializes the runtime state into a buffer.
 * Returns RC_OK on success, or an error indicator.
 * [deprecated] use rc_client_serialize_progress_sized instead
 */
RC_EXPORT int RC_CCONV rc_client_serialize_progress(rc_client_t* client, uint8_t* buffer);

/**
 * Serializes the runtime state into a buffer.
 * Returns RC_OK on success, or an error indicator.
 */
RC_EXPORT int RC_CCONV rc_client_serialize_progress_sized(rc_client_t* client, uint8_t* buffer, size_t buffer_size);

/**
 * Deserializes the runtime state from a buffer.
 * Returns RC_OK on success, or an error indicator.
 * [deprecated] use rc_client_deserialize_progress_sized instead
 */
RC_EXPORT int RC_CCONV rc_client_deserialize_progress(rc_client_t* client, const uint8_t* serialized);

/**
 * Serializes the runtime state into a buffer.
 * Returns RC_OK on success, or an error indicator.
 */
RC_EXPORT int RC_CCONV rc_client_deserialize_progress_sized(rc_client_t* client, const uint8_t* serialized, size_t serialized_size);

RC_END_C_DECLS

#endif /* RC_RUNTIME_H */
