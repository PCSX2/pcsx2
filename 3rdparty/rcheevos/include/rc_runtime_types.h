#ifndef RC_RUNTIME_TYPES_H
#define RC_RUNTIME_TYPES_H

#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>

RC_BEGIN_C_DECLS

#ifndef RC_RUNTIME_H /* prevents pedantic redefiniton error */

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
typedef uint32_t(RC_CCONV* rc_peek_t)(uint32_t address, uint32_t num_bytes, void* ud);

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
  RC_MEMSIZE_DOUBLE32,
  RC_MEMSIZE_DOUBLE32_BE,
  RC_MEMSIZE_VARIABLE
};

typedef struct rc_memref_value_t {
  /* The current value of this memory reference. */
  uint32_t value;
  /* The last differing value of this memory reference. */
  uint32_t prior;

  /* The size of the value. (RC_MEMSIZE_*) */
  uint8_t size;
  /* True if the value changed this frame. */
  uint8_t changed;
  /* The value type of the value. (RC_VALUE_TYPE_*) */
  uint8_t type;
  /* The type of memref (RC_MEMREF_TYPE_*) */
  uint8_t memref_type;
}
rc_memref_value_t;

struct rc_memref_t {
  /* The current value at the specified memory address. */
  rc_memref_value_t value;

  /* The memory address of this variable. */
  uint32_t address;
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
  RC_OPERAND_FUNC,           /* A function that provides the value. */
  RC_OPERAND_PRIOR,          /* The last differing value at this address. */
  RC_OPERAND_BCD,            /* The BCD-decoded value of a live address in RAM. */
  RC_OPERAND_INVERTED,       /* The twos-complement value of a live address in RAM. */
  RC_OPERAND_RECALL          /* The value captured by the last RC_CONDITION_REMEMBER condition */
};

typedef struct rc_operand_t {
  union {
    /* A value read from memory. */
    rc_memref_t* memref;

    /* An integer value. */
    uint32_t num;

    /* A floating point value. */
    double dbl;
  } value;

  /* specifies which member of the value union is being used (RC_OPERAND_*) */
  uint8_t type;

  /* the RC_MEMSIZE of the operand specified in the condition definition - memref.size may differ */
  uint8_t size;

  /* specifies how to read the memref for some types (RC_OPERAND_*) */
  uint8_t memref_access_type;

  /* if set, this operand is combining the current condition with the previous one */
  uint8_t is_combining;
}
rc_operand_t;

RC_EXPORT int RC_CCONV rc_operand_is_memref(const rc_operand_t* operand);

/*****************************************************************************\
| Conditions                                                                  |
\*****************************************************************************/

/* types */
enum {
  RC_CONDITION_STANDARD, /* this should always be 0 */
  RC_CONDITION_PAUSE_IF,
  RC_CONDITION_RESET_IF,
  RC_CONDITION_MEASURED_IF,
  RC_CONDITION_TRIGGER,
  RC_CONDITION_MEASURED,
  RC_CONDITION_ADD_SOURCE,
  RC_CONDITION_SUB_SOURCE,
  RC_CONDITION_ADD_ADDRESS,
  RC_CONDITION_REMEMBER,
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
  RC_OPERATOR_XOR,
  RC_OPERATOR_MOD,
  RC_OPERATOR_ADD,
  RC_OPERATOR_SUB,

  RC_OPERATOR_SUB_PARENT, /* internal use */
  RC_OPERATOR_ADD_ACCUMULATOR, /* internal use */
  RC_OPERATOR_SUB_ACCUMULATOR, /* internal use */
  RC_OPERATOR_INDIRECT_READ /* internal use */
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

  /* Will be non-zero if the condition evaluated true on the last check.
   * - The lowest bit indicates whether the condition itself was true.
   * - The second lowest bit will only ever be set on ResetIf conditions.
   *   If set, it indicates that the condition was responsible for resetting the
   *   trigger. A reset clears all hit counts, so the condition may not appear to
   *   be true just from looking at it (in which case the lower bit will be 0).
   *   Also, the condition might have only met its required_hits target though
   *   an AddHits chain which will have also been reset.
   */
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

  /* The first condition in this condition set. Then follow ->next chain. */
  rc_condition_t* conditions;

  /* The number of pause conditions in this condition set. */
  /* The first pause condition is at "this + RC_ALIGN(sizeof(this)). */
  uint16_t num_pause_conditions;

  /* The number of reset conditions in this condition set. */
  uint16_t num_reset_conditions;

  /* The number of hittarget conditions in this condition set. */
  uint16_t num_hittarget_conditions;

