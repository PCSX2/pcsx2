#include "rc_internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */
#include <math.h>   /* INFINITY/NAN */

#define MEMREF_PLACEHOLDER_ADDRESS 0xFFFFFFFF

rc_memref_t* rc_alloc_memref(rc_parse_state_t* parse, uint32_t address, uint8_t size, uint8_t is_indirect) {
  rc_memref_t** next_memref;
  rc_memref_t* memref;

  if (!is_indirect) {
    /* attempt to find an existing memref that can be shared */
    next_memref = parse->first_memref;
    while (*next_memref) {
      memref = *next_memref;
      if (!memref->value.is_indirect && memref->address == address && memref->value.size == size)
        return memref;

      next_memref = &memref->next;
    }

    /* no match found, create a new entry */
    memref = RC_ALLOC_SCRATCH(rc_memref_t, parse);
    *next_memref = memref;
  }
  else {
    /* indirect references always create a new entry because we can't guarantee that the 
     * indirection amount will be the same between references. because they aren't shared,
     * don't bother putting them in the chain.
     */
    memref = RC_ALLOC(rc_memref_t, parse);
  }

  memset(memref, 0, sizeof(*memref));
  memref->address = address;
  memref->value.size = size;
  memref->value.is_indirect = is_indirect;

  return memref;
}

int rc_parse_memref(const char** memaddr, uint8_t* size, uint32_t* address) {
  const char* aux = *memaddr;
  char* end;
  unsigned long value;

  if (aux[0] == '0') {
    if (aux[1] != 'x' && aux[1] != 'X')
      return RC_INVALID_MEMORY_OPERAND;

    aux += 2;
    switch (*aux++) {
      /* ordered by estimated frequency in case compiler doesn't build a jump table */
      case 'h': case 'H': *size = RC_MEMSIZE_8_BITS; break;
      case ' ':           *size = RC_MEMSIZE_16_BITS; break;
      case 'x': case 'X': *size = RC_MEMSIZE_32_BITS; break;

      case 'm': case 'M': *size = RC_MEMSIZE_BIT_0; break;
      case 'n': case 'N': *size = RC_MEMSIZE_BIT_1; break;
      case 'o': case 'O': *size = RC_MEMSIZE_BIT_2; break;
      case 'p': case 'P': *size = RC_MEMSIZE_BIT_3; break;
      case 'q': case 'Q': *size = RC_MEMSIZE_BIT_4; break;
      case 'r': case 'R': *size = RC_MEMSIZE_BIT_5; break;
      case 's': case 'S': *size = RC_MEMSIZE_BIT_6; break;
      case 't': case 'T': *size = RC_MEMSIZE_BIT_7; break;
      case 'l': case 'L': *size = RC_MEMSIZE_LOW; break;
      case 'u': case 'U': *size = RC_MEMSIZE_HIGH; break;
      case 'k': case 'K': *size = RC_MEMSIZE_BITCOUNT; break;
      case 'w': case 'W': *size = RC_MEMSIZE_24_BITS; break;
      case 'g': case 'G': *size = RC_MEMSIZE_32_BITS_BE; break;
      case 'i': case 'I': *size = RC_MEMSIZE_16_BITS_BE; break;
      case 'j': case 'J': *size = RC_MEMSIZE_24_BITS_BE; break;

      /* case 'v': case 'V': */
      /* case 'y': case 'Y': 64 bit? */
      /* case 'z': case 'Z': 128 bit? */

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        /* legacy support - addresses without a size prefix are assumed to be 16-bit */
        aux--;
        *size = RC_MEMSIZE_16_BITS;
        break;

      default:
        return RC_INVALID_MEMORY_OPERAND;
    }
  }
  else if (aux[0] == 'f' || aux[0] == 'F') {
    ++aux;
    switch (*aux++) {
      case 'f': case 'F': *size = RC_MEMSIZE_FLOAT; break;
      case 'b': case 'B': *size = RC_MEMSIZE_FLOAT_BE; break;
      case 'm': case 'M': *size = RC_MEMSIZE_MBF32; break;
      case 'l': case 'L': *size = RC_MEMSIZE_MBF32_LE; break;

      default:
        return RC_INVALID_FP_OPERAND;
    }
  }
  else {
    return RC_INVALID_MEMORY_OPERAND;
  }

  value = strtoul(aux, &end, 16);

  if (end == aux)
    return RC_INVALID_MEMORY_OPERAND;

  if (value > 0xffffffffU)
    value = 0xffffffffU;

  *address = (uint32_t)value;
  *memaddr = end;
  return RC_OK;
}

