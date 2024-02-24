#ifndef RC_API_COMMON_H
#define RC_API_COMMON_H

#include "rc_api_request.h"

#include <stddef.h>
#include <time.h>

RC_BEGIN_C_DECLS

#define RC_CONTENT_TYPE_URLENCODED "application/x-www-form-urlencoded"

typedef struct rc_api_url_builder_t {
  char* write;
  char* start;
  char* end;
  /* pointer to a preallocated rc_buffer_t */
  rc_buffer_t* buffer;
  int result;
}
rc_api_url_builder_t;

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_buffer_t* buffer, size_t estimated_size);
void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len);
const char* rc_url_builder_finalize(rc_api_url_builder_t* builder);

#define RC_JSON_NEW_FIELD(n) {NULL,NULL,n,sizeof(n)-1,0}

typedef struct rc_json_field_t {
  const char* value_start;
  const char* value_end;
  const char* name;
  size_t name_len;
  uint32_t array_size;
}
rc_json_field_t;

typedef struct rc_json_iterator_t {
  const char* json;
  const char* end;
}
rc_json_iterator_t;

int rc_json_parse_server_response(rc_api_response_t* response, const rc_api_server_response_t* server_response, rc_json_field_t* fields, size_t field_count);
int rc_json_get_string(const char** out, rc_buffer_t* buffer, const rc_json_field_t* field, const char* field_name);
int rc_json_get_num(int32_t* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_float(float* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_datetime(time_t* out, const rc_json_field_t* field, const char* field_name);
void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value);
void rc_json_get_optional_num(int32_t* out, const rc_json_field_t* field, const char* field_name, int default_value);
void rc_json_get_optional_unum(uint32_t* out, const rc_json_field_t* field, const char* field_name, uint32_t default_value);
void rc_json_get_optional_float(float* out, const rc_json_field_t* field, const char* field_name, float default_value);
void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value);
int rc_json_get_optional_array(uint32_t* num_entries, rc_json_field_t* iterator, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_num(int32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_unum(uint32_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_float(float* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_datetime(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name);
int rc_json_get_required_unum_array(uint32_t** entries, uint32_t* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_array(uint32_t* num_entries, rc_json_field_t* array_field, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_iterator_t* iterator);
int rc_json_get_next_object_field(rc_json_iterator_t* iterator, rc_json_field_t* field);
int rc_json_get_object_string_length(const char* json);

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str);
void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, int32_t value);
void rc_url_builder_append_unum_param(rc_api_url_builder_t* builder, const char* param, uint32_t value);
void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value);

void rc_api_url_build_dorequest_url(rc_api_request_t* request);
int rc_api_url_build_dorequest(rc_api_url_builder_t* builder, const char* api, const char* username, const char* api_token);

RC_END_C_DECLS

#endif /* RC_API_COMMON_H */
