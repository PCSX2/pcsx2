#include "rc_validate.h"

#include "rc_consoles.h"
#include "rc_internal.h"

#include "../rc_compat.h"

#include <stddef.h>
#include <stdlib.h>

enum
{
  RC_VALIDATION_ERR_NONE,

  /* sorted by severity - most severe first */

  /* errors that prevent the achievement from functioning */
  RC_VALIDATION_ERR_ADDRESS_OUT_OF_RANGE,
  RC_VALIDATION_ERR_RECALL_WITHOUT_REMEMBER,
  RC_VALIDATION_ERR_RECALL_BEFORE_REMEMBER,
  RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_WITH_MAX,
  RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_INTEGER_TO_FLOAT,
  RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE,
  RC_VALIDATION_ERR_CONFLICTING_CONDITION,

  /* warnings about logic that does nothing */
  RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_INTEGER_TO_FLOAT,
  RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX,
  RC_VALIDATION_ERR_TRAILING_CHAINING_CONDITION,
  RC_VALIDATION_ERR_MEASUREDIF_WITHOUT_MEASURED,
  RC_VALIDATION_ERR_ADDHITS_WITHOUT_TARGET,

  /* warnings about pointer math */
  RC_VALIDATION_ERR_POINTER_FROM_PREVIOUS_FRAME,
  RC_VALIDATION_ERR_POINTER_NON_INTEGER_OFFSET,
  RC_VALIDATION_ERR_POINTER_TRANSFORMED_OFFSET,

  /* warnings about potential logic errors */
  RC_VALIDATION_ERR_COMPARING_DIFFERENT_MEMORY_SIZES,
  RC_VALIDATION_ERR_MASK_RESULT_ALWAYS_ZERO,

  /* warnings that some areas of memory should be avoided */
  RC_VALIDATION_ERR_KERNAL_RAM_REQUIRES_BIOS,
  RC_VALIDATION_ERR_VIRTUAL_RAM_MAY_NOT_BE_EXPOSED,

  /* warnings about redundant logic */
  RC_VALIDATION_ERR_REDUNDANT_CONDITION,
  RC_VALIDATION_ERR_NO_HITS_TO_RESET,
  RC_VALIDATION_ERR_RESET_HIT_TARGET_OF_ONE,
  RC_VALIDATION_ERR_MASK_TOO_LARGE,

  RC_VALIDATION_ERR_COUNT
};

enum
{
  RC_VALIDATION_VIRTUAL_RAM_OTHER,
  RC_VALIDATION_VIRTUAL_RAM_MIRROR,
  RC_VALIDATION_VIRTUAL_RAM_ECHO
};

typedef struct rc_validation_error_t
{
  uint32_t err;
  uint16_t group_index;
  uint16_t cond_index;
  uint32_t data1;
  uint32_t data2;
} rc_validation_error_t;

typedef struct rc_validation_state_t
{
  rc_validation_error_t errors[64];
  uint32_t error_count;
  uint16_t group_index;
  uint16_t cond_index;
  uint32_t console_id;
  uint32_t max_address;
  uint8_t has_alt_groups;
  uint8_t has_hit_targets;
} rc_validation_state_t;

/* this returns a negative value if err1 is more severe than err2, or
 * a positive value is err2 is more severe than err1. */
static int rc_validation_compare_severity(const rc_validation_error_t* err1, const rc_validation_error_t* err2)
{
  /* lower err value is more severe */
  int diff = (err1->err - err2->err);
  if (diff != 0)
    return diff;

  /* lower group index is more severe */
  diff = (err1->group_index - err2->group_index);
  if (diff != 0)
    return diff;

  /* lower condition value is more severe */
  return (err1->cond_index - err2->cond_index);
}

static const rc_validation_error_t* rc_validate_find_most_severe_error(const rc_validation_state_t* state)
{
  const rc_validation_error_t* error = &state->errors[0];
  const rc_validation_error_t* most_severe_error = error;
  const rc_validation_error_t* stop = &state->errors[state->error_count];

  while (++error < stop) {
    if (rc_validation_compare_severity(error, most_severe_error) < 0)
      most_severe_error = error;
  }

  return most_severe_error;
}

static rc_validation_error_t* rc_validate_find_least_severe_error(rc_validation_state_t* state)
{
  rc_validation_error_t* error = &state->errors[0];
  rc_validation_error_t* least_severe_error = error;
  rc_validation_error_t* stop = &state->errors[state->error_count];

  while (++error < stop) {
    if (rc_validation_compare_severity(error, least_severe_error) > 0)
      least_severe_error = error;
  }

  return least_severe_error;
}

static size_t rc_validate_format_cond_index(char buffer[], size_t buffer_size, const rc_validation_state_t* state, uint32_t group_index, uint32_t cond_index)
{
  int written = 0;

  if (cond_index == 0)
    return 0;

  if (state->has_alt_groups) {
    if (group_index == 0)
      written = snprintf(buffer, buffer_size, "Core ");
    else
      written = snprintf(buffer, buffer_size, "Alt%u ", group_index);

    buffer += written;
    buffer_size -= written;
  }

  written += snprintf(buffer, buffer_size, "Condition %u", cond_index);
  return written;
}

static const char* rc_validate_get_condition_type_string(uint32_t type)
{
  switch (type) {
    case RC_CONDITION_ADD_ADDRESS: return "AddAddress";
    case RC_CONDITION_ADD_HITS: return "AddHits";
    case RC_CONDITION_ADD_SOURCE: return "AddSource";
    case RC_CONDITION_AND_NEXT: return "AndNext";
    case RC_CONDITION_MEASURED: return "Measured";
    case RC_CONDITION_MEASURED_IF: return "MeasuredIf";
    case RC_CONDITION_OR_NEXT: return "OrNext";
    case RC_CONDITION_PAUSE_IF: return "PauseIf";
    case RC_CONDITION_REMEMBER: return "Remember";
    case RC_CONDITION_RESET_IF: return "ResetIf";
    case RC_CONDITION_RESET_NEXT_IF: return "ResetNextIf";
    case RC_CONDITION_SUB_HITS: return "SubHits";
    case RC_CONDITION_SUB_SOURCE: return "SubSource";
    case RC_CONDITION_TRIGGER: return "Trigger";
    default: return "???";
  }
}

