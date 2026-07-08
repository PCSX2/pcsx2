#include "rc_client_external.h"

#include "rc_client_external_versions.h"
#include "rc_client_internal.h"

#include "rc_api_runtime.h"

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

#define RC_CONVERSION_FILL(obj, obj_type, src_type) memset((uint8_t*)obj + sizeof(src_type), 0, sizeof(obj_type) - sizeof(src_type))

/* https://media.retroachievements.org/Badge/123456_lock.png is 58 with null terminator */
#define RC_CLIENT_IMAGE_URL_BUFFER_SIZE 64
/* https://media.retroachievements.org/UserPic/TwentyCharUserNameXX.png is 69 with null terminator */
#define RC_CLIENT_USER_IMAGE_URL_BUFFER_SIZE 80

typedef struct rc_client_external_conversions_t {
  rc_client_user_t user;
  rc_client_game_t game;
  rc_client_subset_t subsets[4];
  rc_client_achievement_t achievements[16];
  char user_avatar_url[RC_CLIENT_USER_IMAGE_URL_BUFFER_SIZE];
  char game_badge_url[RC_CLIENT_IMAGE_URL_BUFFER_SIZE];
  char subset_badge_url[4][RC_CLIENT_IMAGE_URL_BUFFER_SIZE];
  char achievement_badge_url[16][RC_CLIENT_IMAGE_URL_BUFFER_SIZE];
  char achievement_badge_locked_url[16][RC_CLIENT_IMAGE_URL_BUFFER_SIZE];
  uint32_t next_subset_index;
  uint32_t next_achievement_index;
} rc_client_external_conversions_t;

static const char* rc_client_external_build_avatar_url(char buffer[], size_t buffer_size, uint32_t image_type, const char* image_name)
{
  rc_api_fetch_image_request_t image_request;
  rc_api_request_t request;
  int result;

  memset(&image_request, 0, sizeof(image_request));
  image_request.image_type = image_type;
  image_request.image_name = image_name;

  result = rc_api_init_fetch_image_request(&request, &image_request);
  if (result != RC_OK)
    return NULL;

  snprintf(buffer, buffer_size, "%s", request.url);
  return buffer;
}

static void rc_client_external_conversions_init(const rc_client_t* client)
{
  if (!client->state.external_client_conversions) {
    rc_client_t* mutable_client = (rc_client_t*)client;
    rc_client_external_conversions_t* conversions = (rc_client_external_conversions_t*)
      rc_buffer_alloc(&mutable_client->state.buffer, sizeof(rc_client_external_conversions_t));

    memset(conversions, 0, sizeof(*conversions));

    mutable_client->state.external_client_conversions = conversions;
  }
}

const rc_client_user_t* rc_client_external_convert_v1_user(const rc_client_t* client, const rc_client_user_t* v1_user)
{
  rc_client_user_t* converted;

  if (!v1_user)
    return NULL;

  rc_client_external_conversions_init(client);

  converted = &client->state.external_client_conversions->user;
  memcpy(converted, v1_user, sizeof(v1_rc_client_user_t));
  RC_CONVERSION_FILL(converted, rc_client_user_t, v1_rc_client_user_t);

  converted->avatar_url = rc_client_external_build_avatar_url(
    client->state.external_client_conversions->user_avatar_url,
    sizeof(client->state.external_client_conversions->user_avatar_url),
    RC_IMAGE_TYPE_USER, v1_user->username);

  return converted;
}

const rc_client_game_t* rc_client_external_convert_v1_game(const rc_client_t* client, const rc_client_game_t* v1_game)
{
  rc_client_game_t* converted;

  if (!v1_game)
    return NULL;

  rc_client_external_conversions_init(client);

  converted = &client->state.external_client_conversions->game;
  memcpy(converted, v1_game, sizeof(v1_rc_client_game_t));
  RC_CONVERSION_FILL(converted, rc_client_game_t, v1_rc_client_game_t);

  converted->badge_url = rc_client_external_build_avatar_url(
    client->state.external_client_conversions->game_badge_url,
    sizeof(client->state.external_client_conversions->game_badge_url),
    RC_IMAGE_TYPE_GAME, v1_game->badge_name);

  return converted;
}

