#ifndef RC_RUNTIME_H
#define RC_RUNTIME_H

#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Forward Declarations (defined in rc_runtime_types.h)                        |
\*****************************************************************************/

#ifndef RC_RUNTIME_TYPES_H /* prevents pedantic redefinition error */

typedef struct rc_trigger_t rc_trigger_t;
typedef struct rc_lboard_t rc_lboard_t;
typedef struct rc_richpresence_t rc_richpresence_t;
typedef struct rc_memref_t rc_memref_t;
typedef struct rc_value_t rc_value_t;

#endif

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

/**
 * Callback used to read num_bytes bytes from memory starting at address. If
 * num_bytes is greater than 1, the value is read in little-endian from
 * memory.
 */
typedef uint32_t(RC_CCONV *rc_runtime_peek_t)(uint32_t address, uint32_t num_bytes, void* ud);

/*****************************************************************************\
| Runtime                                                                     |
\*****************************************************************************/

typedef struct rc_runtime_trigger_t {
  uint32_t id;
  rc_trigger_t* trigger;
  void* buffer;
  rc_memref_t* invalid_memref;
  uint8_t md5[16];
  int32_t serialized_size;
}
rc_runtime_trigger_t;

typedef struct rc_runtime_lboard_t {
  uint32_t id;
  int32_t value;
  rc_lboard_t* lboard;
  void* buffer;
  rc_memref_t* invalid_memref;
  uint8_t md5[16];
  uint32_t serialized_size;
}
rc_runtime_lboard_t;

typedef struct rc_runtime_richpresence_t {
  rc_richpresence_t* richpresence;
  void* buffer;
  uint8_t md5[16];
}
rc_runtime_richpresence_t;

typedef struct rc_runtime_t {
  rc_runtime_trigger_t* triggers;
  uint32_t trigger_count;
  uint32_t trigger_capacity;

  rc_runtime_lboard_t* lboards;
  uint32_t lboard_count;
  uint32_t lboard_capacity;

  rc_runtime_richpresence_t* richpresence;

  struct rc_memrefs_t* memrefs;

  uint8_t owns_self;
}
rc_runtime_t;

RC_EXPORT rc_runtime_t* RC_CCONV rc_runtime_alloc(void);
RC_EXPORT void RC_CCONV rc_runtime_init(rc_runtime_t* runtime);
RC_EXPORT void RC_CCONV rc_runtime_destroy(rc_runtime_t* runtime);

RC_EXPORT int RC_CCONV rc_runtime_activate_achievement(rc_runtime_t* runtime, uint32_t id, const char* memaddr, void* unused_L, int unused_funcs_idx);
RC_EXPORT void RC_CCONV rc_runtime_deactivate_achievement(rc_runtime_t* runtime, uint32_t id);
RC_EXPORT rc_trigger_t* RC_CCONV rc_runtime_get_achievement(const rc_runtime_t* runtime, uint32_t id);
RC_EXPORT int RC_CCONV rc_runtime_get_achievement_measured(const rc_runtime_t* runtime, uint32_t id, unsigned* measured_value, unsigned* measured_target);
RC_EXPORT int RC_CCONV rc_runtime_format_achievement_measured(const rc_runtime_t* runtime, uint32_t id, char *buffer, size_t buffer_size);

RC_EXPORT int RC_CCONV rc_runtime_activate_lboard(rc_runtime_t* runtime, uint32_t id, const char* memaddr, void* unused_L, int unused_funcs_idx);
RC_EXPORT void RC_CCONV rc_runtime_deactivate_lboard(rc_runtime_t* runtime, uint32_t id);
RC_EXPORT rc_lboard_t* RC_CCONV rc_runtime_get_lboard(const rc_runtime_t* runtime, uint32_t id);
RC_EXPORT int RC_CCONV rc_runtime_format_lboard_value(char* buffer, int size, int32_t value, int format);


RC_EXPORT int RC_CCONV rc_runtime_activate_richpresence(rc_runtime_t* runtime, const char* script, void* unused_L, int unused_funcs_idx);
RC_EXPORT int RC_CCONV rc_runtime_get_richpresence(const rc_runtime_t* runtime, char* buffer, size_t buffersize, rc_runtime_peek_t peek, void* peek_ud, void* unused_L);

enum {
  RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, /* from WAITING, PAUSED, or PRIMED to ACTIVE */
  RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_RESET,
  RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED,
  RC_RUNTIME_EVENT_LBOARD_STARTED,
  RC_RUNTIME_EVENT_LBOARD_CANCELED,
  RC_RUNTIME_EVENT_LBOARD_UPDATED,
  RC_RUNTIME_EVENT_LBOARD_TRIGGERED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED,
  RC_RUNTIME_EVENT_LBOARD_DISABLED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED,
  RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED
};

typedef struct rc_runtime_event_t {
  uint32_t id;
  int32_t value;
  uint8_t type;
}
rc_runtime_event_t;

typedef void (RC_CCONV *rc_runtime_event_handler_t)(const rc_runtime_event_t* runtime_event);

RC_EXPORT void RC_CCONV rc_runtime_do_frame(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_runtime_peek_t peek, void* ud, void* unused_L);
RC_EXPORT void RC_CCONV rc_runtime_reset(rc_runtime_t* runtime);

typedef int (RC_CCONV *rc_runtime_validate_address_t)(uint32_t address);
RC_EXPORT void RC_CCONV rc_runtime_validate_addresses(rc_runtime_t* runtime, rc_runtime_event_handler_t event_handler, rc_runtime_validate_address_t validate_handler);
RC_EXPORT void RC_CCONV rc_runtime_invalidate_address(rc_runtime_t* runtime, uint32_t address);

RC_EXPORT uint32_t RC_CCONV rc_runtime_progress_size(const rc_runtime_t* runtime, void* unused_L);

/* [deprecated] use rc_runtime_serialize_progress_sized instead */
RC_EXPORT int RC_CCONV rc_runtime_serialize_progress(void* buffer, const rc_runtime_t* runtime, void* unused_L);
RC_EXPORT int RC_CCONV rc_runtime_serialize_progress_sized(uint8_t* buffer, uint32_t buffer_size, const rc_runtime_t* runtime, void* unused_L);

/* [deprecated] use rc_runtime_deserialize_progress_sized instead */
RC_EXPORT int RC_CCONV rc_runtime_deserialize_progress(rc_runtime_t* runtime, const uint8_t* serialized, void* unused_L);
RC_EXPORT int RC_CCONV rc_runtime_deserialize_progress_sized(rc_runtime_t* runtime, const uint8_t* serialized, uint32_t serialized_size, void* unused_L);

RC_END_C_DECLS

#endif /* RC_RUNTIME_H */
