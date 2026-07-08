#ifndef RC_VALIDATE_H
#define RC_VALIDATE_H

#include "rc_runtime_types.h"

#include <stddef.h>

RC_BEGIN_C_DECLS

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t max_address);
int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t max_address);

int rc_validate_condset_for_console(const rc_condset_t* condset, char result[], const size_t result_size, uint32_t console_id);
int rc_validate_trigger_for_console(const rc_trigger_t* trigger, char result[], const size_t result_size, uint32_t console_id);

RC_END_C_DECLS

#endif /* RC_VALIDATE_H */
