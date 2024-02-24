#include "rc_internal.h"

enum {
  RC_LBOARD_START    = 1 << 0,
  RC_LBOARD_CANCEL   = 1 << 1,
  RC_LBOARD_SUBMIT   = 1 << 2,
  RC_LBOARD_VALUE    = 1 << 3,
  RC_LBOARD_PROGRESS = 1 << 4,
  RC_LBOARD_COMPLETE = RC_LBOARD_START | RC_LBOARD_CANCEL | RC_LBOARD_SUBMIT | RC_LBOARD_VALUE
};

void rc_parse_lboard_internal(rc_lboard_t* self, const char* memaddr, rc_parse_state_t* parse) {
  int found;

  self->progress = 0;
  found = 0;

  for (;;)
  {
    if ((memaddr[0] == 's' || memaddr[0] == 'S') &&
        (memaddr[1] == 't' || memaddr[1] == 'T') &&
        (memaddr[2] == 'a' || memaddr[2] == 'A') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_START) != 0) {
        parse->offset = RC_DUPLICATED_START;
        return;
      }

      memaddr += 4;
      if (*memaddr && *memaddr != ':') {
        found |= RC_LBOARD_START;
        rc_parse_trigger_internal(&self->start, &memaddr, parse);
        self->start.memrefs = 0;
      }
    }
    else if ((memaddr[0] == 'c' || memaddr[0] == 'C') &&
             (memaddr[1] == 'a' || memaddr[1] == 'A') &&
             (memaddr[2] == 'n' || memaddr[2] == 'N') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_CANCEL) != 0) {
        parse->offset = RC_DUPLICATED_CANCEL;
        return;
      }

      memaddr += 4;
      if (*memaddr && *memaddr != ':') {
        found |= RC_LBOARD_CANCEL;
        rc_parse_trigger_internal(&self->cancel, &memaddr, parse);
        self->cancel.memrefs = 0;
      }
    }
    else if ((memaddr[0] == 's' || memaddr[0] == 'S') &&
             (memaddr[1] == 'u' || memaddr[1] == 'U') &&
             (memaddr[2] == 'b' || memaddr[2] == 'B') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_SUBMIT) != 0) {
        parse->offset = RC_DUPLICATED_SUBMIT;
        return;
      }

      memaddr += 4;
      if (*memaddr && *memaddr != ':') {
        found |= RC_LBOARD_SUBMIT;
        rc_parse_trigger_internal(&self->submit, &memaddr, parse);
        self->submit.memrefs = 0;
      }
    }
    else if ((memaddr[0] == 'v' || memaddr[0] == 'V') &&
             (memaddr[1] == 'a' || memaddr[1] == 'A') &&
             (memaddr[2] == 'l' || memaddr[2] == 'L') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_VALUE) != 0) {
        parse->offset = RC_DUPLICATED_VALUE;
        return;
      }

      memaddr += 4;
      if (*memaddr && *memaddr != ':') {
        found |= RC_LBOARD_VALUE;
        rc_parse_value_internal(&self->value, &memaddr, parse);
        self->value.memrefs = 0;
      }
    }
    else if ((memaddr[0] == 'p' || memaddr[0] == 'P') &&
             (memaddr[1] == 'r' || memaddr[1] == 'R') &&
             (memaddr[2] == 'o' || memaddr[2] == 'O') && memaddr[3] == ':') {
      if ((found & RC_LBOARD_PROGRESS) != 0) {
        parse->offset = RC_DUPLICATED_PROGRESS;
        return;
      }

      memaddr += 4;
      if (*memaddr && *memaddr != ':') {
        found |= RC_LBOARD_PROGRESS;

        self->progress = RC_ALLOC(rc_value_t, parse);
        rc_parse_value_internal(self->progress, &memaddr, parse);
        self->progress->memrefs = 0;
      }
    }

    /* encountered an error parsing one of the parts */
    if (parse->offset < 0)
      return;

    /* end of string, or end of quoted string - stop processing */
    if (memaddr[0] == '\0' || memaddr[0] == '\"')
      break;

    /* expect two colons between fields */
    if (memaddr[0] != ':' || memaddr[1] != ':') {
      parse->offset = RC_INVALID_LBOARD_FIELD;
      return;
    }

    memaddr += 2;
  }

  if ((found & RC_LBOARD_COMPLETE) != RC_LBOARD_COMPLETE) {
    if ((found & RC_LBOARD_START) == 0) {
      parse->offset = RC_MISSING_START;
    }
    else if ((found & RC_LBOARD_CANCEL) == 0) {
      parse->offset = RC_MISSING_CANCEL;
    }
    else if ((found & RC_LBOARD_SUBMIT) == 0) {
      parse->offset = RC_MISSING_SUBMIT;
    }
    else if ((found & RC_LBOARD_VALUE) == 0) {
      parse->offset = RC_MISSING_VALUE;
    }

    return;
  }

  self->state = RC_LBOARD_STATE_WAITING;
}