  /* The number of non-hittarget measured conditions in this condition set. */
  uint16_t num_measured_conditions;

  /* The number of other conditions in this condition set. */
  uint16_t num_other_conditions;

  /* The number of indirect conditions in this condition set. */
  uint16_t num_indirect_conditions;

  /* True if any condition in the set is a pause condition. */
  uint8_t has_pause; /* DEPRECATED - just check num_pause_conditions != 0 */
  /* True if the set is currently paused. */
  uint8_t is_paused;
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

  /* The current state of the MEASURED condition. */
  uint32_t measured_value;

  /* The target state of the MEASURED condition */
  uint32_t measured_target;

  /* The current state of the trigger */
  uint8_t state;

  /* True if at least one condition has a non-zero hit count */
  uint8_t has_hits;

  /* True if the measured value should be displayed as a percentage */
  uint8_t measured_as_percent;

  /* True if the trigger has its own rc_memrefs_t */
  uint8_t has_memrefs;
};

RC_EXPORT int RC_CCONV rc_trigger_size(const char* memaddr);
RC_EXPORT rc_trigger_t* RC_CCONV rc_parse_trigger(void* buffer, const char* memaddr, void* unused_L, int unused_funcs_idx);
RC_EXPORT int RC_CCONV rc_evaluate_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, void* unused_L);
RC_EXPORT int RC_CCONV rc_test_trigger(rc_trigger_t* trigger, rc_peek_t peek, void* ud, void* unused_L);
RC_EXPORT void RC_CCONV rc_reset_trigger(rc_trigger_t* self);

/*****************************************************************************\
| Values                                                                      |
\*****************************************************************************/

#define RC_VALUE_MAX_NAME_LENGTH 15

struct rc_value_t {
  /* The current value of the variable. */
  rc_memref_value_t value;

  /* True if the value has its own rc_memrefs_t */
  uint8_t has_memrefs;

  /* The list of possible values (traverse next chain, pick max). */
  rc_condset_t* conditions;

  /* The name of the variable. */
  const char* name;

  /* The next variable in the chain. */
  rc_value_t* next;
};

RC_EXPORT int RC_CCONV rc_value_size(const char* memaddr);
RC_EXPORT rc_value_t* RC_CCONV rc_parse_value(void* buffer, const char* memaddr, void* unused_L, int unused_funcs_idx);
RC_EXPORT int32_t RC_CCONV rc_evaluate_value(rc_value_t* value, rc_peek_t peek, void* ud, void* unused_L);

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

  uint8_t state;
  uint8_t has_memrefs;
};

RC_EXPORT int RC_CCONV rc_lboard_size(const char* memaddr);
RC_EXPORT rc_lboard_t* RC_CCONV rc_parse_lboard(void* buffer, const char* memaddr, void* unused_L, int unused_funcs_idx);
RC_EXPORT int RC_CCONV rc_evaluate_lboard(rc_lboard_t* lboard, int32_t* value, rc_peek_t peek, void* peek_ud, void* unused_L);
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
  RC_FORMAT_UNSIGNED_VALUE,
  RC_FORMAT_UNFORMATTED
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
  rc_operand_t value;
  uint8_t display_type;
};

typedef struct rc_richpresence_display_t rc_richpresence_display_t;

struct rc_richpresence_display_t {
  rc_trigger_t trigger;
  rc_richpresence_display_t* next;
  rc_richpresence_display_part_t* display;
  uint8_t has_required_hits;
};

struct rc_richpresence_t {
  rc_richpresence_display_t* first_display;
  rc_richpresence_lookup_t* first_lookup;
  rc_value_t* values;
  uint8_t has_memrefs;
};

RC_EXPORT int RC_CCONV rc_richpresence_size(const char* script);
RC_EXPORT int RC_CCONV rc_richpresence_size_lines(const char* script, int* lines_read);
RC_EXPORT rc_richpresence_t* RC_CCONV rc_parse_richpresence(void* buffer, const char* script, void* unused_L, int unused_funcs_idx);
RC_EXPORT int RC_CCONV rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, void* unused_L);
RC_EXPORT void RC_CCONV rc_update_richpresence(rc_richpresence_t* richpresence, rc_peek_t peek, void* peek_ud, void* unused_L);
RC_EXPORT int RC_CCONV rc_get_richpresence_display_string(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, void* unused_L);
RC_EXPORT void RC_CCONV rc_reset_richpresence(rc_richpresence_t* self);

RC_END_C_DECLS

#endif /* RC_RUNTIME_TYPES_H */