const rc_client_subset_t* rc_client_external_convert_v1_subset(const rc_client_t* client, const rc_client_subset_t* v1_subset)
{
  rc_client_subset_t* converted = NULL;
  char* badge_url = NULL;
  const uint32_t num_subsets = sizeof(client->state.external_client_conversions->subsets) / sizeof(client->state.external_client_conversions->subsets[0]);
  uint32_t index;

  if (!v1_subset)
    return NULL;

  rc_client_external_conversions_init(client);

  for (index = 0; index < num_subsets; ++index) {
    if (client->state.external_client_conversions->subsets[index].id == v1_subset->id) {
      converted = &client->state.external_client_conversions->subsets[index];
      badge_url = client->state.external_client_conversions->subset_badge_url[index];
      break;
    }
  }

  if (!converted) {
    index = client->state.external_client_conversions->next_subset_index;
    converted = &client->state.external_client_conversions->subsets[index];
    badge_url = client->state.external_client_conversions->subset_badge_url[client->state.external_client_conversions->next_subset_index];
    client->state.external_client_conversions->next_subset_index = (index + 1) % num_subsets;
  }

  memcpy(converted, v1_subset, sizeof(v1_rc_client_subset_t));
  RC_CONVERSION_FILL(converted, rc_client_subset_t, v1_rc_client_subset_t);

  converted->badge_url = rc_client_external_build_avatar_url(badge_url,
    sizeof(client->state.external_client_conversions->subset_badge_url[0]),
    RC_IMAGE_TYPE_GAME, v1_subset->badge_name);

  return converted;
}

const rc_client_achievement_t* rc_client_external_convert_v1_achievement(const rc_client_t* client, const rc_client_achievement_t* v1_achievement)
{
  rc_client_achievement_t* converted = NULL;
  char* badge_url = NULL;
  char* badge_locked_url = NULL;
  const uint32_t num_achievements = sizeof(client->state.external_client_conversions->achievements) / sizeof(client->state.external_client_conversions->achievements[0]);
  uint32_t index;

  if (!v1_achievement)
    return NULL;

  rc_client_external_conversions_init(client);

  for (index = 0; index < num_achievements; ++index) {
    if (client->state.external_client_conversions->achievements[index].id == v1_achievement->id) {
      converted = &client->state.external_client_conversions->achievements[index];
      badge_url = client->state.external_client_conversions->achievement_badge_url[index];
      badge_locked_url = client->state.external_client_conversions->achievement_badge_locked_url[index];
      break;
    }
  }

  if (!converted) {
    index = client->state.external_client_conversions->next_achievement_index;
    converted = &client->state.external_client_conversions->achievements[index];
    badge_url = client->state.external_client_conversions->achievement_badge_url[index];
    badge_locked_url = client->state.external_client_conversions->achievement_badge_locked_url[index];
    client->state.external_client_conversions->next_achievement_index = (index + 1) % num_achievements;
  }

  memcpy(converted, v1_achievement, sizeof(v1_rc_client_achievement_t));
  RC_CONVERSION_FILL(converted, rc_client_achievement_t, v1_rc_client_achievement_t);

  converted->badge_url = rc_client_external_build_avatar_url(badge_url,
    sizeof(client->state.external_client_conversions->achievement_badge_url[0]),
    RC_IMAGE_TYPE_ACHIEVEMENT, v1_achievement->badge_name);
  converted->badge_locked_url = rc_client_external_build_avatar_url(badge_locked_url,
    sizeof(client->state.external_client_conversions->achievement_badge_locked_url[0]),
    RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED, v1_achievement->badge_name);

  return converted;
}

typedef struct rc_client_achievement_list_wrapper_t {
  rc_client_achievement_list_info_t info;
  rc_client_achievement_list_t* source_list;
  rc_client_achievement_t* achievements;
  rc_client_achievement_t** achievements_pointers;
  char* badge_url_buffer;
} rc_client_achievement_list_wrapper_t;

