#include "rc_internal.h"

#include <string.h> /* memset */
#include <ctype.h> /* isdigit */
#include <float.h> /* FLT_EPSILON */

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next_clause;

  next_clause = &self->conditions;

  do
  {
    parse->measured_target = 0; /* passing is_value=1 should prevent any conflicts, but clear it out anyway */
    *next_clause = rc_parse_condset(memaddr, parse, 1);
    if (parse->offset < 0) {
      return;
    }

    if (**memaddr == 'S' || **memaddr == 's') {
      /* alt groups not supported */
      parse->offset = RC_INVALID_VALUE_FLAG;
    }
    else if (parse->measured_target == 0) {
      parse->offset = RC_MISSING_VALUE_MEASURED;
    }
    else if (**memaddr == '$') {
      /* maximum of */
      ++(*memaddr);
      next_clause = &(*next_clause)->next;
      continue;
    }

    break;
  } while (1);

  (*next_clause)->next = 0;
}

void rc_parse_legacy_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  rc_condset_t** next_clause;
  rc_condition_t* cond;
  char buffer[64] = "A:";
  const char* buffer_ptr;
  char* ptr;

  /* convert legacy format into condset */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  memset(self->conditions, 0, sizeof(rc_condset_t));

  next = &self->conditions->conditions;
  next_clause = &self->conditions->next;

  for (;; ++(*memaddr)) {
    buffer[0] = 'A'; /* reset to AddSource */
    ptr = &buffer[2];

    /* extract the next clause */
    for (;; ++(*memaddr)) {
      switch (**memaddr) {
        case '_': /* add next */
        case '$': /* maximum of */
        case '\0': /* end of string */
        case ':': /* end of leaderboard clause */
        case ')': /* end of rich presence macro */
          *ptr = '\0';
          break;

        case '*':
          *ptr++ = '*';

          buffer_ptr = *memaddr + 1;
          if (*buffer_ptr == '-') {
            buffer[0] = 'B'; /* change to SubSource */
            ++(*memaddr); /* don't copy sign */
            ++buffer_ptr; /* ignore sign when doing floating point check */
          }
          else if (*buffer_ptr == '+') {
            ++buffer_ptr; /* ignore sign when doing floating point check  */
          }

          /* if it looks like a floating point number, add the 'f' prefix */
          while (isdigit((unsigned char)*buffer_ptr))
            ++buffer_ptr;
          if (*buffer_ptr == '.')
            *ptr++ = 'f';
          continue;

        default:
          *ptr++ = **memaddr;
          continue;
      }

      break;
    }

    /* process the clause */
    buffer_ptr = buffer;
    cond = rc_parse_condition(&buffer_ptr, parse, 0);
    if (parse->offset < 0)
      return;

    if (*buffer_ptr) {
      /* whatever we copied as a single condition was not fully consumed */
      parse->offset = RC_INVALID_COMPARISON;
      return;
    }

    switch (cond->oper) {
      case RC_OPERATOR_MULT:
      case RC_OPERATOR_DIV:
      case RC_OPERATOR_AND:
      case RC_OPERATOR_XOR:
      case RC_OPERATOR_NONE:
        break;

      default:
        parse->offset = RC_INVALID_OPERATOR;
        return;
    }

    *next = cond;

    if (**memaddr == '_') {
      /* add next */
      next = &cond->next;
      continue;
    }

    if (cond->type == RC_CONDITION_SUB_SOURCE) {
      /* cannot change SubSource to Measured. add a dummy condition */
      next = &cond->next;
      buffer_ptr = "A:0";
      cond = rc_parse_condition(&buffer_ptr, parse, 0);
      *next = cond;
    }

    /* convert final AddSource condition to Measured */
    cond->type = RC_CONDITION_MEASURED;
    cond->next = 0;

    if (**memaddr != '$') {
      /* end of valid string */
      *next_clause = 0;
      break;
    }

    /* max of ($), start a new clause */
    *next_clause = RC_ALLOC(rc_condset_t, parse);

    if (parse->buffer) /* don't clear in sizing mode or pointer will break */
      memset(*next_clause, 0, sizeof(rc_condset_t));

    next = &(*next_clause)->conditions;
    next_clause = &(*next_clause)->next;
  }
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':') {
    rc_parse_cond_value(self, memaddr, parse);
  }
  else {
    rc_parse_legacy_value(self, memaddr, parse);
  }

  self->name = "(unnamed)";
  self->value.value = self->value.prior = 0;
  self->value.changed = 0;
  self->next = 0;
}

