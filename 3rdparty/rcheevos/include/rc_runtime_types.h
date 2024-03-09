#ifndef RC_RUNTIME_TYPES_H
#define RC_RUNTIME_TYPES_H

#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>

RC_BEGIN_C_DECLS

#ifndef RC_RUNTIME_H /* prevents pedantic redefiniton error */

typedef struct lua_State lua_State;

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
typedef uint32_t(RC_CCONV *rc_peek_t)(uint32_t address, uint32_t num_bytes, void* ud);

/*****************************************************************************\
| Memory References                                                           |
\*****************************************************************************/

/* Sizes. */
enum {
  RC_MEMSIZE_8_BITS,
  RC_MEMSIZE_16_BITS,
  RC_MEMSIZE_24_BITS,
  RC_MEMSIZE_32_BITS,
  RC_MEMSIZE_LOW,
  RC_MEMSIZE_HIGH,
  RC_MEMSIZE_BIT_0,
  RC_MEMSIZE_BIT_1,
  RC_MEMSIZE_BIT_2,
  RC_MEMSIZE_BIT_3,
  RC_MEMSIZE_BIT_4,
  RC_MEMSIZE_BIT_5,
  RC_MEMSIZE_BIT_6,
  RC_MEMSIZE_BIT_7,
  RC_MEMSIZE_BITCOUNT,
  RC_MEMSIZE_16_BITS_BE,
  RC_MEMSIZE_24_BITS_BE,
  RC_MEMSIZE_32_BITS_BE,
  RC_MEMSIZE_FLOAT,
  RC_MEMSIZE_MBF32,
  RC_MEMSIZE_MBF32_LE,
  RC_MEMSIZE_FLOAT_BE,
  RC_MEMSIZE_VARIABLE
};

typedef struct rc_memref_value_t {
  /* The current value of this memory reference. */
  uint32_t value;
  /* The last differing value of this memory reference. */
  uint32_t prior;

  /* The size of the value. */
  uint8_t size;
  /* True if the value changed this frame. */
  uint8_t changed;
  /* The value type of the value (for variables) */
  uint8_t type;
  /* True if the reference will be used in indirection.
   * NOTE: This is actually a property of the rc_memref_t, but we put it here to save space */
  uint8_t is_indirect;
}
rc_memref_value_t;

struct rc_memref_t {
  /* The current value at the specified memory address. */
  rc_memref_value_t value;

  /* The memory address of this variable. */
  uint32_t address;

  /* The next memory reference in the chain. */
  rc_memref_t* next;
};

/*****************************************************************************\
| Operands                                                                    |
\*****************************************************************************/

/* types */
enum {
  RC_OPERAND_ADDRESS,        /* The value of a live address in RAM. */
  RC_OPERAND_DELTA,          /* The value last known at this address. */
  RC_OPERAND_CONST,          /* A 32-bit unsigned integer. */
  RC_OPERAND_FP,             /* A floating point value. */
  RC_OPERAND_LUA,            /* A Lua function that provides the value. */
  RC_OPERAND_PRIOR,          /* The last differing value at this address. */
  RC_OPERAND_BCD,            /* The BCD-decoded value of a live address in RAM. */
  RC_OPERAND_INVERTED        /* The twos-complement value of a live address in RAM. */
};

typedef struct rc_operand_t {
  union {
    /* A value read from memory. */
    rc_memref_t* memref;

    /* An integer value. */
    uint32_t num;

    /* A floating point value. */
    double dbl;

    /* A reference to the Lua function that provides the value. */
    int luafunc;
  } value;

  /* specifies which member of the value union is being used */
  uint8_t type;

  /* the actual RC_MEMSIZE of the operand - memref.size may differ */
  uint8_t size;
}
rc_operand_t;

RC_EXPORT int RC_CCONV rc_operand_is_memref(const rc_operand_t* operand);

/*****************************************************************************\
| Conditions                                                                  |
\*****************************************************************************/

/* types */
enum {
  /* NOTE: this enum is ordered to optimize the switch statements in rc_test_condset_internal. the values may change between releases */

  /* non-combining conditions (third switch) */
  RC_CONDITION_STANDARD, /* this should always be 0 */
  RC_CONDITION_PAUSE_IF,
  RC_CONDITION_RESET_IF,
  RC_CONDITION_MEASURED_IF,
  RC_CONDITION_TRIGGER,
  RC_CONDITION_MEASURED, /* measured also appears in the first switch, so place it at the border between them */

  /* modifiers (first switch) */
  RC_CONDITION_ADD_SOURCE, /* everything from this point on affects the condition after it */
  RC_CONDITION_SUB_SOURCE,
  RC_CONDITION_ADD_ADDRESS,

  /* logic flags (second switch) */
  RC_CONDITION_ADD_HITS,
  RC_CONDITION_SUB_HITS,
  RC_CONDITION_RESET_NEXT_IF,
  RC_CONDITION_AND_NEXT,
  RC_CONDITION_OR_NEXT
};