int rc_lboard_size(const char* memaddr) {
  rc_lboard_t* self;
  rc_parse_state_t parse;
  rc_memref_t* first_memref;
  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &first_memref);

  self = RC_ALLOC(rc_lboard_t, &parse);
  rc_parse_lboard_internal(self, memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_lboard_t* rc_parse_lboard(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_lboard_t* self;
  rc_parse_state_t parse;

  if (!buffer || !memaddr)
    return 0;

  rc_init_parse_state(&parse, buffer, L, funcs_ndx);

  self = RC_ALLOC(rc_lboard_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_lboard_internal(self, memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return (parse.offset >= 0) ? self : 0;
}

int rc_evaluate_lboard(rc_lboard_t* self, int32_t* value, rc_peek_t peek, void* peek_ud, lua_State* L) {
  int start_ok, cancel_ok, submit_ok;

  rc_update_memref_values(self->memrefs, peek, peek_ud);

  if (self->state == RC_LBOARD_STATE_INACTIVE || self->state == RC_LBOARD_STATE_DISABLED)
    return RC_LBOARD_STATE_INACTIVE;

  /* these are always tested once every frame, to ensure hit counts work properly */
  start_ok = rc_test_trigger(&self->start, peek, peek_ud, L);
  cancel_ok = rc_test_trigger(&self->cancel, peek, peek_ud, L);
  submit_ok = rc_test_trigger(&self->submit, peek, peek_ud, L);

  switch (self->state)
  {
    case RC_LBOARD_STATE_WAITING:
    case RC_LBOARD_STATE_TRIGGERED:
    case RC_LBOARD_STATE_CANCELED:
      /* don't activate/reactivate until the start condition becomes false */
      if (start_ok) {
        *value = 0;
        return RC_LBOARD_STATE_INACTIVE; /* just return inactive for all of these */
      }

      /* start condition is false, allow the leaderboard to start on future frames */
      self->state = RC_LBOARD_STATE_ACTIVE;
      break;

    case RC_LBOARD_STATE_ACTIVE:
      /* leaderboard attempt is not in progress. if the start condition is true and the cancel condition is not, start the attempt */
      if (start_ok && !cancel_ok) {
        if (submit_ok) {
          /* start and submit are both true in the same frame, just submit without announcing the leaderboard is available */
          self->state = RC_LBOARD_STATE_TRIGGERED;
        }
        else if (self->start.requirement == 0 && self->start.alternative == 0) {
          /* start condition is empty - this leaderboard is submit-only with no measured progress */
        }
        else {
          /* start the leaderboard attempt */
          self->state = RC_LBOARD_STATE_STARTED;

          /* reset any hit counts in the value */
          if (self->progress)
            rc_reset_value(self->progress);

          rc_reset_value(&self->value);
        }
      }
      break;

    case RC_LBOARD_STATE_STARTED:
      /* leaderboard attempt in progress */
      if (cancel_ok) {
        /* cancel condition is true, abort the attempt */
        self->state = RC_LBOARD_STATE_CANCELED;
      }
      else if (submit_ok) {
        /* submit condition is true, submit the current value */
        self->state = RC_LBOARD_STATE_TRIGGERED;
      }
      break;
  }

  /* Calculate the value */
  switch (self->state) {
    case RC_LBOARD_STATE_STARTED:
      if (self->progress) {
        *value = rc_evaluate_value(self->progress, peek, peek_ud, L);
        break;
      }
      /* fallthrough */ /* to RC_LBOARD_STATE_TRIGGERED */

    case RC_LBOARD_STATE_TRIGGERED:
      *value = rc_evaluate_value(&self->value, peek, peek_ud, L);
      break;

    default:
      *value = 0;
      break;
  }

  return self->state;
}

int rc_lboard_state_active(int state) {
  switch (state)
  {
    case RC_LBOARD_STATE_DISABLED:
    case RC_LBOARD_STATE_INACTIVE:
      return 0;

    default:
      return 1;
  }
}

void rc_reset_lboard(rc_lboard_t* self) {
  if (!self)
    return;

  self->state = RC_LBOARD_STATE_WAITING;

  rc_reset_trigger(&self->start);
  rc_reset_trigger(&self->submit);
  rc_reset_trigger(&self->cancel);

  if (self->progress)
    rc_reset_value(self->progress);

  rc_reset_value(&self->value);
}
