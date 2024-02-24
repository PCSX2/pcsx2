#include "rc_internal.h"

#include <string.h> /* memcpy */

static void rc_update_condition_pause(rc_condition_t* condition) {
  rc_condition_t* subclause = condition;

  while (condition) {
    if (condition->type == RC_CONDITION_PAUSE_IF) {
      while (subclause != condition) {
        subclause->pause = 1;
        subclause = subclause->next;
      }
      condition->pause = 1;
    }
    else {
      condition->pause = 0;
    }

    if (!rc_condition_is_combining(condition))
      subclause = condition->next;

    condition = condition->next;
  }
}

rc_condset_t* rc_parse_condset(const char** memaddr, rc_parse_state_t* parse, int is_value) {
  rc_condset_t* self;
  rc_condition_t** next;
  int in_add_address;
  uint32_t measured_target = 0;

  self = RC_ALLOC(rc_condset_t, parse);
  self->has_pause = self->is_paused = self->has_indirect_memrefs = 0;
  next = &self->conditions;

  if (**memaddr == 'S' || **memaddr == 's' || !**memaddr) {
    /* empty group - editor allows it, so we have to support it */
    *next = 0;
    return self;
  }

  in_add_address = 0;
  for (;;) {
    *next = rc_parse_condition(memaddr, parse, in_add_address);

    if (parse->offset < 0) {
      return 0;
    }

    if ((*next)->oper == RC_OPERATOR_NONE) {
      switch ((*next)->type) {
        case RC_CONDITION_ADD_ADDRESS:
        case RC_CONDITION_ADD_SOURCE:
        case RC_CONDITION_SUB_SOURCE:
          /* these conditions don't require a right hand size (implied *1) */
          break;

        case RC_CONDITION_MEASURED:
          /* right hand side is not required when Measured is used in a value */
          if (is_value)
            break;
          /* fallthrough */ /* to default */

        default:
          parse->offset = RC_INVALID_OPERATOR;
          return 0;
      }
    }

    self->has_pause |= (*next)->type == RC_CONDITION_PAUSE_IF;
    in_add_address = (*next)->type == RC_CONDITION_ADD_ADDRESS;
    self->has_indirect_memrefs |= in_add_address;

    switch ((*next)->type) {
    case RC_CONDITION_MEASURED:
      if (measured_target != 0) {
        /* multiple Measured flags cannot exist in the same group */
        parse->offset = RC_MULTIPLE_MEASURED;
        return 0;
      }
      else if (is_value) {
        measured_target = (unsigned)-1;
        switch ((*next)->oper)
        {
          case RC_OPERATOR_AND:
          case RC_OPERATOR_XOR:
          case RC_OPERATOR_DIV:
          case RC_OPERATOR_MULT:
          case RC_OPERATOR_NONE:
            /* measuring value. leave required_hits at 0 */
            break;

          default:
            /* comparison operator, measuring hits. set required_hits to MAX_INT */
            (*next)->required_hits = measured_target;
            break;
        }
      }
      else if ((*next)->required_hits != 0) {
        measured_target = (*next)->required_hits;
      }
      else if ((*next)->operand2.type == RC_OPERAND_CONST) {
        measured_target = (*next)->operand2.value.num;
      }
      else if ((*next)->operand2.type == RC_OPERAND_FP) {
        measured_target = (unsigned)(*next)->operand2.value.dbl;
      }
      else {
        parse->offset = RC_INVALID_MEASURED_TARGET;
        return 0;
      }

      if (parse->measured_target && measured_target != parse->measured_target) {
        /* multiple Measured flags in separate groups must have the same target */
        parse->offset = RC_MULTIPLE_MEASURED;
        return 0;
      }

      parse->measured_target = measured_target;
      break;

    case RC_CONDITION_STANDARD:
    case RC_CONDITION_TRIGGER:
      /* these flags are not allowed in value expressions */
      if (is_value) {
        parse->offset = RC_INVALID_VALUE_FLAG;
        return 0;
      }
      break;

    default:
      break;
    }

    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;

  if (parse->buffer != 0)
    rc_update_condition_pause(self->conditions);

  return self;
}

static void rc_condset_update_indirect_memrefs(rc_condition_t* condition, int processing_pause, rc_eval_state_t* eval_state) {
  for (; condition != 0; condition = condition->next) {
    if (condition->pause != processing_pause)
      continue;

    if (condition->type == RC_CONDITION_ADD_ADDRESS) {
      rc_typed_value_t value;
      rc_evaluate_condition_value(&value, condition, eval_state);
      rc_typed_value_convert(&value, RC_VALUE_TYPE_UNSIGNED);
      eval_state->add_address = value.value.u32;
      continue;
    }

    /* call rc_get_memref_value to update the indirect memrefs. it won't do anything with non-indirect
     * memrefs and avoids a second check of is_indirect. also, we ignore the response, so it doesn't
     * matter what operand type we pass. assume RC_OPERAND_ADDRESS is the quickest. */
    if (rc_operand_is_memref(&condition->operand1))
      rc_get_memref_value(condition->operand1.value.memref, RC_OPERAND_ADDRESS, eval_state);

    if (rc_operand_is_memref(&condition->operand2))
      rc_get_memref_value(condition->operand2.value.memref, RC_OPERAND_ADDRESS, eval_state);

    eval_state->add_address = 0;
  }
}

static int rc_test_condset_internal(rc_condset_t* self, int processing_pause, rc_eval_state_t* eval_state) {
  rc_condition_t* condition;
  rc_typed_value_t value;
  int set_valid, cond_valid, and_next, or_next, reset_next, measured_from_hits, can_measure;
  rc_typed_value_t measured_value;
  uint32_t total_hits;

  measured_value.type = RC_VALUE_TYPE_NONE;
  measured_from_hits = 0;
  can_measure = 1;
  total_hits = 0;

  eval_state->primed = 1;
  set_valid = 1;
  and_next = 1;
  or_next = 0;
  reset_next = 0;
  eval_state->add_value.type = RC_VALUE_TYPE_NONE;
  eval_state->add_hits = eval_state->add_address = 0;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    if (condition->pause != processing_pause)
      continue;

    /* STEP 1: process modifier conditions */
    switch (condition->type) {
      case RC_CONDITION_ADD_SOURCE:
        rc_evaluate_condition_value(&value, condition, eval_state);
        rc_typed_value_add(&eval_state->add_value, &value);
        eval_state->add_address = 0;
        continue;

      case RC_CONDITION_SUB_SOURCE:
        rc_evaluate_condition_value(&value, condition, eval_state);
        rc_typed_value_negate(&value);
        rc_typed_value_add(&eval_state->add_value, &value);
        eval_state->add_address = 0;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        rc_evaluate_condition_value(&value, condition, eval_state);
        rc_typed_value_convert(&value, RC_VALUE_TYPE_UNSIGNED);
        eval_state->add_address = value.value.u32;
        continue;

      case RC_CONDITION_MEASURED:
        if (condition->required_hits == 0 && can_measure) {
          /* Measured condition without a hit target measures the value of the left operand */
          rc_evaluate_condition_value(&measured_value, condition, eval_state);
          rc_typed_value_add(&measured_value, &eval_state->add_value);
        }
        break;

      default:
        break;
    }

    /* STEP 2: evaluate the current condition */
    condition->is_true = (char)rc_test_condition(condition, eval_state);
    eval_state->add_value.type = RC_VALUE_TYPE_NONE;
    eval_state->add_address = 0;

    /* apply logic flags and reset them for the next condition */
    cond_valid = condition->is_true;
    cond_valid &= and_next;
    cond_valid |= or_next;
    and_next = 1;
    or_next = 0;

    if (reset_next) {
      /* previous ResetNextIf resets the hit count on this condition and prevents it from being true */
      if (condition->current_hits)
        eval_state->was_cond_reset = 1;

      condition->current_hits = 0;
      cond_valid = 0;
    }
    else if (cond_valid) {
      /* true conditions should update hit count */
      eval_state->has_hits = 1;

      if (condition->required_hits == 0) {
        /* no target hit count, just keep tallying */
        ++condition->current_hits;
      }
      else if (condition->current_hits < condition->required_hits) {
        /* target hit count hasn't been met, tally and revalidate - only true if hit count becomes met */
        ++condition->current_hits;
        cond_valid = (condition->current_hits == condition->required_hits);
      }
      else {
        /* target hit count has been met, do nothing */
      }
    }
    else if (condition->current_hits > 0) {
      /* target has been true in the past, if the hit target is met, consider it true now */
      eval_state->has_hits = 1;
      cond_valid = (condition->current_hits == condition->required_hits);
    }

    /* STEP 3: handle logic flags */
    switch (condition->type) {
      case RC_CONDITION_ADD_HITS:
        eval_state->add_hits += condition->current_hits;
        reset_next = 0; /* ResetNextIf was applied to this AddHits condition; don't apply it to future conditions */
        continue;

      case RC_CONDITION_SUB_HITS:
        eval_state->add_hits -= condition->current_hits;
        reset_next = 0; /* ResetNextIf was applied to this AddHits condition; don't apply it to future conditions */
        continue;

      case RC_CONDITION_RESET_NEXT_IF:
        reset_next = cond_valid;
        continue;

      case RC_CONDITION_AND_NEXT:
        and_next = cond_valid;
        continue;

      case RC_CONDITION_OR_NEXT:
        or_next = cond_valid;
        continue;

      default:
        break;
    }

    /* reset logic flags for next condition */
    reset_next = 0;

    /* STEP 4: calculate total hits */
    total_hits = condition->current_hits;

    if (eval_state->add_hits) {
      if (condition->required_hits != 0) {
        /* if the condition has a target hit count, we have to recalculate cond_valid including the AddHits counter */
        const int signed_hits = (int)condition->current_hits + eval_state->add_hits;
        total_hits = (signed_hits >= 0) ? (unsigned)signed_hits : 0;
        cond_valid = (total_hits >= condition->required_hits);
      }
      else {
        /* no target hit count. we can't tell if the add_hits value is from this frame or not, so ignore it.
           complex condition will only be true if the current condition is true */
      }

      eval_state->add_hits = 0;
    }

    /* STEP 5: handle special flags */
    switch (condition->type) {
      case RC_CONDITION_PAUSE_IF:
        /* as soon as we find a PauseIf that evaluates to true, stop processing the rest of the group */
        if (cond_valid) {
          /* indirect memrefs are not updated as part of the rc_update_memref_values call.
           * an active pause aborts processing of the remaining part of the pause subset and the entire non-pause subset.
           * if the set has any indirect memrefs, manually update them now so the deltas are correct */
          if (self->has_indirect_memrefs) {
            /* first, update any indirect memrefs in the remaining part of the pause subset  */
            rc_condset_update_indirect_memrefs(condition->next, 1, eval_state);

            /* then, update all indirect memrefs in the non-pause subset */
            rc_condset_update_indirect_memrefs(self->conditions, 0, eval_state);
          }

          return 1;
        }

        /* if we make it to the end of the function, make sure we indicate that nothing matched. if we do find
           a later PauseIf match, it'll automatically return true via the previous condition. */
        set_valid = 0;

        if (condition->required_hits == 0) {
          /* PauseIf didn't evaluate true, and doesn't have a HitCount, reset the HitCount to indicate the condition didn't match */
          condition->current_hits = 0;
        }
        else {
          /* PauseIf has a HitCount that hasn't been met, ignore it for now. */
        }

        continue;

      case RC_CONDITION_RESET_IF:
        if (cond_valid) {
          eval_state->was_reset = 1; /* let caller know to reset all hit counts */
          set_valid = 0; /* cannot be valid if we've hit a reset condition */
        }
        continue;

      case RC_CONDITION_MEASURED:
        if (condition->required_hits != 0) {
          /* if there's a hit target, capture the current hits for recording Measured value later */
          measured_from_hits = 1;
          if (can_measure) {
            measured_value.value.u32 = total_hits;
            measured_value.type = RC_VALUE_TYPE_UNSIGNED;
          }
        }
        break;

      case RC_CONDITION_MEASURED_IF:
        if (!cond_valid) {
          measured_value.value.u32 = 0;
          measured_value.type = RC_VALUE_TYPE_UNSIGNED;
          can_measure = 0;
        }
        break;

      case RC_CONDITION_TRIGGER:
        /* update truthiness of set, but do not update truthiness of primed state */
        set_valid &= cond_valid;
        continue;

      default:
        break;
    }

    /* STEP 5: update overall truthiness of set and primed state */
    eval_state->primed &= cond_valid;
    set_valid &= cond_valid;
  }

  if (measured_value.type != RC_VALUE_TYPE_NONE) {
    /* if no previous Measured value was captured, or the new one is greater, keep the new one */
    if (eval_state->measured_value.type == RC_VALUE_TYPE_NONE ||
        rc_typed_value_compare(&measured_value, &eval_state->measured_value, RC_OPERATOR_GT)) {
      memcpy(&eval_state->measured_value, &measured_value, sizeof(measured_value));
      eval_state->measured_from_hits = (char)measured_from_hits;
    }
  }

  return set_valid;
}

int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state) {
  if (self->conditions == 0) {
    /* important: empty group must evaluate true */
    return 1;
  }

  if (self->has_pause) {
    /* one or more Pause conditions exists, if any of them are true, stop processing this group */
    self->is_paused = (char)rc_test_condset_internal(self, 1, eval_state);
    if (self->is_paused) {
      eval_state->primed = 0;
      return 0;
    }
  }

  return rc_test_condset_internal(self, 0, eval_state);
}

void rc_reset_condset(rc_condset_t* self) {
  rc_condition_t* condition;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    condition->current_hits = 0;
  }
}
