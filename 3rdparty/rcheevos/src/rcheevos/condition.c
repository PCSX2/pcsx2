#include "rc_internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int rc_test_condition_compare(uint32_t value1, uint32_t value2, uint8_t oper) {
  switch (oper) {
    case RC_OPERATOR_EQ: return value1 == value2;
    case RC_OPERATOR_NE: return value1 != value2;
    case RC_OPERATOR_LT: return value1 < value2;
    case RC_OPERATOR_LE: return value1 <= value2;
    case RC_OPERATOR_GT: return value1 > value2;
    case RC_OPERATOR_GE: return value1 >= value2;
    default: return 1;
  }
}

static uint8_t rc_condition_determine_comparator(const rc_condition_t* self) {
  switch (self->oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_NE:
    case RC_OPERATOR_LT:
    case RC_OPERATOR_LE:
    case RC_OPERATOR_GT:
    case RC_OPERATOR_GE:
      break;

    default:
      /* not a comparison. should not be getting compared. but if it is, legacy behavior was to return 1 */
      return RC_PROCESSING_COMPARE_ALWAYS_TRUE;
  }

  if ((self->operand1.type == RC_OPERAND_ADDRESS || self->operand1.type == RC_OPERAND_DELTA) &&
    /* TODO: allow modified memref comparisons */
      self->operand1.value.memref->value.memref_type == RC_MEMREF_TYPE_MEMREF && !rc_operand_is_float(&self->operand1)) {
    /* left side is an integer memory reference */
    int needs_translate = (self->operand1.size != self->operand1.value.memref->value.size);

    if (self->operand2.type == RC_OPERAND_CONST) {
      /* right side is a constant */
      if (self->operand1.type == RC_OPERAND_ADDRESS)
        return needs_translate ? RC_PROCESSING_COMPARE_MEMREF_TO_CONST_TRANSFORMED : RC_PROCESSING_COMPARE_MEMREF_TO_CONST;

      return needs_translate ? RC_PROCESSING_COMPARE_DELTA_TO_CONST_TRANSFORMED : RC_PROCESSING_COMPARE_DELTA_TO_CONST;
    }
    else if ((self->operand2.type == RC_OPERAND_ADDRESS || self->operand2.type == RC_OPERAND_DELTA) &&
             self->operand2.value.memref->value.memref_type == RC_MEMREF_TYPE_MEMREF && !rc_operand_is_float(&self->operand2)) {
      /* right side is an integer memory reference */
      const int is_same_memref = (self->operand1.value.memref == self->operand2.value.memref);
      needs_translate |= (self->operand2.size != self->operand2.value.memref->value.size);

      if (self->operand1.type == RC_OPERAND_ADDRESS) {
        if (self->operand2.type == RC_OPERAND_ADDRESS) {
          if (is_same_memref && !needs_translate) {
            /* comparing a memref to itself, will evaluate to a constant */
            return rc_test_condition_compare(0, 0, self->oper) ? RC_PROCESSING_COMPARE_ALWAYS_TRUE : RC_PROCESSING_COMPARE_ALWAYS_FALSE;
          }

          return needs_translate ? RC_PROCESSING_COMPARE_MEMREF_TO_MEMREF_TRANSFORMED : RC_PROCESSING_COMPARE_MEMREF_TO_MEMREF;
        }

        assert(self->operand2.type == RC_OPERAND_DELTA);

        if (is_same_memref) {
          /* delta comparison is optimized to compare with itself (for detecting change) */
          return needs_translate ? RC_PROCESSING_COMPARE_MEMREF_TO_DELTA_TRANSFORMED : RC_PROCESSING_COMPARE_MEMREF_TO_DELTA;
        }
      }
      else {
        assert(self->operand1.type == RC_OPERAND_DELTA);

        if (self->operand2.type == RC_OPERAND_ADDRESS) {
          if (is_same_memref) {
            /* delta comparison is optimized to compare with itself (for detecting change) */
            return needs_translate ? RC_PROCESSING_COMPARE_DELTA_TO_MEMREF_TRANSFORMED : RC_PROCESSING_COMPARE_DELTA_TO_MEMREF;
          }
        }
      }
    }
  }

  if (self->operand1.type == RC_OPERAND_CONST && self->operand2.type == RC_OPERAND_CONST) {
    /* comparing constants will always generate a constant result */
    return rc_test_condition_compare(self->operand1.value.num, self->operand2.value.num, self->oper) ?
        RC_PROCESSING_COMPARE_ALWAYS_TRUE : RC_PROCESSING_COMPARE_ALWAYS_FALSE;
  }

  return RC_PROCESSING_COMPARE_DEFAULT;
}

