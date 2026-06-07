#include "rc_api_user.h"
#include "rc_api_common.h"
#include "rc_api_runtime.h"
#include "rc_consoles.h"

#include "../rc_version.h"

#include <stdlib.h>
#include <string.h>

/* --- Login --- */

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* api_params) {
  return rc_api_init_login_request_hosted(request, api_params, &g_host);
}

int rc_api_init_login_request_hosted(rc_api_request_t* request,
                                     const rc_api_login_request_t* api_params,
                                     const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (!api_params->username || !*api_params->username)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "login2");
  rc_url_builder_append_str_param(&builder, "u", api_params->username);

  if (api_params->password && api_params->password[0])
    rc_url_builder_append_str_param(&builder, "p", api_params->password);
  else if (api_params->api_token && api_params->api_token[0])
    rc_url_builder_append_str_param(&builder, "t", api_params->api_token);
  else
    return RC_INVALID_STATE;

  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_login_server_response(response, &response_obj);
}

int rc_api_process_login_server_response(rc_api_login_response_t* response, const rc_api_server_response_t* server_response) {
  int result;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Code"),
    RC_JSON_NEW_FIELD("User"),
    RC_JSON_NEW_FIELD("Token"),
    RC_JSON_NEW_FIELD("Score"),
    RC_JSON_NEW_FIELD("SoftcoreScore"),
    RC_JSON_NEW_FIELD("Messages"),
    RC_JSON_NEW_FIELD("AvatarUrl")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_string(&response->username, &response->response, &fields[3], "User"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->api_token, &response->response, &fields[4], "Token"))
    return RC_MISSING_VALUE;

  rc_json_get_optional_unum(&response->score, &fields[5], "Score", 0);
  rc_json_get_optional_unum(&response->score_softcore, &fields[6], "SoftcoreScore", 0);
  rc_json_get_optional_unum(&response->num_unread_messages, &fields[7], "Messages", 0);

  /* For the highest level of backwards compatibility, we have decided to just send the
   * display_name back to the client as the "case-corrected username", and allow it to
   * be used as the Username field for the other APIs. */
  response->display_name = response->username;

  rc_json_get_optional_string(&response->avatar_url, &response->response, &fields[8], "AvatarUrl", NULL);
  if (!response->avatar_url)
    response->avatar_url = rc_api_build_avatar_url(&response->response.buffer, RC_IMAGE_TYPE_USER, response->username);

  return RC_OK;
}

