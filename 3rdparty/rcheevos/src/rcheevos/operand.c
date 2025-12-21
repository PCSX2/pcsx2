#include "rc_internal.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

static int rc_parse_operand_func_call(rc_operand_t* self, const char** memaddr) {
  const char* aux = *memaddr;

  if (*aux++ != '@') {
    return RC_INVALID_FUNC_OPERAND;
  }

  if (!isalpha((unsigned char)*aux)) {
    return RC_INVALID_FUNC_OPERAND;
  }

  while (isalnum((unsigned char)*aux) || *aux == '_') {
    aux++;
  }

  self->type = RC_OPERAND_FUNC;
  self->size = RC_MEMSIZE_32_BITS;
  self->memref_access_type = RC_OPERAND_ADDRESS;
  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_variable(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  size_t i;
  char varName[RC_VALUE_MAX_NAME_LENGTH + 1] = { 0 };

  for (i = 0; i < RC_VALUE_MAX_NAME_LENGTH && *aux != '}'; i++) {
    if (!rc_is_valid_variable_character(*aux, i == 0))
      return RC_INVALID_VARIABLE_NAME;

    varName[i] = *aux++;
  }

  if (i == 0)
    return RC_INVALID_VARIABLE_NAME;

  if (*aux != '}')
    return RC_INVALID_VARIABLE_NAME;

  ++aux;

  if (strcmp(varName, "recall") == 0) {
    if (parse->remember.type == RC_OPERAND_NONE) {
      self->value.memref = NULL;
      self->size = RC_MEMSIZE_32_BITS;
      self->memref_access_type = RC_OPERAND_ADDRESS;
    }
    else {
      memcpy(self, &parse->remember, sizeof(*self));
      self->is_combining = 0;
      self->memref_access_type = self->type;
    }
    self->type = RC_OPERAND_RECALL;
  }
  else { /* process named variable when feature is available.*/
    return RC_UNKNOWN_VARIABLE_NAME;
  }

  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_memory(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  uint32_t address;
  uint8_t size;
  int ret;

  switch (*aux) {
    case 'd': case 'D':
      self->type = RC_OPERAND_DELTA;
      ++aux;
      break;

    case 'p': case 'P':
      self->type = RC_OPERAND_PRIOR;
      ++aux;
      break;

    case 'b': case 'B':
      self->type = RC_OPERAND_BCD;
      ++aux;
      break;

    case '~':
      self->type = RC_OPERAND_INVERTED;
      ++aux;
      break;

    default:
      self->type = RC_OPERAND_ADDRESS;
      break;
  }

  self->memref_access_type = self->type;

  ret = rc_parse_memref(&aux, &self->size, &address);
  if (ret != RC_OK)
    return ret;

  size = rc_memref_shared_size(self->size);
  if (size != self->size && self->type == RC_OPERAND_PRIOR) {
    /* if the shared size differs from the requested size and it's a prior operation, we
     * have to check to make sure both sizes use the same mask, or the prior value may be
     * updated when bits outside the mask are modified, which would make it look like the
     * current value once the mask is applied. if the mask differs, create a new
     * non-shared record for tracking the prior data. */
    if (rc_memref_mask(size) != rc_memref_mask(self->size))
      size = self->size;
  }

  if (parse->indirect_parent.type != RC_OPERAND_NONE) {
    rc_operand_t offset;
    rc_operand_set_const(&offset, address);

    self->value.memref = (rc_memref_t*)rc_alloc_modified_memref(parse,
      size, &parse->indirect_parent, RC_OPERATOR_INDIRECT_READ, &offset);
  }
  else {
    self->value.memref = rc_alloc_memref(parse, address, size);
  }

  if (parse->offset < 0)
    return parse->offset;

  *memaddr = aux;
  return RC_OK;
}

int rc_parse_operand(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  char* end;
  int ret;
  unsigned long value;
  int negative;
  int allow_decimal = 0;

  self->is_combining = 0;

  switch (*aux) {
    case 'h': case 'H': /* hex constant */
      if (aux[2] == 'x' || aux[2] == 'X') {
        /* H0x1234 is a typo - either H1234 or 0xH1234 was probably meant */
        return RC_INVALID_CONST_OPERAND;
      }

      value = strtoul(++aux, &end, 16);
      if (end == aux)
        return RC_INVALID_CONST_OPERAND;

      if (value > 0xffffffffU)
        value = 0xffffffffU;

      rc_operand_set_const(self, (unsigned)value);

      aux = end;
      break;

    case 'f': case 'F': /* floating point constant */
      if (isalpha((unsigned char)aux[1])) {
        ret = rc_parse_operand_memory(self, &aux, parse);

        if (ret < 0)
          return ret;

        break;
      }
      allow_decimal = 1;
      /* fall through */
    case 'v': case 'V': /* signed integer constant */
      ++aux;
      /* fall through */
    case '+': case '-': /* signed integer constant */
      negative = 0;
      if (*aux == '-') {
        negative = 1;
        ++aux;
      }
      else if (*aux == '+') {
        ++aux;
      }

      value = strtoul(aux, &end, 10);

      if (*end == '.' && allow_decimal) {
        /* custom parser for decimal values to ignore locale */
        unsigned long shift = 1;
        unsigned long fraction = 0;
        double dbl_val;

        aux = end + 1;
        if (*aux < '0' || *aux > '9')
          return RC_INVALID_FP_OPERAND;

        do {
          /* only keep as many digits as will fit in a 32-bit value to prevent overflow.
           * float only has around 7 digits of precision anyway. */
          if (shift < 1000000000) {
            fraction *= 10;
            fraction += (*aux - '0');
            shift *= 10;
          }
          ++aux;
        } while (*aux >= '0' && *aux <= '9');

        if (fraction != 0) {
          /* non-zero fractional part, convert to double and merge in integer portion */
          const double dbl_fraction = ((double)fraction) / ((double)shift);
          if (negative)
            dbl_val = ((double)(-((long)value))) - dbl_fraction;
          else
            dbl_val = (double)value + dbl_fraction;
        }
        else {
          /* fractional part is 0, just convert the integer portion */
          if (negative)
            dbl_val = (double)(-((long)value));
          else
            dbl_val = (double)value;
        }

        rc_operand_set_float_const(self, dbl_val);
      }
      else {
        /* not a floating point value, make sure something was read and advance the read pointer */
        if (end == aux)
          return allow_decimal ? RC_INVALID_FP_OPERAND : RC_INVALID_CONST_OPERAND;

        aux = end;

        if (value > 0x7fffffffU)
          value = 0x7fffffffU;

        if (negative)
          rc_operand_set_const(self, (unsigned)(-((long)value)));
        else
          rc_operand_set_const(self, (unsigned)value);
      }
      break;
    case '{': /* variable */
      ++aux;
      ret = rc_parse_operand_variable(self, &aux, parse);
      if (ret < 0)
        return ret;

      break;

    case '0':
      if (aux[1] == 'x' || aux[1] == 'X') { /* hex integer constant */
        /* fallthrough */ /* to default */
    default:
        ret = rc_parse_operand_memory(self, &aux, parse);

        if (ret < 0)
          return ret;

        break;
      }
      /* fallthrough */ /* to case '1' for case '0' where not '0x' */
    case '1': case '2': case '3': case '4': case '5': /* unsigned integer constant */
    case '6': case '7': case '8': case '9':
      value = strtoul(aux, &end, 10);
      if (end == aux)
        return RC_INVALID_CONST_OPERAND;

      if (value > 0xffffffffU)
        value = 0xffffffffU;

      rc_operand_set_const(self, (unsigned)value);

      aux = end;
      break;

    case '@':
      ret = rc_parse_operand_func_call(self, &aux);

      if (ret < 0)
        return ret;

      break;
  }

  *memaddr = aux;
  return RC_OK;
}

void rc_operand_set_const(rc_operand_t* self, uint32_t value) {
  self->size = RC_MEMSIZE_32_BITS;
  self->type = RC_OPERAND_CONST;
  self->memref_access_type = RC_OPERAND_NONE;
  self->value.num = value;
}

void rc_operand_set_float_const(rc_operand_t* self, double value) {
  self->size = RC_MEMSIZE_FLOAT;
  self->type = RC_OPERAND_FP;
  self->memref_access_type = RC_OPERAND_NONE;
  self->value.dbl = value;
}

int rc_operands_are_equal(const rc_operand_t* left, const rc_operand_t* right) {
  if (left == right)
    return 1;

  if (left->type != right->type)
    return 0;

  switch (left->type) {
    case RC_OPERAND_CONST:
      return (left->value.num == right->value.num);
    case RC_OPERAND_FP:
      return (left->value.dbl == right->value.dbl);
    case RC_OPERAND_RECALL:
      return (left->value.memref == right->value.memref);
    default:
      break;
  }

  /* comparing two memrefs - look for exact matches on type and size */
  if (left->size != right->size || left->value.memref->value.memref_type != right->value.memref->value.memref_type)
    return 0;

  switch (left->value.memref->value.memref_type) {
    case RC_MEMREF_TYPE_MODIFIED_MEMREF:
    {
      const rc_modified_memref_t* left_memref = (const rc_modified_memref_t*)left->value.memref;
      const rc_modified_memref_t* right_memref = (const rc_modified_memref_t*)right->value.memref;
      return (left_memref->modifier_type == right_memref->modifier_type &&
              left_memref->depth == right_memref->depth &&
              rc_operands_are_equal(&left_memref->modifier, &right_memref->modifier) &&
              rc_operands_are_equal(&left_memref->parent, &right_memref->parent) &&
              1 == 1
        );
    }

    default:
      return (left->value.memref->address == right->value.memref->address &&
              left->value.memref->value.size == right->value.memref->value.size);
  }
}

int rc_operator_is_modifying(int oper) {
  switch (oper) {
    case RC_OPERATOR_AND:
    case RC_OPERATOR_XOR:
    case RC_OPERATOR_DIV:
    case RC_OPERATOR_MULT:
    case RC_OPERATOR_MOD:
    case RC_OPERATOR_ADD:
    case RC_OPERATOR_SUB:
    case RC_OPERATOR_NONE: /* NONE operator implies "* 1" */
      return 1;

    default:
      return 0;
  }
}

int rc_memsize_is_float(uint8_t size) {
  switch (size) {
    case RC_MEMSIZE_FLOAT:
    case RC_MEMSIZE_FLOAT_BE:
    case RC_MEMSIZE_DOUBLE32:
    case RC_MEMSIZE_DOUBLE32_BE:
    case RC_MEMSIZE_MBF32:
    case RC_MEMSIZE_MBF32_LE:
      return 1;

    default:
      return 0;
  }
}

int rc_operand_is_float_memref(const rc_operand_t* self) {
  if (!rc_operand_is_memref(self))
    return 0;

  if (self->value.memref->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
    const rc_modified_memref_t* memref = (const rc_modified_memref_t*)self->value.memref;
    if (memref->modifier_type != RC_OPERATOR_INDIRECT_READ)
      return rc_memsize_is_float(self->value.memref->value.size);
  }

  return rc_memsize_is_float(self->size);
}

int rc_operand_type_is_memref(uint8_t type) {
  switch (type) {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_FUNC:
    case RC_OPERAND_RECALL:
      return 0;

    default:
      return 1;
  }
}

int rc_operand_type_is_transform(uint8_t type) {
  switch (type) {
    case RC_OPERAND_BCD:
    case RC_OPERAND_INVERTED:
      return 1;

    default:
      return 0;
  }
}

int rc_operand_is_memref(const rc_operand_t* self) {
  return rc_operand_type_is_memref(self->type);
}

int rc_operand_is_recall(const rc_operand_t* self) {
  switch (self->type) {
    case RC_OPERAND_RECALL:
      return 1;

    default:
      return 0;
  }
}

int rc_operand_is_float(const rc_operand_t* self) {
  if (self->type == RC_OPERAND_FP)
    return 1;

  if (self->type == RC_OPERAND_RECALL)
    return rc_memsize_is_float(self->size);

  return rc_operand_is_float_memref(self);
}

static uint32_t rc_transform_operand_value(uint32_t value, const rc_operand_t* self) {
  switch (self->type)
  {
    case RC_OPERAND_BCD:
      switch (self->size)
      {
        case RC_MEMSIZE_8_BITS:
          value = ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_16_BITS:
        case RC_MEMSIZE_16_BITS_BE:
          value = ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_24_BITS:
        case RC_MEMSIZE_24_BITS_BE:
          value = ((value >> 20) & 0x0f) * 100000
                + ((value >> 16) & 0x0f) * 10000
                + ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_32_BITS:
        case RC_MEMSIZE_32_BITS_BE:
        case RC_MEMSIZE_VARIABLE:
          value = ((value >> 28) & 0x0f) * 10000000
                + ((value >> 24) & 0x0f) * 1000000
                + ((value >> 20) & 0x0f) * 100000
                + ((value >> 16) & 0x0f) * 10000
                + ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        default:
          break;
      }
      break;

    case RC_OPERAND_INVERTED:
      switch (self->size)
      {
        case RC_MEMSIZE_LOW:
        case RC_MEMSIZE_HIGH:
          value ^= 0x0f;
          break;

        case RC_MEMSIZE_8_BITS:
          value ^= 0xff;
          break;

        case RC_MEMSIZE_16_BITS:
        case RC_MEMSIZE_16_BITS_BE:
          value ^= 0xffff;
          break;

        case RC_MEMSIZE_24_BITS:
        case RC_MEMSIZE_24_BITS_BE:
          value ^= 0xffffff;
          break;

        case RC_MEMSIZE_32_BITS:
        case RC_MEMSIZE_32_BITS_BE:
        case RC_MEMSIZE_VARIABLE:
          value ^= 0xffffffff;
          break;

        default:
          value ^= 0x01;
          break;
      }
      break;

    default:
      break;
  }

  return value;
}

void rc_operand_addsource(rc_operand_t* self, rc_parse_state_t* parse, uint8_t new_size) {
  rc_modified_memref_t* modified_memref;

  if ((self->type == RC_OPERAND_DELTA || self->type == RC_OPERAND_PRIOR) &&
        self->type == parse->addsource_parent.type) {
    /* if adding prev(x) and prev(y), just add x and y and take the prev of that.
     * same for adding prior(x) and prior(y). */
    rc_operand_t modifier;
    memcpy(&modifier, self, sizeof(modifier));
    modifier.type = parse->addsource_parent.type = RC_OPERAND_ADDRESS;

    modified_memref = rc_alloc_modified_memref(parse,
        new_size, &parse->addsource_parent, parse->addsource_oper, &modifier);
  }
  else {
    modified_memref = rc_alloc_modified_memref(parse,
        new_size, &parse->addsource_parent, parse->addsource_oper, self);

    /* the modified memref will contain the combination of modified values, take the current value from that */
    self->type = self->memref_access_type = RC_OPERAND_ADDRESS;
  }

  self->value.memref = (rc_memref_t*)modified_memref;

  /* result of an AddSource operation is always a 32-bit integer (even if parent or modifier is a float) */
  self->size = RC_MEMSIZE_32_BITS;
}

void rc_evaluate_operand(rc_typed_value_t* result, const rc_operand_t* self, rc_eval_state_t* eval_state) {
  /* step 1: read memory */
  switch (self->type) {
    case RC_OPERAND_CONST:
      result->type = RC_VALUE_TYPE_UNSIGNED;
      result->value.u32 = self->value.num;
      return;

    case RC_OPERAND_FP:
      result->type = RC_VALUE_TYPE_FLOAT;
      result->value.f32 = (float)self->value.dbl;
      return;

    case RC_OPERAND_FUNC:
      /* this feature was never actualized */
      result->type = RC_VALUE_TYPE_UNSIGNED;
      result->value.u32 = 0;
      return;

    case RC_OPERAND_RECALL:
      if (!rc_operand_type_is_memref(self->memref_access_type)) {
        rc_operand_t recall;
        memcpy(&recall, self, sizeof(recall));
        recall.type = self->memref_access_type;
        rc_evaluate_operand(result, &recall, eval_state);
        return;
      }

      if (!self->value.memref) {
        result->type = RC_VALUE_TYPE_UNSIGNED;
        result->value.u32 = 0;
        return;
      }

      rc_get_memref_value(result, self->value.memref, self->memref_access_type);
      break;

    default:
      rc_get_memref_value(result, self->value.memref, self->type);
      break;
  }

  /* step 2: convert read memory to desired format */
  rc_transform_memref_value(result, self->size);

  /* step 3: apply logic (BCD/invert) */
  if (result->type == RC_VALUE_TYPE_UNSIGNED)
    result->value.u32 = rc_transform_operand_value(result->value.u32, self);
}