static float rc_build_float(uint32_t mantissa_bits, int32_t exponent, int sign) {
  /* 32-bit float has a 23-bit mantissa and 8-bit exponent */
  const uint32_t implied_bit = 1 << 23;
  const uint32_t mantissa = mantissa_bits | implied_bit;
  double dbl = ((double)mantissa) / ((double)implied_bit);

  if (exponent > 127) {
    /* exponent above 127 is a special number */
    if (mantissa_bits == 0) {
      /* infinity */
#ifdef INFINITY /* INFINITY and NAN #defines require C99 */
      dbl = INFINITY;
#else
      dbl = -log(0.0);
#endif
    }
    else {
      /* NaN */
#ifdef NAN
      dbl = NAN;
#else
      dbl = -sqrt(-1);
#endif
    }
  }
  else if (exponent > 0) {
    /* exponent from 1 to 127 is a number greater than 1 */
    while (exponent > 30) {
      dbl *= (double)(1 << 30);
      exponent -= 30;
    }
    dbl *= (double)((long long)1 << exponent);
  }
  else if (exponent < 0) {
    /* exponent from -1 to -127 is a number less than 1 */

    if (exponent == -127) {
      /* exponent -127 (all exponent bits were zero) is a denormalized value
       * (no implied leading bit) with exponent -126 */
      dbl = ((double)mantissa_bits) / ((double)implied_bit);
      exponent = 126;
    } else {
      exponent = -exponent;
    }

    while (exponent > 30) {
      dbl /= (double)(1 << 30);
      exponent -= 30;
    }
    dbl /= (double)((long long)1 << exponent);
  }
  else {
    /* exponent of 0 requires no adjustment */
  }

  return (sign) ? (float)-dbl : (float)dbl;
}

static void rc_transform_memref_float(rc_typed_value_t* value) {
  /* decodes an IEEE 754 float */
  const uint32_t mantissa = (value->value.u32 & 0x7FFFFF);
  const int32_t exponent = (int32_t)((value->value.u32 >> 23) & 0xFF) - 127;
  const int sign = (value->value.u32 & 0x80000000);
  value->value.f32 = rc_build_float(mantissa, exponent, sign);
  value->type = RC_VALUE_TYPE_FLOAT;
}

static void rc_transform_memref_float_be(rc_typed_value_t* value) {
  /* decodes an IEEE 754 float in big endian format */
  const uint32_t mantissa = ((value->value.u32 & 0xFF000000) >> 24) |
                            ((value->value.u32 & 0x00FF0000) >> 8) |
                            ((value->value.u32 & 0x00007F00) << 8);
  const int32_t exponent = (int32_t)(((value->value.u32 & 0x0000007F) << 1) |
                                     ((value->value.u32 & 0x00008000) >> 15)) - 127;
  const int sign = (value->value.u32 & 0x00000080);
  value->value.f32 = rc_build_float(mantissa, exponent, sign);
  value->type = RC_VALUE_TYPE_FLOAT;
}

static void rc_transform_memref_mbf32(rc_typed_value_t* value) {
  /* decodes a Microsoft Binary Format float */
  /* NOTE: 32-bit MBF is stored in memory as big endian (at least for Apple II) */
  const uint32_t mantissa = ((value->value.u32 & 0xFF000000) >> 24) |
                            ((value->value.u32 & 0x00FF0000) >> 8) |
                            ((value->value.u32 & 0x00007F00) << 8);
  const int32_t exponent = (int32_t)(value->value.u32 & 0xFF) - 129;
  const int sign = (value->value.u32 & 0x00008000);

  if (mantissa == 0 && exponent == -129)
    value->value.f32 = (sign) ? -0.0f : 0.0f;
  else
    value->value.f32 = rc_build_float(mantissa, exponent, sign);

  value->type = RC_VALUE_TYPE_FLOAT;
}

static void rc_transform_memref_mbf32_le(rc_typed_value_t* value) {
  /* decodes a Microsoft Binary Format float */
  /* Locomotive BASIC (CPC) uses MBF40, but in little endian format */
  const uint32_t mantissa = value->value.u32 & 0x007FFFFF;
  const int32_t exponent = (int32_t)(value->value.u32 >> 24) - 129;
  const int sign = (value->value.u32 & 0x00800000);

  if (mantissa == 0 && exponent == -129)
    value->value.f32 = (sign) ? -0.0f : 0.0f;
  else
    value->value.f32 = rc_build_float(mantissa, exponent, sign);

  value->type = RC_VALUE_TYPE_FLOAT;
}