static void rc_validate_format_error_compare_int_to_float(char buffer[], size_t buffer_size, uint32_t encoded_data, const char* limiter)
{
  int written;
  rc_typed_value_t value;
  value.value.u32 = encoded_data;

  written = snprintf(buffer, buffer_size, "Comparison is %s true (integer can never be %.06f", limiter, value.value.f32);
  while (buffer[written - 1] == '0')
    written--;
  if (buffer[written] == '.')
    written++;
  buffer[written] = ')';
  buffer[written + 1] = '\0';
}

static int rc_validate_format_error(char buffer[], size_t buffer_size, const rc_validation_state_t* state, const rc_validation_error_t* error)
{
  size_t written = rc_validate_format_cond_index(buffer, buffer_size, state, error->group_index, error->cond_index);
  buffer += written;
  buffer_size -= written;

  if (buffer_size < 2)
    return 0;

  buffer_size -= 2;
  *buffer++ = ':';
  *buffer++ = ' ';

  switch (error->err) {
    case RC_VALIDATION_ERR_ADDHITS_WITHOUT_TARGET:
      snprintf(buffer, buffer_size, "Final condition in AddHits chain must have a hit target");
      break;

    case RC_VALIDATION_ERR_ADDRESS_OUT_OF_RANGE:
      snprintf(buffer, buffer_size, "Address %04X out of range (max %04X)", error->data1, error->data2);
      break;

    case RC_VALIDATION_ERR_COMPARING_DIFFERENT_MEMORY_SIZES:
      snprintf(buffer, buffer_size, "Comparing different memory sizes");
      break;

    case RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_INTEGER_TO_FLOAT:
      rc_validate_format_error_compare_int_to_float(buffer, buffer_size, error->data1, "always");
      break;

    case RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX:
      snprintf(buffer, buffer_size, "Comparison is always true (max %u)", error->data1);
      break;

    case RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE:
      snprintf(buffer, buffer_size, "Comparison is never true");
      break;

    case RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_INTEGER_TO_FLOAT:
      rc_validate_format_error_compare_int_to_float(buffer, buffer_size, error->data1, "never");
      break;

    case RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_WITH_MAX:
      snprintf(buffer, buffer_size, "Comparison is never true (max %u)", error->data1);
      break;

    case RC_VALIDATION_ERR_CONFLICTING_CONDITION:
      written = snprintf(buffer, buffer_size, "Conflicts with ");
      buffer += written;
      buffer_size -= written;
      rc_validate_format_cond_index(buffer, buffer_size, state, error->data1, error->data2);
      break;

    case RC_VALIDATION_ERR_KERNAL_RAM_REQUIRES_BIOS:
      snprintf(buffer, buffer_size, "Kernel RAM may not be initialized without real BIOS (address %04X)", error->data1);
      break;

    case RC_VALIDATION_ERR_MASK_RESULT_ALWAYS_ZERO:
      snprintf(buffer, buffer_size, "Result of mask is always 0");
      break;

    case RC_VALIDATION_ERR_MASK_TOO_LARGE:
      snprintf(buffer, buffer_size, "Mask has more bits than source");
      break;

    case RC_VALIDATION_ERR_MEASUREDIF_WITHOUT_MEASURED:
      snprintf(buffer, buffer_size, "MeasuredIf without Measured");
      break;

    case RC_VALIDATION_ERR_NO_HITS_TO_RESET:
      snprintf(buffer, buffer_size, "No captured hits to reset");
      break;

    case RC_VALIDATION_ERR_POINTER_FROM_PREVIOUS_FRAME:
      snprintf(buffer, buffer_size, "Using pointer from previous frame");
      break;

    case RC_VALIDATION_ERR_POINTER_NON_INTEGER_OFFSET:
      snprintf(buffer, buffer_size, "Using non-integer value in AddAddress calculation");
      break;

    case RC_VALIDATION_ERR_POINTER_TRANSFORMED_OFFSET:
      snprintf(buffer, buffer_size, "Using transformed value in AddAddress calculation");
      break;

    case RC_VALIDATION_ERR_RECALL_BEFORE_REMEMBER:
      if (error->data1 == RC_CONDITION_PAUSE_IF)
        snprintf(buffer, buffer_size, "PauseIf cannot use Remembered value not associated to PauseIf chain");
      else
        snprintf(buffer, buffer_size, "Recall used before Remember");
      break;

    case RC_VALIDATION_ERR_RECALL_WITHOUT_REMEMBER:
      snprintf(buffer, buffer_size, "Recall used without Remember");
      break;

    case RC_VALIDATION_ERR_RESET_HIT_TARGET_OF_ONE:
      snprintf(buffer, buffer_size, "Hit target of 1 is redundant on ResetIf");
      break;

    case RC_VALIDATION_ERR_REDUNDANT_CONDITION:
      written = snprintf(buffer, buffer_size, "Redundant with ");
      buffer += written;
      buffer_size -= written;
      rc_validate_format_cond_index(buffer, buffer_size, state, error->data1, error->data2);
      break;

    case RC_VALIDATION_ERR_TRAILING_CHAINING_CONDITION:
      snprintf(buffer, buffer_size, "%s condition type expects another condition to follow", rc_validate_get_condition_type_string(error->data1));
      break;

    case RC_VALIDATION_ERR_VIRTUAL_RAM_MAY_NOT_BE_EXPOSED:
      snprintf(buffer, buffer_size, "%s RAM may not be exposed by emulator (address %04X)",
        error->data2 == RC_VALIDATION_VIRTUAL_RAM_MIRROR ? "Mirror" :
          error->data2 == RC_VALIDATION_VIRTUAL_RAM_ECHO ? "Echo" : "Virtual",
        error->data1);
      break;
  }

  return 0;
}

static void rc_validate_add_error(rc_validation_state_t* state, uint32_t error_code, uint32_t data1, uint32_t data2)
{
  rc_validation_error_t* error;
  if (state->error_count == sizeof(state->errors) / sizeof(state->errors[0]))
    error = rc_validate_find_least_severe_error(state);
  else
    error = &state->errors[state->error_count++];

  error->err = error_code;
  error->group_index = state->group_index;
  error->cond_index = state->cond_index;
  error->data1 = data1;
  error->data2 = data2;
}