void rc_api_destroy_login_response(rc_api_login_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Start Session --- */

int rc_api_init_start_session_request(rc_api_request_t* request, const rc_api_start_session_request_t* api_params) {
  return rc_api_init_start_session_request_hosted(request, api_params, &g_host);
}

int rc_api_init_start_session_request_hosted(rc_api_request_t* request,
                                             const rc_api_start_session_request_t* api_params,
                                             const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "startsession", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);

    if (api_params->game_hash && *api_params->game_hash) {
      rc_url_builder_append_unum_param(&builder, "h", api_params->hardcore);
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
    }

    rc_url_builder_append_str_param(&builder, "l", RCHEEVOS_VERSION_STRING);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_start_session_response(rc_api_start_session_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_start_session_server_response(response, &response_obj);
}

int rc_api_process_start_session_server_response(rc_api_start_session_response_t* response, const rc_api_server_response_t* server_response) {
  rc_api_unlock_entry_t* unlock;
  rc_json_field_t array_field;
  rc_json_iterator_t iterator;
  uint32_t timet;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Unlocks"),
    RC_JSON_NEW_FIELD("HardcoreUnlocks"),
    RC_JSON_NEW_FIELD("ServerNow")
  };

  rc_json_field_t unlock_entry_fields[] = {
    RC_JSON_NEW_FIELD("ID"),
    RC_JSON_NEW_FIELD("When")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (rc_json_get_optional_array(&response->num_unlocks, &array_field, &fields[2], "Unlocks") && response->num_unlocks) {
    response->unlocks = (rc_api_unlock_entry_t*)rc_buffer_alloc(&response->response.buffer, response->num_unlocks * sizeof(rc_api_unlock_entry_t));
    if (!response->unlocks)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array_field.value_start;
    iterator.end = array_field.value_end;

    unlock = response->unlocks;
    while (rc_json_get_array_entry_object(unlock_entry_fields, sizeof(unlock_entry_fields) / sizeof(unlock_entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_unum(&unlock->achievement_id, &response->response, &unlock_entry_fields[0], "ID"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_unum(&timet, &response->response, &unlock_entry_fields[1], "When"))
        return RC_MISSING_VALUE;
      unlock->when = (time_t)timet;

      ++unlock;
    }
  }

  if (rc_json_get_optional_array(&response->num_hardcore_unlocks, &array_field, &fields[3], "HardcoreUnlocks") && response->num_hardcore_unlocks) {
    response->hardcore_unlocks = (rc_api_unlock_entry_t*)rc_buffer_alloc(&response->response.buffer, response->num_hardcore_unlocks * sizeof(rc_api_unlock_entry_t));
    if (!response->hardcore_unlocks)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array_field.value_start;
    iterator.end = array_field.value_end;

    unlock = response->hardcore_unlocks;
    while (rc_json_get_array_entry_object(unlock_entry_fields, sizeof(unlock_entry_fields) / sizeof(unlock_entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_unum(&unlock->achievement_id, &response->response, &unlock_entry_fields[0], "ID"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_unum(&timet, &response->response, &unlock_entry_fields[1], "When"))
        return RC_MISSING_VALUE;
      unlock->when = (time_t)timet;

      ++unlock;
    }
  }

  rc_json_get_optional_unum(&timet, &fields[4], "ServerNow", 0);
  response->server_now = (time_t)timet;

  return RC_OK;
}

void rc_api_destroy_start_session_response(rc_api_start_session_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch User Unlocks --- */

int rc_api_init_fetch_user_unlocks_request(rc_api_request_t* request, const rc_api_fetch_user_unlocks_request_t* api_params) {
  return rc_api_init_fetch_user_unlocks_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_user_unlocks_request_hosted(rc_api_request_t* request,
                                                  const rc_api_fetch_user_unlocks_request_t* api_params,
                                                  const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "unlocks", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
    rc_url_builder_append_unum_param(&builder, "h", api_params->hardcore ? 1 : 0);
    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_fetch_user_unlocks_server_response(response, &response_obj);
}

int rc_api_process_fetch_user_unlocks_server_response(rc_api_fetch_user_unlocks_response_t* response, const rc_api_server_response_t* server_response) {
  int result;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("UserUnlocks")
    /* unused fields
    RC_JSON_NEW_FIELD("GameID"),
    RC_JSON_NEW_FIELD("HardcoreMode")
     * unused fields */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  result = rc_json_get_required_unum_array(&response->achievement_ids, &response->num_achievement_ids, &response->response, &fields[2], "UserUnlocks");
  return result;
}

void rc_api_destroy_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch Followed Users --- */

int rc_api_init_fetch_followed_users_request(rc_api_request_t* request, const rc_api_fetch_followed_users_request_t* api_params) {
  return rc_api_init_fetch_followed_users_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_followed_users_request_hosted(rc_api_request_t* request,
                                                    const rc_api_fetch_followed_users_request_t* api_params,
                                                    const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "getfriendlist", api_params->username, api_params->api_token)) {
    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_fetch_followed_users_server_response(rc_api_fetch_followed_users_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_field_t array_field;
  rc_json_iterator_t iterator;
  rc_api_followed_user_t* user;
  uint32_t timet;
  int result;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Friends")
  };

  rc_json_field_t followed_user_entry_fields[] = {
    RC_JSON_NEW_FIELD("Friend"),
    RC_JSON_NEW_FIELD("AvatarUrl"),
    RC_JSON_NEW_FIELD("RAPoints"),
    RC_JSON_NEW_FIELD("LastSeen"),
    RC_JSON_NEW_FIELD("LastSeenTime"),
    RC_JSON_NEW_FIELD("LastGameId"),
    RC_JSON_NEW_FIELD("LastGameTitle"),
    RC_JSON_NEW_FIELD("LastGameIconUrl"),
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_array(&response->num_users, &array_field, &response->response, &fields[2], "Friends"))
    return RC_MISSING_VALUE;

  if (response->num_users) {
    response->users = (rc_api_followed_user_t*)rc_buffer_alloc(&response->response.buffer, response->num_users * sizeof(rc_api_followed_user_t));
    if (!response->users)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array_field.value_start;
    iterator.end = array_field.value_end;

    user = response->users;
    while (rc_json_get_array_entry_object(followed_user_entry_fields, sizeof(followed_user_entry_fields) / sizeof(followed_user_entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_string(&user->display_name, &response->response, &followed_user_entry_fields[0], "Friend"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&user->avatar_url, &response->response, &followed_user_entry_fields[1], "AvatarUrl"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_unum(&user->score, &response->response, &followed_user_entry_fields[2], "RAPoints"))
        return RC_MISSING_VALUE;

      rc_json_get_optional_string(&user->recent_activity.description, &response->response, &followed_user_entry_fields[3], "LastSeen", NULL);

      rc_json_get_optional_unum(&timet, &followed_user_entry_fields[4], "LastSeenTime", 0);
      user->recent_activity.when = (time_t)timet;

      rc_json_get_optional_unum(&user->recent_activity.context_id, &followed_user_entry_fields[5], "LastGameId", 0);
      rc_json_get_optional_string(&user->recent_activity.context, &response->response, &followed_user_entry_fields[6], "LastGameTitle", NULL);
      rc_json_get_optional_string(&user->recent_activity.context_image_url, &response->response, &followed_user_entry_fields[7], "LastGameIconUrl", NULL);

      ++user;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_followed_users_response(rc_api_fetch_followed_users_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch All Progress --- */

int rc_api_init_fetch_all_user_progress_request(rc_api_request_t* request,
                                                const rc_api_fetch_all_user_progress_request_t* api_params)
{
  return rc_api_init_fetch_all_user_progress_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_all_user_progress_request_hosted(rc_api_request_t* request,
                                                       const rc_api_fetch_all_user_progress_request_t* api_params,
                                                       const rc_api_host_t* host)
{
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->console_id == RC_CONSOLE_UNKNOWN)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "allprogress", api_params->username, api_params->api_token))
  {
    rc_url_builder_append_unum_param(&builder, "c", api_params->console_id);
    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_fetch_all_user_progress_server_response(rc_api_fetch_all_user_progress_response_t* response,
                                                           const rc_api_server_response_t* server_response)
{
  rc_api_all_user_progress_entry_t* entry;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Response"),
  };

  rc_json_field_t entry_fields[] = {
    RC_JSON_NEW_FIELD("Achievements"),
    RC_JSON_NEW_FIELD("Unlocked"),
    RC_JSON_NEW_FIELD("UnlockedHardcore"),
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result =
    rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!fields[2].value_start) {
    /* call rc_json_get_required_object to generate the error message */
    rc_json_get_required_object(NULL, 0, &response->response, &fields[2], "Response");
    return RC_MISSING_VALUE;
  }

  response->num_entries = fields[2].array_size;

  if (response->num_entries > 0) {
    rc_json_iterator_t iterator;
    rc_json_field_t field;
    char* end;

    rc_buffer_reserve(&response->response.buffer, response->num_entries * sizeof(rc_api_all_user_progress_entry_t));

    response->entries = (rc_api_all_user_progress_entry_t*)rc_buffer_alloc(
      &response->response.buffer, response->num_entries * sizeof(rc_api_all_user_progress_entry_t));
    if (!response->entries)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = fields[2].value_start;
    iterator.end = fields[2].value_end;

    entry = response->entries;
    while (rc_json_get_next_object_field(&iterator, &field))
    {
      entry->game_id = strtol(field.name, &end, 10);

      field.name = "";
      if (!rc_json_get_required_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]),
                                       &response->response, &field, ""))
      {
        return RC_MISSING_VALUE;
      }

      rc_json_get_optional_unum(&entry->num_achievements, &entry_fields[0], "Achievements", 0);
      rc_json_get_optional_unum(&entry->num_unlocked_achievements, &entry_fields[1], "Unlocked", 0);
      rc_json_get_optional_unum(&entry->num_unlocked_achievements_hardcore, &entry_fields[2], "UnlockedHardcore", 0);

      ++entry;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_all_user_progress_response(rc_api_fetch_all_user_progress_response_t* response)
{
  rc_buffer_destroy(&response->response.buffer);
}