int rc_value_size(const char* memaddr) {
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_memref_t* first_memref;
  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &first_memref);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_value_t* self;
  rc_parse_state_t parse;

  if (!buffer || !memaddr)
    return NULL;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : NULL;
}

int rc_evaluate_value_typed(rc_value_t* self, rc_typed_value_t* value, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int valid = 0;

  rc_update_memref_values(self->memrefs, peek, ud);

  value->value.i32 = 0;
  value->type = RC_VALUE_TYPE_SIGNED;

  for (condset = self->conditions; condset != NULL; condset = condset->next) {
    memset(&eval_state, 0, sizeof(eval_state));
    eval_state.peek = peek;
    eval_state.peek_userdata = ud;
    eval_state.L = L;

    rc_test_condset(condset, &eval_state);

    if (condset->is_paused)
      continue;

    if (eval_state.was_reset) {
      /* if any ResetIf condition was true, reset the hit counts
       * NOTE: ResetIf only affects the current condset when used in values!
       */
      rc_reset_condset(condset);

      /* if the measured value came from a hit count, reset it too */
      if (eval_state.measured_from_hits) {
        eval_state.measured_value.value.u32 = 0;
        eval_state.measured_value.type = RC_VALUE_TYPE_UNSIGNED;
      }
    }

    if (!valid) {
      /* capture the first valid measurement */
      memcpy(value, &eval_state.measured_value, sizeof(*value));
      valid = 1;
    }
    else {
      /* multiple condsets are currently only used for the MAX_OF operation.
       * only keep the condset's value if it's higher than the current highest value.
       */
      if (rc_typed_value_compare(&eval_state.measured_value, value, RC_OPERATOR_GT))
        memcpy(value, &eval_state.measured_value, sizeof(*value));
    }
  }

  return valid;
}

int32_t rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_typed_value_t result;
  int valid = rc_evaluate_value_typed(self, &result, peek, ud, L);

  if (valid) {
    /* if not paused, store the value so that it's available when paused. */
    rc_typed_value_convert(&result, RC_VALUE_TYPE_UNSIGNED);
    rc_update_memref_value(&self->value, result.value.u32);
  }
  else {
    /* when paused, the Measured value will not be captured, use the last captured value. */
    result.value.u32 = self->value.value;
    result.type = RC_VALUE_TYPE_UNSIGNED;
  }

  rc_typed_value_convert(&result, RC_VALUE_TYPE_SIGNED);
  return result.value.i32;
}

void rc_reset_value(rc_value_t* self) {
  rc_condset_t* condset = self->conditions;
  while (condset != NULL) {
    rc_reset_condset(condset);
    condset = condset->next;
  }

  self->value.value = self->value.prior = 0;
  self->value.changed = 0;
}

int rc_value_from_hits(rc_value_t* self)
{
  rc_condset_t* condset = self->conditions;
  for (; condset != NULL; condset = condset->next) {
    rc_condition_t* condition = condset->conditions;
    for (; condition != NULL; condition = condition->next) {
      if (condition->type == RC_CONDITION_MEASURED)
        return (condition->required_hits != 0);
    }
  }

  return 0;
}

void rc_init_parse_state_variables(rc_parse_state_t* parse, rc_value_t** variables) {
  parse->variables = variables;
  *variables = 0;
}