/* rc_condition_is_combining doesn't look at conditions that build a memref (like AddSource) */
static int rc_validate_is_combining_condition(const rc_condition_t* condition)
{
  switch (condition->type)
  {
    case RC_CONDITION_ADD_ADDRESS:
    case RC_CONDITION_ADD_HITS:
    case RC_CONDITION_ADD_SOURCE:
    case RC_CONDITION_AND_NEXT:
    case RC_CONDITION_OR_NEXT:
    case RC_CONDITION_RESET_NEXT_IF:
    case RC_CONDITION_SUB_HITS:
    case RC_CONDITION_SUB_SOURCE:
    case RC_CONDITION_REMEMBER:
      return 1;

    default:
      return 0;
  }
}

static const rc_condition_t* rc_validate_get_next_non_combining_condition(const rc_condition_t* cond)
{
  while (cond && rc_validate_is_combining_condition(cond))
    cond = cond->next;

  return cond;
}

static int rc_validate_has_condition(const rc_condition_t* cond, uint8_t type)
{
  for (; cond; cond = cond->next) {
    if (cond->type == type)
      return 1;
  }

  return 0;
}

static int rc_validate_is_invalid_recall(const rc_operand_t* operand)
{
  /* if operand is {recall}, but memref is null, that means the Remember wasn't found */
  return rc_operand_is_recall(operand) &&
    rc_operand_type_is_memref(operand->memref_access_type) &&
    !operand->value.memref;
}

static void rc_validate_memref(const rc_memref_t* memref, rc_validation_state_t* state)
{
  if (memref->address > state->max_address) {
    rc_validate_add_error(state, RC_VALIDATION_ERR_ADDRESS_OUT_OF_RANGE, memref->address, state->max_address);
    return;
  }

  switch (state->console_id) {
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
      if (memref->address >= 0x0800 && memref->address <= 0x1FFF)
        rc_validate_add_error(state, RC_VALIDATION_ERR_VIRTUAL_RAM_MAY_NOT_BE_EXPOSED, memref->address, RC_VALIDATION_VIRTUAL_RAM_MIRROR);
      break;

    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_COLOR:
      if (memref->address >= 0xE000 && memref->address <= 0xFDFF)
        rc_validate_add_error(state, RC_VALIDATION_ERR_VIRTUAL_RAM_MAY_NOT_BE_EXPOSED, memref->address, RC_VALIDATION_VIRTUAL_RAM_ECHO);
      break;

    case RC_CONSOLE_PLAYSTATION:
      if (memref->address <= 0xFFFF)
        rc_validate_add_error(state, RC_VALIDATION_ERR_KERNAL_RAM_REQUIRES_BIOS, memref->address, 0);
      break;
  }
}

static uint32_t rc_console_max_address(uint32_t console_id)
{
  const rc_memory_regions_t* memory_regions;
  memory_regions = rc_console_memory_regions(console_id);
  if (memory_regions && memory_regions->num_regions > 0)
    return memory_regions->region[memory_regions->num_regions - 1].end_address;

  return 0xFFFFFFFF;
}

int rc_validate_memrefs_for_console(const rc_memrefs_t* memrefs, char result[], const size_t result_size, uint32_t console_id)
{
  const rc_memref_list_t* memref_list = &memrefs->memrefs;

  rc_validation_state_t state;
  memset(&state, 0, sizeof(state));
  state.console_id = console_id;
  state.max_address = rc_console_max_address(console_id);

  result[0] = '\0';
  do
  {
    const rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_stop = memref + memref_list->count;
    for (; memref < memref_stop; ++memref)
    {
      rc_validate_memref(memref, &state);
      if (state.error_count) {
        rc_validate_format_error(result, result_size, &state, &state.errors[0]);
        return 0;
      }
    }

    memref_list = memref_list->next;
  } while (memref_list);

  return 1;
}

static uint32_t rc_max_value(const rc_operand_t* operand)
{
  if (operand->type == RC_OPERAND_CONST)
    return operand->value.num;

  if (!rc_operand_is_memref(operand))
    return 0xFFFFFFFF;

  switch (operand->size) {
    case RC_MEMSIZE_BIT_0:
    case RC_MEMSIZE_BIT_1:
    case RC_MEMSIZE_BIT_2:
    case RC_MEMSIZE_BIT_3:
    case RC_MEMSIZE_BIT_4:
    case RC_MEMSIZE_BIT_5:
    case RC_MEMSIZE_BIT_6:
    case RC_MEMSIZE_BIT_7:
      return 1;

    case RC_MEMSIZE_LOW:
    case RC_MEMSIZE_HIGH:
      return 0xF;

    case RC_MEMSIZE_BITCOUNT:
      return 8;

    case RC_MEMSIZE_8_BITS:
      /* NOTE: BCD should max out at 0x99, but because each digit can be 15, it actually maxes at 15*10 + 15 */
      return (operand->type == RC_OPERAND_BCD) ? 165 : 0xFF;

    case RC_MEMSIZE_16_BITS:
    case RC_MEMSIZE_16_BITS_BE:
      return (operand->type == RC_OPERAND_BCD) ? 16665 : 0xFFFF;

    case RC_MEMSIZE_24_BITS:
    case RC_MEMSIZE_24_BITS_BE:
      return (operand->type == RC_OPERAND_BCD) ? 1666665 : 0xFFFFFF;

    default:
      return (operand->type == RC_OPERAND_BCD) ? 166666665 : 0xFFFFFFFF;
  }
}

