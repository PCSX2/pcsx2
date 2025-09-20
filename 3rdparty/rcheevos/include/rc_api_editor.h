#ifndef RC_API_EDITOR_H
#define RC_API_EDITOR_H

#include "rc_api_request.h"

#include <stdint.h>

RC_BEGIN_C_DECLS

/* --- Fetch Code Notes --- */

/**
 * API parameters for a fetch code notes request.
 */
typedef struct rc_api_fetch_code_notes_request_t {
  /* The unique identifier of the game */
  uint32_t game_id;
}
rc_api_fetch_code_notes_request_t;

/* A code note definiton */
typedef struct rc_api_code_note_t {
  /* The address the note is associated to */
  uint32_t address;
  /* The name of the use who last updated the note */
  const char* author;
  /* The contents of the note */
  const char* note;
} rc_api_code_note_t;

/**
 * Response data for a fetch code notes request.
 */
typedef struct rc_api_fetch_code_notes_response_t {
  /* An array of code notes for the game */
  rc_api_code_note_t* notes;
  /* The number of items in the notes array */
  uint32_t num_notes;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_code_notes_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_code_notes_request(rc_api_request_t* request, const rc_api_fetch_code_notes_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_code_notes_request_hosted(rc_api_request_t* request, const rc_api_fetch_code_notes_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_fetch_code_notes_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_code_notes_server_response(rc_api_fetch_code_notes_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response);

/* --- Update Code Note --- */

/**
 * API parameters for an update code note request.
 */
typedef struct rc_api_update_code_note_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The address the note is associated to */
  uint32_t address;
  /* The contents of the note (NULL or empty to delete a note) */
  const char* note;
}
rc_api_update_code_note_request_t;

/**
 * Response data for an update code note request.
 */
typedef struct rc_api_update_code_note_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_code_note_response_t;

RC_EXPORT int RC_CCONV rc_api_init_update_code_note_request(rc_api_request_t* request, const rc_api_update_code_note_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_update_code_note_request_hosted(rc_api_request_t* request, const rc_api_update_code_note_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_update_code_note_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_update_code_note_response(rc_api_update_code_note_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_update_code_note_server_response(rc_api_update_code_note_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_update_code_note_response(rc_api_update_code_note_response_t* response);

/* --- Update Achievement --- */

/**
 * API parameters for an update achievement request.
 */
typedef struct rc_api_update_achievement_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement (0 to create a new achievement) */
  uint32_t achievement_id;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The name of the achievement */
  const char* title;
  /* The description of the achievement */
  const char* description;
  /* The badge name for the achievement */
  const char* badge;
  /* The serialized trigger for the achievement */
  const char* trigger;
  /* The number of points the achievement is worth */
  uint32_t points;
  /* The category of the achievement */
  uint32_t category;
  /* The type of the achievement */
  uint32_t type;
}
rc_api_update_achievement_request_t;

/**
 * Response data for an update achievement request.
 */
typedef struct rc_api_update_achievement_response_t {
  /* The unique identifier of the achievement */
  uint32_t achievement_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_achievement_response_t;

RC_EXPORT int RC_CCONV rc_api_init_update_achievement_request(rc_api_request_t* request, const rc_api_update_achievement_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_update_achievement_request_hosted(rc_api_request_t* request, const rc_api_update_achievement_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_update_achievement_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_update_achievement_response(rc_api_update_achievement_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_update_achievement_server_response(rc_api_update_achievement_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_update_achievement_response(rc_api_update_achievement_response_t* response);

/* --- Update Leaderboard --- */

/**
 * API parameters for an update leaderboard request.
 */
typedef struct rc_api_update_leaderboard_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the leaderboard (0 to create a new leaderboard) */
  uint32_t leaderboard_id;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The name of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The start trigger for the leaderboard */
  const char* start_trigger;
  /* The submit trigger for the leaderboard */
  const char* submit_trigger;
  /* The cancel trigger for the leaderboard */
  const char* cancel_trigger;
  /* The value definition for the leaderboard */
  const char* value_definition;
  /* The format of leaderboard values */
  const char* format;
  /* Whether or not lower scores are better for the leaderboard */
  uint32_t lower_is_better;
}
rc_api_update_leaderboard_request_t;

/**
 * Response data for an update leaderboard request.
 */
typedef struct rc_api_update_leaderboard_response_t {
  /* The unique identifier of the leaderboard */
  uint32_t leaderboard_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_leaderboard_response_t;

RC_EXPORT int RC_CCONV rc_api_init_update_leaderboard_request(rc_api_request_t* request, const rc_api_update_leaderboard_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_update_leaderboard_request_hosted(rc_api_request_t* request, const rc_api_update_leaderboard_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_update_leaderboard_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_update_leaderboard_response(rc_api_update_leaderboard_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_update_leaderboard_server_response(rc_api_update_leaderboard_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_update_leaderboard_response(rc_api_update_leaderboard_response_t* response);

/* --- Update Rich Presence --- */

/**
 * API parameters for an update rich presence request.
 */
typedef struct rc_api_update_rich_presence_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* The script for the rich_presence */
  const char* script;
}
rc_api_update_rich_presence_request_t;

/**
 * Response data for an update rich presence request.
 */
typedef struct rc_api_update_rich_presence_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_rich_presence_response_t;

RC_EXPORT int RC_CCONV rc_api_init_update_rich_presence_request(rc_api_request_t* request, const rc_api_update_rich_presence_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_update_rich_presence_request_hosted(rc_api_request_t* request, const rc_api_update_rich_presence_request_t* api_params, const rc_api_host_t* host);
RC_EXPORT int RC_CCONV rc_api_process_update_rich_presence_server_response(rc_api_update_rich_presence_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_update_rich_presence_response(rc_api_update_rich_presence_response_t* response);

/* --- Fetch Badge Range --- */

/**
 * API parameters for a fetch badge range request.
 */
typedef struct rc_api_fetch_badge_range_request_t {
  /* Unused */
  uint32_t unused;
}
rc_api_fetch_badge_range_request_t;

/**
 * Response data for a fetch badge range request.
 */
typedef struct rc_api_fetch_badge_range_response_t {
  /* The numeric identifier of the first valid badge ID */
  uint32_t first_badge_id;
  /* The numeric identifier of the first unassigned badge ID */
  uint32_t next_badge_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_badge_range_response_t;

RC_EXPORT int RC_CCONV rc_api_init_fetch_badge_range_request(rc_api_request_t* request, const rc_api_fetch_badge_range_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_fetch_badge_range_request_hosted(rc_api_request_t* request, const rc_api_fetch_badge_range_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_fetch_badge_range_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_fetch_badge_range_response(rc_api_fetch_badge_range_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_fetch_badge_range_server_response(rc_api_fetch_badge_range_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_fetch_badge_range_response(rc_api_fetch_badge_range_response_t* response);

/* --- Add Game Hash --- */

/**
 * API parameters for an add game hash request.
 */
typedef struct rc_api_add_game_hash_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game (0 to create a new game entry) */
  uint32_t game_id;
  /* The unique identifier of the console for the game */
  uint32_t console_id;
  /* The title of the game */
  const char* title;
  /* The hash being added */
  const char* hash;
  /* A description of the hash being added (usually the normalized ROM name) */
  const char* hash_description;
}
rc_api_add_game_hash_request_t;

/**
 * Response data for an update code note request.
 */
typedef struct rc_api_add_game_hash_response_t {
  /* The unique identifier of the game */
  uint32_t game_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_add_game_hash_response_t;

RC_EXPORT int RC_CCONV rc_api_init_add_game_hash_request(rc_api_request_t* request, const rc_api_add_game_hash_request_t* api_params);
RC_EXPORT int RC_CCONV rc_api_init_add_game_hash_request_hosted(rc_api_request_t* request, const rc_api_add_game_hash_request_t* api_params, const rc_api_host_t* host);
/* [deprecated] use rc_api_process_add_game_hash_server_response instead */
RC_EXPORT int RC_CCONV rc_api_process_add_game_hash_response(rc_api_add_game_hash_response_t* response, const char* server_response);
RC_EXPORT int RC_CCONV rc_api_process_add_game_hash_server_response(rc_api_add_game_hash_response_t* response, const rc_api_server_response_t* server_response);
RC_EXPORT void RC_CCONV rc_api_destroy_add_game_hash_response(rc_api_add_game_hash_response_t* response);

RC_END_C_DECLS

#endif /* RC_EDITOR_H */
