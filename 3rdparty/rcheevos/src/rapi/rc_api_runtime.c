#include "rc_api_runtime.h"
#include "rc_api_common.h"

#include "rc_runtime.h"
#include "rc_runtime_types.h"
#include "../rc_compat.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* --- Resolve Hash --- */

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params) {
  return rc_api_init_resolve_hash_request_hosted(request, api_params, &g_host);
}

int rc_api_init_resolve_hash_request_hosted(rc_api_request_t* request,
                                            const rc_api_resolve_hash_request_t* api_params,
                                            const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (!api_params->game_hash || !*api_params->game_hash)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "gameid");
  rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
  request->post_data = rc_url_builder_finalize(&builder);
  request->content_type = RC_CONTENT_TYPE_URLENCODED;

  return builder.result;
}

int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_resolve_hash_server_response(response, &response_obj);
}

int rc_api_process_resolve_hash_server_response(rc_api_resolve_hash_response_t* response, const rc_api_server_response_t* server_response) {
  int result;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("GameID")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  rc_json_get_required_unum(&response->game_id, &response->response, &fields[2], "GameID");
  return RC_OK;
}

void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch Game Data --- */

int rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params) {
  return rc_api_init_fetch_game_data_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_game_data_request_hosted(rc_api_request_t* request,
                                               const rc_api_fetch_game_data_request_t* api_params,
                                               const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0 && (!api_params->game_hash || !api_params->game_hash[0]))
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "patch", api_params->username, api_params->api_token)) {
    if (api_params->game_id)
      rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
    else
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_fetch_game_data_server_response(response, &response_obj);
}