static void rc_combine_ranges(uint32_t* min_val, uint32_t* max_val, uint8_t oper, uint32_t oper_min_val, uint32_t oper_max_val)
{
  switch (oper) {
    case RC_OPERATOR_MULT:
    {
      unsigned long long scaled = ((unsigned long long)*min_val) * oper_min_val;
      *min_val = (scaled > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)scaled;

      scaled = ((unsigned long long)*max_val) * oper_max_val;
      *max_val = (scaled > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)scaled;
      break;
    }

    case RC_OPERATOR_DIV:
      *min_val = (oper_max_val == 0) ? *min_val : (*min_val / oper_max_val);
      *max_val = (oper_min_val == 0) ? *max_val : (*max_val / oper_min_val);
      break;

    case RC_OPERATOR_AND:
      *min_val = 0;
      *max_val &= oper_max_val;
      break;

    case RC_OPERATOR_XOR:
      *min_val = 0;
      *max_val |= oper_max_val;
      break;

    case RC_OPERATOR_MOD:
      *min_val = 0;
      *max_val = (*max_val >= oper_max_val) ? oper_max_val - 1 : *max_val;
      break;

    case RC_OPERATOR_ADD:
    case RC_OPERATOR_ADD_ACCUMULATOR:
      if (*min_val > *max_val) { /* underflow occurred */
        *max_val += oper_max_val;
      }
      else {
        unsigned long long scaled = ((unsigned long long)*max_val) + oper_max_val;
        *max_val = (scaled > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)scaled;
      }

      *min_val += oper_min_val;
      break;

    case RC_OPERATOR_SUB:
    case RC_OPERATOR_SUB_ACCUMULATOR:
      *min_val -= oper_max_val;
      *max_val -= oper_min_val;
      break;

    case RC_OPERATOR_SUB_PARENT:
    {
      uint32_t temp = oper_min_val - *max_val;
      *max_val = oper_max_val - *min_val;
      *min_val = temp;
      break;
    }

    default:
      break;
  }
}

static void rc_chain_get_value_range(const rc_operand_t* operand, uint32_t* min_val, uint32_t* max_val)
{
  if (rc_operand_is_memref(operand) && operand->value.memref->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
    const rc_modified_memref_t* modified_memref = (const rc_modified_memref_t*)operand->value.memref;
    if (modified_memref->modifier_type != RC_OPERATOR_INDIRECT_READ) {
      if (modified_memref->modifier_type == RC_OPERATOR_DIV &&
          rc_operand_is_memref(&modified_memref->modifier) &&
          rc_operands_are_equal(&modified_memref->modifier, &modified_memref->parent)) {
        /* division by self can only return 0 or 1. */
        *min_val = 0;
        *max_val = 1;
      }
      else {
        uint32_t modifier_min_val, modifier_max_val;
        rc_chain_get_value_range(&modified_memref->parent, min_val, max_val);
        rc_chain_get_value_range(&modified_memref->modifier, &modifier_min_val, &modifier_max_val);
        rc_combine_ranges(min_val, max_val, modified_memref->modifier_type, modifier_min_val, modifier_max_val);
      }
      return;
    }
  }

  *min_val = (operand->type == RC_OPERAND_CONST) ? operand->value.num : 0;
  *max_val = rc_max_value(operand);
}

static int rc_validate_get_condition_index(const rc_condset_t* condset, const rc_condition_t* condition)
{
   int index = 1;
   const rc_condition_t* scan;
   for (scan = condset->conditions; scan != NULL; scan = scan->next)
   {
     if (scan == condition)
       return index;

     ++index;
   }

   return 0;
}

static void rc_validate_range(uint32_t min_val, uint32_t max_val, char oper, uint32_t max, rc_validation_state_t* state)
{
  switch (oper) {
    case RC_OPERATOR_AND:
      if (min_val > max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_MASK_TOO_LARGE, 0, 0);
      else if (min_val == 0 && max_val == 0)
        rc_validate_add_error(state, RC_VALIDATION_ERR_MASK_RESULT_ALWAYS_ZERO, 0, 0);
      break;

    case RC_OPERATOR_EQ:
      if (min_val > max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_WITH_MAX, max, 0);
      break;

    case RC_OPERATOR_NE:
      if (min_val > max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX, max, 0);
      break;

    case RC_OPERATOR_GE:
      if (min_val > max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_WITH_MAX, max, 0);
      else if (max_val == 0)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX, max, 0);
      break;

    case RC_OPERATOR_GT:
      if (min_val >= max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_WITH_MAX, max, 0);
      break;

    case RC_OPERATOR_LE:
      if (min_val >= max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX, max, 0);
      break;

    case RC_OPERATOR_LT:
      if (min_val > max)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_WITH_MAX, max, 0);
      else if (max_val == 0)
        rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE, 0, 0);
      break;
  }
}

