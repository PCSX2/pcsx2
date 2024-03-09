#ifndef RC_API_INFO_H
#define RC_API_INFO_H

#include "rc_api_request.h"

#include <stdint.h>
#include <time.h>

RC_BEGIN_C_DECLS

/* --- Fetch Achievement Info --- */

/**
 * API parameters for a fetch achievement info request.
 */
typedef struct rc_api_fetch_achievement_info_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement */
  uint32_t achievement_id;
  /* The 1-based index of the first entry to retrieve */
  uint32_t first_entry;
  /* The number of entries to retrieve */
  uint32_t count;
  /* Non-zero to only return unlocks earned by the user's friends */
  uint32_t friends_only;
}
rc_api_fetch_achievement_info_request_t;

/* An achievement awarded entry */
typedef struct rc_api_achievement_awarded_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* When the achievement was awarded */
  time_t awarded;
}
rc_api_achievement_awarded_entry_t;

/**
 * Response data for a fetch achievement info request.
 */
typedef struct rc_api_fetch_achievement_info_response_t {
  /* The unique identifier of the achievement */
  uint32_t id;
  /* The unique identifier of the game to which the leaderboard is associated */
  uint32_t game_id;
  /* The number of times the achievement has been awarded */
  uint32_t num_awarded;
  /* The number of players that have earned at least one achievement for the game */
  uint32_t num_players;

  /* An array of recently rewarded entries */
  rc_api_achievement_awarded_entry_t* recently_awarded;
  /* The number of items in the recently_awarded array */
  uint32_t num_recently_awarded;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_achievement_info_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_achievement_info_request(rc_api_request_t* request, const rc_api_fetch_achievement_info_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_process_fetch_achievement_info_response(rc_api_fetch_achievement_info_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_achievement_info_server_response(rc_api_fetch_achievement_info_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_achievement_info_response(rc_api_fetch_achievement_info_response_t* response);

/* --- Fetch Leaderboard Info --- */

/**
 * API parameters for a fetch leaderboard info request.
 */
typedef struct rc_api_fetch_leaderboard_info_request_t {
  /* The unique identifier of the leaderboard */
  uint32_t leaderboard_id;
  /* The number of entries to retrieve */
  uint32_t count;
  /* The 1-based index of the first entry to retrieve */
  uint32_t first_entry;
  /* The username of the player around whom the entries should be returned */
  const char* username;
}
rc_api_fetch_leaderboard_info_request_t;

/* A leaderboard info entry */
typedef struct rc_api_lboard_info_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  uint32_t rank;
  /* The index of the entry */
  uint32_t index;
  /* The value of the entry */
  int32_t score;
  /* When the entry was submitted */
  time_t submitted;
}
rc_api_lboard_info_entry_t;

/**
 * Response data for a fetch leaderboard info request.
 */
typedef struct rc_api_fetch_leaderboard_info_response_t {
  /* The unique identifier of the leaderboard */
  uint32_t id;
  /* The format to pass to rc_format_value to format the leaderboard value */
  int format;
  /* If non-zero, indicates that lower scores appear first */
  uint32_t lower_is_better;
  /* The title of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The definition of the leaderboard to be passed to rc_runtime_activate_lboard */
  const char* definition;
  /* The unique identifier of the game to which the leaderboard is associated */
  uint32_t game_id;
  /* The author of the leaderboard */
  const char* author;
  /* When the leaderboard was first uploaded to the server */
  time_t created;
  /* When the leaderboard was last modified on the server */
  time_t updated;

  /* An array of requested entries */
  rc_api_lboard_info_entry_t* entries;
  /* The number of items in the entries array */
  uint32_t num_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_leaderboard_info_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_leaderboard_info_request(rc_api_request_t* request, const rc_api_fetch_leaderboard_info_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_process_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_leaderboard_info_server_response(rc_api_fetch_leaderboard_info_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response);

/* --- Fetch Games List --- */

/**
 * API parameters for a fetch games list request.
 */
typedef struct rc_api_fetch_games_list_request_t {
  /* The unique identifier of the console to query */
  uint32_t console_id;
}
rc_api_fetch_games_list_request_t;

/* A game list entry */
typedef struct rc_api_game_list_entry_t {
  /* The unique identifier of the game */
  uint32_t id;
  /* The name of the game */
  const char* name;
}
rc_api_game_list_entry_t;

/**
 * Response data for a fetch games list request.
 */
typedef struct rc_api_fetch_games_list_response_t {
  /* An array of requested entries */
  rc_api_game_list_entry_t* entries;
  /* The number of items in the entries array */
  uint32_t num_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_games_list_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_games_list_request(rc_api_request_t* request, const rc_api_fetch_games_list_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_process_fetch_games_list_response(rc_api_fetch_games_list_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_games_list_server_response(rc_api_fetch_games_list_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_games_list_response(rc_api_fetch_games_list_response_t* response);

RC_END_C_DECLS

#endif /* RC_API_INFO_H */