static void rc_client_destroy_achievement_list_wrapper(rc_client_achievement_list_info_t* info)
{
  rc_client_achievement_list_wrapper_t* wrapper = (rc_client_achievement_list_wrapper_t*)info;

  if (wrapper->achievements)
    free(wrapper->achievements);
  if (wrapper->achievements_pointers)
    free(wrapper->achievements_pointers);
  if (wrapper->info.public_.buckets)
    free((void*)wrapper->info.public_.buckets);
  if (wrapper->badge_url_buffer)
    free(wrapper->badge_url_buffer);

  rc_client_destroy_achievement_list(wrapper->source_list);

  free(wrapper);
}

rc_client_achievement_list_t* rc_client_external_convert_v1_achievement_list(const rc_client_t* client, rc_client_achievement_list_t* v1_achievement_list)
{
  rc_client_achievement_list_wrapper_t* new_list;
  (void)client;

  if (!v1_achievement_list)
    return NULL;

  new_list = (rc_client_achievement_list_wrapper_t*)calloc(1, sizeof(*new_list));
  if (!new_list)
    return NULL;

  new_list->source_list = v1_achievement_list;
  new_list->info.destroy_func = rc_client_destroy_achievement_list_wrapper;

  if (v1_achievement_list->num_buckets) {
    const v1_rc_client_achievement_bucket_t* src_bucket = (const v1_rc_client_achievement_bucket_t*)&v1_achievement_list->buckets[0];
    const v1_rc_client_achievement_bucket_t* stop_bucket = src_bucket + v1_achievement_list->num_buckets;
    rc_client_achievement_bucket_t* bucket;
    uint32_t num_achievements = 0;
    char* badge_url = NULL;

    new_list->info.public_.buckets = bucket = (rc_client_achievement_bucket_t*)calloc(v1_achievement_list->num_buckets, sizeof(*new_list->info.public_.buckets));
    if (!new_list->info.public_.buckets)
      return (rc_client_achievement_list_t*)new_list;

    for (; src_bucket < stop_bucket; src_bucket++)
      num_achievements += src_bucket->num_achievements;

    if (num_achievements) {
      new_list->achievements = (rc_client_achievement_t*)calloc(num_achievements, sizeof(*new_list->achievements));
      new_list->achievements_pointers = (rc_client_achievement_t**)malloc(num_achievements * sizeof(rc_client_achievement_t*));
      new_list->badge_url_buffer = badge_url = (char*)malloc(num_achievements * 2 * RC_CLIENT_IMAGE_URL_BUFFER_SIZE);
      if (!new_list->achievements || !new_list->achievements_pointers || !new_list->badge_url_buffer)
        return (rc_client_achievement_list_t*)new_list;
    }

    num_achievements = 0;
    src_bucket = (const v1_rc_client_achievement_bucket_t*)&v1_achievement_list->buckets[0];
    for (; src_bucket < stop_bucket; src_bucket++, bucket++) {
      memcpy(bucket, src_bucket, sizeof(*src_bucket));

      if (src_bucket->num_achievements) {
        const v1_rc_client_achievement_t** src_achievement = (const v1_rc_client_achievement_t**)src_bucket->achievements;
        const v1_rc_client_achievement_t** stop_achievement = src_achievement + src_bucket->num_achievements;
        rc_client_achievement_t** achievement = &new_list->achievements_pointers[num_achievements];

        bucket->achievements = (const rc_client_achievement_t**)achievement;

        for (; src_achievement < stop_achievement; ++src_achievement, ++achievement) {
          *achievement = &new_list->achievements[num_achievements++];
          memcpy(*achievement, *src_achievement, sizeof(**src_achievement));

          (*achievement)->badge_url = rc_client_external_build_avatar_url(badge_url,
            sizeof(client->state.external_client_conversions->achievement_badge_url[0]),
            RC_IMAGE_TYPE_ACHIEVEMENT, (*achievement)->badge_name);
          badge_url += RC_CLIENT_IMAGE_URL_BUFFER_SIZE;
          (*achievement)->badge_locked_url = rc_client_external_build_avatar_url(badge_url,
            sizeof(client->state.external_client_conversions->achievement_badge_locked_url[0]),
            RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED, (*achievement)->badge_name);
          badge_url += RC_CLIENT_IMAGE_URL_BUFFER_SIZE;
        }
      }
    }

    new_list->info.public_.num_buckets = v1_achievement_list->num_buckets;
  }

  return (rc_client_achievement_list_t*)new_list;
}

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */
