#ifndef RC_API_REQUEST_H
#define RC_API_REQUEST_H

#include "rc_error.h"
#include "rc_util.h"

#include <stddef.h>

RC_BEGIN_C_DECLS

/**
 * Information about the server being connected to.
 */
typedef struct rc_api_host_t {
  /* The host name for the API calls (retroachievements.org) */
  const char* host;
  /* The host name for media URLs (media.retroachievements.org) */
  const char* media_host;
}
rc_api_host_t;

/**
 * A constructed request to send to the retroachievements server.
 */
typedef struct rc_api_request_t {
  /* The URL to send the request to (contains protocol, host, path, and query args) */
  const char* url;
  /* Additional query args that should be sent via a POST command. If null, GET may be used */
  const char* post_data;
  /* The HTTP Content-Type of the POST data. */
  const char* content_type;

  /* Storage for the url and post_data */
  rc_buffer_t buffer;
}
rc_api_request_t;

/**
 * Common attributes for all server responses.
 */
typedef struct rc_api_response_t {
  /* Server-provided success indicator (non-zero on success, zero on failure) */
  int succeeded;
  /* Server-provided message associated to the failure */
  const char* error_message;
  /* Server-provided error code associated to the failure */
  const char* error_code;

  /* Storage for the response data */
  rc_buffer_t buffer;
}
rc_api_response_t;

RC_EXPORT void RC_CCONV rc_api_destroy_request(rc_api_request_t* request);

/* [deprecated] use rc_api_init_*_hosted instead */
RC_EXPORT void RC_CCONV rc_api_set_host(const char* hostname);
/* [deprecated] use rc_api_init_*_hosted instead */
RC_EXPORT void RC_CCONV rc_api_set_image_host(const char* hostname);

typedef struct rc_api_server_response_t {
  /* Pointer to the data returned from the server */
  const char* body;
  /* Length of data returned from the server (Content-Length) */
  size_t body_length;
  /* HTTP status code returned from the server */
  int http_status_code;
} rc_api_server_response_t;

enum {
  RC_API_SERVER_RESPONSE_CLIENT_ERROR = -1,
  RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR = -2
};

RC_END_C_DECLS

#endif /* RC_API_REQUEST_H */
