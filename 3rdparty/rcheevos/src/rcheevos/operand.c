#include "rc_internal.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifndef RC_DISABLE_LUA

RC_BEGIN_C_DECLS

#include <lua.h>
#include <lauxlib.h>

RC_END_C_DECLS

#endif /* RC_DISABLE_LUA */

static int rc_parse_operand_lua(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
#ifndef RC_DISABLE_LUA
  const char* id;
#endif

  if (*aux++ != '@') {
    return RC_INVALID_LUA_OPERAND;
  }

  if (!isalpha((unsigned char)*aux)) {
    return RC_INVALID_LUA_OPERAND;
  }

#ifndef RC_DISABLE_LUA
  id = aux;
#endif

  while (isalnum((unsigned char)*aux) || *aux == '_') {
    aux++;
  }

#ifndef RC_DISABLE_LUA

  if (parse->L != 0) {
    if (!lua_istable(parse->L, parse->funcs_ndx)) {
      return RC_INVALID_LUA_OPERAND;
    }

    lua_pushlstring(parse->L, id, aux - id);
    lua_gettable(parse->L, parse->funcs_ndx);

    if (!lua_isfunction(parse->L, -1)) {
      lua_pop(parse->L, 1);
      return RC_INVALID_LUA_OPERAND;
    }

    self->value.luafunc = luaL_ref(parse->L, LUA_REGISTRYINDEX);
  }

#else
  (void)parse;
#endif /* RC_DISABLE_LUA */

  self->type = RC_OPERAND_LUA;
  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_memory(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse, uint8_t is_indirect) {
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

  self->value.memref = rc_alloc_memref(parse, address, size, is_indirect);
  if (parse->offset < 0)
    return parse->offset;

  *memaddr = aux;
  return RC_OK;
}

int rc_parse_operand(rc_operand_t* self, const char** memaddr, uint8_t is_indirect, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  char* end;
  int ret;
  unsigned long value;
  int negative;
  int allow_decimal = 0;

  self->size = RC_MEMSIZE_32_BITS;

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

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;

    case 'f': case 'F': /* floating point constant */
      if (isalpha((unsigned char)aux[1])) {
        ret = rc_parse_operand_memory(self, &aux, parse, is_indirect);

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
            self->value.dbl = ((double)(-((long)value))) - dbl_fraction;
          else
            self->value.dbl = (double)value + dbl_fraction;
        }
        else {
          /* fractional part is 0, just convert the integer portion */
          if (negative)
            self->value.dbl = (double)(-((long)value));
          else
            self->value.dbl = (double)value;
        }

        self->type = RC_OPERAND_FP;
      }
      else {
        /* not a floating point value, make sure something was read and advance the read pointer */
        if (end == aux)
          return allow_decimal ? RC_INVALID_FP_OPERAND : RC_INVALID_CONST_OPERAND;

        aux = end;

        if (value > 0x7fffffffU)
          value = 0x7fffffffU;

        self->type = RC_OPERAND_CONST;

        if (negative)
          self->value.num = (unsigned)(-((long)value));
        else
          self->value.num = (unsigned)value;
      }
      break;

    case '0':
      if (aux[1] == 'x' || aux[1] == 'X') { /* hex integer constant */
        /* fallthrough */ /* to default */
    default:
        ret = rc_parse_operand_memory(self, &aux, parse, is_indirect);

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

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;

    case '@':
      ret = rc_parse_operand_lua(self, &aux, parse);

      if (ret < 0)
        return ret;

      break;
  }

  *memaddr = aux;
  return RC_OK;
}

#ifndef RC_DISABLE_LUA

typedef struct {
  rc_peek_t peek;
  void* ud;
}
rc_luapeek_t;

static int rc_luapeek(lua_State* L) {
  uint32_t address = (uint32_t)luaL_checkinteger(L, 1);
  uint32_t num_bytes = (uint32_t)luaL_checkinteger(L, 2);
  rc_luapeek_t* luapeek = (rc_luapeek_t*)lua_touserdata(L, 3);

  uint32_t value = luapeek->peek(address, num_bytes, luapeek->ud);

  lua_pushinteger(L, value);
  return 1;
}

#endif /* RC_DISABLE_LUA */

int rc_operand_is_float_memref(const rc_operand_t* self) {
  switch (self->size) {
    case RC_MEMSIZE_FLOAT:
    case RC_MEMSIZE_FLOAT_BE:
    case RC_MEMSIZE_MBF32:
    case RC_MEMSIZE_MBF32_LE:
      return 1;

    default:
      return 0;
  }
}

int rc_operand_is_memref(const rc_operand_t* self) {
  switch (self->type) {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_LUA:
      return 0;

    default:
      return 1;
  }
}

int rc_operand_is_float(const rc_operand_t* self) {
  if (self->type == RC_OPERAND_FP)
    return 1;

  return rc_operand_is_float_memref(self);
}

uint32_t rc_transform_operand_value(uint32_t value, const rc_operand_t* self) {
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

void rc_evaluate_operand(rc_typed_value_t* result, rc_operand_t* self, rc_eval_state_t* eval_state) {
#ifndef RC_DISABLE_LUA
  rc_luapeek_t luapeek;
#endif /* RC_DISABLE_LUA */

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

    case RC_OPERAND_LUA:
      result->type = RC_VALUE_TYPE_UNSIGNED;
      result->value.u32 = 0;

#ifndef RC_DISABLE_LUA
      if (eval_state->L != 0) {
        lua_rawgeti(eval_state->L, LUA_REGISTRYINDEX, self->value.luafunc);
        lua_pushcfunction(eval_state->L, rc_luapeek);

        luapeek.peek = eval_state->peek;
        luapeek.ud = eval_state->peek_userdata;

        lua_pushlightuserdata(eval_state->L, &luapeek);

        if (lua_pcall(eval_state->L, 2, 1, 0) == LUA_OK) {
          if (lua_isboolean(eval_state->L, -1)) {
            result->value.u32 = (uint32_t)lua_toboolean(eval_state->L, -1);
          }
          else {
            result->value.u32 = (uint32_t)lua_tonumber(eval_state->L, -1);
          }
        }

        lua_pop(eval_state->L, 1);
      }

#endif /* RC_DISABLE_LUA */

      break;

    default:
      result->type = RC_VALUE_TYPE_UNSIGNED;
      result->value.u32 = rc_get_memref_value(self->value.memref, self->type, eval_state);
      break;
  }

  /* step 2: convert read memory to desired format */
  rc_transform_memref_value(result, self->size);

  /* step 3: apply logic (BCD/invert) */
  if (result->type == RC_VALUE_TYPE_UNSIGNED)
    result->value.u32 = rc_transform_operand_value(result->value.u32, self);
}
