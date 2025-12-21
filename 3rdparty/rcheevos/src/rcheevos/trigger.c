#include "rc_internal.h"

#include <stddef.h>
#include <string.h> /* memset */

void rc_parse_trigger_internal(rc_trigger_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next;
  const char* aux;

  aux = *memaddr;
  next = &self->alternative;

  /* reset in case multiple triggers are parsed by the same parse_state */
  parse->measured_target = 0;
  parse->has_required_hits = 0;
  parse->measured_as_percent = 0;

  if (*aux == 's' || *aux == 'S') {
    self->requirement = NULL;
  }
  else {
    self->requirement = rc_parse_condset(&aux, parse);

    if (parse->offset < 0)
      return;

    self->requirement->next = NULL;
  }

  while (*aux == 's' || *aux == 'S') {
    aux++;
    *next = rc_parse_condset(&aux, parse);

    if (parse->offset < 0) {
      return;
    }

    next = &(*next)->next;
  }

  *next = NULL;
  *memaddr = aux;

  self->measured_target = parse->measured_target;
  self->measured_value = parse->measured_target ? RC_MEASURED_UNKNOWN : 0;
  self->measured_as_percent = parse->measured_as_percent;
  self->state = RC_TRIGGER_STATE_WAITING;
  self->has_hits = 0;
  self->has_memrefs = 0;
}

int rc_trigger_size(const char* memaddr) {
  rc_trigger_with_memrefs_t* trigger;
  rc_preparse_state_t preparse;
  rc_init_preparse_state(&preparse);

  trigger = RC_ALLOC(rc_trigger_with_memrefs_t, &preparse.parse);
  rc_parse_trigger_internal(&trigger->trigger, &memaddr, &preparse.parse);
  rc_preparse_alloc_memrefs(NULL, &preparse);

  rc_destroy_preparse_state(&preparse);
  return preparse.parse.offset;
}

rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, void* unused_L, int unused_funcs_idx) {
  rc_trigger_with_memrefs_t* trigger;
  rc_preparse_state_t preparse;
  const char* preparse_memaddr = memaddr;

  (void)unused_L;
  (void)unused_funcs_idx;

  if (!buffer || !memaddr)
    return NULL;

  /* first pass : determine how many memrefs are needed */
  rc_init_preparse_state(&preparse);
  trigger = RC_ALLOC(rc_trigger_with_memrefs_t, &preparse.parse);
  rc_parse_trigger_internal(&trigger->trigger, &preparse_memaddr, &preparse.parse);

  /* allocate the trigger and memrefs */
  rc_reset_parse_state(&preparse.parse, buffer);
  trigger = RC_ALLOC(rc_trigger_with_memrefs_t, &preparse.parse);
  rc_preparse_alloc_memrefs(&trigger->memrefs, &preparse);

  /* parse the trigger */
  rc_parse_trigger_internal(&trigger->trigger, &memaddr, &preparse.parse);
  trigger->trigger.has_memrefs = 1;

  rc_destroy_preparse_state(&preparse);
  return (preparse.parse.offset >= 0) ? &trigger->trigger : NULL;
}

int rc_trigger_state_active(int state)
{
  switch (state)
  {
    case RC_TRIGGER_STATE_DISABLED:
    case RC_TRIGGER_STATE_INACTIVE:
    case RC_TRIGGER_STATE_TRIGGERED:
      return 0;

    default:
      return 1;
  }
}

static int rc_condset_is_measured_from_hitcount(const rc_condset_t* condset, uint32_t measured_value)
{
  const rc_condition_t* condition;
  for (condition = condset->conditions; condition; condition = condition->next) {
    if (condition->type == RC_CONDITION_MEASURED && condition->required_hits &&
        condition->current_hits == measured_value) {
      return 1;
    }
  }

  return 0;
}

static void rc_reset_trigger_hitcounts(rc_trigger_t* self) {
  rc_condset_t* condset;

  if (self->requirement) {
    rc_reset_condset(self->requirement);
  }

  condset = self->alternative;

  while (condset) {
    rc_reset_condset(condset);
    condset = condset->next;
  }
}

static void rc_update_trigger_memrefs(rc_trigger_t* self, rc_peek_t peek, void* ud) {
  if (self->has_memrefs) {
    rc_trigger_with_memrefs_t* trigger = (rc_trigger_with_memrefs_t*)self;
    rc_update_memref_values(&trigger->memrefs, peek, ud);
  }
}

rc_memrefs_t* rc_trigger_get_memrefs(rc_trigger_t* self) {
  if (self->has_memrefs) {
    rc_trigger_with_memrefs_t* trigger = (rc_trigger_with_memrefs_t*)self;
    return &trigger->memrefs;
  }

  return NULL;
}

