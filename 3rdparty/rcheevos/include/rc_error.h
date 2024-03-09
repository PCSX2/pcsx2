#ifndef RC_ERROR_H
#define RC_ERROR_H

#include "rc_export.h"

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Return values                                                               |
\*****************************************************************************/

enum {
  RC_OK = 0,
  RC_INVALID_LUA_OPERAND = -1,
  RC_INVALID_MEMORY_OPERAND = -2,
  RC_INVALID_CONST_OPERAND = -3,
  RC_INVALID_FP_OPERAND = -4,
  RC_INVALID_CONDITION_TYPE = -5,
  RC_INVALID_OPERATOR = -6,
  RC_INVALID_REQUIRED_HITS = -7,
  RC_DUPLICATED_START = -8,
  RC_DUPLICATED_CANCEL = -9,
  RC_DUPLICATED_SUBMIT = -10,
  RC_DUPLICATED_VALUE = -11,
  RC_DUPLICATED_PROGRESS = -12,
  RC_MISSING_START = -13,
  RC_MISSING_CANCEL = -14,
  RC_MISSING_SUBMIT = -15,
  RC_MISSING_VALUE = -16,
  RC_INVALID_LBOARD_FIELD = -17,
  RC_MISSING_DISPLAY_STRING = -18,
  RC_OUT_OF_MEMORY = -19,
  RC_INVALID_VALUE_FLAG = -20,
  RC_MISSING_VALUE_MEASURED = -21,
  RC_MULTIPLE_MEASURED = -22,
  RC_INVALID_MEASURED_TARGET = -23,
  RC_INVALID_COMPARISON = -24,
  RC_INVALID_STATE = -25,
  RC_INVALID_JSON = -26,
  RC_API_FAILURE = -27,
  RC_LOGIN_REQUIRED = -28,
  RC_NO_GAME_LOADED = -29,
  RC_HARDCORE_DISABLED = -30,
  RC_ABORTED = -31,
  RC_NO_RESPONSE = -32,
  RC_ACCESS_DENIED = -33,
  RC_INVALID_CREDENTIALS = -34,
  RC_EXPIRED_TOKEN = -35
};

RC_EXPORT const char* RC_CCONV rc_error_str(int ret);

RC_END_C_DECLS

#endif /* RC_ERROR_H */
