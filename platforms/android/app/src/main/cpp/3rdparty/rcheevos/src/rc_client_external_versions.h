#ifndef RC_CLIENT_EXTERNAL_CONVERSIONS_H
#define RC_CLIENT_EXTERNAL_CONVERSIONS_H

#include "rc_client_internal.h"

RC_BEGIN_C_DECLS

/* user */

typedef struct v1_rc_client_user_t {
  const char* display_name;
  const char* username;
  const char* token;
  uint32_t score;
  uint32_t score_softcore;
  uint32_t num_unread_messages;
} v1_rc_client_user_t;

typedef struct v3_rc_client_user_t {
  const char* display_name;
  const char* username;
  const char* token;
  uint32_t score;
  uint32_t score_softcore;
  uint32_t num_unread_messages;
  const char* avatar_url;
} v3_rc_client_user_t;

/* game */

typedef struct v1_rc_client_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  const char* badge_name;
} v1_rc_client_game_t;

typedef struct v3_rc_client_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  const char* badge_name;
  const char* badge_url;
} v3_rc_client_game_t;

/* subset */

typedef struct v1_rc_client_subset_t {
  uint32_t id;
  const char* title;
  char badge_name[16];
  uint32_t num_achievements;
  uint32_t num_leaderboards;
} v1_rc_client_subset_t;

typedef struct v3_rc_client_subset_t {
  uint32_t id;
  const char* title;
  char badge_name[16];
  uint32_t num_achievements;
  uint32_t num_leaderboards;
  const char* badge_url;
} v3_rc_client_subset_t;

/* achievement */

typedef struct v1_rc_client_achievement_t {
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
  float rarity;
  float rarity_hardcore;
  uint8_t type;
} v1_rc_client_achievement_t;

typedef struct v3_rc_client_achievement_t {
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
  float rarity;
  float rarity_hardcore;
  uint8_t type;
  const char* badge_url;
  const char* badge_locked_url;
} v3_rc_client_achievement_t;

/* achievement list */

typedef struct v1_rc_client_achievement_bucket_t {
  v1_rc_client_achievement_t** achievements;
  uint32_t num_achievements;

  const char* label;
  uint32_t subset_id;
  uint8_t bucket_type;
} v1_rc_client_achievement_bucket_t;

typedef struct v1_rc_client_achievement_list_t {
  v1_rc_client_achievement_bucket_t* buckets;
  uint32_t num_buckets;
} v1_rc_client_achievement_list_t;

typedef struct v1_rc_client_achievement_list_info_t {
  v1_rc_client_achievement_list_t public_;
  rc_client_destroy_achievement_list_func_t destroy_func;
} v1_rc_client_achievement_list_info_t;

typedef struct v3_rc_client_achievement_bucket_t {
  const v3_rc_client_achievement_t** achievements;
  uint32_t num_achievements;

  const char* label;
  uint32_t subset_id;
  uint8_t bucket_type;
} v3_rc_client_achievement_bucket_t;

typedef struct v3_rc_client_achievement_list_t {
  const v3_rc_client_achievement_bucket_t* buckets;
  uint32_t num_buckets;
} v3_rc_client_achievement_list_t;

typedef struct v3_rc_client_achievement_list_info_t {
  v3_rc_client_achievement_list_t public_;
  rc_client_destroy_achievement_list_func_t destroy_func;
} v3_rc_client_achievement_list_info_t;

/* user_game_summary */

typedef struct v1_rc_client_user_game_summary_t {
  uint32_t num_core_achievements;
  uint32_t num_unofficial_achievements;
  uint32_t num_unlocked_achievements;
  uint32_t num_unsupported_achievements;
  uint32_t points_core;
  uint32_t points_unlocked;
} v1_rc_client_user_game_summary_t;

typedef struct v5_rc_client_user_game_summary_t {
  uint32_t num_core_achievements;
  uint32_t num_unofficial_achievements;
  uint32_t num_unlocked_achievements;
  uint32_t num_unsupported_achievements;
  uint32_t points_core;
  uint32_t points_unlocked;
  time_t beaten_time;
  time_t completed_time;
} v5_rc_client_user_game_summary_t;

RC_END_C_DECLS

#endif /* RC_CLIENT_EXTERNAL_CONVERSIONS_H */