int rc_evaluate_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, void* unused_L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  rc_typed_value_t measured_value;
  int measured_from_hits = 0;
  int ret;
  char is_paused;
  char is_primed;

  (void)unused_L;

  switch (self->state)
  {
    case RC_TRIGGER_STATE_TRIGGERED:
      /* previously triggered. do nothing - return INACTIVE so caller doesn't think it triggered again */
      return RC_TRIGGER_STATE_INACTIVE;

    case RC_TRIGGER_STATE_DISABLED:
      /* unsupported. do nothing - return INACTIVE */
      return RC_TRIGGER_STATE_INACTIVE;

    case RC_TRIGGER_STATE_INACTIVE:
      /* not yet active. update the memrefs so deltas are correct when it becomes active, then return INACTIVE */
      rc_update_trigger_memrefs(self, peek, ud);
      return RC_TRIGGER_STATE_INACTIVE;

    default:
      break;
  }

  /* update the memory references */
  rc_update_trigger_memrefs(self, peek, ud);

  /* process the trigger */
  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;

  measured_value.type = RC_VALUE_TYPE_NONE;

  if (self->requirement != NULL) {
    ret = rc_test_condset(self->requirement, &eval_state);
    is_paused = eval_state.is_paused;
    is_primed = eval_state.is_primed;

    if (eval_state.measured_value.type != RC_VALUE_TYPE_NONE) {
      memcpy(&measured_value, &eval_state.measured_value, sizeof(measured_value));
      measured_from_hits = eval_state.measured_from_hits;
    }
  } else {
    ret = 1;
    is_paused = 0;
    is_primed = 1;
  }

  condset = self->alternative;
  if (condset) {
    int sub = 0;
    char sub_paused = 1;
    char sub_primed = 0;

    do {
      sub |= rc_test_condset(condset, &eval_state);
      sub_paused &= eval_state.is_paused;
      sub_primed |= eval_state.is_primed;

      if (eval_state.measured_value.type != RC_VALUE_TYPE_NONE) {
        /* if no previous Measured value was captured, or the new one is greater, keep the new one */
        if (measured_value.type == RC_VALUE_TYPE_NONE ||
            rc_typed_value_compare(&eval_state.measured_value, &measured_value, RC_OPERATOR_GT)) {
          memcpy(&measured_value, &eval_state.measured_value, sizeof(measured_value));
          measured_from_hits = eval_state.measured_from_hits;
        }
      }

      condset = condset->next;
    } while (condset);

    /* to trigger, the core must be true and at least one alt must be true */
    ret &= sub;
    is_primed &= sub_primed;

    /* if the core is not paused, all alts must be paused to count as a paused trigger */
    is_paused |= sub_paused;
  }

  if (is_paused) {
    /* if the trigger is fully paused, ignore any updates to the measured value */
  }
  else if (measured_value.type == RC_VALUE_TYPE_NONE) {
    /* if a measured value was not captured, keep the old value (it's possible to pause
     * an alt that is generating the measured value without fully pausing the trigger) */
  }
  else {
    rc_typed_value_convert(&measured_value, RC_VALUE_TYPE_UNSIGNED);
    self->measured_value = measured_value.value.u32;
  }

  /* if any ResetIf condition was true, reset the hit counts */
  if (eval_state.was_reset) {
    /* if the measured value came from a hit count, reset it. do this before calling
     * rc_reset_trigger_hitcounts in case we need to call rc_condset_is_measured_from_hitcount */
    if (measured_from_hits) {
      self->measured_value = 0;
    }
    else if (is_paused && self->measured_value) {
      /* if the measured value is in a paused group, measured_from_hits won't have been set.
       * attempt to determine if it should have been */
      if (self->requirement && self->requirement->is_paused &&
          rc_condset_is_measured_from_hitcount(self->requirement, self->measured_value)) {
        self->measured_value = 0;
      }
      else {
        for (condset = self->alternative; condset; condset = condset->next) {
          if (condset->is_paused && rc_condset_is_measured_from_hitcount(condset, self->measured_value)) {
            self->measured_value = 0;
            break;
          }
        }
      }
    }

    rc_reset_trigger_hitcounts(self);

    /* if there were hit counts to clear, return RESET, but don't change the state */
    if (self->has_hits) {
      self->has_hits = 0;

      /* cannot be PRIMED while ResetIf is true */
      if (self->state == RC_TRIGGER_STATE_PRIMED)
        self->state = RC_TRIGGER_STATE_ACTIVE;

      return RC_TRIGGER_STATE_RESET;
    }

    /* any hits that were tallied were just reset */
    eval_state.has_hits = 0;
    is_primed = 0;
  }
  else if (ret) {
    /* if the state is WAITING and the trigger is ready to fire, ignore it and reset the hit counts */
    if (self->state == RC_TRIGGER_STATE_WAITING) {
      rc_reset_trigger(self);
      self->has_hits = 0;
      return RC_TRIGGER_STATE_WAITING;
    }

    /* trigger was triggered */
    self->state = RC_TRIGGER_STATE_TRIGGERED;
    return RC_TRIGGER_STATE_TRIGGERED;
  }

  /* did not trigger this frame - update the information we'll need for next time */
  self->has_hits = eval_state.has_hits;

  if (is_paused) {
    self->state = RC_TRIGGER_STATE_PAUSED;
  }
  else if (is_primed) {
    self->state = RC_TRIGGER_STATE_PRIMED;
  }
  else {
    self->state = RC_TRIGGER_STATE_ACTIVE;
  }

  /* if an individual condition was reset, notify the caller */
  if (eval_state.was_cond_reset)
    return RC_TRIGGER_STATE_RESET;

  /* otherwise, just return the current state */
  return self->state;
}

int rc_test_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, void* unused_L) {
  /* for backwards compatibilty, rc_test_trigger always assumes the achievement is active */
  self->state = RC_TRIGGER_STATE_ACTIVE;

  return (rc_evaluate_trigger(self, peek, ud, unused_L) == RC_TRIGGER_STATE_TRIGGERED);
}

void rc_reset_trigger(rc_trigger_t* self) {
  if (!self)
    return;

  rc_reset_trigger_hitcounts(self);

  self->state = RC_TRIGGER_STATE_WAITING;

  if (self->measured_target)
    self->measured_value = RC_MEASURED_UNKNOWN;

  self->has_hits = 0;
}
