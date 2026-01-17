#include "rc_internal.h"

#include <string.h> /* memset */
#include <ctype.h> /* isdigit */
#include <float.h> /* FLT_EPSILON */
#include <math.h> /* fmod */

int rc_is_valid_variable_character(char ch, int is_first) {
  if (is_first) {
    if (!isalpha((unsigned char)ch))
      return 0;
  }
  else {
    if (!isalnum((unsigned char)ch))
      return 0;
  }
  return 1;
}

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next_clause;

  next_clause = &self->conditions;

  do
  {
    parse->measured_target = 0; /* passing is_value=1 should prevent any conflicts, but clear it out anyway */
    *next_clause = rc_parse_condset(memaddr, parse);
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

static void rc_parse_legacy_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_with_trailing_conditions_t* condset_with_conditions;
  rc_condition_t** next;
  rc_condset_t** next_clause;
  rc_condset_t* condset;
  rc_condition_t local_cond;
  rc_condition_t* cond;
  uint32_t num_measured_conditions;
  char buffer[64] = "A:";
  const char* buffer_ptr;
  char* ptr;
  int done;

  /* convert legacy format into condset */
  next_clause = &self->conditions;
  do {
    /* count the number of joiners and add one to determine the number of clauses.  */
    buffer[0] = 'A'; /* reset to AddSource */
    done = 0;
    num_measured_conditions = 1;
    buffer_ptr = *memaddr;
    do {
      switch (*buffer_ptr++) {
        case '_': /* add next */
          ++num_measured_conditions;
          buffer[0] = 'A'; /* reset to AddSource */
          break;

        case '*': /* multiply */
          if (*buffer_ptr == '-') {
            /* multiplication by a negative number will convert to SubSource */
            ++buffer_ptr;
            buffer[0] = 'B';
          }
          break;

        case '\0': /* end of string */
        case '$': /* maximum of */
        case ':': /* end of leaderboard clause */
        case ')': /* end of rich presence macro */
          done = 1;
          break;

        default: /* assume everything else is valid - bad stuff will be filtered out later */
          break;
      }
    } while (!done);

    /* if last condition is not AddSource, we'll need to add a dummy condition for the Measured */
    if (buffer[0] != 'A')
      ++num_measured_conditions;

    condset_with_conditions = RC_ALLOC_WITH_TRAILING(rc_condset_with_trailing_conditions_t,
                                                     rc_condition_t, conditions, num_measured_conditions, parse);
    if (parse->offset < 0)
      return;

    condset = (rc_condset_t*)condset_with_conditions;
    memset(condset, 0, sizeof(*condset));
    condset->num_measured_conditions = num_measured_conditions;
    cond = &condset_with_conditions->conditions[0];

    next = &condset->conditions;

    for (;; ++(*memaddr)) {
      buffer[0] = 'A'; /* reset to AddSource */
      ptr = &buffer[2];

      /* extract the next clause */
      for (;; ++(*memaddr)) {
        switch (**memaddr) {
          case '_': /* add next */
            *ptr = '\0';
            break;

          case '$': /* maximum of */
          case '\0': /* end of string */
          case ':': /* end of leaderboard clause */
          case ')': /* end of rich presence macro */
            /* the last condition needs to be Measured - AddSource can be changed here,
             * SubSource will be handled later */
            if (buffer[0] == 'A')
              buffer[0] = 'M';

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
      if (!parse->buffer)
        cond = &local_cond;

      buffer_ptr = buffer;
      rc_parse_condition_internal(cond, &buffer_ptr, parse);
      if (parse->offset < 0)
        return;

      if (*buffer_ptr) {
        /* whatever we copied as a single condition was not fully consumed */
        parse->offset = RC_INVALID_COMPARISON;
        return;
      }

      if (cond->type == RC_CONDITION_MEASURED && !rc_operator_is_modifying(cond->oper)) {
        /* ignore non-modifying operator on measured clause. if it were parsed as an AddSource
         * or SubSource, that would have already happened in rc_parse_condition_internal, and
         * legacy formatted values are essentially a series of AddSources. */
        cond->oper = RC_OPERATOR_NONE;
      }

      rc_condition_update_parse_state(cond, parse);

      *next = cond;
      next = &cond->next;

      if (**memaddr != '_') /* add next */
        break;

      ++cond;
    }

    /* -- end of clause -- */

    /* clause must end in a Measured. if it doesn't, append one */
    if (cond->type != RC_CONDITION_MEASURED) {
      if (!parse->buffer)
        cond = &local_cond;
      else
        ++cond;

      buffer_ptr = "M:0";
      rc_parse_condition_internal(cond, &buffer_ptr, parse);
      *next = cond;
      next = &cond->next;
      rc_condition_update_parse_state(cond, parse);
    }

    *next = NULL;

    /* finalize clause */
    *next_clause = condset;
    next_clause = &condset->next;

    if (**memaddr != '$') {
      /* end of valid string */
      *next_clause = NULL;
      break;
    }

    /* max of ($), start a new clause */
    ++(*memaddr);
  } while (1);
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const uint8_t was_value = parse->is_value;
  const rc_condition_t* condition;
  parse->is_value = 1;

  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':')
    rc_parse_cond_value(self, memaddr, parse);
  else
    rc_parse_legacy_value(self, memaddr, parse);

  if (parse->offset >= 0 && parse->buffer) {
    self->name = "(unnamed)";
    self->value.value = self->value.prior = 0;
    self->value.memref_type = RC_MEMREF_TYPE_VALUE;
    self->value.changed = 0;
    self->has_memrefs = 0;

    for (condition = self->conditions->conditions; condition; condition = condition->next) {
      if (condition->type == RC_CONDITION_MEASURED) {
        if (rc_operand_is_float(&condition->operand1)) {
          self->value.size = RC_MEMSIZE_FLOAT;
          self->value.type = RC_VALUE_TYPE_FLOAT;
        }
        else {
          self->value.size = RC_MEMSIZE_32_BITS;
          self->value.type = RC_VALUE_TYPE_UNSIGNED;
        }
        break;
      }
    }
  }

  parse->is_value = was_value;
}

int rc_value_size(const char* memaddr) {
  rc_value_with_memrefs_t* value;
  rc_preparse_state_t preparse;
  rc_init_preparse_state(&preparse);

  value = RC_ALLOC(rc_value_with_memrefs_t, &preparse.parse);
  rc_parse_value_internal(&value->value, &memaddr, &preparse.parse);
  rc_preparse_alloc_memrefs(NULL, &preparse);

  rc_destroy_preparse_state(&preparse);
  return preparse.parse.offset;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, void* unused_L, int unused_funcs_idx) {
  rc_value_with_memrefs_t* value;
  rc_preparse_state_t preparse;
  const char* preparse_memaddr = memaddr;

  (void)unused_L;
  (void)unused_funcs_idx;

  if (!buffer || !memaddr)
    return NULL;

  rc_init_preparse_state(&preparse);
  value = RC_ALLOC(rc_value_with_memrefs_t, &preparse.parse);
  rc_parse_value_internal(&value->value, &preparse_memaddr, &preparse.parse);

  rc_reset_parse_state(&preparse.parse, buffer);
  value = RC_ALLOC(rc_value_with_memrefs_t, &preparse.parse);
  rc_preparse_alloc_memrefs(&value->memrefs, &preparse);

  rc_parse_value_internal(&value->value, &memaddr, &preparse.parse);
  value->value.has_memrefs = 1;

  rc_destroy_preparse_state(&preparse);
  return (preparse.parse.offset >= 0) ? &value->value : NULL;
}

static void rc_update_value_memrefs(rc_value_t* self, rc_peek_t peek, void* ud) {
  if (self->has_memrefs) {
    rc_value_with_memrefs_t* value = (rc_value_with_memrefs_t*)self;
    rc_update_memref_values(&value->memrefs, peek, ud);
  }
}

int rc_evaluate_value_typed(rc_value_t* self, rc_typed_value_t* value, rc_peek_t peek, void* ud) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int valid = 0;

  rc_update_value_memrefs(self, peek, ud);

  value->value.i32 = 0;
  value->type = RC_VALUE_TYPE_SIGNED;

  for (condset = self->conditions; condset != NULL; condset = condset->next) {
    memset(&eval_state, 0, sizeof(eval_state));
    eval_state.peek = peek;
    eval_state.peek_userdata = ud;

    rc_test_condset(condset, &eval_state);

    if (condset->is_paused)
      continue;

    if (eval_state.was_reset) {
      /* if any ResetIf condition was true, reset the hit counts
       * NOTE: ResetIf only affects the current condset when used in values!
       */
      rc_reset_condset(condset);
    }

    if (eval_state.measured_value.type != RC_VALUE_TYPE_NONE) {
      if (!valid) {
        /* capture the first valid measurement, which may be negative */
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
  }

  return valid;
}

int32_t rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, void* unused_L) {
  rc_typed_value_t result;
  int valid = rc_evaluate_value_typed(self, &result, peek, ud);

  (void)unused_L;

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

rc_value_t* rc_alloc_variable(const char* memaddr, size_t memaddr_len, rc_parse_state_t* parse) {
  rc_value_t** value_ptr = parse->variables;
  rc_value_t* value;
  const char* name;
  uint32_t measured_target;

  if (!value_ptr)
    return NULL;

  while (*value_ptr) {
    value = *value_ptr;
    if (strncmp(value->name, memaddr, memaddr_len) == 0 && value->name[memaddr_len] == 0)
      return value;

    value_ptr = &value->next;
  }

  /* capture name before calling parse as parse will update memaddr pointer */
  name = rc_alloc_str(parse, memaddr, memaddr_len);
  if (!name)
    return NULL;

  /* no match found, create a new entry */
  value = RC_ALLOC_SCRATCH(rc_value_t, parse);
  memset(value, 0, sizeof(value->value));
  value->value.size = RC_MEMSIZE_VARIABLE;
  value->next = NULL;

  /* the helper variable likely has a Measured condition. capture the current measured_target so we can restore it
   * after generating the variable so the variable's Measured target doesn't conflict with the rest of the trigger. */
  measured_target = parse->measured_target;
  rc_parse_value_internal(value, &memaddr, parse);
  parse->measured_target = measured_target;

  /* store name after calling parse as parse will set name to (unnamed) */
  value->name = name;

  *value_ptr = value;
  return value;
}

uint32_t rc_count_values(const rc_value_t* values) {
  uint32_t count = 0;
  while (values) {
    ++count;
    values = values->next;
  }

  return count;
}

void rc_update_values(rc_value_t* values, rc_peek_t peek, void* ud) {
  rc_typed_value_t result;

  rc_value_t* value = values;
  for (; value; value = value->next) {
    if (rc_evaluate_value_typed(value, &result, peek, ud)) {
      /* store the raw bytes and type to be restored by rc_typed_value_from_memref_value  */
      rc_update_memref_value(&value->value, result.value.u32);
      value->value.type = result.type;
    }
  }
}

void rc_reset_values(rc_value_t* values) {
  rc_value_t* value = values;

  for (; value; value = value->next)
    rc_reset_value(value);
}

void rc_typed_value_from_memref_value(rc_typed_value_t* value, const rc_memref_value_t* memref) {
  /* raw value is always u32, type can mark it as something else */
  value->value.u32 = memref->value;
  value->type = memref->type;
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

  if (amount->type != value->type && value->type != RC_VALUE_TYPE_NONE) {
    if (amount->type == RC_VALUE_TYPE_FLOAT)
      rc_typed_value_convert(value, RC_VALUE_TYPE_FLOAT);
    else
      amount = rc_typed_value_convert_into(&converted, amount, value->type);
  }

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

void rc_typed_value_modulus(rc_typed_value_t* value, const rc_typed_value_t* amount) {
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
          value->value.u32 %= amount->value.u32;
          return;
        case RC_VALUE_TYPE_SIGNED: /* integer math */
          value->value.i32 %= (int)amount->value.u32;
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
          value->value.i32 %= amount->value.i32;
          return;
        case RC_VALUE_TYPE_UNSIGNED: /* integer math */
          value->value.u32 %= (unsigned)amount->value.i32;
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
  value->value.f32 = (float)fmod(value->value.f32, amount->value.f32);
}

void rc_typed_value_combine(rc_typed_value_t* value, rc_typed_value_t* amount, uint8_t oper) {
  switch (oper) {
    case RC_OPERATOR_MULT:
      rc_typed_value_multiply(value, amount);
      break;

    case RC_OPERATOR_DIV:
      rc_typed_value_divide(value, amount);
      break;

    case RC_OPERATOR_AND:
      rc_typed_value_convert(value, RC_VALUE_TYPE_UNSIGNED);
      rc_typed_value_convert(amount, RC_VALUE_TYPE_UNSIGNED);
      value->value.u32 &= amount->value.u32;
      break;

    case RC_OPERATOR_XOR:
      rc_typed_value_convert(value, RC_VALUE_TYPE_UNSIGNED);
      rc_typed_value_convert(amount, RC_VALUE_TYPE_UNSIGNED);
      value->value.u32 ^= amount->value.u32;
      break;

    case RC_OPERATOR_MOD:
      rc_typed_value_modulus(value, amount);
      break;

    case RC_OPERATOR_ADD:
      rc_typed_value_add(value, amount);
      break;

    case RC_OPERATOR_SUB:
      rc_typed_value_negate(amount);
      rc_typed_value_add(value, amount);
      break;
  }
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
  rc_typed_value_t converted_value;
  if (value2->type != value1->type) {
    /* if either side is a float, convert both sides to float. otherwise, assume the signed-ness of the left side. */
    if (value2->type == RC_VALUE_TYPE_FLOAT)
      value1 = rc_typed_value_convert_into(&converted_value, value1, value2->type);
    else
      value2 = rc_typed_value_convert_into(&converted_value, value2, value1->type);
  }

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