static int rc_validate_condset_internal(const rc_condset_t* condset, rc_validation_state_t* state)
{
  const rc_condition_t* cond;
  int in_add_hits = 0;
  int in_add_address = 0;
  int is_combining = 0;
  int has_measured = 0;
  int measuredif_index = -1;
  uint32_t errors_before = state->error_count;

  if (!condset)
    return 1;

  state->cond_index = 1;

  for (cond = condset->conditions; cond; cond = cond->next, ++state->cond_index) {
    /* validate the original operands first */
    const rc_operand_t* operand1 = rc_condition_get_real_operand1(cond);
    int is_memref1 = rc_operand_is_memref(operand1);
    const int is_memref2 = rc_operand_is_memref(&cond->operand2);

    if (!in_add_address) {
      if (is_memref1)
        rc_validate_memref(operand1->value.memref, state);
      if (is_memref2)
        rc_validate_memref(cond->operand2.value.memref, state);
    }
    else {
      in_add_address = 0;
    }

    /* if operand is {recall}, but memref is null, that means the Remember wasn't found */
    if (rc_validate_is_invalid_recall(operand1) || rc_validate_is_invalid_recall(&cond->operand2)) {
      if (!rc_validate_has_condition(condset->conditions, RC_CONDITION_REMEMBER)) {
        rc_validate_add_error(state, RC_VALIDATION_ERR_RECALL_WITHOUT_REMEMBER, 0, 0);
      }
      else {
        const rc_condition_t* next_cond = rc_validate_get_next_non_combining_condition(cond);
        const uint8_t next_cond_type = next_cond ? next_cond->type : RC_CONDITION_STANDARD;
        rc_validate_add_error(state, RC_VALIDATION_ERR_RECALL_BEFORE_REMEMBER, next_cond_type, 0);
      }
    }

    switch (cond->type) {
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_REMEMBER:
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        if (operand1->type == RC_OPERAND_DELTA || operand1->type == RC_OPERAND_PRIOR)
          rc_validate_add_error(state, RC_VALIDATION_ERR_POINTER_FROM_PREVIOUS_FRAME, 0, 0);
        else if (rc_operand_is_float(operand1) || rc_operand_is_float(&cond->operand2))
          rc_validate_add_error(state, RC_VALIDATION_ERR_POINTER_NON_INTEGER_OFFSET, 0, 0);
        else if (rc_operand_type_is_transform(operand1->type) && cond->oper != RC_OPERATOR_MULT)
          rc_validate_add_error(state, RC_VALIDATION_ERR_POINTER_TRANSFORMED_OFFSET, 0, 0);

        in_add_address = 1;
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_SUB_HITS:
        in_add_hits = 1;
        is_combining = 1;
        break;

      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_OR_NEXT:
      case RC_CONDITION_RESET_NEXT_IF:
        is_combining = 1;
        break;

      case RC_CONDITION_RESET_IF:
        if (in_add_hits) {
          /* ResetIf at the end of a hit chain does not require a hit target.
           * It's meant to reset things if some subset of conditions have been true. */
          in_add_hits = 0;
          is_combining = 0;
          break;
        }
        if (!state->has_hit_targets)
          rc_validate_add_error(state, RC_VALIDATION_ERR_NO_HITS_TO_RESET, 0, 0);
        else if (cond->required_hits == 1)
          rc_validate_add_error(state, RC_VALIDATION_ERR_RESET_HIT_TARGET_OF_ONE, 0, 0);
        /* fallthrough */ /* to default */
      default:
        if (in_add_hits) {
          if (cond->required_hits == 0)
            rc_validate_add_error(state, RC_VALIDATION_ERR_ADDHITS_WITHOUT_TARGET, 0, 0);

          in_add_hits = 0;
        }

        has_measured |= (cond->type == RC_CONDITION_MEASURED);
        if (cond->type == RC_CONDITION_MEASURED_IF && measuredif_index == -1)
          measuredif_index = state->cond_index;

        is_combining = 0;
        break;
    }

    /* original operands are valid. now switch to the derived operands for logic
     * combining/comparing them */
    operand1 = &cond->operand1;
    is_memref1 = rc_operand_is_memref(operand1);

    /* check for comparing two differently sized memrefs */
    if (is_memref1 && is_memref2 &&
        operand1->value.memref->value.memref_type == RC_MEMREF_TYPE_MEMREF &&
        cond->operand2.value.memref->value.memref_type == RC_MEMREF_TYPE_MEMREF &&
        rc_max_value(operand1) != rc_max_value(&cond->operand2)) {
      rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARING_DIFFERENT_MEMORY_SIZES, 0, 0);
    }

    if (is_memref1 && rc_operand_is_float(operand1)) {
      /* if left side is a float, right side will be converted to a float, so don't do range validation */
    }
    else if (is_memref1 || is_memref2) {
      /* if either side is a memref, check for impossible comparisons */
      const rc_operand_t* operand2 = &cond->operand2;
      uint8_t oper = cond->oper;
      uint32_t min, max;
      uint32_t max_val = rc_max_value(operand2);
      uint32_t min_val;
      rc_typed_value_t typed_value;

      rc_chain_get_value_range(operand1, &min, &max);
      if (min > max) { /* underflow */
        min = 0;
        max = 0xFFFFFFFF;
      }

      if (!is_memref1) {
        /* pretend constant was on right side */
        operand2 = operand1;
        operand1 = &cond->operand2;
        max_val = max;
        max = rc_max_value(&cond->operand2);

        switch (oper) {
          case RC_OPERATOR_LT: oper = RC_OPERATOR_GT; break;
          case RC_OPERATOR_LE: oper = RC_OPERATOR_GE; break;
          case RC_OPERATOR_GT: oper = RC_OPERATOR_LT; break;
          case RC_OPERATOR_GE: oper = RC_OPERATOR_LE; break;
        }
      }

      switch (operand2->type) {
        case RC_OPERAND_CONST:
          min_val = operand2->value.num;
          break;

        case RC_OPERAND_FP:
          min_val = (int)operand2->value.dbl;

          /* cannot compare an integer memory reference to a non-integral floating point value */
          if (!rc_operand_is_float_memref(operand1) && (float)min_val != operand2->value.dbl) {
            switch (oper) {
              case RC_OPERATOR_EQ:
                typed_value.value.f32 = (float)operand2->value.dbl;
                rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_NEVER_TRUE_INTEGER_TO_FLOAT, typed_value.value.u32, 0);
                break;

              case RC_OPERATOR_NE:
                typed_value.value.f32 = (float)operand2->value.dbl;
                rc_validate_add_error(state, RC_VALIDATION_ERR_COMPARISON_ALWAYS_TRUE_INTEGER_TO_FLOAT, typed_value.value.u32, 0);
                break;

              case RC_OPERATOR_GT: /* value could be greater than floor(float) */
              case RC_OPERATOR_LE: /* value could be less than or equal to floor(float) */
                break;

              case RC_OPERATOR_GE: /* value could be greater than or equal to ceil(float) */
              case RC_OPERATOR_LT: /* value could be less than ceil(float) */
                ++min_val;
                break;
            }
          }

          break;

        default: /* right side is memref or add source chain */
          min_val = 0;
          break;
      }

      /* min_val and max_val are the range allowed by operand2. max is the upper value from operand1. */
      rc_validate_range(min_val, max_val, oper, max, state);
    }
  }

  if (is_combining) {
    /* find the final condition so we can extract the type */
    state->cond_index--;
    cond = condset->conditions;
    while (cond->next)
      cond = cond->next;

    rc_validate_add_error(state, RC_VALIDATION_ERR_TRAILING_CHAINING_CONDITION, cond->type, 0);
  }

  if (measuredif_index != -1 && !has_measured) {
    state->cond_index = measuredif_index;
    rc_validate_add_error(state, RC_VALIDATION_ERR_MEASUREDIF_WITHOUT_MEASURED, 0, 0);
  }

  return (state->error_count == errors_before);
}

static int rc_condset_has_hittargets(const rc_condset_t* condset)
{
  if (condset->num_hittarget_conditions > 0)
    return 1;

  /* pause and reset conditions may have hittargets and won't be classified as hittarget conditions.
   * measured conditions may also have hittargets.
   * other conditions may have hittarget conditions in an AndNext/OrNext chain.
   * basically, check everything other than hittarget (explicitly known) and indirect (cannot have).
   */
  if (condset->num_pause_conditions || condset->num_reset_conditions || condset->num_measured_conditions || condset->num_other_conditions) {
    const rc_condition_t* condition = rc_condset_get_conditions((rc_condset_t*)condset);
    /* ASSERT: don't need to add num_hittarget_conditions because it must be 0 per earlier check */
    const rc_condition_t* stop = condition + condset->num_pause_conditions
        + condset->num_reset_conditions + condset->num_measured_conditions
        + condset->num_other_conditions;
    for (; condition < stop; ++condition) {
      if (condition->required_hits)
        return 1;
    }
  }

  return 0;
}

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t max_address)
{
  rc_validation_state_t state;
  memset(&state, 0, sizeof(state));
  state.has_hit_targets = rc_condset_has_hittargets(condset);
  state.max_address = max_address;

  result[0] = '\0';
  if (rc_validate_condset_internal(condset, &state)) {
    const rc_validation_error_t* most_severe_error = rc_validate_find_most_severe_error(&state);
    return rc_validate_format_error(result, result_size, &state, most_severe_error);
  }

  return 1;
}

