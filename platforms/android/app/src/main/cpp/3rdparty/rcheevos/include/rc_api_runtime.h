#ifndef RC_API_RUNTIME_H
#define RC_API_RUNTIME_H

#include "rc_api_request.h"

#include <stdint.h>
#include <time.h>

RC_BEGIN_C_DECLS

/* --- Fetch Image --- */

/**
 * API parameters for a fetch image request.
 * NOTE: fetch image server response is the raw image data. There is no rc_api_process_fetch_image_response function.
 */
typedef struct rc_api_fetch_image_request_t {
  /* The name of the image to fetch */
  const char* image_name;
  /* The type of image to fetch */
  uint32_t image_type;
}
rc_api_fetch_image_request_t;

#define RC_IMAGE_TYPE_GAME 1
#define RC_IMAGE_TYPE_ACHIEVEMENT 2
#define RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED 3
#define RC_IMAGE_TYPE_USER 4

RC_EXPORT int RC_CCONV rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_image_request_hosted(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params, const rc_api_host_t* host);

/* --- Resolve Hash --- */

/**
 * API parameters for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_request_t {
  /* Unused - hash lookup does not require credentials */
  const char* username;
  /* Unused - hash lookup does not require credentials */
  const char* api_token;
  /* The generated hash of the game to be identified */
  const char* game_hash;
}
rc_api_resolve_hash_request_t;

/**
 * Response data for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_response_t {
  /* The unique identifier of the game, 0 if no match was found */
  uint32_t game_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_resolve_hash_response_t;

RC_EXPORT int RC_CCONV rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_resolve_hash_request_hosted(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_resolve_hash_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_resolve_hash_server_response(rc_api_resolve_hash_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response);

/* --- Fetch Game Data --- */

/**
 * API parameters for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The generated hash of the game to be identified (ignored if game_id is not 0) */
  const char* game_hash;
}
rc_api_fetch_game_data_request_t;

/* A leaderboard definition */
typedef struct rc_api_leaderboard_definition_t {
  /* The unique identifier of the leaderboard */
  uint32_t id;
  /* The format to pass to rc_format_value to format the leaderboard value */
  int format;
  /* The title of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The definition of the leaderboard to be passed to rc_runtime_activate_lboard */
  const char* definition;
  /* Non-zero if lower values are better for this leaderboard */
  uint8_t lower_is_better;
  /* Non-zero if the leaderboard should not be displayed in a list of leaderboards */
  uint8_t hidden;
}
rc_api_leaderboard_definition_t;

/* An achievement definition */
typedef struct rc_api_achievement_definition_t {
  /* The unique identifier of the achievement */
  uint32_t id;
  /* The number of points the achievement is worth */
  uint32_t points;
  /* The achievement category (core, unofficial) */
  uint32_t category;
  /* The title of the achievement */
  const char* title;
  /* The description of the achievement */
  const char* description;
  /* The definition of the achievement to be passed to rc_runtime_activate_achievement */
  const char* definition;
  /* The author of the achievment */
  const char* author;
  /* The image name for the achievement badge */
  const char* badge_name;
  /* When the achievement was first uploaded to the server */
  time_t created;
  /* When the achievement was last modified on the server */
  time_t updated;
  /* The achievement type (win/progression/missable) */
  uint32_t type;
  /* The approximate rarity of the achievement (X% of users have earned the achievement) */
  float rarity;
  /* The approximate rarity of the achievement in hardcore (X% of users have earned the achievement in hardcore) */
  float rarity_hardcore;
  /* The URL for the achievement badge */
  const char* badge_url;
  /* The URL for the locked achievement badge */
  const char* badge_locked_url;
}
rc_api_achievement_definition_t;

#define RC_ACHIEVEMENT_CATEGORY_CORE 3
#define RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL 5

#define RC_ACHIEVEMENT_TYPE_STANDARD 0
#define RC_ACHIEVEMENT_TYPE_MISSABLE 1
#define RC_ACHIEVEMENT_TYPE_PROGRESSION 2
#define RC_ACHIEVEMENT_TYPE_WIN 3

/**
 * Response data for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_response_t {
  /* The unique identifier of the game */
  uint32_t id;
  /* The console associated to the game */
  uint32_t console_id;
  /* The title of the game */
  const char* title;
  /* The image name for the game badge */
  const char* image_name;
  /* The URL for the game badge */
  const char* image_url;
  /* The rich presence script for the game to be passed to rc_runtime_activate_richpresence */
  const char* rich_presence_script;

  /* An array of achievements for the game */
  rc_api_achievement_definition_t* achievements;
  /* The number of items in the achievements array */
  uint32_t num_achievements;

  /* An array of leaderboards for the game */
  rc_api_leaderboard_definition_t* leaderboards;
  /* The number of items in the leaderboards array */
  uint32_t num_leaderboards;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_game_data_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_game_data_request_hosted(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_fetch_game_data_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_game_data_server_response(rc_api_fetch_game_data_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response);

/* --- Fetch Game Sets --- */

/**
 * API parameters for a fetch game data request.
 */
typedef struct rc_api_fetch_game_sets_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The generated hash of the game to be identified (ignored if game_id is not 0) */
  const char* game_hash;
}
rc_api_fetch_game_sets_request_t;

#define RC_ACHIEVEMENT_SET_TYPE_CORE 0
#define RC_ACHIEVEMENT_SET_TYPE_BONUS 1
#define RC_ACHIEVEMENT_SET_TYPE_SPECIALTY 2
#define RC_ACHIEVEMENT_SET_TYPE_EXCLUSIVE 3