static int rc_parse_operator(const char** memaddr) {
  const char* oper = *memaddr;

  switch (*oper) {
    case '=':
      ++(*memaddr);
      (*memaddr) += (**memaddr == '=');
      return RC_OPERATOR_EQ;

    case '!':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_NE;
      }
      /* fall through */
    default:
      return RC_INVALID_OPERATOR;

    case '<':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_LE;
      }

      ++(*memaddr);
      return RC_OPERATOR_LT;

    case '>':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_GE;
      }

      ++(*memaddr);
      return RC_OPERATOR_GT;

    case '*':
      ++(*memaddr);
      return RC_OPERATOR_MULT;

    case '/':
      ++(*memaddr);
      return RC_OPERATOR_DIV;

    case '&':
      ++(*memaddr);
      return RC_OPERATOR_AND;

    case '^':
      ++(*memaddr);
      return RC_OPERATOR_XOR;

    case '%':
      ++(*memaddr);
      return RC_OPERATOR_MOD;

    case '+':
      ++(*memaddr);
      return RC_OPERATOR_ADD;

    case '-':
      ++(*memaddr);
      return RC_OPERATOR_SUB;

    case '\0':/* end of string */
    case '_': /* next condition */
    case 'S': /* next condset */
    case ')': /* end of macro */
    case '$': /* maximum of values */
      /* valid condition separator, condition may not have an operator */
      return RC_OPERATOR_NONE;
  }
}

void rc_condition_convert_to_operand(const rc_condition_t* condition, rc_operand_t* operand, rc_parse_state_t* parse) {
  if (condition->oper == RC_OPERATOR_NONE) {
    if (operand != &condition->operand1)
      memcpy(operand, &condition->operand1, sizeof(*operand));
  }
  else {
    uint8_t new_size = RC_MEMSIZE_32_BITS;
    if (rc_operand_is_float(&condition->operand1) || rc_operand_is_float(&condition->operand2))
      new_size = RC_MEMSIZE_FLOAT;

    /* NOTE: this makes the operand include the modification, but we have to also
     * leave the modification in the condition so the condition reflects the actual
     * definition. This doesn't affect the evaluation logic since this method is only
     * called for combining conditions and Measured, and the Measured handling function
     * ignores the operator assuming it's been handled by a modified memref chain */
    operand->value.memref = (rc_memref_t*)rc_alloc_modified_memref(parse,
      new_size, &condition->operand1, condition->oper, &condition->operand2);

    /* not actually an address, just a non-delta memref read */
    operand->type = operand->memref_access_type = RC_OPERAND_ADDRESS;

    operand->size = new_size;
  }
}

rc_condition_t* rc_parse_condition(const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t * self = RC_ALLOC(rc_condition_t, parse);
  rc_parse_condition_internal(self, memaddr, parse);
  return (parse->offset < 0) ? NULL : self;
}