rc_value_t* rc_alloc_helper_variable(const char* memaddr, size_t memaddr_len, rc_parse_state_t* parse)
{
  rc_value_t** variables = parse->variables;
  rc_value_t* value;
  const char* name;
  uint32_t measured_target;

  while ((value = *variables) != NULL) {
    if (strncmp(value->name, memaddr, memaddr_len) == 0 && value->name[memaddr_len] == 0)
      return value;

    variables = &value->next;
  }

  value = RC_ALLOC_SCRATCH(rc_value_t, parse);
  memset(&value->value, 0, sizeof(value->value));
  value->value.size = RC_MEMSIZE_VARIABLE;
  value->memrefs = NULL;

  /* capture name before calling parse as parse will update memaddr pointer */
  name = rc_alloc_str(parse, memaddr, memaddr_len);
  if (!name)
    return NULL;

  /* the helper variable likely has a Measured condition. capture the current measured_target so we can restore it
   * after generating the variable so the variable's Measured target doesn't conflict with the rest of the trigger. */
  measured_target = parse->measured_target;

  /* disable variable resolution when defining a variable to prevent infinite recursion */
  variables = parse->variables;
  parse->variables = NULL;
  rc_parse_value_internal(value, &memaddr, parse);
  parse->variables = variables;

  /* restore the measured target */
  parse->measured_target = measured_target;

  /* store name after calling parse as parse will set name to (unnamed) */
  value->name = name;

  /* append the new variable to the end of the list (have to re-evaluate in case any others were added) */
  while (*variables != NULL)
    variables = &(*variables)->next;
  *variables = value;

  return value;
}

void rc_update_variables(rc_value_t* variable, rc_peek_t peek, void* ud, lua_State* L) {
  rc_typed_value_t result;

  while (variable) {
    if (rc_evaluate_value_typed(variable, &result, peek, ud, L)) {
      /* store the raw bytes and type to be restored by rc_typed_value_from_memref_value  */
      rc_update_memref_value(&variable->value, result.value.u32);
      variable->value.type = result.type;
    }

    variable = variable->next;
  }
}

void rc_typed_value_from_memref_value(rc_typed_value_t* value, const rc_memref_value_t* memref) {
  value->value.u32 = memref->value;

  if (memref->size == RC_MEMSIZE_VARIABLE) {
    /* a variable can be any of the supported types, but the raw data was copied into u32 */
    value->type = memref->type;
  }
  else {
    /* not a variable, only u32 is supported */
    value->type = RC_VALUE_TYPE_UNSIGNED;
  }
}

void rc_typed_value_convert(rc_typed_value_t* value, char new_type) {
  switch (new_type) {
    case RC_VALUE_TYPE_UNSIGNED:
      switch (value->type) {
        case RC_VALUE_TYPE_UNSIGNED:
          return;
        case RC_VALUE_TYPE_SIGNED:
          value->value.u32 = (unsigned)value->value.i32;
          break;
        case RC_VALUE_TYPE_FLOAT:
          value->value.u32 = (unsigned)value->value.f32;
          break;
        default:
          value->value.u32 = 0;
          break;
      }
      break;

    case RC_VALUE_TYPE_SIGNED:
      switch (value->type) {
        case RC_VALUE_TYPE_SIGNED:
          return;
        case RC_VALUE_TYPE_UNSIGNED:
          value->value.i32 = (int)value->value.u32;
          break;
        case RC_VALUE_TYPE_FLOAT:
          value->value.i32 = (int)value->value.f32;
          break;
        default:
          value->value.i32 = 0;
          break;
      }
      break;

    case RC_VALUE_TYPE_FLOAT:
      switch (value->type) {
        case RC_VALUE_TYPE_FLOAT:
          return;
        case RC_VALUE_TYPE_UNSIGNED:
          value->value.f32 = (float)value->value.u32;
          break;
        case RC_VALUE_TYPE_SIGNED:
          value->value.f32 = (float)value->value.i32;
          break;
        default:
          value->value.f32 = 0.0;
          break;
      }
      break;

    default:
      break;
  }

  value->type = new_type;
}

static rc_typed_value_t* rc_typed_value_convert_into(rc_typed_value_t* dest, const rc_typed_value_t* source, char new_type) {
  memcpy(dest, source, sizeof(rc_typed_value_t));
  rc_typed_value_convert(dest, new_type);
  return dest;
}

void rc_typed_value_negate(rc_typed_value_t* value) {
  switch (value->type)
  {
    case RC_VALUE_TYPE_UNSIGNED:
      rc_typed_value_convert(value, RC_VALUE_TYPE_SIGNED);
      /* fallthrough */ /* to RC_VALUE_TYPE_SIGNED */

    case RC_VALUE_TYPE_SIGNED:
      value->value.i32 = -(value->value.i32);
      break;

    case RC_VALUE_TYPE_FLOAT:
      value->value.f32 = -(value->value.f32);
      break;

    default:
      break;
  }
}