int rc_validate_condset_for_console(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t console_id)
{
  rc_validation_state_t state;
  memset(&state, 0, sizeof(state));
  state.console_id = console_id;
  state.max_address = rc_console_max_address(console_id);
  state.has_hit_targets = rc_condset_has_hittargets(condset);

  result[0] = '\0';
  if (rc_validate_condset_internal(condset, &state)) {
    const rc_validation_error_t* most_severe_error = rc_validate_find_most_severe_error(&state);
    return rc_validate_format_error(result, result_size, &state, most_severe_error);
  }

  return 1;
}

static int rc_validate_get_opposite_comparison(int oper)
{
  switch (oper)
  {
    case RC_OPERATOR_EQ: return RC_OPERATOR_NE;
    case RC_OPERATOR_NE: return RC_OPERATOR_EQ;
    case RC_OPERATOR_LT: return RC_OPERATOR_GE;
    case RC_OPERATOR_LE: return RC_OPERATOR_GT;
    case RC_OPERATOR_GT: return RC_OPERATOR_LE;
    case RC_OPERATOR_GE: return RC_OPERATOR_LT;
    default: return oper;
  }
}

static const rc_operand_t* rc_validate_get_comparison(const rc_condition_t* condition, int* comparison, unsigned* value)
{
  if (rc_operand_is_memref(&condition->operand1))
  {
    if (condition->operand2.type != RC_OPERAND_CONST)
      return NULL;

    *comparison = condition->oper;
    *value = condition->operand2.value.num;
    return &condition->operand1;
  }

  if (condition->operand1.type != RC_OPERAND_CONST)
    return NULL;

  if (!rc_operand_is_memref(&condition->operand2))
    return NULL;

  *comparison = rc_validate_get_opposite_comparison(condition->oper);
  *value = condition->operand1.value.num;
  return &condition->operand2;
}

enum {
  RC_OVERLAP_NONE = 0,
  RC_OVERLAP_CONFLICTING,
  RC_OVERLAP_REDUNDANT,
  RC_OVERLAP_DEFER
};