void rc_parse_condition_internal(rc_condition_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux;
  int result;
  int can_modify = 0;

  aux = *memaddr;
  self->current_hits = 0;
  self->is_true = 0;
  self->optimized_comparator = RC_PROCESSING_COMPARE_DEFAULT;

  if (*aux != 0 && aux[1] == ':') {
    switch (*aux) {
      case 'p': case 'P': self->type = RC_CONDITION_PAUSE_IF; break;
      case 'r': case 'R': self->type = RC_CONDITION_RESET_IF; break;
      case 'a': case 'A': self->type = RC_CONDITION_ADD_SOURCE; can_modify = 1; break;
      case 'b': case 'B': self->type = RC_CONDITION_SUB_SOURCE; can_modify = 1; break;
      case 'c': case 'C': self->type = RC_CONDITION_ADD_HITS; break;
      case 'd': case 'D': self->type = RC_CONDITION_SUB_HITS; break;
      case 'n': case 'N': self->type = RC_CONDITION_AND_NEXT; break;
      case 'o': case 'O': self->type = RC_CONDITION_OR_NEXT; break;
      case 'm': case 'M': self->type = RC_CONDITION_MEASURED; break;
      case 'q': case 'Q': self->type = RC_CONDITION_MEASURED_IF; break;
      case 'i': case 'I': self->type = RC_CONDITION_ADD_ADDRESS; can_modify = 1; break;
      case 't': case 'T': self->type = RC_CONDITION_TRIGGER; break;
      case 'k': case 'K': self->type = RC_CONDITION_REMEMBER; can_modify = 1; break;
      case 'z': case 'Z': self->type = RC_CONDITION_RESET_NEXT_IF; break;
      case 'g': case 'G':
          parse->measured_as_percent = 1;
          self->type = RC_CONDITION_MEASURED;
          break;

      /* e f h j l s u v w x y */
      default:
        parse->offset = RC_INVALID_CONDITION_TYPE;
        return;
    }

    aux += 2;
  }
  else {
    self->type = RC_CONDITION_STANDARD;
  }

  result = rc_parse_operand(&self->operand1, &aux, parse);
  if (result < 0) {
    parse->offset = result;
    return;
  }

  result = rc_parse_operator(&aux);
  if (result < 0) {
    parse->offset = result;
    return;
  }

  self->oper = (uint8_t)result;

  if (self->oper == RC_OPERATOR_NONE) {
    /* non-modifying statements must have a second operand */
    if (!can_modify) {
      /* measured does not require a second operand when used in a value */
      if (self->type != RC_CONDITION_MEASURED && !parse->ignore_non_parse_errors) {
        parse->offset = RC_INVALID_OPERATOR;
        return;
      }
    }

    /* provide dummy operand of '1' and no required hits */
    rc_operand_set_const(&self->operand2, 1);
    self->required_hits = 0;
    *memaddr = aux;
    return;
  }

  if (can_modify && !rc_operator_is_modifying(self->oper)) {
    /* comparison operators are not valid on modifying statements */
    switch (self->type) {
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_ADD_ADDRESS:
        /* prevent parse errors on legacy achievements where a condition was present before changing the type */
        self->oper = RC_OPERATOR_NONE;
        break;

      default:
        if (!parse->ignore_non_parse_errors) {
          parse->offset = RC_INVALID_OPERATOR;
          return;
        }
        break;
    }
  }

  result = rc_parse_operand(&self->operand2, &aux, parse);
  if (result < 0) {
    parse->offset = result;
    return;
  }

  if (self->oper == RC_OPERATOR_NONE) {
    /* if operator is none, explicitly clear out the right side */
    rc_operand_set_const(&self->operand2, 0);
  }

  if (*aux == '(') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != ')') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return;
    }

    /* if operator is none, explicitly clear out the required hits */
    if (self->oper == RC_OPERATOR_NONE)
      self->required_hits = 0;
    else
      parse->has_required_hits = 1;

    aux = end + 1;
  }
  else if (*aux == '.') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != '.') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return;
    }

    /* if operator is none, explicitly clear out the required hits */
    if (self->oper == RC_OPERATOR_NONE)
      self->required_hits = 0;
    else
      parse->has_required_hits = 1;

    aux = end + 1;
  }
  else {
    self->required_hits = 0;
  }

  if (parse->buffer != 0)
    self->optimized_comparator = rc_condition_determine_comparator(self);

  *memaddr = aux;
}