/* operators */
enum {
  RC_OPERATOR_EQ,
  RC_OPERATOR_LT,
  RC_OPERATOR_LE,
  RC_OPERATOR_GT,
  RC_OPERATOR_GE,
  RC_OPERATOR_NE,
  RC_OPERATOR_NONE,
  RC_OPERATOR_MULT,
  RC_OPERATOR_DIV,
  RC_OPERATOR_AND,
  RC_OPERATOR_XOR
};

typedef struct rc_condition_t rc_condition_t;

struct rc_condition_t {
  /* The condition's operands. */
  rc_operand_t operand1;
  rc_operand_t operand2;

  /* Required hits to fire this condition. */
  uint32_t required_hits;
  /* Number of hits so far. */
  uint32_t current_hits;

  /* The next condition in the chain. */
  rc_condition_t* next;

  /* The type of the condition. (RC_CONDITION_*) */
  uint8_t type;

  /* The comparison operator to use. (RC_OPERATOR_*) */
  uint8_t oper; /* operator is a reserved word in C++. */

  /* Set if the condition needs to processed as part of the "check if paused" pass. (bool) */
  uint8_t pause;

  /* Whether or not the condition evaluated true on the last check. (bool) */
  uint8_t is_true;

  /* Unique identifier of optimized comparator to use. (RC_PROCESSING_COMPARE_*) */
  uint8_t optimized_comparator;
};

/*****************************************************************************\
| Condition sets                                                              |
\*****************************************************************************/

typedef struct rc_condset_t rc_condset_t;

struct rc_condset_t {
  /* The next condition set in the chain. */
  rc_condset_t* next;

  /* The list of conditions in this condition set. */
  rc_condition_t* conditions;

  /* True if any condition in the set is a pause condition. */
  uint8_t has_pause;

  /* True if the set is currently paused. */
  uint8_t is_paused;

  /* True if the set has indirect memory references. */
  uint8_t has_indirect_memrefs;
};

/*****************************************************************************\
| Trigger                                                                     |
\*****************************************************************************/

enum {
  RC_TRIGGER_STATE_INACTIVE,   /* achievement is not being processed */
  RC_TRIGGER_STATE_WAITING,    /* achievement cannot trigger until it has been false for at least one frame */
  RC_TRIGGER_STATE_ACTIVE,     /* achievement is active and may trigger */
  RC_TRIGGER_STATE_PAUSED,     /* achievement is currently paused and will not trigger */
  RC_TRIGGER_STATE_RESET,      /* achievement hit counts were reset */
  RC_TRIGGER_STATE_TRIGGERED,  /* achievement has triggered */
  RC_TRIGGER_STATE_PRIMED,     /* all non-Trigger conditions are true */
  RC_TRIGGER_STATE_DISABLED    /* achievement cannot be processed at this time */
};

struct rc_trigger_t {
  /* The main condition set. */
  rc_condset_t* requirement;

  /* The list of sub condition sets in this test. */
  rc_condset_t* alternative;

  /* The memory references required by the trigger. */
  rc_memref_t* memrefs;

  /* The current state of the MEASURED condition. */
  uint32_t measured_value;

  /* The target state of the MEASURED condition */
  uint32_t measured_target;

  /* The current state of the trigger */
  uint8_t state;

  /* True if at least one condition has a non-zero hit count */
  uint8_t has_hits;

  /* True if at least one condition has a non-zero required hit count */
  uint8_t has_required_hits;

  /* True if the measured value should be displayed as a percentage */
  uint8_t measured_as_percent;
};

RC_EXPORT int RC_CCONV rc_trigger_size(const char* memaddr);
RC_EXPORT rc_trigger_t* RC_CCONV rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
RC_EXPORT int RC_CCONV rc_evaluate_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, lua_State* L);
RC_EXPORT int RC_CCONV rc_test_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, lua_State* L);
RC_EXPORT void RC_CCONV rc_reset_trigger(rc_trigger_t* self);

/*****************************************************************************\
| Values                                                                      |
\*****************************************************************************/

struct rc_value_t {
  /* The current value of the variable. */
  rc_memref_value_t value;

  /* The list of conditions to evaluate. */
  rc_condset_t* conditions;

  /* The memory references required by the variable. */
  rc_memref_t* memrefs;

  /* The name of the variable. */
  const char* name;

  /* The next variable in the chain. */
  rc_value_t* next;
};

RC_EXPORT int RC_CCONV rc_value_size(const char* memaddr);
RC_EXPORT rc_value_t* RC_CCONV rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
RC_EXPORT int32_t RC_CCONV rc_evaluate_value(rc_value_t* value, rc_peek_t peek, void* ud, lua_State* L);

/*****************************************************************************\
| Leaderboards                                                                |
\*****************************************************************************/

