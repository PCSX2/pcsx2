#include "rc_validate.h"

#include "rc_consoles.h"
#include "rc_internal.h"

#include "../rc_compat.h"

#include <stddef.h>
#include <stdlib.h>

static int rc_validate_memref(const rc_memref_t* memref, char result[], const size_t result_size, uint32_t console_id, uint32_t max_address)
{
  if (memref->address > max_address) {
    snprintf(result, result_size, "Address %04X out of range (max %04X)", memref->address, max_address);
    return 0;
  }

  switch (console_id) {
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
      if (memref->address >= 0x0800 && memref->address <= 0x1FFF) {
        snprintf(result, result_size, "Mirror RAM may not be exposed by emulator (address %04X)", memref->address);
        return 0;
      }
      break;

    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_COLOR:
      if (memref->address >= 0xE000 && memref->address <= 0xFDFF) {
        snprintf(result, result_size, "Echo RAM may not be exposed by emulator (address %04X)", memref->address);
        return 0;
      }
      break;

    case RC_CONSOLE_PLAYSTATION:
      if (memref->address <= 0xFFFF) {
        snprintf(result, result_size, "Kernel RAM may not be initialized without real BIOS (address %04X)", memref->address);
        return 0;
      }
      break;
  }

  return 1;
}

int rc_validate_memrefs(const rc_memrefs_t* memrefs, char result[], const size_t result_size, uint32_t max_address)
{
  const rc_memref_list_t* memref_list = &memrefs->memrefs;
  do {
    const rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_stop = memref + memref_list->count;
    for (; memref < memref_stop; ++memref) {
      if (!rc_validate_memref(memref, result, result_size, 0, max_address))
        return 0;
    }

    memref_list = memref_list->next;
  } while (memref_list);

  return 1;
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
  const uint32_t max_address = rc_console_max_address(console_id);
  const rc_memref_list_t* memref_list = &memrefs->memrefs;
  do
  {
    const rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_stop = memref + memref_list->count;
    for (; memref < memref_stop; ++memref)
    {
      if (!rc_validate_memref(memref, result, result_size, console_id, max_address))
        return 0;
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

static uint32_t rc_scale_value(uint32_t value, uint8_t oper, const rc_operand_t* operand)
{
  switch (oper) {
    case RC_OPERATOR_MULT:
    {
      unsigned long long scaled = ((unsigned long long)value) * rc_max_value(operand);
      if (scaled > 0xFFFFFFFF)
        return 0xFFFFFFFF;

      return (uint32_t)scaled;
    }

    case RC_OPERATOR_DIV:
    {
      const uint32_t min_val = (operand->type == RC_OPERAND_CONST) ? operand->value.num : 1;
      return value / min_val;
    }

    case RC_OPERATOR_AND:
      return rc_max_value(operand);

    case RC_OPERATOR_XOR:
      return value | rc_max_value(operand);

    case RC_OPERATOR_MOD:
    {
      const uint32_t divisor = (operand->type == RC_OPERAND_CONST) ? operand->value.num : 1;
      return (divisor >= value) ? (divisor - 1) : value;
    }

    case RC_OPERATOR_ADD:
    {
      unsigned long scaled = ((unsigned long)value) + rc_max_value(operand);
      if (scaled > 0xFFFFFFFF)
        return 0xFFFFFFFF;

      return (uint32_t)scaled;
    }

    case RC_OPERATOR_SUB:
    {
      const uint32_t op_max = (operand->type == RC_OPERAND_CONST) ? operand->value.num : rc_max_value(operand);
      if (value > op_max)
        return value - op_max;

      return 0xFFFFFFFF;
    }

    case RC_OPERATOR_SUB_PARENT:
    {
      const uint32_t op_max = (operand->type == RC_OPERAND_CONST) ? operand->value.num : rc_max_value(operand);
      if (op_max > value)
        return op_max - value;

      return 0xFFFFFFFF;
    }

    default:
      return value;
  }
}

static uint32_t rc_max_chain_value(const rc_operand_t* operand)
{
  if (rc_operand_is_memref(operand) && operand->value.memref->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
    const rc_modified_memref_t* modified_memref = (const rc_modified_memref_t*)operand->value.memref;
    if (modified_memref->modifier_type != RC_OPERATOR_INDIRECT_READ) {
      const uint32_t op_max = rc_max_chain_value(&modified_memref->parent);
      return rc_scale_value(op_max, modified_memref->modifier_type, &modified_memref->modifier);
    }
  }

  return rc_max_value(operand);
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

static int rc_validate_range(uint32_t min_val, uint32_t max_val, char oper, uint32_t max, char result[], const size_t result_size)
{
  switch (oper) {
    case RC_OPERATOR_AND:
      if (min_val > max) {
        snprintf(result, result_size, "Mask has more bits than source");
        return 0;
      }
      else if (min_val == 0 && max_val == 0) {
        snprintf(result, result_size, "Result of mask always 0");
        return 0;
      }
      break;

    case RC_OPERATOR_EQ:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_NE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GT:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_LE:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_LT:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;
  }

  return 1;
}

static int rc_validate_condset_internal(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t console_id, uint32_t max_address)
{
  const rc_condition_t* cond;
  char buffer[128];
  int index = 1;
  int in_add_hits = 0;
  int in_add_address = 0;
  int is_combining = 0;

  if (!condset) {
    *result = '\0';
    return 1;
  }

  for (cond = condset->conditions; cond; cond = cond->next, ++index) {
    /* validate the original operands first */
    const rc_operand_t* operand1 = rc_condition_get_real_operand1(cond);
    int is_memref1 = rc_operand_is_memref(operand1);
    const int is_memref2 = rc_operand_is_memref(&cond->operand2);

    if (!in_add_address) {
      if (is_memref1 && !rc_validate_memref(operand1->value.memref, buffer, sizeof(buffer), console_id, max_address)) {
        snprintf(result, result_size, "Condition %d: %s", index, buffer);
        return 0;
      }
      if (is_memref2 && !rc_validate_memref(cond->operand2.value.memref, buffer, sizeof(buffer), console_id, max_address)) {
        snprintf(result, result_size, "Condition %d: %s", index, buffer);
        return 0;
      }
    }
    else {
      in_add_address = 0;
    }

    if (rc_operand_is_recall(operand1)) {
      if (rc_operand_type_is_memref(operand1->memref_access_type) && !operand1->value.memref) {
        snprintf(result, result_size, "Condition %d: Recall used before Remember", index);
        return 0;
      }
    }

    if (rc_operand_is_recall(&cond->operand2)) {
      if (rc_operand_type_is_memref(cond->operand2.memref_access_type) && !cond->operand2.value.memref) {
        snprintf(result, result_size, "Condition %d: Recall used before Remember", index);
        return 0;
      }
    }

    switch (cond->type) {
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_REMEMBER:
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        if (operand1->type == RC_OPERAND_DELTA || operand1->type == RC_OPERAND_PRIOR) {
          snprintf(result, result_size, "Condition %d: Using pointer from previous frame", index);
          return 0;
        }
        if (rc_operand_is_float(&cond->operand1) || rc_operand_is_float(&cond->operand2)) {
          snprintf(result, result_size, "Condition %d: Using non-integer value in AddAddress calcuation", index);
          return 0;
        }
        if (rc_operand_type_is_transform(cond->operand1.type)) {
          snprintf(result, result_size, "Condition %d: Using transformed value in AddAddress calcuation", index);
          return 0;
        }
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
        if (cond->required_hits == 1) {
          snprintf(result, result_size, "Condition %d: Hit target of 1 is redundant on ResetIf", index);
          return 0;
        }
        /* fallthrough */ /* to default */
      default:
        if (in_add_hits) {
          if (cond->required_hits == 0) {
            snprintf(result, result_size, "Condition %d: Final condition in AddHits chain must have a hit target", index);
            return 0;
          }

          in_add_hits = 0;
        }

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
      snprintf(result, result_size, "Condition %d: Comparing different memory sizes", index);
      return 0;
    }

    if (is_memref1 && rc_operand_is_float(operand1)) {
      /* if left side is a float, right side will be converted to a float, so don't do range validation */
    }
    else if (is_memref1 || is_memref2) {
      /* if either side is a memref, check for impossible comparisons */
      const size_t prefix_length = snprintf(result, result_size, "Condition %d: ", index);
      const rc_operand_t* operand2 = &cond->operand2;
      uint8_t oper = cond->oper;
      uint32_t max = rc_max_chain_value(operand1);
      uint32_t max_val = rc_max_value(operand2);
      uint32_t min_val;

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
                snprintf(result + prefix_length, result_size - prefix_length, "Comparison is never true");
                return 0;
              case RC_OPERATOR_NE:
                snprintf(result + prefix_length, result_size - prefix_length, "Comparison is always true");
                return 0;
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

      if (!rc_validate_range(min_val, max_val, oper, max, result + prefix_length, result_size - prefix_length))
        return 0;
    }
  }

  if (is_combining) {
    snprintf(result, result_size, "Final condition type expects another condition to follow");
    return 0;
  }

  *result = '\0';
  return 1;
}

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t max_address)
{
  return rc_validate_condset_internal(condset, result, result_size, 0, max_address);
}

int rc_validate_condset_for_console(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t console_id)
{
  const uint32_t max_address = rc_console_max_address(console_id);
  return rc_validate_condset_internal(condset, result, result_size, console_id, max_address);
}

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
    const char* prefix, const char* compare_prefix, char result[], const size_t result_size)
{
  int comparison1, comparison2;
  uint32_t value1, value2;
  const rc_operand_t* operand1;
  const rc_operand_t* operand2;
  const rc_condition_t* compare_condition;
  const rc_condition_t* condition;
  const rc_condition_t* condition_chain_start;
  int overlap;
  int chain_matches;

  /* empty group */
  if (conditions == NULL || compare_conditions == NULL)
    return 1;

  /* outer loop is the source conditions */
  for (condition = conditions->conditions; condition != NULL; condition = condition->next)
  {
    condition_chain_start = condition;
    while (condition && rc_condition_is_combining(condition))
      condition = condition->next;
    if (!condition)
      break;

    /* hits can be captured at any time, so any potential conflict will not be conflicting at another time */
    if (condition->required_hits)
      continue;

    operand1 = rc_validate_get_comparison(condition, &comparison1, &value1);
    if (!operand1)
      continue;

    switch (condition->type)
    {
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
    for (compare_condition = compare_conditions->conditions; compare_condition != NULL; compare_condition = compare_condition->next)
    {
      if (compare_condition == condition_chain_start)
      {
        /* skip condition we're already looking at */
        while (compare_condition != condition)
          compare_condition = compare_condition->next;

        continue;
      }

      /* if combining conditions exist, make sure the same combining conditions exist in the
       * compare logic. conflicts can only occur if the combinining conditions match. */
      chain_matches = 1;
      if (condition_chain_start != condition)
      {
        const rc_condition_t* condition_chain_iter = condition_chain_start;
        while (condition_chain_iter != condition)
        {
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

          if (!compare_condition->next)
          {
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

          compare_condition = compare_condition->next;
          condition_chain_iter = condition_chain_iter->next;
        }
      }

      /* combining field didn't match, or there's more unmatched combining fields. ignore this condition */
      if (!chain_matches || rc_validate_is_combining_condition(compare_condition))
      {
        while (compare_condition->next && rc_validate_is_combining_condition(compare_condition))
          compare_condition = compare_condition->next;
        continue;
      }

      if (compare_condition->required_hits)
        continue;

      operand2 = rc_validate_get_comparison(compare_condition, &comparison2, &value2);
      if (!operand2 || operand2->value.memref->address != operand1->value.memref->address ||
          operand2->size != operand1->size || operand2->type != operand1->type)
        continue;

      switch (compare_condition->type)
      {
        case RC_CONDITION_PAUSE_IF:
          if (conditions != compare_conditions)
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
          if (prefix != compare_prefix && strcmp(compare_prefix, "Core") == 0)
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

      if (compare_prefix && *compare_prefix)
      {
        snprintf(result, result_size, "%s Condition %d: %s with %s Condition %d",
            compare_prefix, rc_validate_get_condition_index(compare_conditions, compare_condition),
            (overlap == RC_OVERLAP_REDUNDANT) ? "Redundant" : "Conflicts",
            prefix, rc_validate_get_condition_index(conditions, condition));
      }
      else
      {
        snprintf(result, result_size, "Condition %d: %s with Condition %d",
            rc_validate_get_condition_index(compare_conditions, compare_condition),
            (overlap == RC_OVERLAP_REDUNDANT) ? "Redundant" : "Conflicts",
            rc_validate_get_condition_index(conditions, condition));
      }
      return 0;
    }
  }

  return 1;
}

static int rc_validate_trigger_internal(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t console_id, uint32_t max_address)
{
  const rc_condset_t* alt;
  int index;

  if (!trigger->alternative) {
    if (!rc_validate_condset_internal(trigger->requirement, result, result_size, console_id, max_address))
      return 0;

    return rc_validate_conflicting_conditions(trigger->requirement, trigger->requirement, "", "", result, result_size);
  }

  snprintf(result, result_size, "Core ");
  if (!rc_validate_condset_internal(trigger->requirement, result + 5, result_size - 5, console_id, max_address))
    return 0;

  /* compare core to itself */
  if (!rc_validate_conflicting_conditions(trigger->requirement, trigger->requirement, "Core", "Core", result, result_size))
    return 0;

  index = 1;
  for (alt = trigger->alternative; alt; alt = alt->next, ++index) {
    char altname[16];
    const size_t prefix_length = snprintf(result, result_size, "Alt%d ", index);
    if (!rc_validate_condset_internal(alt, result + prefix_length, result_size - prefix_length, console_id, max_address))
      return 0;

    /* compare alt to itself */
    snprintf(altname, sizeof(altname), "Alt%d", index);
    if (!rc_validate_conflicting_conditions(alt, alt, altname, altname, result, result_size))
      return 0;

    /* compare alt to core */
    if (!rc_validate_conflicting_conditions(trigger->requirement, alt, "Core", altname, result, result_size))
      return 0;

    /* compare core to alt */
    if (!rc_validate_conflicting_conditions(alt, trigger->requirement, altname, "Core", result, result_size))
      return 0;
  }

  *result = '\0';
  return 1;
}

int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t max_address)
{
  return rc_validate_trigger_internal(trigger, result, result_size, 0, max_address);
}

int rc_validate_trigger_for_console(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t console_id)
{
  const uint32_t max_address = rc_console_max_address(console_id);
  return rc_validate_trigger_internal(trigger, result, result_size, console_id, max_address);
}