void rc_condition_update_parse_state(rc_condition_t* condition, rc_parse_state_t* parse) {
  /* type of values in the chain are determined by the parent.
   * the last element of a chain is determined by the operand
   *
   * 1   + 1.5 + 1.75 + 1.0 =>   (int)1   +   (int)1   +   (int)1    + (float)1   = (float)4.0
   * 1.0 + 1.5 + 1.75 + 1.0 => (float)1.0 + (float)1.5 + (float)1.75 + (float)1.0 = (float)5.25
   * 1.0 + 1.5 + 1.75 + 1   => (float)1.0 + (float)1.5 + (float)1.75 +   (int)1   =   (int)5
   */

  switch (condition->type) {
    case RC_CONDITION_ADD_ADDRESS:
      if (condition->oper != RC_OPERAND_NONE)
        rc_condition_convert_to_operand(condition, &parse->indirect_parent, parse);
      else
        memcpy(&parse->indirect_parent, &condition->operand1, sizeof(parse->indirect_parent));

      break;

    case RC_CONDITION_ADD_SOURCE:
      if (parse->addsource_parent.type == RC_OPERAND_NONE) {
        rc_condition_convert_to_operand(condition, &parse->addsource_parent, parse);
      }
      else {
        rc_operand_t cond_operand;
        /* type determined by parent */
        const uint8_t new_size = rc_operand_is_float(&parse->addsource_parent) ? RC_MEMSIZE_FLOAT : RC_MEMSIZE_32_BITS;

        rc_condition_convert_to_operand(condition, &cond_operand, parse);
        rc_operand_addsource(&cond_operand, parse, new_size);
        memcpy(&parse->addsource_parent, &cond_operand, sizeof(cond_operand));
      }

      parse->addsource_oper = RC_OPERATOR_ADD_ACCUMULATOR;
      parse->indirect_parent.type = RC_OPERAND_NONE;
      break;

    case RC_CONDITION_SUB_SOURCE:
      if (parse->addsource_parent.type == RC_OPERAND_NONE) {
        rc_condition_convert_to_operand(condition, &parse->addsource_parent, parse);
        parse->addsource_oper = RC_OPERATOR_SUB_PARENT;
      }
      else {
        rc_operand_t cond_operand;
        /* type determined by parent */
        const uint8_t new_size = rc_operand_is_float(&parse->addsource_parent) ? RC_MEMSIZE_FLOAT : RC_MEMSIZE_32_BITS;

        if (parse->addsource_oper == RC_OPERATOR_ADD_ACCUMULATOR && !rc_operand_is_memref(&parse->addsource_parent)) {
          /* if the previous element was a constant we have to turn it into a memref by adding zero */
          rc_modified_memref_t* memref;
          rc_operand_t zero;
          rc_operand_set_const(&zero, 0);
          memref = rc_alloc_modified_memref(parse,
              parse->addsource_parent.size, &parse->addsource_parent, RC_OPERATOR_ADD_ACCUMULATOR, &zero);
          parse->addsource_parent.value.memref = (rc_memref_t*)memref;
          parse->addsource_parent.type = RC_OPERAND_ADDRESS;
        }
        else if (parse->addsource_oper == RC_OPERATOR_SUB_PARENT) {
          /* if the previous element was also a SubSource, we have to insert a 0 and start subtracting from there */
          rc_modified_memref_t* negate;
          rc_operand_t zero;

          if (rc_operand_is_float(&parse->addsource_parent))
            rc_operand_set_float_const(&zero, 0.0);
          else
            rc_operand_set_const(&zero, 0);

          negate = rc_alloc_modified_memref(parse, new_size, &parse->addsource_parent, RC_OPERATOR_SUB_PARENT, &zero);
          parse->addsource_parent.value.memref = (rc_memref_t*)negate;
          parse->addsource_parent.size = zero.size;
        }

        /* subtract the condition from the chain */
        parse->addsource_oper = rc_operand_is_memref(&parse->addsource_parent) ? RC_OPERATOR_SUB_ACCUMULATOR : RC_OPERATOR_SUB_PARENT;
        rc_condition_convert_to_operand(condition, &cond_operand, parse);
        rc_operand_addsource(&cond_operand, parse, new_size);
        memcpy(&parse->addsource_parent, &cond_operand, sizeof(cond_operand));

        /* indicate the next value can be added to the chain */
        parse->addsource_oper = RC_OPERATOR_ADD_ACCUMULATOR;
      }

      parse->indirect_parent.type = RC_OPERAND_NONE;
      break;

    case RC_CONDITION_REMEMBER:
      if (condition->operand1.type == RC_OPERAND_RECALL &&
          condition->oper == RC_OPERATOR_NONE &&
          parse->addsource_parent.type == RC_OPERAND_NONE &&
          parse->indirect_parent.type == RC_OPERAND_NONE) {
        /* Remembering {recall} without any modifications is a no-op */
        break;
      }

      rc_condition_convert_to_operand(condition, &condition->operand1, parse);

      if (parse->addsource_parent.type != RC_OPERAND_NONE) {
        /* type determined by leaf */
        rc_operand_addsource(&condition->operand1, parse, condition->operand1.size);
        condition->operand1.is_combining = 1;
      }

      memcpy(&parse->remember, &condition->operand1, sizeof(parse->remember));

      parse->addsource_parent.type = RC_OPERAND_NONE;
      parse->indirect_parent.type = RC_OPERAND_NONE;
      break;

    case RC_CONDITION_MEASURED:
      /* Measured condition can have modifiers in values */
      if (parse->is_value) {
        switch (condition->oper) {
          case RC_OPERATOR_AND:
          case RC_OPERATOR_XOR:
          case RC_OPERATOR_DIV:
          case RC_OPERATOR_MULT:
          case RC_OPERATOR_MOD:
          case RC_OPERATOR_ADD:
          case RC_OPERATOR_SUB:
            rc_condition_convert_to_operand(condition, &condition->operand1, parse);
            break;

          default:
            break;
        }
      }

      /* fallthrough */ /* to default */

    default:
      if (parse->addsource_parent.type != RC_OPERAND_NONE) {
        /* type determined by leaf */
        if (parse->addsource_oper == RC_OPERATOR_ADD_ACCUMULATOR)
          parse->addsource_oper = RC_OPERATOR_ADD;

        rc_operand_addsource(&condition->operand1, parse, condition->operand1.size);
        condition->operand1.is_combining = 1;

        if (parse->buffer)
          condition->optimized_comparator = rc_condition_determine_comparator(condition);
      }

      parse->addsource_parent.type = RC_OPERAND_NONE;
      parse->indirect_parent.type = RC_OPERAND_NONE;
      break;
  }
}