/* Return values for rc_evaluate_lboard. */
enum {
  RC_LBOARD_STATE_INACTIVE,  /* leaderboard is not being processed */
  RC_LBOARD_STATE_WAITING,   /* leaderboard cannot activate until the start condition has been false for at least one frame */
  RC_LBOARD_STATE_ACTIVE,    /* leaderboard is active and may start */
  RC_LBOARD_STATE_STARTED,   /* leaderboard attempt in progress */
  RC_LBOARD_STATE_CANCELED,  /* leaderboard attempt canceled */
  RC_LBOARD_STATE_TRIGGERED, /* leaderboard attempt complete, value should be submitted */
  RC_LBOARD_STATE_DISABLED   /* leaderboard cannot be processed at this time */
};

struct rc_lboard_t {
  rc_trigger_t start;
  rc_trigger_t submit;
  rc_trigger_t cancel;
  rc_value_t value;
  rc_value_t* progress;
  rc_memref_t* memrefs;

  uint8_t state;
};

RC_EXPORT int RC_CCONV rc_lboard_size(const char* memaddr);
RC_EXPORT rc_lboard_t* RC_CCONV rc_parse_lboard(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx);
RC_EXPORT int RC_CCONV rc_evaluate_lboard(rc_lboard_t* lboard, int32_t* value, rc_peek_t peek, void* peek_ud, lua_State* L);
RC_EXPORT void RC_CCONV rc_reset_lboard(rc_lboard_t* lboard);

/*****************************************************************************\
| Value formatting                                                            |
\*****************************************************************************/

/* Supported formats. */
enum {
  RC_FORMAT_FRAMES,
  RC_FORMAT_SECONDS,
  RC_FORMAT_CENTISECS,
  RC_FORMAT_SCORE,
  RC_FORMAT_VALUE,
  RC_FORMAT_MINUTES,
  RC_FORMAT_SECONDS_AS_MINUTES,
  RC_FORMAT_FLOAT1,
  RC_FORMAT_FLOAT2,
  RC_FORMAT_FLOAT3,
  RC_FORMAT_FLOAT4,
  RC_FORMAT_FLOAT5,
  RC_FORMAT_FLOAT6,
  RC_FORMAT_FIXED1,
  RC_FORMAT_FIXED2,
  RC_FORMAT_FIXED3,
  RC_FORMAT_TENS,
  RC_FORMAT_HUNDREDS,
  RC_FORMAT_THOUSANDS,
  RC_FORMAT_UNSIGNED_VALUE
};

RC_EXPORT int RC_CCONV rc_parse_format(const char* format_str);
RC_EXPORT int RC_CCONV rc_format_value(char* buffer, int size, int32_t value, int format);

/*****************************************************************************\
| Rich Presence                                                               |
\*****************************************************************************/

typedef struct rc_richpresence_lookup_item_t rc_richpresence_lookup_item_t;

struct rc_richpresence_lookup_item_t {
  uint32_t first;
  uint32_t last;
  rc_richpresence_lookup_item_t* left;
  rc_richpresence_lookup_item_t* right;
  const char* label;
};

typedef struct rc_richpresence_lookup_t rc_richpresence_lookup_t;

struct rc_richpresence_lookup_t {
  rc_richpresence_lookup_item_t* root;
  rc_richpresence_lookup_t* next;
  const char* name;
  const char* default_label;
  uint8_t format;
};

typedef struct rc_richpresence_display_part_t rc_richpresence_display_part_t;

struct rc_richpresence_display_part_t {
  rc_richpresence_display_part_t* next;
  const char* text;
  rc_richpresence_lookup_t* lookup;
  rc_memref_value_t *value;
  uint8_t display_type;
};

typedef struct rc_richpresence_display_t rc_richpresence_display_t;

struct rc_richpresence_display_t {
  rc_trigger_t trigger;
  rc_richpresence_display_t* next;
  rc_richpresence_display_part_t* display;
};

struct rc_richpresence_t {
  rc_richpresence_display_t* first_display;
  rc_richpresence_lookup_t* first_lookup;
  rc_memref_t* memrefs;
  rc_value_t* variables;
};

RC_EXPORT int RC_CCONV rc_richpresence_size(const char* script);
RC_EXPORT int RC_CCONV rc_richpresence_size_lines(const char* script, int* lines_read);
RC_EXPORT rc_richpresence_t* RC_CCONV rc_parse_richpresence(void* buffer, const char* script, lua_State* L, int funcs_ndx);
RC_EXPORT int RC_CCONV rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, lua_State* L);
RC_EXPORT void RC_CCONV rc_update_richpresence(rc_richpresence_t* richpresence, rc_peek_t peek, void* peek_ud, lua_State* L);
RC_EXPORT int RC_CCONV rc_get_richpresence_display_string(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, lua_State* L);
RC_EXPORT void RC_CCONV rc_reset_richpresence(rc_richpresence_t* self);

RC_END_C_DECLS

#endif /* RC_RUNTIME_TYPES_H */
