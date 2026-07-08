#ifndef RC_API_USER_H
#define RC_API_USER_H

#include "rc_api_request.h"

#include <stdint.h>
#include <time.h>

RC_BEGIN_C_DECLS

/* --- Login --- */

/**
 * API parameters for a login request.
 * If both password and api_token are provided, api_token will be ignored.
 */
typedef struct rc_api_login_request_t {
  /* The username of the player being logged in */
  const char* username;
  /* The API token from a previous login */
  const char* api_token;
  /* The player's password */
  const char* password;
}
rc_api_login_request_t;

/**
 * Response data for a login request.
 */
typedef struct rc_api_login_response_t {
  /* The case-corrected username of the player */
  const char* username;
  /* The API token to use for all future requests */
  const char* api_token;
  /* The current score of the player */
  uint32_t score;
  /* The current softcore score of the player */
  uint32_t score_softcore;
  /* The number of unread messages waiting for the player on the web site */
  uint32_t num_unread_messages;
  /* The preferred name to display for the player */
  const char* display_name;
  /* A URL to the user's avatar image */
  const char* avatar_url;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_login_response_t;

RC_EXPORT int RC_CCONV rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_login_request_hosted(rc_api_request_t* request, const rc_api_login_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_login_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_login_server_response(rc_api_login_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_login_response(rc_api_login_response_t* response);

/* --- Start Session --- */

/**
 * API parameters for a start session request.
 */
typedef struct rc_api_start_session_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* (recommended) The hash associated to the game being played */
  const char* game_hash;
  /* Non-zero if hardcore is currently enabled (ignored if game_hash is null) */
  uint32_t hardcore;
}
rc_api_start_session_request_t;

/**
 * Response data for an achievement unlock.
 */
typedef struct rc_api_unlock_entry_t {
  /* The unique identifier of the unlocked achievement */
  uint32_t achievement_id;
  /* When the achievement was unlocked */
  time_t when;
}
rc_api_unlock_entry_t;

/**
 * Response data for a start session request.
 */
typedef struct rc_api_start_session_response_t {
  /* An array of hardcore user unlocks */
  rc_api_unlock_entry_t* hardcore_unlocks;
  /* An array of user unlocks */
  rc_api_unlock_entry_t* unlocks;

  /* The number of items in the hardcore_unlocks array */
  uint32_t num_hardcore_unlocks;
  /* The number of items in the unlocks array */
  uint32_t num_unlocks;

  /* The server timestamp when the response was generated */
  time_t server_now;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_start_session_response_t;

RC_EXPORT int RC_CCONV rc_api_init_start_session_request(rc_api_request_t* request, const rc_api_start_session_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_start_session_request_hosted(rc_api_request_t* request, const rc_api_start_session_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_start_session_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_start_session_response(rc_api_start_session_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_start_session_server_response(rc_api_start_session_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_start_session_response(rc_api_start_session_response_t* response);

/* --- Fetch User Unlocks --- */

/**
 * API parameters for a fetch user unlocks request.
 */
typedef struct rc_api_fetch_user_unlocks_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* Non-zero to fetch hardcore unlocks, 0 to fetch non-hardcore unlocks */
  uint32_t hardcore;
}
rc_api_fetch_user_unlocks_request_t;

/**
 * Response data for a fetch user unlocks request.
 */
typedef struct rc_api_fetch_user_unlocks_response_t {
  /* An array of achievement IDs previously unlocked by the user */
  uint32_t* achievement_ids;
  /* The number of items in the achievement_ids array */
  uint32_t num_achievement_ids;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_user_unlocks_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_user_unlocks_request(rc_api_request_t* request, const rc_api_fetch_user_unlocks_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_user_unlocks_request_hosted(rc_api_request_t* request, const rc_api_fetch_user_unlocks_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_fetch_user_unlocks_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_user_unlocks_server_response(rc_api_fetch_user_unlocks_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response);

/* --- Fetch Followed Users --- */

/**
 * API parameters for a fetch followed users request.
 */
typedef struct rc_api_fetch_followed_users_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
}
rc_api_fetch_followed_users_request_t;

typedef struct rc_api_followed_user_activity_t {
  /* The record associated to the activity */
  const char* context;
  /* The image of the record associated to the activity */
  const char* context_image_url;
  /* The description of the activity */
  const char* description;
  /* The time of the activity */
  time_t when;
  /* The unique identifier of the record associated to the activity */
  uint32_t context_id;
}
rc_api_followed_user_activity_t;

/**
 * Response data for a followed user.
 */
typedef struct rc_api_followed_user_t {
  /* The preferred name to display for the player */
  const char* display_name;
  /* A URL to the user's avatar image */
  const char* avatar_url;
  /* The player's last registered activity */
  rc_api_followed_user_activity_t recent_activity;
  /* The current score of the player */
  uint32_t score;
}
rc_api_followed_user_t;

/**
 * Response data for a fetch followed users request.
 */
typedef struct rc_api_fetch_followed_users_response_t {
  /* An array of followed user information */
  rc_api_followed_user_t* users;
  /* The number of items in the users array */
  uint32_t num_users;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_followed_users_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_followed_users_request(rc_api_request_t* request, const rc_api_fetch_followed_users_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_followed_users_request_hosted(rc_api_request_t* request, const rc_api_fetch_followed_users_request_t* api_params, const rc_api_host_t* host);
RC_EXPORT int RC_CCONV rc_api_process_fetch_followed_users_server_response(rc_api_fetch_followed_users_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_followed_users_response(rc_api_fetch_followed_users_response_t* response);

/* --- Fetch All Progress --- */

/**
 * API parameters for a fetch all user progress request.
 */
typedef struct rc_api_fetch_all_user_progress_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the console to query */
  uint32_t console_id;
} rc_api_fetch_all_user_progress_request_t;

/* An all-user-progress entry */
typedef struct rc_api_all_user_progress_entry_t {
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The total number of achievements for this game */
  uint32_t num_achievements;
  /* The total number of unlocked achievements for this game in softcore mode */
  uint32_t num_unlocked_achievements;
  /* The total number of unlocked achievements for this game in hardcore mode */
  uint32_t num_unlocked_achievements_hardcore;
} rc_api_all_user_progress_entry_t;

/**
 * Response data for a fetch all user progress request.
 */
typedef struct rc_api_fetch_all_user_progress_response_t {
  /* An array of entries, one per game */
  rc_api_all_user_progress_entry_t* entries;
  /* The number of items in the entries array */
  uint32_t num_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
} rc_api_fetch_all_user_progress_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_all_user_progress_request(rc_api_request_t* request, const rc_api_fetch_all_user_progress_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_all_user_progress_request_hosted(rc_api_request_t* request, const rc_api_fetch_all_user_progress_request_t* api_params, const rc_api_host_t* host);
RC_EXPORT int RC_CCONV rc_api_process_fetch_all_user_progress_server_response(rc_api_fetch_all_user_progress_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_all_user_progress_response(rc_api_fetch_all_user_progress_response_t* response);

RC_END_C_DECLS

#endif /* RC_API_H */