static const rc_modified_memref_t* rc_operand_get_modified_memref(const rc_operand_t* operand) {
  if (!rc_operand_is_memref(operand))
    return NULL;

  if (operand->value.memref->value.memref_type != RC_MEMREF_TYPE_MODIFIED_MEMREF)
    return NULL;

  return (rc_modified_memref_t*)operand->value.memref;
}

/* rc_condition_update_parse_state will mutate the operand1 to point at the modified memref
 * containing the accumulated result up until that point. this function returns the original
 * unmodified operand1 from parsing the definition.
 */
const rc_operand_t* rc_condition_get_real_operand1(const rc_condition_t* self) {
  const rc_operand_t* operand = &self->operand1;
  const rc_modified_memref_t* modified_memref;

  if (operand->is_combining) {
    /* operand = "previous + current" - extract current */
    const rc_modified_memref_t* combining_modified_memref = rc_operand_get_modified_memref(operand);
    if (combining_modified_memref)
      operand = &combining_modified_memref->modifier;
  }

  /* modifying operators are merged into an rc_modified_memref_t
   * if operand1 is a modified memref, assume it's been merged with the right side and
   * extract the parent which is the actual operand1. */
  modified_memref = rc_operand_get_modified_memref(operand);
  if (modified_memref) {
    if (modified_memref->modifier_type == RC_OPERATOR_INDIRECT_READ) {
      /* if the modified memref is an indirect read, the parent is the indirect
       * address and the modifier is the offset. the actual size and address are
       * stored in the modified memref - use it */
    } else if (rc_operator_is_modifying(self->oper) && self->oper != RC_OPERATOR_NONE) {
      /* operand = "parent*modifier" - extract parent.modifier will already be in operand2 */
      operand = &modified_memref->parent;
    }
  }

  return operand;
}

int rc_condition_is_combining(const rc_condition_t* self) {
  switch (self->type) {
    case RC_CONDITION_STANDARD:
    case RC_CONDITION_PAUSE_IF:
    case RC_CONDITION_RESET_IF:
    case RC_CONDITION_MEASURED_IF:
    case RC_CONDITION_TRIGGER:
    case RC_CONDITION_MEASURED:
      return 0;

    default:
      return 1;
  }
}