static int rc_api_process_fetch_game_data_achievements(rc_api_response_t* response, rc_api_achievement_definition_t* achievement, rc_json_field_t* array_field) {
  rc_json_iterator_t iterator;
  const char* last_author = "";
  const char* last_author_field = "";
  size_t last_author_len = 0;
  uint32_t timet;
  size_t len;

  rc_json_field_t achievement_fields[] = {
    RC_JSON_NEW_FIELD("ID"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("Description"),
    RC_JSON_NEW_FIELD("Flags"),
    RC_JSON_NEW_FIELD("Points"),
    RC_JSON_NEW_FIELD("MemAddr"),
    RC_JSON_NEW_FIELD("Author"),
    RC_JSON_NEW_FIELD("BadgeName"),
    RC_JSON_NEW_FIELD("Created"),
    RC_JSON_NEW_FIELD("Modified"),
    RC_JSON_NEW_FIELD("Type"),
    RC_JSON_NEW_FIELD("Rarity"),
    RC_JSON_NEW_FIELD("RarityHardcore"),
    RC_JSON_NEW_FIELD("BadgeURL"),
    RC_JSON_NEW_FIELD("BadgeLockedURL")
  };

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = array_field->value_start;
  iterator.end = array_field->value_end;

  while (rc_json_get_array_entry_object(achievement_fields, sizeof(achievement_fields) / sizeof(achievement_fields[0]), &iterator)) {
    if (!rc_json_get_required_unum(&achievement->id, response, &achievement_fields[0], "ID"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&achievement->title, response, &achievement_fields[1], "Title"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&achievement->description, response, &achievement_fields[2], "Description"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_unum(&achievement->category, response, &achievement_fields[3], "Flags"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_unum(&achievement->points, response, &achievement_fields[4], "Points"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&achievement->definition, response, &achievement_fields[5], "MemAddr"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&achievement->badge_name, response, &achievement_fields[7], "BadgeName"))
      return RC_MISSING_VALUE;

    rc_json_get_optional_string(&achievement->badge_url, response, &achievement_fields[13], "BadgeURL", "");
    if (!achievement->badge_url[0])
      achievement->badge_url = rc_api_build_avatar_url(&response->buffer, RC_IMAGE_TYPE_ACHIEVEMENT, achievement->badge_name);

    rc_json_get_optional_string(&achievement->badge_locked_url, response, &achievement_fields[14], "BadgeLockedURL", "");
    if (!achievement->badge_locked_url[0])
      achievement->badge_locked_url = rc_api_build_avatar_url(&response->buffer, RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED, achievement->badge_name);

    len = achievement_fields[6].value_end - achievement_fields[6].value_start;
    if (len == last_author_len && memcmp(achievement_fields[6].value_start, last_author_field, len) == 0) {
      achievement->author = last_author;
    }
    else {
      if (!rc_json_get_required_string(&achievement->author, response, &achievement_fields[6], "Author"))
        return RC_MISSING_VALUE;

      if (achievement->author == NULL) {
        /* ensure we don't pass NULL out to client */
        last_author = achievement->author = "";
        last_author_len = 0;
      } else {
        last_author = achievement->author;
        last_author_field = achievement_fields[6].value_start;
        last_author_len = len;
      }
    }

    if (!rc_json_get_required_unum(&timet, response, &achievement_fields[8], "Created"))
      return RC_MISSING_VALUE;
    achievement->created = (time_t)timet;
    if (!rc_json_get_required_unum(&timet, response, &achievement_fields[9], "Modified"))
      return RC_MISSING_VALUE;
    achievement->updated = (time_t)timet;

    if (rc_json_field_string_matches(&achievement_fields[10], ""))
      achievement->type = RC_ACHIEVEMENT_TYPE_STANDARD;
    else if (rc_json_field_string_matches(&achievement_fields[10], "progression"))
      achievement->type = RC_ACHIEVEMENT_TYPE_PROGRESSION;
    else if (rc_json_field_string_matches(&achievement_fields[10], "missable"))
      achievement->type = RC_ACHIEVEMENT_TYPE_MISSABLE;
    else if (rc_json_field_string_matches(&achievement_fields[10], "win_condition"))
      achievement->type = RC_ACHIEVEMENT_TYPE_WIN;
    else
      achievement->type = RC_ACHIEVEMENT_TYPE_STANDARD;

    /* legacy support : if title contains[m], change type to missable and remove[m] from title */
    if (memcmp(achievement->title, "[m]", 3) == 0) {
      len = 3;
      while (achievement->title[len] == ' ')
        ++len;
      achievement->title += len;
      achievement->type = RC_ACHIEVEMENT_TYPE_MISSABLE;
    }
    else if (achievement_fields[1].value_end && memcmp(achievement_fields[1].value_end - 4, "[m]", 3) == 0) {
      len = strlen(achievement->title) - 3;
      while (achievement->title[len - 1] == ' ')
        --len;
      ((char*)achievement->title)[len] = '\0';
      achievement->type = RC_ACHIEVEMENT_TYPE_MISSABLE;
    }

    rc_json_get_optional_float(&achievement->rarity, &achievement_fields[11], "Rarity", 100.0);
    rc_json_get_optional_float(&achievement->rarity_hardcore, &achievement_fields[12], "RarityHardcore", 100.0);

    ++achievement;
  }

  return RC_OK;
}

static int rc_api_process_fetch_game_data_leaderboards(rc_api_response_t* response, rc_api_leaderboard_definition_t* leaderboard, rc_json_field_t* array_field) {
  rc_json_iterator_t iterator;
  size_t len;
  int result;
  char format[16];

  rc_json_field_t leaderboard_fields[] = {
    RC_JSON_NEW_FIELD("ID"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("Description"),
    RC_JSON_NEW_FIELD("Mem"),
    RC_JSON_NEW_FIELD("Format"),
    RC_JSON_NEW_FIELD("LowerIsBetter"),
    RC_JSON_NEW_FIELD("Hidden")
  };

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = array_field->value_start;
  iterator.end = array_field->value_end;

  while (rc_json_get_array_entry_object(leaderboard_fields, sizeof(leaderboard_fields) / sizeof(leaderboard_fields[0]), &iterator)) {
    if (!rc_json_get_required_unum(&leaderboard->id, response, &leaderboard_fields[0], "ID"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&leaderboard->title, response, &leaderboard_fields[1], "Title"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&leaderboard->description, response, &leaderboard_fields[2], "Description"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&leaderboard->definition, response, &leaderboard_fields[3], "Mem"))
      return RC_MISSING_VALUE;
    rc_json_get_optional_bool(&result, &leaderboard_fields[5], "LowerIsBetter", 0);
    leaderboard->lower_is_better = (uint8_t)result;
    rc_json_get_optional_bool(&result, &leaderboard_fields[6], "Hidden", 0);
    leaderboard->hidden = (uint8_t)result;

    if (!leaderboard_fields[4].value_end)
      return RC_MISSING_VALUE;
    len = leaderboard_fields[4].value_end - leaderboard_fields[4].value_start - 2;
    if (len < sizeof(format) - 1) {
      memcpy(format, leaderboard_fields[4].value_start + 1, len);
      format[len] = '\0';
      leaderboard->format = rc_parse_format(format);
    }
    else {
      leaderboard->format = RC_FORMAT_VALUE;
    }

    ++leaderboard;
  }

  return RC_OK;
}

int rc_api_process_fetch_game_data_server_response(rc_api_fetch_game_data_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_field_t array_field;
  size_t len;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Code"),
    RC_JSON_NEW_FIELD("PatchData") /* nested object */
  };

  rc_json_field_t patchdata_fields[] = {
    RC_JSON_NEW_FIELD("ID"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("ConsoleID"),
    RC_JSON_NEW_FIELD("ImageIcon"),
    RC_JSON_NEW_FIELD("ImageIconURL"),
    RC_JSON_NEW_FIELD("RichPresencePatch"),
    RC_JSON_NEW_FIELD("Achievements"), /* array */
    RC_JSON_NEW_FIELD("Leaderboards"), /* array */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_object(patchdata_fields, sizeof(patchdata_fields) / sizeof(patchdata_fields[0]), &response->response, &fields[3], "PatchData"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_unum(&response->id, &response->response, &patchdata_fields[0], "ID"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->title, &response->response, &patchdata_fields[1], "Title"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->console_id, &response->response, &patchdata_fields[2], "ConsoleID"))
    return RC_MISSING_VALUE;

  /* ImageIcon will be '/Images/0123456.png' - only return the '0123456' */
  rc_json_extract_filename(&patchdata_fields[3]);
  rc_json_get_optional_string(&response->image_name, &response->response, &patchdata_fields[3], "ImageIcon", "");
  rc_json_get_optional_string(&response->image_url, &response->response, &patchdata_fields[4], "ImageIconURL", "");
  if (!response->image_url[0])
    response->image_url = rc_api_build_avatar_url(&response->response.buffer, RC_IMAGE_TYPE_GAME, response->image_name);

  /* estimate the amount of space necessary to store the rich presence script, achievements, and leaderboards.
     determine how much space each takes as a string in the JSON, then subtract out the non-data (field names, punctuation)
     and add space for the structures. */
  len = patchdata_fields[5].value_end - patchdata_fields[5].value_start; /* rich presence */

  len += (patchdata_fields[6].value_end - patchdata_fields[6].value_start) - /* achievements */
          patchdata_fields[6].array_size * (80 - sizeof(rc_api_achievement_definition_t));

  len += (patchdata_fields[7].value_end - patchdata_fields[7].value_start) - /* leaderboards */
          patchdata_fields[7].array_size * (60 - sizeof(rc_api_leaderboard_definition_t));

  rc_buffer_reserve(&response->response.buffer, len);
  /* end estimation */

  rc_json_get_optional_string(&response->rich_presence_script, &response->response, &patchdata_fields[5], "RichPresencePatch", "");
  if (!response->rich_presence_script)
    response->rich_presence_script = "";

  if (!rc_json_get_required_array(&response->num_achievements, &array_field, &response->response, &patchdata_fields[6], "Achievements"))
    return RC_MISSING_VALUE;

  if (response->num_achievements) {
    response->achievements = (rc_api_achievement_definition_t*)rc_buffer_alloc(&response->response.buffer, response->num_achievements * sizeof(rc_api_achievement_definition_t));
    if (!response->achievements)
      return RC_OUT_OF_MEMORY;

    result = rc_api_process_fetch_game_data_achievements(&response->response, response->achievements, &array_field);
    if (result != RC_OK)
      return result;
  }

  if (!rc_json_get_required_array(&response->num_leaderboards, &array_field, &response->response, &patchdata_fields[7], "Leaderboards"))
    return RC_MISSING_VALUE;

  if (response->num_leaderboards) {
    response->leaderboards = (rc_api_leaderboard_definition_t*)rc_buffer_alloc(&response->response.buffer, response->num_leaderboards * sizeof(rc_api_leaderboard_definition_t));
    if (!response->leaderboards)
      return RC_OUT_OF_MEMORY;

    result = rc_api_process_fetch_game_data_leaderboards(&response->response, response->leaderboards, &array_field);
    if (result != RC_OK)
      return result;
  }

  return RC_OK;
}

void rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Fetch Game Sets --- */

int rc_api_init_fetch_game_sets_request(rc_api_request_t* request, const rc_api_fetch_game_sets_request_t* api_params) {
  return rc_api_init_fetch_game_sets_request_hosted(request, api_params, &g_host);
}

int rc_api_init_fetch_game_sets_request_hosted(rc_api_request_t* request,
                                               const rc_api_fetch_game_sets_request_t* api_params,
                                               const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (!api_params->game_id && (!api_params->game_hash || !api_params->game_hash[0]))
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "achievementsets", api_params->username, api_params->api_token)) {
    if (api_params->game_id)
      rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);
    else
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

static int rc_api_process_fetch_game_sets_achievement_sets(rc_api_fetch_game_sets_response_t* response,
                                                           rc_api_achievement_set_definition_t* subset,
                                                           rc_json_field_t* subset_array_field) {
  rc_json_iterator_t iterator;
  rc_json_field_t array_field;
  size_t len;
  int result;

  rc_json_field_t subset_fields[] = {
    RC_JSON_NEW_FIELD("AchievementSetId"),
    RC_JSON_NEW_FIELD("GameId"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("Type"),
    RC_JSON_NEW_FIELD("ImageIconUrl"),
    RC_JSON_NEW_FIELD("Achievements"), /* array */
    RC_JSON_NEW_FIELD("Leaderboards") /* array */
  };

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = subset_array_field->value_start;
  iterator.end = subset_array_field->value_end;

  while (rc_json_get_array_entry_object(subset_fields, sizeof(subset_fields) / sizeof(subset_fields[0]), &iterator)) {
    if (!rc_json_get_required_unum(&subset->id, &response->response, &subset_fields[0], "AchievementSetId"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_unum(&subset->game_id, &response->response, &subset_fields[1], "GameId"))
      return RC_MISSING_VALUE;

    if (!rc_json_get_required_string(&subset->title, &response->response, &subset_fields[2], "Title"))
      return RC_MISSING_VALUE;
    if (!subset->title || !subset->title[0])
      subset->title = response->title;

    if (rc_json_field_string_matches(&subset_fields[3], "core"))
      subset->type = RC_ACHIEVEMENT_SET_TYPE_CORE;
    else if (rc_json_field_string_matches(&subset_fields[3], "bonus"))
      subset->type = RC_ACHIEVEMENT_SET_TYPE_BONUS;
    else if (rc_json_field_string_matches(&subset_fields[3], "specialty"))
      subset->type = RC_ACHIEVEMENT_SET_TYPE_SPECIALTY;
    else if (rc_json_field_string_matches(&subset_fields[3], "exclusive"))
      subset->type = RC_ACHIEVEMENT_SET_TYPE_EXCLUSIVE;
    else
      subset->type = RC_ACHIEVEMENT_SET_TYPE_BONUS;

    if (rc_json_field_string_matches(&subset_fields[4], response->image_url)) {
      subset->image_url = response->image_url;
      subset->image_name = response->image_name;
    }
    else {
      if (!rc_json_get_required_string(&subset->image_url, &response->response, &subset_fields[4], "ImageIconUrl"))
        return RC_MISSING_VALUE;
      rc_json_extract_filename(&subset_fields[4]);
      rc_json_get_optional_string(&subset->image_name, &response->response, &subset_fields[4], "ImageIconUrl", "");
    }

    /* estimate the amount of space necessary to store the achievements, and leaderboards.
       determine how much space each takes as a string in the JSON, then subtract out the non-data (field names, punctuation)
       and add space for the structures. */
    len = (subset_fields[5].value_end - subset_fields[5].value_start) - /* achievements */
      subset_fields[5].array_size * (80 - sizeof(rc_api_achievement_definition_t));
    len += (subset_fields[6].value_end - subset_fields[6].value_start) - /* leaderboards */
      subset_fields[6].array_size * (60 - sizeof(rc_api_leaderboard_definition_t));

    rc_buffer_reserve(&response->response.buffer, len);
    /* end estimation */

    if (!rc_json_get_required_array(&subset->num_achievements, &array_field, &response->response, &subset_fields[5], "Achievements"))
      return RC_MISSING_VALUE;

    if (subset->num_achievements) {
      subset->achievements = (rc_api_achievement_definition_t*)rc_buffer_alloc(&response->response.buffer, subset->num_achievements * sizeof(rc_api_achievement_definition_t));
      if (!subset->achievements)
        return RC_OUT_OF_MEMORY;

      result = rc_api_process_fetch_game_data_achievements(&response->response, subset->achievements, &array_field);
      if (result != RC_OK)
        return result;
    }

    if (!rc_json_get_required_array(&subset->num_leaderboards, &array_field, &response->response, &subset_fields[6], "Leaderboards"))
      return RC_MISSING_VALUE;

    if (subset->num_leaderboards) {
      subset->leaderboards = (rc_api_leaderboard_definition_t*)rc_buffer_alloc(&response->response.buffer, subset->num_leaderboards * sizeof(rc_api_leaderboard_definition_t));
      if (!subset->leaderboards)
        return RC_OUT_OF_MEMORY;

      result = rc_api_process_fetch_game_data_leaderboards(&response->response, subset->leaderboards, &array_field);
      if (result != RC_OK)
        return result;
    }

    ++subset;
  }

  return RC_OK;
}

int rc_api_process_fetch_game_sets_server_response(rc_api_fetch_game_sets_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_field_t array_field;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Code"),
    RC_JSON_NEW_FIELD("GameId"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("ConsoleId"),
    RC_JSON_NEW_FIELD("ImageIconUrl"),
    RC_JSON_NEW_FIELD("RichPresenceGameId"),
    RC_JSON_NEW_FIELD("RichPresencePatch"),
    RC_JSON_NEW_FIELD("Sets") /* array */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_unum(&response->id, &response->response, &fields[3], "GameId"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->title, &response->response, &fields[4], "Title"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->console_id, &response->response, &fields[5], "ConsoleId"))
    return RC_MISSING_VALUE;

  rc_json_get_required_string(&response->image_url, &response->response, &fields[6], "ImageIconUrl");
  rc_json_extract_filename(&fields[6]);
  rc_json_get_required_string(&response->image_name, &response->response, &fields[6], "ImageIconUrl");

  rc_json_get_optional_unum(&response->session_game_id, &fields[7], "RichPresenceGameId", response->id);

  rc_json_get_optional_string(&response->rich_presence_script, &response->response, &fields[8], "RichPresencePatch", "");
  if (!response->rich_presence_script)
    response->rich_presence_script = "";

  rc_json_get_optional_array(&response->num_sets, &array_field, &fields[9], "Sets");
  if (response->num_sets) {
    response->sets = (rc_api_achievement_set_definition_t*)rc_buffer_alloc(&response->response.buffer, response->num_sets * sizeof(rc_api_achievement_set_definition_t));
    if (!response->sets)
      return RC_OUT_OF_MEMORY;

    result = rc_api_process_fetch_game_sets_achievement_sets(response, response->sets, &array_field);
    if (result != RC_OK)
      return result;
  }

  return RC_OK;
}

void rc_api_destroy_fetch_game_sets_response(rc_api_fetch_game_sets_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Ping --- */

int rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params) {
  return rc_api_init_ping_request_hosted(request, api_params, &g_host);
}

int rc_api_init_ping_request_hosted(rc_api_request_t* request,
                                    const rc_api_ping_request_t* api_params,
                                    const rc_api_host_t* host) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  if (rc_api_url_build_dorequest(&builder, "ping", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);

    if (api_params->rich_presence && *api_params->rich_presence)
      rc_url_builder_append_str_param(&builder, "m", api_params->rich_presence);

    if (api_params->game_hash && *api_params->game_hash) {
      rc_url_builder_append_unum_param(&builder, "h", api_params->hardcore);
      rc_url_builder_append_str_param(&builder, "x", api_params->game_hash);
    }

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_ping_server_response(response, &response_obj);
}

int rc_api_process_ping_server_response(rc_api_ping_response_t* response, const rc_api_server_response_t* server_response) {
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  return rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
}

void rc_api_destroy_ping_response(rc_api_ping_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Award Achievement --- */

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params) {
  return rc_api_init_award_achievement_request_hosted(request, api_params, &g_host);
}

int rc_api_init_award_achievement_request_hosted(rc_api_request_t* request,
                                                 const rc_api_award_achievement_request_t* api_params,
                                                 const rc_api_host_t* host) {
  rc_api_url_builder_t builder;
  char buffer[33];
  md5_state_t md5;
  md5_byte_t digest[16];

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->achievement_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 96);
  if (rc_api_url_build_dorequest(&builder, "awardachievement", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "a", api_params->achievement_id);
    rc_url_builder_append_unum_param(&builder, "h", api_params->hardcore ? 1 : 0);
    if (api_params->game_hash && *api_params->game_hash)
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
    if (api_params->seconds_since_unlock)
      rc_url_builder_append_unum_param(&builder, "o", api_params->seconds_since_unlock);

    /* Evaluate the signature. */
    md5_init(&md5);
    snprintf(buffer, sizeof(buffer), "%u", api_params->achievement_id);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
    snprintf(buffer, sizeof(buffer), "%d", api_params->hardcore ? 1 : 0);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    if (api_params->seconds_since_unlock) {
      /* second achievement id is needed by delegated unlock. including it here allows overloading
       * the hash generating code on the server */
      snprintf(buffer, sizeof(buffer), "%u", api_params->achievement_id);
      md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
      snprintf(buffer, sizeof(buffer), "%u", api_params->seconds_since_unlock);
      md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    }
    md5_finish(&md5, digest);
    rc_format_md5(buffer, digest);
    rc_url_builder_append_str_param(&builder, "v", buffer);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_award_achievement_server_response(response, &response_obj);
}

int rc_api_process_award_achievement_server_response(rc_api_award_achievement_response_t* response, const rc_api_server_response_t* server_response) {
  int result;
  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Score"),
    RC_JSON_NEW_FIELD("SoftcoreScore"),
    RC_JSON_NEW_FIELD("AchievementID"),
    RC_JSON_NEW_FIELD("AchievementsRemaining")
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!response->response.succeeded) {
    if (response->response.error_message &&
        memcmp(response->response.error_message, "User already has", 16) == 0) {
      /* not really an error, the achievement is unlocked, just not by the current call.
       *  hardcore:     User already has hardcore and regular achievements awarded.
       *  non-hardcore: User already has this achievement awarded.
       */
      response->response.succeeded = 1;
    } else {
      return result;
    }
  }

  rc_json_get_optional_unum(&response->new_player_score, &fields[2], "Score", 0);
  rc_json_get_optional_unum(&response->new_player_score_softcore, &fields[3], "SoftcoreScore", 0);
  rc_json_get_optional_unum(&response->awarded_achievement_id, &fields[4], "AchievementID", 0);
  rc_json_get_optional_unum(&response->achievements_remaining, &fields[5], "AchievementsRemaining", (unsigned)-1);

  return RC_OK;
}

void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}

/* --- Submit Leaderboard Entry --- */

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params) {
  return rc_api_init_submit_lboard_entry_request_hosted(request, api_params, &g_host);
}

int rc_api_init_submit_lboard_entry_request_hosted(rc_api_request_t* request,
                                                   const rc_api_submit_lboard_entry_request_t* api_params,
                                                   const rc_api_host_t* host) {
  rc_api_url_builder_t builder;
  char buffer[33];
  md5_state_t md5;
  md5_byte_t digest[16];

  rc_api_url_build_dorequest_url(request, host);

  if (api_params->leaderboard_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 96);
  if (rc_api_url_build_dorequest(&builder, "submitlbentry", api_params->username, api_params->api_token)) {
    rc_url_builder_append_unum_param(&builder, "i", api_params->leaderboard_id);
    rc_url_builder_append_num_param(&builder, "s", api_params->score);

    if (api_params->game_hash && *api_params->game_hash)
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);

    if (api_params->seconds_since_completion)
      rc_url_builder_append_unum_param(&builder, "o", api_params->seconds_since_completion);

    /* Evaluate the signature. */
    md5_init(&md5);
    snprintf(buffer, sizeof(buffer), "%u", api_params->leaderboard_id);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    md5_append(&md5, (md5_byte_t*)api_params->username, (int)strlen(api_params->username));
    snprintf(buffer, sizeof(buffer), "%d", api_params->score);
    md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    if (api_params->seconds_since_completion) {
      snprintf(buffer, sizeof(buffer), "%u", api_params->seconds_since_completion);
      md5_append(&md5, (md5_byte_t*)buffer, (int)strlen(buffer));
    }
    md5_finish(&md5, digest);
    rc_format_md5(buffer, digest);
    rc_url_builder_append_str_param(&builder, "v", buffer);

    request->post_data = rc_url_builder_finalize(&builder);
    request->content_type = RC_CONTENT_TYPE_URLENCODED;
  }

  return builder.result;
}

int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response) {
  rc_api_server_response_t response_obj;

  memset(&response_obj, 0, sizeof(response_obj));
  response_obj.body = server_response;
  response_obj.body_length = rc_json_get_object_string_length(server_response);

  return rc_api_process_submit_lboard_entry_server_response(response, &response_obj);
}

int rc_api_process_submit_lboard_entry_server_response(rc_api_submit_lboard_entry_response_t* response, const rc_api_server_response_t* server_response) {
  rc_api_lboard_entry_t* entry;
  rc_json_field_t array_field;
  rc_json_iterator_t iterator;
  const char* str;
  int result;

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Response") /* nested object */
  };

  rc_json_field_t response_fields[] = {
    RC_JSON_NEW_FIELD("Score"),
    RC_JSON_NEW_FIELD("BestScore"),
    RC_JSON_NEW_FIELD("RankInfo"), /* nested object */
    RC_JSON_NEW_FIELD("TopEntries") /* array */
    /* unused fields
    RC_JSON_NEW_FIELD("LBData"), / * array * /
    RC_JSON_NEW_FIELD("ScoreFormatted"),
    RC_JSON_NEW_FIELD("TopEntriesFriends") / * array * /
      * unused fields */
  };

  /* unused fields
  rc_json_field_t lbdata_fields[] = {
    RC_JSON_NEW_FIELD("Format"),
    RC_JSON_NEW_FIELD("LeaderboardID"),
    RC_JSON_NEW_FIELD("GameID"),
    RC_JSON_NEW_FIELD("Title"),
    RC_JSON_NEW_FIELD("LowerIsBetter")
  };
    * unused fields */

  rc_json_field_t entry_fields[] = {
    RC_JSON_NEW_FIELD("User"),
    RC_JSON_NEW_FIELD("Rank"),
    RC_JSON_NEW_FIELD("Score")
    /* unused fields
    RC_JSON_NEW_FIELD("DateSubmitted")
     * unused fields */
  };

  rc_json_field_t rank_info_fields[] = {
    RC_JSON_NEW_FIELD("Rank"),
    RC_JSON_NEW_FIELD("NumEntries")
    /* unused fields
    RC_JSON_NEW_FIELD("LowerIsBetter"),
    RC_JSON_NEW_FIELD("UserRank")
      * unused fields */
  };

  memset(response, 0, sizeof(*response));
  rc_buffer_init(&response->response.buffer);

  result = rc_json_parse_server_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_object(response_fields, sizeof(response_fields) / sizeof(response_fields[0]), &response->response, &fields[2], "Response"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_num(&response->submitted_score, &response->response, &response_fields[0], "Score"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_num(&response->best_score, &response->response, &response_fields[1], "BestScore"))
    return RC_MISSING_VALUE;

  if (!rc_json_get_required_object(rank_info_fields, sizeof(rank_info_fields) / sizeof(rank_info_fields[0]), &response->response, &response_fields[2], "RankInfo"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_unum(&response->new_rank, &response->response, &rank_info_fields[0], "Rank"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&str, &response->response, &rank_info_fields[1], "NumEntries"))
    return RC_MISSING_VALUE;
  response->num_entries = (unsigned)atoi(str);

  if (!rc_json_get_required_array(&response->num_top_entries, &array_field, &response->response, &response_fields[3], "TopEntries"))
    return RC_MISSING_VALUE;

  if (response->num_top_entries) {
    response->top_entries = (rc_api_lboard_entry_t*)rc_buffer_alloc(&response->response.buffer, response->num_top_entries * sizeof(rc_api_lboard_entry_t));
    if (!response->top_entries)
      return RC_OUT_OF_MEMORY;

    memset(&iterator, 0, sizeof(iterator));
    iterator.json = array_field.value_start;
    iterator.end = array_field.value_end;

    entry = response->top_entries;
    while (rc_json_get_array_entry_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]), &iterator)) {
      if (!rc_json_get_required_string(&entry->username, &response->response, &entry_fields[0], "User"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_unum(&entry->rank, &response->response, &entry_fields[1], "Rank"))
        return RC_MISSING_VALUE;

      if (!rc_json_get_required_num(&entry->score, &response->response, &entry_fields[2], "Score"))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }

  return RC_OK;
}

void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response) {
  rc_buffer_destroy(&response->response.buffer);
}