void rc_typed_value_add(rc_typed_value_t* value, const rc_typed_value_t* amount) {
  rc_typed_value_t converted;

  if (amount->type != value->type && value->type != RC_VALUE_TYPE_NONE)
    amount = rc_typed_value_convert_into(&converted, amount, value->type);

  switch (value->type)
  {
    case RC_VALUE_TYPE_UNSIGNED:
      value->value.u32 += amount->value.u32;
      break;

    case RC_VALUE_TYPE_SIGNED:
      value->value.i32 += amount->value.i32;
      break;

    case RC_VALUE_TYPE_FLOAT:
      value->value.f32 += amount->value.f32;
      break;

    case RC_VALUE_TYPE_NONE:
      memcpy(value, amount, sizeof(rc_typed_value_t));
      break;

    default:
      break;
  }
}

void rc_typed_value_multiply(rc_typed_value_t* value, const rc_typed_value_t* amount) {
  rc_typed_value_t converted;

  switch (value->type)
  {
    case RC_VALUE_TYPE_UNSIGNED:
      switch (amount->type)
      {
        case RC_VALUE_TYPE_UNSIGNED:
          /* the c standard for unsigned multiplication is well defined as non-overflowing truncation
           * to the type's size. this allows negative multiplication through twos-complements. i.e.
           *   1 * -1 (0xFFFFFFFF) = 0xFFFFFFFF = -1
           *   3 * -2 (0xFFFFFFFE) = 0x2FFFFFFFA & 0xFFFFFFFF = 0xFFFFFFFA = -6
           *  10 * -5 (0xFFFFFFFB) = 0x9FFFFFFCE & 0xFFFFFFFF = 0xFFFFFFCE = -50
           */
          value->value.u32 *= amount->value.u32;
          break;

        case RC_VALUE_TYPE_SIGNED:
          value->value.u32 *= (unsigned)amount->value.i32;
          break;

        case RC_VALUE_TYPE_FLOAT:
          rc_typed_value_convert(value, RC_VALUE_TYPE_FLOAT);
          value->value.f32 *= amount->value.f32;
          break;

        default:
          value->type = RC_VALUE_TYPE_NONE;
          break;
      }
      break;

    case RC_VALUE_TYPE_SIGNED:
      switch (amount->type)
      {
        case RC_VALUE_TYPE_SIGNED:
          value->value.i32 *= amount->value.i32;
          break;

        case RC_VALUE_TYPE_UNSIGNED:
          value->value.i32 *= (int)amount->value.u32;
          break;

        case RC_VALUE_TYPE_FLOAT:
          rc_typed_value_convert(value, RC_VALUE_TYPE_FLOAT);
          value->value.f32 *= amount->value.f32;
          break;

        default:
          value->type = RC_VALUE_TYPE_NONE;
          break;
      }
      break;

    case RC_VALUE_TYPE_FLOAT:
      if (amount->type == RC_VALUE_TYPE_NONE) {
        value->type = RC_VALUE_TYPE_NONE;
      }
      else {
        amount = rc_typed_value_convert_into(&converted, amount, RC_VALUE_TYPE_FLOAT);
        value->value.f32 *= amount->value.f32;
      }
      break;

    default:
      value->type = RC_VALUE_TYPE_NONE;
      break;
  }
}