static int rc_test_condition_compare_memref_to_const(rc_condition_t* self) {
  const uint32_t value1 = self->operand1.value.memref->value.value;
  const uint32_t value2 = self->operand2.value.num;
  assert(self->operand1.size == self->operand1.value.memref->value.size);
  return rc_test_condition_compare(value1, value2, self->oper);
}

static int rc_test_condition_compare_delta_to_const(rc_condition_t* self) {
  const rc_memref_value_t* memref1 = &self->operand1.value.memref->value;
  const uint32_t value1 = (memref1->changed) ? memref1->prior : memref1->value;
  const uint32_t value2 = self->operand2.value.num;
  assert(self->operand1.size == self->operand1.value.memref->value.size);
  return rc_test_condition_compare(value1, value2, self->oper);
}

static int rc_test_condition_compare_memref_to_memref(rc_condition_t* self) {
  const uint32_t value1 = self->operand1.value.memref->value.value;
  const uint32_t value2 = self->operand2.value.memref->value.value;
  assert(self->operand1.size == self->operand1.value.memref->value.size);
  assert(self->operand2.size == self->operand2.value.memref->value.size);
  return rc_test_condition_compare(value1, value2, self->oper);
}

static int rc_test_condition_compare_memref_to_delta(rc_condition_t* self) {
  const rc_memref_value_t* memref = &self->operand1.value.memref->value;
  assert(self->operand1.value.memref == self->operand2.value.memref);
  assert(self->operand1.size == self->operand1.value.memref->value.size);
  assert(self->operand2.size == self->operand2.value.memref->value.size);

  if (memref->changed)
    return rc_test_condition_compare(memref->value, memref->prior, self->oper);

  switch (self->oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_GE:
    case RC_OPERATOR_LE:
      return 1;

    default:
      return 0;
  }
}

static int rc_test_condition_compare_delta_to_memref(rc_condition_t* self) {
  const rc_memref_value_t* memref = &self->operand1.value.memref->value;
  assert(self->operand1.value.memref == self->operand2.value.memref);
  assert(self->operand1.size == self->operand1.value.memref->value.size);
  assert(self->operand2.size == self->operand2.value.memref->value.size);

  if (memref->changed)
    return rc_test_condition_compare(memref->prior, memref->value, self->oper);

  switch (self->oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_GE:
    case RC_OPERATOR_LE:
      return 1;

    default:
      return 0;
  }
}

static int rc_test_condition_compare_memref_to_const_transformed(rc_condition_t* self) {
  rc_typed_value_t value1;
  const uint32_t value2 = self->operand2.value.num;

  value1.type = RC_VALUE_TYPE_UNSIGNED;
  value1.value.u32 = self->operand1.value.memref->value.value;
  rc_transform_memref_value(&value1, self->operand1.size);

  return rc_test_condition_compare(value1.value.u32, value2, self->oper);
}

static int rc_test_condition_compare_delta_to_const_transformed(rc_condition_t* self) {
  rc_typed_value_t value1;
  const rc_memref_value_t* memref1 = &self->operand1.value.memref->value;
  const uint32_t value2 = self->operand2.value.num;

  value1.type = RC_VALUE_TYPE_UNSIGNED;
  value1.value.u32 = (memref1->changed) ? memref1->prior : memref1->value;
  rc_transform_memref_value(&value1, self->operand1.size);

  return rc_test_condition_compare(value1.value.u32, value2, self->oper);
}

static int rc_test_condition_compare_memref_to_memref_transformed(rc_condition_t* self) {
  rc_typed_value_t value1, value2;

  value1.type = RC_VALUE_TYPE_UNSIGNED;
  value1.value.u32 = self->operand1.value.memref->value.value;
  rc_transform_memref_value(&value1, self->operand1.size);

  value2.type = RC_VALUE_TYPE_UNSIGNED;
  value2.value.u32 = self->operand2.value.memref->value.value;
  rc_transform_memref_value(&value2, self->operand2.size);

  return rc_test_condition_compare(value1.value.u32, value2.value.u32, self->oper);
}

