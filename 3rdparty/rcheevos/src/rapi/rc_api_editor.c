#include "rc_api_editor.h"
#include "rc_api_common.h"
#include "rc_api_runtime.h"

#include "../rc_compat.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <string.h>

/* --- Fetch Code Notes --- */

int rc_api_init_fetch_code_notes_request(rc_api_request_t* request, const rc_api_fetch_code_notes_request_t* api_params) {
  return rc_api_init_fetch_code_notes_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_code_notes_request_hosted(rc_api_request_t* request,
                                                const rc_api_fetch_code_notes_request_t* api_params,
                                                const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "codenotes2");
  rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_fetch_code_notes_server_response(response, &response_obj);
}

int rc_api_process_fetch_code_notes_server_response(rc_api_fetch_code_notes_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_field_t array_field;
  rc_json_iterator_t iterator;
  rc_api_code_note_t* note;
  const char* address_str;
  const char* last_author = "";
  size_t last_author_len = 0;
  size_t len;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("CodeNotes")
  };

  rc_json_field_t note_fields[] = {
    RC_JSON_NEW_FIELD("Address"),
    RC_JSON_NEW_FIELD("User"),
    RC_JSON_NEW_FIELD("Note")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_array(&response->num_notes, &array_field, &response->response, &fields[2], "CodeNotes"))
    return RC_MISSING_VALUE;

  if (response->num_notes) {
    response->notes = (rc_api_code_note_t*)rc_buffer_alloc(&response->response.buffer, response->num_notes * sizeof(rc_api_code_note_t));
    if (!response->notes)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array_field.value_start;
    iterator.end = array_field.value_end;

    note = response->notes;
    while (rc_json_get_array_entry_object(note_fields, sizeof(note_fields) / sizeof(note_fields[0]), &iterator)) {
      /* an empty note represents a record that was deleted on the server */
      /* a note set to '' also represents a deleted note (remnant of a bug) */
      /* NOTE: len will include the quotes */
      if (note_fields[2].value_start) {
        len = note_fields[2].value_end - note_fields[2].value_start;
        if (len == 2 || (len == 4 && note_fields[2].value_start[1] == '\'' && note_fields[2].value_start[2] == '\'')) {
          --response->num_notes;
          continue;
        }
      }

      if (!rc_json_get_required_string(&address_str, &response->response, &note_fields[0], "Address"))
        return RC_MISSING_VALUE;
      note->address = (uint32_t)strtoul(address_str, NULL, 16);
      if (!rc_json_get_required_string(&note->note, &response->response, &note_fields[2], "Note"))
        return RC_MISSING_VALUE;

      len = note_fields[1].value_end - note_fields[1].value_start;
      if (len == last_author_len && memcmp(note_fields[1].value_start, last_author, len) == 0) {
        note->author = last_author;
      }
      else {
        if (!rc_json_get_required_string(&note->author, &response->response, &note_fields[1], "User"))
          return RC_MISSING_VALUE;

        if (note->author == NULL) {
          /* ensure we don't pass NULL out to client */
          last_author = note->author = "";
          last_author_len = 0;
        } else {
          last_author = note->author;
          last_author_len = len;
        }
      }

      ++note;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Update Code Note --- */

int rc_api_init_update_code_note_request(rc_api_request_t* request, const rc_api_update_code_note_request_t* api_params) {
  return rc_api_init_update_code_note_request_hosted(request, api_params, &g_host);
}

int rc_api_init_update_code_note_request_hosted(rc_api_request_t* request,
                                                const rc_api_update_code_note_request_t* api_params,
                                                const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 128);
  if (!rc_api_url_build_dorequest(&builder, "submitcodenote", api_params->username, api_params->api_token))
    return builder.result;

  rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
  rc_url_builder_append_unum_param(&builder, "m", api_params->address);

  if (api_params->note && *api_params->note)
    rc_url_builder_append_str_param(&builder, "n", api_params->note);

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_update_code_note_response(rc_api_update_code_note_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_update_code_note_server_response(response, &response_obj);
}

int rc_api_process_update_code_note_server_response(rc_api_update_code_note_response_t* response, const rc_api_server_response_t* server_response) {
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error")
    /* unused fields
    RC_JSON_NEW_FIELD("GameID"),
    RC_JSON_NEW_FIELD("Address"),
    RC_JSON_NEW_FIELD("Note")
    */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  return RC_OK;
}

void rc_api_destroy_update_code_note_response(rc_api_update_code_note_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Update Achievement --- */

static const char* rc_type_string(uint32_t type) {
  switch (type) {
    case RC_ACHIEVEMENT_TYPE_MISSABLE: return "missable";
    case RC_ACHIEVEMENT_TYPE_PROGRESSION: return "progression";
    case RC_ACHIEVEMENT_TYPE_WIN: return "win_condition";
    default: return "";
  }
}

int rc_api_init_update_achievement_request(rc_api_request_t* request, const rc_api_update_achievement_request_t* api_params) {
  return rc_api_init_update_achievement_request_hosted(request, api_params, &g_host);
}

int rc_api_init_update_achievement_request_hosted(rc_api_request_t* request,
                                                  const rc_api_update_achievement_request_t* api_params,
                                                  const rc_api_host_t* host) {
  rc_api_url_builder_t builder;
  char buffer[33];
  md5_state_t md5;
  md5_byte_t hash[16];

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0 || api_params->category == 0)
    return RC_INVALID_STATE;
  if (!api_params->title || !*api_params->title)
    return RC_INVALID_STATE;
  if (!api_params->description || !*api_params->description)
    return RC_INVALID_STATE;
  if (!api_params->trigger || !*api_params->trigger)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 128);
  if (!rc_api_url_build_dorequest(&builder, "uploadachievement", api_params->username, api_params->api_token))
    return builder.result;

  if (api_params->achievement_id)
    rc_url_builder_append_unum_param(&builder, "a", api_params->achievement_id);
  rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
  rc_url_builder_append_str_param(&builder, "n", api_params->title);
  rc_url_builder_append_str_param(&builder, "d", api_params->description);
  rc_url_builder_append_str_param(&builder, "m", api_params->trigger);
  rc_url_builder_append_unum_param(&builder, "z", api_params->points);
  rc_url_builder_append_unum_param(&builder, "f", api_params->category);
  if (api_params->badge)
    rc_url_builder_append_str_param(&builder, "b", api_params->badge);
  rc_url_builder_append_str_param(&builder, "x", rc_type_string(api_params->type));

  /* Evaluate the signature. */
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
  md5_append(&md5, (md5_byte_t*)"SECRET", 6);
  snprintf(buffer, sizeof(buffer), "%u", api_params->achievement_id);
  md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
  md5_append(&md5, (md5_byte_t*)"SEC", 3);
  md5_append(&md5, (md5_byte_t*)api_params->trigger, (int)strlen(api_params->trigger));
  snprintf(buffer, sizeof(buffer), "%u", api_params->points);
  md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
  md5_append(&md5, (md5_byte_t*)"RE2", 3);
  snprintf(buffer, sizeof(buffer), "%u", api_params->points * 3);
  md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
  md5_finish(&md5, hash);
  rc_format_md5(buffer, hash);
  rc_url_builder_append_str_param(&builder, "h", buffer);

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_update_achievement_response(rc_api_update_achievement_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_update_achievement_server_response(response, &response_obj);
}

int rc_api_process_update_achievement_server_response(rc_api_update_achievement_response_t* response, const rc_api_server_response_t* server_response) {
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("AchievementID")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_unum(&response->achievement_id, &response->response, &fields[2], "AchievementID"))
    return RC_MISSING_VALUE;

  return RC_OK;
}

void rc_api_destroy_update_achievement_response(rc_api_update_achievement_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Update Leaderboard --- */

int rc_api_init_update_leaderboard_request(rc_api_request_t* request, const rc_api_update_leaderboard_request_t* api_params) {
  return rc_api_init_update_leaderboard_request_hosted(request, api_params, &g_host);
}

int rc_api_init_update_leaderboard_request_hosted(rc_api_request_t* request,
                                                  const rc_api_update_leaderboard_request_t* api_params,
                                                  const rc_api_host_t* host) {
    rc_api_url_builder_t builder;
    char buffer[33];
    md5_state_t md5;
    md5_byte_t hash[16];

    rc_api_url_build_dorequest_url(request, host);

    if (api_params->game_id == 0)
        return RC_INVALID_STATE;
    if (!api_params->title || !*api_params->title)
        return RC_INVALID_STATE;
    if (!api_params->description)
        return RC_INVALID_STATE;
    if (!api_params->start_trigger || !*api_params->start_trigger)
        return RC_INVALID_STATE;
    if (!api_params->submit_trigger || !*api_params->submit_trigger)
        return RC_INVALID_STATE;
    if (!api_params->cancel_trigger || !*api_params->cancel_trigger)
        return RC_INVALID_STATE;
    if (!api_params->value_definition || !*api_params->value_definition)
        return RC_INVALID_STATE;
    if (!api_params->format || !*api_params->format)
        return RC_INVALID_STATE;

    rc_url_builder_init(&builder, &request->buffer, 128);
    if (!rc_api_url_build_dorequest(&builder, "uploadleaderboard", api_params->username, api_params->api_token))
        return builder.result;

    if (api_params->leaderboard_id)
        rc_url_builder_append_unum_param(&builder, "i", api_params->leaderboard_id);
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
    rc_url_builder_append_str_param(&builder, "n", api_params->title);
    rc_url_builder_append_str_param(&builder, "d", api_params->description);
    rc_url_builder_append_str_param(&builder, "s", api_params->start_trigger);
    rc_url_builder_append_str_param(&builder, "b", api_params->submit_trigger);
    rc_url_builder_append_str_param(&builder, "c", api_params->cancel_trigger);
    rc_url_builder_append_str_param(&builder, "l", api_params->value_definition);
    rc_url_builder_append_num_param(&builder, "w", api_params->lower_is_better);
    rc_url_builder_append_str_param(&builder, "f", api_params->format);

    /* Evaluate the signature. */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
    md5_append(&md5, (md5_byte_t*)"SECRET", 6);
    snprintf(buffer, sizeof(buffer), "%u", api_params->leaderboard_id);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_append(&md5, (md5_byte_t*)"SEC", 3);
    md5_append(&md5, (md5_byte_t*)api_params->start_trigger, (int)strlen(api_params->start_trigger));
    md5_append(&md5, (md5_byte_t*)api_params->submit_trigger, (int)strlen(api_params->submit_trigger));
    md5_append(&md5, (md5_byte_t*)api_params->cancel_trigger, (int)strlen(api_params->cancel_trigger));
    md5_append(&md5, (md5_byte_t*)api_params->value_definition, (int)strlen(api_params->value_definition));
    md5_append(&md5, (md5_byte_t*)"RE2", 3);
    md5_append(&md5, (md5_byte_t*)api_params->format, (int)strlen(api_params->format));
    md5_finish(&md5, hash);
    rc_format_md5(buffer, hash);
    rc_url_builder_append_str_param(&builder, "h", buffer);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;

    return builder.result;
}

int rc_api_process_update_leaderboard_response(rc_api_update_leaderboard_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_update_leaderboard_server_response(response, &response_obj);
}

int rc_api_process_update_leaderboard_server_response(rc_api_update_leaderboard_response_t* response, const rc_api_server_response_t* server_response) {
    int result;

    rc_json_field_t fields[] = {
      RC_JSON_NEW_FIELD("Success"),
      RC_JSON_NEW_FIELD("Error"),
      RC_JSON_NEW_FIELD("LeaderboardID")
    };

    memset(response, 0, sizeof(*response));
    rc_buffer_init(&response->response.buffer);

    result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
    if (result != RC_OK || !response->response.succeeded)
        return result;

    if (!rc_json_get_required_unum(&response->leaderboard_id, &response->response, &fields[2], "LeaderboardID"))
        return RC_MISSING_VALUE;

    return RC_OK;
}

void rc_api_destroy_update_leaderboard_response(rc_api_update_leaderboard_response_t* response) {
    rc_buffer_destroy(&response->response.buffer);
}

/* --- Update Rich Presence --- */

int rc_api_init_update_rich_presence_request(rc_api_request_t* request, const rc_api_update_rich_presence_request_t* api_params) {
  return rc_api_init_update_rich_presence_request_hosted(request, api_params, &g_host);
}

int rc_api_init_update_rich_presence_request_hosted(rc_api_request_t* request,
  const rc_api_update_rich_presence_request_t* api_params,
  const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;
  if (!api_params->script)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 128);
  if (!rc_api_url_build_dorequest(&builder, "submitrichpresence", api_params->username, api_params->api_token))
    return builder.result;

  rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
  rc_url_builder_append_str_param(&builder, "d", api_params->script);

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_update_rich_presence_server_response(rc_api_update_rich_presence_response_t* response, const rc_api_server_response_t* server_response) {
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Code"),
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  return RC_OK;
}

void rc_api_destroy_update_rich_presence_response(rc_api_update_rich_presence_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch Badge Range --- */

int rc_api_init_fetch_badge_range_request(rc_api_request_t* request, const rc_api_fetch_badge_range_request_t* api_params) {
  return rc_api_init_fetch_badge_range_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_badge_range_request_hosted(rc_api_request_t* request,
                                                 const rc_api_fetch_badge_range_request_t* api_params,
                                                 const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "badgeiter");

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  (void)api_params;

  return builder.result;
}

int rc_api_process_fetch_badge_range_response(rc_api_fetch_badge_range_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_fetch_badge_range_server_response(response, &response_obj);
}

int rc_api_process_fetch_badge_range_server_response(rc_api_fetch_badge_range_response_t* response, const rc_api_server_response_t* server_response) {
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("FirstBadge"),
    RC_JSON_NEW_FIELD("NextBadge")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_unum(&response->first_badge_id, &response->response, &fields[2], "FirstBadge"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->next_badge_id, &response->response, &fields[3], "NextBadge"))
    return RC_MISSING_VALUE;

  return RC_OK;
}

void rc_api_destroy_fetch_badge_range_response(rc_api_fetch_badge_range_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Add Game Hash --- */

int rc_api_init_add_game_hash_request(rc_api_request_t* request, const rc_api_add_game_hash_request_t* api_params) {
  return rc_api_init_add_game_hash_request_hosted(request, api_params, &g_host);
}

int rc_api_init_add_game_hash_request_hosted(rc_api_request_t* request,
                                             const rc_api_add_game_hash_request_t* api_params,
                                             const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->console_id == 0)
    return RC_INVALID_STATE;
  if (!api_params->hash || !*api_params->hash)
    return RC_INVALID_STATE;
  if (api_params->game_id == 0 && (!api_params->title || !*api_params->title))
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 128);
  if (!rc_api_url_build_dorequest(&builder, "submitgametitle", api_params->username, api_params->api_token))
    return builder.result;

  rc_url_builder_append_unum_param(&builder, "c", api_params->console_id);
  rc_url_builder_append_str_param(&builder, "m", api_params->hash);
  if (api_params->title)
    rc_url_builder_append_str_param(&builder, "i", api_params->title);
  if (api_params->game_id)
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
  if (api_params->hash_description && *api_params->hash_description)
    rc_url_builder_append_str_param(&builder, "d", api_params->hash_description);

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_add_game_hash_response(rc_api_add_game_hash_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_add_game_hash_server_response(response, &response_obj);
}

int rc_api_process_add_game_hash_server_response(rc_api_add_game_hash_response_t* response, const rc_api_server_response_t* server_response) {
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Response")
  };

  rc_json_field_t response_fields[] = {
    RC_JSON_NEW_FIELD("GameID")
    /* unused fields
    RC_JSON_NEW_FIELD("MD5"),
    RC_JSON_NEW_FIELD("ConsoleID"),
    RC_JSON_NEW_FIELD("GameTitle"),
    RC_JSON_NEW_FIELD("Success")
    */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_object(response_fields, sizeof(response_fields) / sizeof(response_fields[0]), &response->response, &fields[2], "Response"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_unum(&response->game_id, &response->response, &response_fields[0], "GameID"))
    return RC_MISSING_VALUE;

  return RC_OK;
}

void rc_api_destroy_add_game_hash_response(rc_api_add_game_hash_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}