void rc_typed_value_divide(rc_typed_value_t* value, const rc_typed_value_t* amount) {
  rc_typed_value_t converted;

  switch (amount->type)
  {
    case RC_VALUE_TYPE_UNSIGNED:
      if (amount->value.u32 == 0) { /* divide by zero */
        value->type = RC_VALUE_TYPE_NONE;
        return;
      }

      switch (value->type) {
        case RC_VALUE_TYPE_UNSIGNED: /* integer math */
          value->value.u32 /= amount->value.u32;
          return;
        case RC_VALUE_TYPE_SIGNED: /* integer math */
          value->value.i32 /= (int)amount->value.u32;
          return;
        case RC_VALUE_TYPE_FLOAT:
          amount = rc_typed_value_convert_into(&converted, amount, RC_VALUE_TYPE_FLOAT);
          break;
        default:
          value->type = RC_VALUE_TYPE_NONE;
          return;
      }
      break;

    case RC_VALUE_TYPE_SIGNED:
      if (amount->value.i32 == 0) { /* divide by zero */
        value->type = RC_VALUE_TYPE_NONE;
        return;
      }

      switch (value->type) {
        case RC_VALUE_TYPE_SIGNED: /* integer math */
          value->value.i32 /= amount->value.i32;
          return;
        case RC_VALUE_TYPE_UNSIGNED: /* integer math */
          value->value.u32 /= (unsigned)amount->value.i32;
          return;
        case RC_VALUE_TYPE_FLOAT:
          amount = rc_typed_value_convert_into(&converted, amount, RC_VALUE_TYPE_FLOAT);
          break;
        default:
          value->type = RC_VALUE_TYPE_NONE;
          return;
      }
      break;

    case RC_VALUE_TYPE_FLOAT:
      break;

    default:
      value->type = RC_VALUE_TYPE_NONE;
      return;
  }

  if (amount->value.f32 == 0.0) { /* divide by zero */
    value->type = RC_VALUE_TYPE_NONE;
    return;
  }

  rc_typed_value_convert(value, RC_VALUE_TYPE_FLOAT);
  value->value.f32 /= amount->value.f32;
}

static int rc_typed_value_compare_floats(float f1, float f2, char oper) {
  if (f1 == f2) {
    /* exactly equal */
  }
  else {
    /* attempt to match 7 significant digits (24-bit mantissa supports just over 7 significant decimal digits) */
    /* https://stackoverflow.com/questions/17333/what-is-the-most-effective-way-for-float-and-double-comparison */
    const float abs1 = (f1 < 0) ? -f1 : f1;
    const float abs2 = (f2 < 0) ? -f2 : f2;
    const float threshold = ((abs1 < abs2) ? abs1 : abs2) * FLT_EPSILON;
    const float diff = f1 - f2;
    const float abs_diff = (diff < 0) ? -diff : diff;

    if (abs_diff <= threshold) {
      /* approximately equal */
    }
    else if (diff > threshold) {
      /* greater */
      switch (oper) {
        case RC_OPERATOR_NE:
        case RC_OPERATOR_GT:
        case RC_OPERATOR_GE:
          return 1;

        default:
          return 0;
      }
    }
    else {
      /* lesser */
      switch (oper) {
        case RC_OPERATOR_NE:
        case RC_OPERATOR_LT:
        case RC_OPERATOR_LE:
          return 1;

        default:
          return 0;
      }
    }
  }

  /* exactly or approximately equal */
  switch (oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_GE:
    case RC_OPERATOR_LE:
      return 1;

    default:
      return 0;
  }
}

int rc_typed_value_compare(const rc_typed_value_t* value1, const rc_typed_value_t* value2, char oper) {
  rc_typed_value_t converted_value2;
  if (value2->type != value1->type)
    value2 = rc_typed_value_convert_into(&converted_value2, value2, value1->type);

  switch (value1->type) {
    case RC_VALUE_TYPE_UNSIGNED:
      switch (oper) {
        case RC_OPERATOR_EQ: return value1->value.u32 == value2->value.u32;
        case RC_OPERATOR_NE: return value1->value.u32 != value2->value.u32;
        case RC_OPERATOR_LT: return value1->value.u32 < value2->value.u32;
        case RC_OPERATOR_LE: return value1->value.u32 <= value2->value.u32;
        case RC_OPERATOR_GT: return value1->value.u32 > value2->value.u32;
        case RC_OPERATOR_GE: return value1->value.u32 >= value2->value.u32;
        default: return 1;
      }

    case RC_VALUE_TYPE_SIGNED:
      switch (oper) {
        case RC_OPERATOR_EQ: return value1->value.i32 == value2->value.i32;
        case RC_OPERATOR_NE: return value1->value.i32 != value2->value.i32;
        case RC_OPERATOR_LT: return value1->value.i32 < value2->value.i32;
        case RC_OPERATOR_LE: return value1->value.i32 <= value2->value.i32;
        case RC_OPERATOR_GT: return value1->value.i32 > value2->value.i32;
        case RC_OPERATOR_GE: return value1->value.i32 >= value2->value.i32;
        default: return 1;
      }

    case RC_VALUE_TYPE_FLOAT:
      return rc_typed_value_compare_floats(value1->value.f32, value2->value.f32, oper);

    default:
      return 1;
  }
}