static int rc_validate_comparison_overlap(int comparison1, uint32_t value1, int comparison2, uint32_t value2)
{
  /* NOTE: this only cares if comp2 conflicts with comp1.
   * If comp1 conflicts with comp2, we'll catch that later (return RC_OVERLAP_NONE for now) */
  switch (comparison2)
  {
    case RC_OPERATOR_EQ:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a == 1 | a == 1 && a == 2 | a == 2 && a == 1 */
                              /*    redundant           conflict           conflict     */
           return (value1 == value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a == 1 | a <= 1 && a == 2 | a <= 2 && a == 1 */
                              /*      defer             conflict            defer       */
           return (value1 < value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a == 1 | a >= 1 && a == 2 | a >= 2 && a == 1 */
                              /*      defer              defer             conflict     */
           return (value1 > value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a == 1 | a != 1 && a == 2 | a != 2 && a == 1 */
                              /*     conflict            defer              defer       */
           return (value1 == value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_LT: /* a <  1 && a == 1 | a <  1 && a == 2 | a <  2 && a == 1 */
                              /*     conflict           conflict            defer       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a == 1 | a >  1 && a == 2 | a >  2 && a == 1 */
                              /*     conflict            defer            conflict      */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
      }
      break;

    case RC_OPERATOR_NE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a != 1 | a == 1 && a != 2 | a == 2 && a != 1 */
                              /*    conflict           redundant           redundant    */
           return (value1 == value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_REDUNDANT;
         case RC_OPERATOR_LE: /* a <= 1 && a != 1 | a <= 1 && a != 2 | a <= 2 && a != 1 */
                              /*       none            redundant             none       */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_GE: /* a >= 1 && a != 1 | a >= 1 && a != 2 | a >= 2 && a != 1 */
                              /*       none               none             redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a != 1 | a != 1 && a != 2 | a != 2 && a != 1 */
                              /*     redundant            none               none       */
           return (value1 == value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a != 1 | a <  1 && a != 2 | a <  2 && a != 1 */
                              /*     redundant          redundant           none        */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a != 1 | a >  1 && a != 2 | a >  2 && a != 1 */
                              /*     redundant           none             redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_LT:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a <  1 | a == 1 && a <  2 | a == 2 && a <  1 */
                              /*    conflict           redundant           conflict     */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a <  1 | a <= 1 && a <  2 | a <= 2 && a <  1 */
                              /*      defer            redundant            defer       */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a <  1 | a >= 1 && a <  2 | a >= 2 && a <  1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a <  1 | a != 1 && a <  2 | a != 2 && a <  1 */
                              /*      defer               none              defer       */
           return (value1 >= value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a <  1 | a <  1 && a <  2 | a <  2 && a <  1 */
                              /*     redundant          redundant           defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a <  1 | a >  1 && a <  2 | a >  2 && a <  1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_LE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a <= 1 | a == 1 && a <= 2 | a == 2 && a <= 1 */
                              /*    redundant           redundant          conflict     */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a <= 1 | a <= 1 && a <= 2 | a <= 2 && a <= 1 */
                              /*    redundant           redundant            defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a <= 1 | a >= 1 && a <= 2 | a >= 2 && a <= 1 */
                              /*       none               none             conflict     */
           return (value1 > value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a <= 1 | a != 1 && a <= 2 | a != 2 && a <= 1 */
                              /*       none               none              defer       */
           return (value1 > value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a <= 1 | a <  1 && a <= 2 | a <  2 && a <= 1 */
                              /*     redundant          redundant           defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a <= 1 | a >  1 && a <= 2 | a >  2 && a <= 1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_GT:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a >  1 | a == 1 && a >  2 | a == 2 && a >  1 */
                              /*     conflict           conflict          redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a >  1 | a <= 1 && a >  2 | a <= 2 && a >  1 */
                              /*     conflict           conflict            defer       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a >  1 | a >= 1 && a >  2 | a >= 2 && a >  1 */
                              /*      defer              defer            redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a >  1 | a != 1 && a >  2 | a != 2 && a >  1 */
                              /*      defer              defer               none       */
           return (value1 <= value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a >  1 | a <  1 && a >  2 | a <  2 && a >  1 */
                              /*     conflict           conflict             none       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a >  1 | a >  1 && a >  2 | a >  2 && a >  1 */
                              /*    redundant            defer            redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
      }
      break;

    case RC_OPERATOR_GE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a >= 1 | a == 1 && a >= 2 | a == 2 && a >= 1 */
                              /*    redundant           conflict          redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a >= 1 | a <= 1 && a >= 2 | a <= 2 && a >= 1 */
                              /*       none             conflict            none        */
           return (value1 < value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GE: /* a >= 1 && a >= 1 | a >= 1 && a >= 2 | a >= 2 && a >= 1 */
                              /*    redundant          redundant            defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a >= 1 | a != 1 && a >= 2 | a != 2 && a >= 1 */
                              /*       none              defer               none       */
           return (value1 < value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a >= 1 | a <  1 && a >= 2 | a <  2 && a >= 1 */
                              /*     conflict           conflict             none       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a >= 1 | a >  1 && a >= 2 | a >  2 && a >= 1 */
                              /*    redundant            defer            redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
      }
      break;
  }

  return RC_OVERLAP_NONE;
}

static int rc_validate_conflicting_conditions(const rc_condset_t* conditions, const rc_condset_t* compare_conditions,
    uint32_t group_index, rc_validation_state_t* state)
{
  int comparison1, comparison2;
  uint32_t value1, value2;
  const rc_operand_t* operand1;
  const rc_operand_t* operand2;
  const rc_condition_t* compare_condition;
  const rc_condition_t* condition;
  const rc_condition_t* condition_chain_start;
  uint32_t errors_before = state->error_count;
  int overlap;
  int chain_matches;

  /* empty group */
  if (conditions == NULL || compare_conditions == NULL)
    return 1;

  /* outer loop is the source conditions */
  for (condition = conditions->conditions; condition != NULL; condition = condition->next, ++state->cond_index) {
    condition_chain_start = condition;
    while (condition && rc_condition_is_combining(condition)) {
      condition = condition->next;
    }
    if (!condition)
      break;

    /* hits can be captured at any time, so any potential conflict will not be conflicting at another time */
    if (condition->required_hits)
      continue;

    operand1 = rc_validate_get_comparison(condition, &comparison1, &value1);
    if (!operand1)
      continue;

    switch (condition->type) {
      case RC_CONDITION_PAUSE_IF:
        if (conditions != compare_conditions)
          break;
        /* fallthrough */
      case RC_CONDITION_RESET_IF:
        comparison1 = rc_validate_get_opposite_comparison(comparison1);
        break;
      default:
        if (rc_validate_is_combining_condition(condition))
          continue;
        break;
    }

    /* inner loop is the potentially conflicting conditions */
    state->cond_index = 1;
    for (compare_condition = compare_conditions->conditions; compare_condition != NULL; compare_condition = compare_condition->next, ++state->cond_index) {
      if (compare_condition == condition_chain_start) {
        /* skip condition we're already looking at */
        while (compare_condition != condition) {
          ++state->cond_index;
          compare_condition = compare_condition->next;
        }

        continue;
      }

      /* if combining conditions exist, make sure the same combining conditions exist in the
       * compare logic. conflicts can only occur if the combinining conditions match. */
      chain_matches = 1;
      if (condition_chain_start != condition) {
        const rc_condition_t* condition_chain_iter = condition_chain_start;
        while (condition_chain_iter != condition) {
          if (compare_condition->type != condition_chain_iter->type ||
            compare_condition->oper != condition_chain_iter->oper ||
            compare_condition->required_hits != condition_chain_iter->required_hits ||
            !rc_operands_are_equal(&compare_condition->operand1, &condition_chain_iter->operand1))
          {
            chain_matches = 0;
            break;
          }

          if (compare_condition->oper != RC_OPERATOR_NONE &&
            !rc_operands_are_equal(&compare_condition->operand2, &condition_chain_iter->operand2))
          {
            chain_matches = 0;
            break;
          }

          if (!compare_condition->next) {
            chain_matches = 0;
            break;
          }

          if (compare_condition->type != RC_CONDITION_ADD_ADDRESS &&
              compare_condition->type != RC_CONDITION_ADD_SOURCE &&
              compare_condition->type != RC_CONDITION_SUB_SOURCE &&
              compare_condition->type != RC_CONDITION_AND_NEXT)
          {
            /* things like AddHits and OrNext are hard to definitively detect conflicts. ignore them. */
            chain_matches = 0;
            break;
          }

          ++state->cond_index;
          compare_condition = compare_condition->next;
          condition_chain_iter = condition_chain_iter->next;
        }
      }

      /* combining field didn't match, or there's more unmatched combining fields. ignore this condition */
      if (!chain_matches || rc_validate_is_combining_condition(compare_condition)) {
        while (compare_condition->next && rc_validate_is_combining_condition(compare_condition))
          compare_condition = compare_condition->next;
        continue;
      }

      if (compare_condition->required_hits)
        continue;

      operand2 = rc_validate_get_comparison(compare_condition, &comparison2, &value2);
      if (!operand2 || !rc_operands_are_equal(operand1, operand2))
        continue;

      switch (compare_condition->type) {
        case RC_CONDITION_PAUSE_IF:
          if (conditions != compare_conditions) /* PauseIf only affects conditions in same group */
            break;
          /* fallthrough */
        case RC_CONDITION_RESET_IF:
          comparison2 = rc_validate_get_opposite_comparison(comparison2);
          break;
        default:
          if (rc_validate_is_combining_condition(compare_condition))
            continue;
          break;
      }

      overlap = rc_validate_comparison_overlap(comparison1, value1, comparison2, value2);
      switch (overlap)
      {
        case RC_OVERLAP_CONFLICTING:
          if (compare_condition->type == RC_CONDITION_PAUSE_IF || condition->type == RC_CONDITION_PAUSE_IF)
          {
            /* ignore PauseIf conflicts between groups, unless both conditions are PauseIfs */
            if (conditions != compare_conditions && compare_condition->type != condition->type)
              continue;
          }
          break;

        case RC_OVERLAP_REDUNDANT:
          if (group_index != state->group_index && state->group_index == 0)
          {
            /* if the alt condition is more restrictive than the core condition, ignore it */
            if (rc_validate_comparison_overlap(comparison2, value2, comparison1, value1) != RC_OVERLAP_REDUNDANT)
              continue;
          }

          if (compare_condition->type == RC_CONDITION_PAUSE_IF || condition->type == RC_CONDITION_PAUSE_IF)
          {
            /* ignore PauseIf redundancies between groups */
            if (conditions != compare_conditions)
              continue;

            /* if the PauseIf is less restrictive than the other condition, it's just a guard. ignore it */
            if (rc_validate_comparison_overlap(comparison2, value2, comparison1, value1) != RC_OVERLAP_REDUNDANT)
              continue;

            /* PauseIf redundant with ResetIf is a conflict (both are inverted comparisons) */
            if (compare_condition->type == RC_CONDITION_RESET_IF || condition->type == RC_CONDITION_RESET_IF)
              overlap = RC_OVERLAP_CONFLICTING;
          }
          else if (compare_condition->type == RC_CONDITION_RESET_IF && condition->type != RC_CONDITION_RESET_IF)
          {
            /* only ever report the redundancy on the non-ResetIf condition. The ResetIf is allowed to
             * fire when the non-ResetIf condition is not true. */
            if (state->has_hit_targets)
              continue;
          }
          else if (condition->type == RC_CONDITION_RESET_IF && compare_condition->type != RC_CONDITION_RESET_IF)
          {
            /* if the ResetIf condition is more restrictive than the non-ResetIf condition,
               and there aren't any hits to clear, ignore it */
            if (state->has_hit_targets)
              continue;
          }
          else if (compare_condition->type == RC_CONDITION_MEASURED_IF || condition->type == RC_CONDITION_MEASURED_IF)
          {
            /* ignore MeasuredIf redundancies between groups */
            if (conditions != compare_conditions)
              continue;

            if (compare_condition->type == RC_CONDITION_MEASURED_IF && condition->type != RC_CONDITION_MEASURED_IF)
            {
              /* only ever report the redundancy on the non-MeasuredIf condition. The MeasuredIf provides
               * additional functionality. */
              continue;
            }
          }
          else if (condition->type == RC_CONDITION_TRIGGER && compare_condition->type != RC_CONDITION_TRIGGER)
          {
            /* Trigger is allowed to be redundant with non-trigger conditions as there may be limits that start a
             * challenge that are further reduced for the completion of the challenge */
            continue;
          }
          break;

        default:
          continue;
      }

      /* if condition A conflicts with condition B, condition B will also conflict with
       * condition A. don't report both. */
      {
        int already_reported = 0;
        const rc_validation_error_t* error = state->errors;
        const rc_validation_error_t* stop = state->errors + state->error_count;
        for (; error < stop; ++error) {
          if (error->data2 == state->cond_index && error->data1 == state->group_index) {
            if (error->err == RC_VALIDATION_ERR_REDUNDANT_CONDITION ||
                error->err == RC_VALIDATION_ERR_CONFLICTING_CONDITION) {
              already_reported = 1;
            }
          }
        }

        if (already_reported)
          continue;
      }

      rc_validate_add_error(state, (overlap == RC_OVERLAP_REDUNDANT) ?
          RC_VALIDATION_ERR_REDUNDANT_CONDITION : RC_VALIDATION_ERR_CONFLICTING_CONDITION,
          group_index, rc_validate_get_condition_index(conditions, condition));
    }
  }

  return (state->error_count == errors_before);
}

static int rc_validate_trigger_internal(const rc_trigger_t* trigger, rc_validation_state_t* state)
{
  rc_condset_t* alt;
  uint32_t index;

  state->has_alt_groups = (trigger->alternative != NULL);

  state->has_hit_targets = trigger->requirement && rc_condset_has_hittargets(trigger->requirement);
  if (!state->has_hit_targets) {
    for (alt = trigger->alternative; alt; alt = alt->next) {
      if (rc_condset_has_hittargets(alt)) {
        state->has_hit_targets = 1;
        break;
      }
    }
  }

  state->group_index = 0;
  if (rc_validate_condset_internal(trigger->requirement, state)) {
    /* compare core to itself */
    rc_validate_conflicting_conditions(trigger->requirement, trigger->requirement, 0, state);
  }

  index = 1;
  for (alt = trigger->alternative; alt; alt = alt->next, ++index) {
    state->group_index = index;
    if (rc_validate_condset_internal(alt, state)) {
      /* compare alt to itself */
      if (!rc_validate_conflicting_conditions(alt, alt, index, state))
        continue;

      /* compare alt to core */
      if (!rc_validate_conflicting_conditions(trigger->requirement, alt, 0, state))
        continue;

      /* compare core to alt */
      state->group_index = 0;
      if (!rc_validate_conflicting_conditions(alt, trigger->requirement, index, state))
        continue;
    }
  }

  return (state->error_count == 0);
}

int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t max_address)
{
  rc_validation_state_t state;
  memset(&state, 0, sizeof(state));
  state.max_address = max_address;

  result[0] = '\0';
  if (!rc_validate_trigger_internal(trigger, &state)) {
    const rc_validation_error_t* most_severe_error = rc_validate_find_most_severe_error(&state);
    return rc_validate_format_error(result, result_size, &state, most_severe_error);
  }

  return 1;
}

int rc_validate_trigger_for_console(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t console_id)
{
  rc_validation_state_t state;
  memset(&state, 0, sizeof(state));
  state.console_id = console_id;
  state.max_address = rc_console_max_address(console_id);

  result[0] = '\0';
  if (!rc_validate_trigger_internal(trigger, &state)) {
    const rc_validation_error_t* most_severe_error = rc_validate_find_most_severe_error(&state);
    return rc_validate_format_error(result, result_size, &state, most_severe_error);
  }

  return 1;
}