static int rc_test_condition_compare_memref_to_delta_transformed(rc_condition_t* self) {
  const rc_memref_value_t* memref = &self->operand1.value.memref->value;
  assert(self->operand1.value.memref == self->operand2.value.memref);

  if (memref->changed) {
    rc_typed_value_t value1, value2;

    value1.type = RC_VALUE_TYPE_UNSIGNED;
    value1.value.u32 = memref->value;
    rc_transform_memref_value(&value1, self->operand1.size);

    value2.type = RC_VALUE_TYPE_UNSIGNED;
    value2.value.u32 = memref->prior;
    rc_transform_memref_value(&value2, self->operand2.size);

    return rc_test_condition_compare(value1.value.u32, value2.value.u32, self->oper);
  }

  switch (self->oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_GE:
    case RC_OPERATOR_LE:
      return 1;

    default:
      return 0;
  }
}

static int rc_test_condition_compare_delta_to_memref_transformed(rc_condition_t* self) {
  const rc_memref_value_t* memref = &self->operand1.value.memref->value;
  assert(self->operand1.value.memref == self->operand2.value.memref);

  if (memref->changed) {
    rc_typed_value_t value1, value2;

    value1.type = RC_VALUE_TYPE_UNSIGNED;
    value1.value.u32 = memref->prior;
    rc_transform_memref_value(&value1, self->operand1.size);

    value2.type = RC_VALUE_TYPE_UNSIGNED;
    value2.value.u32 = memref->value;
    rc_transform_memref_value(&value2, self->operand2.size);

    return rc_test_condition_compare(value1.value.u32, value2.value.u32, self->oper);
  }

  switch (self->oper) {
    case RC_OPERATOR_EQ:
    case RC_OPERATOR_GE:
    case RC_OPERATOR_LE:
      return 1;

    default:
      return 0;
  }
}

int rc_test_condition(rc_condition_t* self, rc_eval_state_t* eval_state) {
  rc_typed_value_t value1, value2;

  /* use an optimized comparator whenever possible */
  switch (self->optimized_comparator) {
    case RC_PROCESSING_COMPARE_MEMREF_TO_CONST:
      return rc_test_condition_compare_memref_to_const(self);
    case RC_PROCESSING_COMPARE_MEMREF_TO_DELTA:
      return rc_test_condition_compare_memref_to_delta(self);
    case RC_PROCESSING_COMPARE_MEMREF_TO_MEMREF:
      return rc_test_condition_compare_memref_to_memref(self);
    case RC_PROCESSING_COMPARE_DELTA_TO_CONST:
      return rc_test_condition_compare_delta_to_const(self);
    case RC_PROCESSING_COMPARE_DELTA_TO_MEMREF:
      return rc_test_condition_compare_delta_to_memref(self);
    case RC_PROCESSING_COMPARE_MEMREF_TO_CONST_TRANSFORMED:
      return rc_test_condition_compare_memref_to_const_transformed(self);
    case RC_PROCESSING_COMPARE_MEMREF_TO_DELTA_TRANSFORMED:
      return rc_test_condition_compare_memref_to_delta_transformed(self);
    case RC_PROCESSING_COMPARE_MEMREF_TO_MEMREF_TRANSFORMED:
      return rc_test_condition_compare_memref_to_memref_transformed(self);
    case RC_PROCESSING_COMPARE_DELTA_TO_CONST_TRANSFORMED:
      return rc_test_condition_compare_delta_to_const_transformed(self);
    case RC_PROCESSING_COMPARE_DELTA_TO_MEMREF_TRANSFORMED:
      return rc_test_condition_compare_delta_to_memref_transformed(self);
    case RC_PROCESSING_COMPARE_ALWAYS_TRUE:
      return 1;
    case RC_PROCESSING_COMPARE_ALWAYS_FALSE:
      return 0;
    default:
      rc_evaluate_operand(&value1, &self->operand1, eval_state);
      break;
  }

  rc_evaluate_operand(&value2, &self->operand2, eval_state);

  return rc_typed_value_compare(&value1, &value2, self->oper);
}

void rc_evaluate_condition_value(rc_typed_value_t* value, rc_condition_t* self, rc_eval_state_t* eval_state) {
  rc_typed_value_t amount;

  rc_evaluate_operand(value, &self->operand1, eval_state);
  rc_evaluate_operand(&amount, &self->operand2, eval_state);

  rc_typed_value_combine(value, &amount, self->oper);
}