/* A game subset definition */
typedef struct rc_api_achievement_set_definition_t {
  /* The unique identifier of the achievement set */
  uint32_t id;
  /* The legacy game_id of the achievement set (used for editor API calls) */
  uint32_t game_id;
  /* The title of the achievement set */
  const char* title;
  /* The image name for the achievement set badge */
  const char* image_name;
  /* The URL for the achievement set badge */
  const char* image_url;

  /* An array of achievements for the achievement set */
  rc_api_achievement_definition_t* achievements;
  /* The number of items in the achievements array */
  uint32_t num_achievements;

  /* An array of leaderboards for the achievement set */
  rc_api_leaderboard_definition_t* leaderboards;
  /* The number of items in the leaderboards array */
  uint32_t num_leaderboards;

  /* The type of the achievement set */
  uint8_t type;
}
rc_api_achievement_set_definition_t;

/**
 * Response data for a fetch game sets request.
 */
typedef struct rc_api_fetch_game_sets_response_t {
  /* The unique identifier of the game */
  uint32_t id;
  /* The console associated to the game */
  uint32_t console_id;
  /* The title of the game */
  const char* title;
  /* The image name for the game badge */
  const char* image_name;
  /* The URL for the game badge */
  const char* image_url;
  /* The rich presence script for the game to be passed to rc_runtime_activate_richpresence */
  const char* rich_presence_script;
  /* The unique identifier of the game to use for session requests (startsession/ping/etc) */
  uint32_t session_game_id;

  /* An array of sets for the game */
  rc_api_achievement_set_definition_t* sets;
  /* The number of items in the sets array */
  uint32_t num_sets;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_game_sets_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_game_sets_request(rc_api_request_t* request, const rc_api_fetch_game_sets_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_game_sets_request_hosted(rc_api_request_t* request, const rc_api_fetch_game_sets_request_t* api_params, const rc_api_host_t* host);
RC_EXPORT int RC_CCONV rc_api_process_fetch_game_sets_server_response(rc_api_fetch_game_sets_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_game_sets_response(rc_api_fetch_game_sets_response_t* response);

/* --- Ping --- */

/**
 * API parameters for a ping request.
 */
typedef struct rc_api_ping_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* (optional) The current rich presence evaluation for the user */
  const char* rich_presence;
  /* (recommended) The hash associated to the game being played */
  const char* game_hash;
  /* Non-zero if hardcore is currently enabled (ignored if game_hash is null) */
  uint32_t hardcore;
}
rc_api_ping_request_t;

/**
 * Response data for a ping request.
 */
typedef struct rc_api_ping_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_ping_response_t;

RC_EXPORT int RC_CCONV rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_ping_request_hosted(rc_api_request_t* request, const rc_api_ping_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_ping_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_ping_server_response(rc_api_ping_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_ping_response(rc_api_ping_response_t* response);

/* --- Award Achievement --- */

/**
 * API parameters for an award achievement request.
 */
typedef struct rc_api_award_achievement_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement */
  uint32_t achievement_id;
  /* Non-zero if the achievement was earned in hardcore */
  uint32_t hardcore;
  /* The hash associated to the game being played */
  const char* game_hash;
  /* The number of seconds since the achievement was unlocked */
  uint32_t seconds_since_unlock;
}
rc_api_award_achievement_request_t;

/**
 * Response data for an award achievement request.
 */
typedef struct rc_api_award_achievement_response_t {
  /* The unique identifier of the achievement that was awarded */
  uint32_t awarded_achievement_id;
  /* The updated player score */
  uint32_t new_player_score;
  /* The updated player softcore score */
  uint32_t new_player_score_softcore;
  /* The number of achievements the user has not yet unlocked for this game
   * (in hardcore/non-hardcore per hardcore flag in request) */
  uint32_t achievements_remaining;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_award_achievement_response_t;

RC_EXPORT int RC_CCONV rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_award_achievement_request_hosted(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_award_achievement_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_award_achievement_server_response(rc_api_award_achievement_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response);

/* --- Submit Leaderboard Entry --- */

/**
 * API parameters for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the leaderboard */
  uint32_t leaderboard_id;
  /* The value being submitted */
  int32_t score;
  /* The hash associated to the game being played */
  const char* game_hash;
  /* The number of seconds since the leaderboard attempt was completed */
  uint32_t seconds_since_completion;
}
rc_api_submit_lboard_entry_request_t;

/* A leaderboard entry */
typedef struct rc_api_lboard_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  uint32_t rank;
  /* The value of the entry */
  int32_t score;
}
rc_api_lboard_entry_t;

/**
 * Response data for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_response_t {
  /* The value that was submitted */
  int32_t submitted_score;
  /* The player's best submitted value */
  int32_t best_score;
  /* The player's new rank within the leaderboard */
  uint32_t new_rank;
  /* The total number of entries in the leaderboard */
  uint32_t num_entries;

  /* An array of the top entries for the leaderboard */
  rc_api_lboard_entry_t* top_entries;
  /* The number of items in the top_entries array */
  uint32_t num_top_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_submit_lboard_entry_response_t;

RC_EXPORT int RC_CCONV rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_submit_lboard_entry_request_hosted(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_submit_lboard_entry_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_submit_lboard_entry_server_response(rc_api_submit_lboard_entry_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response);

RC_END_C_DECLS

#endif /* RC_API_RUNTIME_H */