static const uint8_t rc_bits_set[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };

void rc_transform_memref_value(rc_typed_value_t* value, uint8_t size) {
  /* ASSERT: value->type == RC_VALUE_TYPE_UNSIGNED */
  switch (size)
  {
    case RC_MEMSIZE_8_BITS:
      value->value.u32 = (value->value.u32 & 0x000000ff);
      break;

    case RC_MEMSIZE_16_BITS:
      value->value.u32 = (value->value.u32 & 0x0000ffff);
      break;

    case RC_MEMSIZE_24_BITS:
      value->value.u32 = (value->value.u32 & 0x00ffffff);
      break;

    case RC_MEMSIZE_32_BITS:
      break;

    case RC_MEMSIZE_BIT_0:
      value->value.u32 = (value->value.u32 >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value->value.u32 = (value->value.u32 >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value->value.u32 = (value->value.u32 >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value->value.u32 = (value->value.u32 >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value->value.u32 = (value->value.u32 >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value->value.u32 = (value->value.u32 >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value->value.u32 = (value->value.u32 >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value->value.u32 = (value->value.u32 >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value->value.u32 = value->value.u32 & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value->value.u32 = (value->value.u32 >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_BITCOUNT:
      value->value.u32 = rc_bits_set[(value->value.u32 & 0x0F)]
                       + rc_bits_set[((value->value.u32 >> 4) & 0x0F)];
      break;

    case RC_MEMSIZE_16_BITS_BE:
      value->value.u32 = ((value->value.u32 & 0xFF00) >> 8) |
                         ((value->value.u32 & 0x00FF) << 8);
      break;

    case RC_MEMSIZE_24_BITS_BE:
      value->value.u32 = ((value->value.u32 & 0xFF0000) >> 16) |
                          (value->value.u32 & 0x00FF00) |
                         ((value->value.u32 & 0x0000FF) << 16);
      break;

    case RC_MEMSIZE_32_BITS_BE:
      value->value.u32 = ((value->value.u32 & 0xFF000000) >> 24) |
                         ((value->value.u32 & 0x00FF0000) >> 8) |
                         ((value->value.u32 & 0x0000FF00) << 8) |
                         ((value->value.u32 & 0x000000FF) << 24);
      break;

    case RC_MEMSIZE_FLOAT:
      rc_transform_memref_float(value);
      break;

    case RC_MEMSIZE_FLOAT_BE:
      rc_transform_memref_float_be(value);
      break;

    case RC_MEMSIZE_MBF32:
      rc_transform_memref_mbf32(value);
      break;

    case RC_MEMSIZE_MBF32_LE:
      rc_transform_memref_mbf32_le(value);
      break;

    default:
      break;
  }
}

static const uint32_t rc_memref_masks[] = {
  0x000000ff, /* RC_MEMSIZE_8_BITS     */
  0x0000ffff, /* RC_MEMSIZE_16_BITS    */
  0x00ffffff, /* RC_MEMSIZE_24_BITS    */
  0xffffffff, /* RC_MEMSIZE_32_BITS    */
  0x0000000f, /* RC_MEMSIZE_LOW        */
  0x000000f0, /* RC_MEMSIZE_HIGH       */
  0x00000001, /* RC_MEMSIZE_BIT_0      */
  0x00000002, /* RC_MEMSIZE_BIT_1      */
  0x00000004, /* RC_MEMSIZE_BIT_2      */
  0x00000008, /* RC_MEMSIZE_BIT_3      */
  0x00000010, /* RC_MEMSIZE_BIT_4      */
  0x00000020, /* RC_MEMSIZE_BIT_5      */
  0x00000040, /* RC_MEMSIZE_BIT_6      */
  0x00000080, /* RC_MEMSIZE_BIT_7      */
  0x000000ff, /* RC_MEMSIZE_BITCOUNT   */
  0x0000ffff, /* RC_MEMSIZE_16_BITS_BE */
  0x00ffffff, /* RC_MEMSIZE_24_BITS_BE */
  0xffffffff, /* RC_MEMSIZE_32_BITS_BE */
  0xffffffff, /* RC_MEMSIZE_FLOAT      */
  0xffffffff, /* RC_MEMSIZE_MBF32      */
  0xffffffff, /* RC_MEMSIZE_MBF32_LE   */
  0xffffffff, /* RC_MEMSIZE_FLOAT_BE   */
  0xffffffff  /* RC_MEMSIZE_VARIABLE   */
};

uint32_t rc_memref_mask(uint8_t size) {
  const size_t index = (size_t)size;
  if (index >= sizeof(rc_memref_masks) / sizeof(rc_memref_masks[0]))
    return 0xffffffff;

  return rc_memref_masks[index];
}

/* all sizes less than 8-bits (1 byte) are mapped to 8-bits. 24-bit is mapped to 32-bit
 * as we don't expect the client to understand a request for 3 bytes. all other reads are
 * mapped to the little-endian read of the same size. */
static const uint8_t rc_memref_shared_sizes[] = {
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_8_BITS     */
  RC_MEMSIZE_16_BITS, /* RC_MEMSIZE_16_BITS    */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_24_BITS    */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_32_BITS    */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_LOW        */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_HIGH       */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_0      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_1      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_2      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_3      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_4      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_5      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_6      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BIT_7      */
  RC_MEMSIZE_8_BITS,  /* RC_MEMSIZE_BITCOUNT   */
  RC_MEMSIZE_16_BITS, /* RC_MEMSIZE_16_BITS_BE */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_24_BITS_BE */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_32_BITS_BE */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_FLOAT      */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_MBF32      */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_MBF32_LE   */
  RC_MEMSIZE_32_BITS, /* RC_MEMSIZE_FLOAT_BE   */
  RC_MEMSIZE_32_BITS  /* RC_MEMSIZE_VARIABLE   */
};

uint8_t rc_memref_shared_size(uint8_t size) {
  const size_t index = (size_t)size;
  if (index >= sizeof(rc_memref_shared_sizes) / sizeof(rc_memref_shared_sizes[0]))
    return size;

  return rc_memref_shared_sizes[index];
}

uint32_t rc_peek_value(uint32_t address, uint8_t size, rc_peek_t peek, void* ud) {
  if (!peek)
    return 0;

  switch (size)
  {
    case RC_MEMSIZE_8_BITS:
      return peek(address, 1, ud);

    case RC_MEMSIZE_16_BITS:
      return peek(address, 2, ud);

    case RC_MEMSIZE_32_BITS:
      return peek(address, 4, ud);

    default:
    {
      uint32_t value;
      const size_t index = (size_t)size;
      if (index >= sizeof(rc_memref_shared_sizes) / sizeof(rc_memref_shared_sizes[0]))
        return 0;

      /* fetch the larger value and mask off the bits associated to the specified size
       * for correct deduction of prior value. non-prior memrefs should already be using
       * shared size memrefs to minimize the total number of memory reads required. */
      value = rc_peek_value(address, rc_memref_shared_sizes[index], peek, ud);
      return value & rc_memref_masks[index];
    }
  }
}

void rc_update_memref_value(rc_memref_value_t* memref, uint32_t new_value) {
  if (memref->value == new_value) {
    memref->changed = 0;
  }
  else {
    memref->prior = memref->value;
    memref->value = new_value;
    memref->changed = 1;
  }
}

void rc_update_memref_values(rc_memref_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    /* indirect memory references are not shared and will be updated in rc_get_memref_value */
    if (!memref->value.is_indirect)
      rc_update_memref_value(&memref->value, rc_peek_value(memref->address, memref->value.size, peek, ud));

    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_t** memrefs) {
  parse->first_memref = memrefs;
  *memrefs = 0;
}

static uint32_t rc_get_memref_value_value(const rc_memref_value_t* memref, int operand_type) {
  switch (operand_type)
  {
    /* most common case explicitly first, even though it could be handled by default case.
     * this helps the compiler to optimize if it turns the switch into a series of if/elses */
    case RC_OPERAND_ADDRESS:
      return memref->value;

    case RC_OPERAND_DELTA:
      if (!memref->changed) {
        /* fallthrough */
    default:
        return memref->value;
      }
      /* fallthrough */
    case RC_OPERAND_PRIOR:
      return memref->prior;
  }
}

uint32_t rc_get_memref_value(rc_memref_t* memref, int operand_type, rc_eval_state_t* eval_state) {
  /* if this is an indirect reference, handle the indirection. */
  if (memref->value.is_indirect) {
    const uint32_t new_address = memref->address + eval_state->add_address;
    rc_update_memref_value(&memref->value, rc_peek_value(new_address, memref->value.size, eval_state->peek, eval_state->peek_userdata));
  }

  return rc_get_memref_value_value(&memref->value, operand_type);
}
