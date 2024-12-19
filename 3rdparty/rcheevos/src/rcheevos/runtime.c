#include "rc_runtime.h"
#include "rc_internal.h"

#include "../rc_compat.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <string.h>

#define RC_RICHPRESENCE_DISPLAY_BUFFER_SIZE 256

rc_runtime_t* rc_runtime_alloc(void) {
  rc_runtime_t* self = malloc(sizeof(rc_runtime_t));

  if (self) {
    rc_runtime_init(self);
    self->owns_self = 1;
  }

  return self;
}

void rc_runtime_init(rc_runtime_t* self) {
  memset(self, 0, sizeof(rc_runtime_t));
  self->next_memref = &self->memrefs;
  self->next_variable = &self->variables;
}

void rc_runtime_destroy(rc_runtime_t* self) {
  uint32_t i;

  if (self->triggers) {
    for (i = 0; i < self->trigger_count; ++i)
      free(self->triggers[i].buffer);

    free(self->triggers);
    self->triggers = NULL;

    self->trigger_count = self->trigger_capacity = 0;
  }

  if (self->lboards) {
    for (i = 0; i < self->lboard_count; ++i)
      free(self->lboards[i].buffer);

    free(self->lboards);
    self->lboards = NULL;

    self->lboard_count = self->lboard_capacity = 0;
  }

  while (self->richpresence) {
    rc_runtime_richpresence_t* previous = self->richpresence->previous;

    free(self->richpresence->buffer);
    free(self->richpresence);
    self->richpresence = previous;
  }

  self->next_memref = 0;
  self->memrefs = 0;

  if (self->owns_self) {
    free(self);
  }
}

void rc_runtime_checksum(const char* memaddr, uint8_t* md5) {
  md5_state_t state;
  md5_init(&state);
  md5_append(&state, (unsigned char*)memaddr, (int)strlen(memaddr));
  md5_finish(&state, md5);
}

static char rc_runtime_allocated_memrefs(rc_runtime_t* self) {
  char owns_memref = 0;

  /* if at least one memref was allocated within the object, we can't free the buffer when the object is deactivated */
  if (*self->next_memref != NULL) {
    owns_memref = 1;
    /* advance through the new memrefs so we're ready for the next allocation */
    do {
      self->next_memref = &(*self->next_memref)->next;
    } while (*self->next_memref != NULL);
  }

  /* if at least one variable was allocated within the object, we can't free the buffer when the object is deactivated */
  if (*self->next_variable != NULL) {
    owns_memref = 1;
    /* advance through the new variables so we're ready for the next allocation */
    do {
      self->next_variable = &(*self->next_variable)->next;
    } while (*self->next_variable != NULL);
  }

  return owns_memref;
}

static void rc_runtime_deactivate_trigger_by_index(rc_runtime_t* self, uint32_t index) {
  if (self->triggers[index].owns_memrefs) {
    /* if the trigger has one or more memrefs in its buffer, we can't free the buffer.
     * just null out the trigger so the runtime processor will skip it
     */
    rc_reset_trigger(self->triggers[index].trigger);
    self->triggers[index].trigger = NULL;
  }
  else {
    /* trigger doesn't own any memrefs, go ahead and free it, then replace it with the last trigger */
    free(self->triggers[index].buffer);

    if (--self->trigger_count > index)
      memcpy(&self->triggers[index], &self->triggers[self->trigger_count], sizeof(rc_runtime_trigger_t));
  }
}

void rc_runtime_deactivate_achievement(rc_runtime_t* self, uint32_t id) {
  uint32_t i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL)
      rc_runtime_deactivate_trigger_by_index(self, i);
  }
}

int rc_runtime_activate_achievement(rc_runtime_t* self, uint32_t id, const char* memaddr, lua_State* L, int funcs_idx) {
  void* trigger_buffer;
  rc_trigger_t* trigger;
  rc_runtime_trigger_t* runtime_trigger;
  rc_parse_state_t parse;
  uint8_t md5[16];
  int32_t size;
  uint32_t i;

  if (memaddr == NULL)
    return RC_INVALID_MEMORY_OPERAND;

  rc_runtime_checksum(memaddr, md5);

  /* check to see if the id is already registered with an active trigger */
  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL) {
      if (memcmp(self->triggers[i].md5, md5, 16) == 0) {
        /* if the checksum hasn't changed, we can reuse the existing item */
        rc_reset_trigger(self->triggers[i].trigger);
        return RC_OK;
      }

      /* checksum has changed, deactivate the the item */
      rc_runtime_deactivate_trigger_by_index(self, i);

      /* deactivate may reorder the list so we should continue from the current index. however, we
       * assume that only one trigger is active per id, so having found that, just stop scanning.
       */
      break;
    }
  }

  /* check to see if a disabled trigger for the specific id matches the trigger being registered */
  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && memcmp(self->triggers[i].md5, md5, 16) == 0) {
      /* retrieve the trigger pointer from the buffer */
      size = 0;
      trigger = (rc_trigger_t*)rc_alloc(self->triggers[i].buffer, &size, sizeof(rc_trigger_t), RC_ALIGNOF(rc_trigger_t), NULL, -1);
      self->triggers[i].trigger = trigger;

      rc_reset_trigger(trigger);
      return RC_OK;
    }
  }

  /* item has not been previously registered, determine how much space we need for it, and allocate it */
  size = rc_trigger_size(memaddr);
  if (size < 0)
    return size;

  trigger_buffer = malloc(size);
  if (!trigger_buffer)
    return RC_OUT_OF_MEMORY;

  /* populate the item, using the communal memrefs pool */
  rc_init_parse_state(&parse, trigger_buffer, L, funcs_idx);
  parse.first_memref = &self->memrefs;
  trigger = RC_ALLOC(rc_trigger_t, &parse);
  rc_parse_trigger_internal(trigger, &memaddr, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(trigger_buffer);
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  /* grow the trigger buffer if necessary */
  if (self->trigger_count == self->trigger_capacity) {
    self->trigger_capacity += 32;
    if (!self->triggers)
      self->triggers = (rc_runtime_trigger_t*)malloc(self->trigger_capacity * sizeof(rc_runtime_trigger_t));
    else
      self->triggers = (rc_runtime_trigger_t*)realloc(self->triggers, self->trigger_capacity * sizeof(rc_runtime_trigger_t));

    if (!self->triggers) {
      free(trigger_buffer);
      *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
      return RC_OUT_OF_MEMORY;
    }
  }

  /* assign the new trigger */
  runtime_trigger = &self->triggers[self->trigger_count];
  runtime_trigger->id = id;
  runtime_trigger->trigger = trigger;
  runtime_trigger->buffer = trigger_buffer;
  runtime_trigger->invalid_memref = NULL;
  memcpy(runtime_trigger->md5, md5, 16);
  runtime_trigger->serialized_size = 0;
  runtime_trigger->owns_memrefs = rc_runtime_allocated_memrefs(self);
  ++self->trigger_count;

  /* reset it, and return it */
  trigger->memrefs = NULL;
  rc_reset_trigger(trigger);
  return RC_OK;
}

rc_trigger_t* rc_runtime_get_achievement(const rc_runtime_t* self, uint32_t id)
{
  uint32_t i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].id == id && self->triggers[i].trigger != NULL)
      return self->triggers[i].trigger;
  }

  return NULL;
}

int rc_runtime_get_achievement_measured(const rc_runtime_t* runtime, uint32_t id, unsigned* measured_value, unsigned* measured_target)
{
  const rc_trigger_t* trigger = rc_runtime_get_achievement(runtime, id);
  if (!measured_value || !measured_target)
    return 0;

  if (!trigger) {
    *measured_value = *measured_target = 0;
    return 0;
  }

  if (rc_trigger_state_active(trigger->state)) {
    *measured_value = (trigger->measured_value == RC_MEASURED_UNKNOWN) ? 0 : trigger->measured_value;
    *measured_target = trigger->measured_target;
  }
  else {
    /* don't report measured information for inactive triggers */
    *measured_value = *measured_target = 0;
  }

  return 1;
}

int rc_runtime_format_achievement_measured(const rc_runtime_t* runtime, uint32_t id, char* buffer, size_t buffer_size)
{
  const rc_trigger_t* trigger = rc_runtime_get_achievement(runtime, id);
  uint32_t value;
  if (!buffer || !buffer_size)
    return 0;

  if (!trigger || /* no trigger */
      trigger->measured_target == 0 || /* not measured */
      !rc_trigger_state_active(trigger->state)) { /* don't report measured value for inactive triggers */
    *buffer = '\0';
    return 0;
  }

  /* cap the value at the target so we can count past the target: "107 >= 100" */
  value = (trigger->measured_value == RC_MEASURED_UNKNOWN) ? 0 : trigger->measured_value;
  if (value > trigger->measured_target)
    value = trigger->measured_target;

  if (trigger->measured_as_percent) {
    const uint32_t percent = (uint32_t)(((unsigned long long)value * 100) / trigger->measured_target);
    return snprintf(buffer, buffer_size, "%u%%", percent);
  }

  return snprintf(buffer, buffer_size, "%u/%u", value, trigger->measured_target);
}

static void rc_runtime_deactivate_lboard_by_index(rc_runtime_t* self, uint32_t index) {
  if (self->lboards[index].owns_memrefs) {
    /* if the lboard has one or more memrefs in its buffer, we can't free the buffer.
     * just null out the lboard so the runtime processor will skip it
     */
    rc_reset_lboard(self->lboards[index].lboard);
    self->lboards[index].lboard = NULL;
  }
  else {
    /* lboard doesn't own any memrefs, go ahead and free it, then replace it with the last lboard */
    free(self->lboards[index].buffer);

    if (--self->lboard_count > index)
      memcpy(&self->lboards[index], &self->lboards[self->lboard_count], sizeof(rc_runtime_lboard_t));
  }
}

void rc_runtime_deactivate_lboard(rc_runtime_t* self, uint32_t id) {
  uint32_t i;

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL)
      rc_runtime_deactivate_lboard_by_index(self, i);
  }
}

int rc_runtime_activate_lboard(rc_runtime_t* self, uint32_t id, const char* memaddr, lua_State* L, int funcs_idx) {
  void* lboard_buffer;
  uint8_t md5[16];
  rc_lboard_t* lboard;
  rc_parse_state_t parse;
  rc_runtime_lboard_t* runtime_lboard;
  int size;
  uint32_t i;

  if (memaddr == 0)
    return RC_INVALID_MEMORY_OPERAND;

  rc_runtime_checksum(memaddr, md5);

  /* check to see if the id is already registered with an active lboard */
  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL) {
      if (memcmp(self->lboards[i].md5, md5, 16) == 0) {
        /* if the checksum hasn't changed, we can reuse the existing item */
        rc_reset_lboard(self->lboards[i].lboard);
        return RC_OK;
      }

      /* checksum has changed, deactivate the the item */
      rc_runtime_deactivate_lboard_by_index(self, i);

      /* deactivate may reorder the list so we should continue from the current index. however, we
       * assume that only one trigger is active per id, so having found that, just stop scanning.
       */
      break;
    }
  }

  /* check to see if a disabled lboard for the specific id matches the lboard being registered */
  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && memcmp(self->lboards[i].md5, md5, 16) == 0) {
      /* retrieve the lboard pointer from the buffer */
      size = 0;
      lboard = (rc_lboard_t*)rc_alloc(self->lboards[i].buffer, &size, sizeof(rc_lboard_t), RC_ALIGNOF(rc_lboard_t), NULL, -1);
      self->lboards[i].lboard = lboard;

      rc_reset_lboard(lboard);
      return RC_OK;
    }
  }

  /* item has not been previously registered, determine how much space we need for it, and allocate it */
  size = rc_lboard_size(memaddr);
  if (size < 0)
    return size;

  lboard_buffer = malloc(size);
  if (!lboard_buffer)
    return RC_OUT_OF_MEMORY;

  /* populate the item, using the communal memrefs pool */
  rc_init_parse_state(&parse, lboard_buffer, L, funcs_idx);
  lboard = RC_ALLOC(rc_lboard_t, &parse);
  parse.first_memref = &self->memrefs;
  rc_parse_lboard_internal(lboard, memaddr, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(lboard_buffer);
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  /* grow the lboard buffer if necessary */
  if (self->lboard_count == self->lboard_capacity) {
    self->lboard_capacity += 16;
    if (!self->lboards)
      self->lboards = (rc_runtime_lboard_t*)malloc(self->lboard_capacity * sizeof(rc_runtime_lboard_t));
    else
      self->lboards = (rc_runtime_lboard_t*)realloc(self->lboards, self->lboard_capacity * sizeof(rc_runtime_lboard_t));

    if (!self->lboards) {
      free(lboard_buffer);
      *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
      return RC_OUT_OF_MEMORY;
    }
  }

  /* assign the new lboard */
  runtime_lboard = &self->lboards[self->lboard_count++];
  runtime_lboard->id = id;
  runtime_lboard->value = 0;
  runtime_lboard->lboard = lboard;
  runtime_lboard->buffer = lboard_buffer;
  runtime_lboard->invalid_memref = NULL;
  memcpy(runtime_lboard->md5, md5, 16);
  runtime_lboard->serialized_size = 0;
  runtime_lboard->owns_memrefs = rc_runtime_allocated_memrefs(self);

  /* reset it, and return it */
  lboard->memrefs = NULL;
  rc_reset_lboard(lboard);
  return RC_OK;
}

rc_lboard_t* rc_runtime_get_lboard(const rc_runtime_t* self, uint32_t id)
{
  uint32_t i;

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].id == id && self->lboards[i].lboard != NULL)
      return self->lboards[i].lboard;
  }

  return NULL;
}

int rc_runtime_format_lboard_value(char* buffer, int size, int32_t value, int format)
{
  return rc_format_value(buffer, size, value, format);
}

int rc_runtime_activate_richpresence(rc_runtime_t* self, const char* script, lua_State* L, int funcs_idx) {
  rc_richpresence_t* richpresence;
  rc_runtime_richpresence_t* previous;
  rc_runtime_richpresence_t** previous_ptr;
  rc_parse_state_t parse;
  uint8_t md5[16];
  int size;

  if (script == NULL)
    return RC_MISSING_DISPLAY_STRING;

  rc_runtime_checksum(script, md5);

  /* look for existing match */
  previous_ptr = NULL;
  previous = self->richpresence;
  while (previous) {
    if (previous && self->richpresence->richpresence && memcmp(self->richpresence->md5, md5, 16) == 0) {
      /* unchanged. reset all of the conditions */
      rc_reset_richpresence(self->richpresence->richpresence);

      /* move to front of linked list*/
      if (previous_ptr) {
        *previous_ptr = previous->previous;
        if (!self->richpresence->owns_memrefs) {
          free(self->richpresence->buffer);
          previous->previous = self->richpresence->previous;
        }
        else {
          previous->previous = self->richpresence;
        }

        self->richpresence = previous;
      }

      /* return success*/
      return RC_OK;
    }

    previous_ptr = &previous->previous;
    previous = previous->previous;
  }

  /* no existing match found, parse script */
  size = rc_richpresence_size(script);
  if (size < 0)
    return size;

  /* if the previous script doesn't have any memrefs, free it */
  previous = self->richpresence;
  if (previous) {
    if (!previous->owns_memrefs) {
      free(previous->buffer);
      previous = previous->previous;
    }
  }

  /* allocate and process the new script */
  self->richpresence = (rc_runtime_richpresence_t*)malloc(sizeof(rc_runtime_richpresence_t));
  if (!self->richpresence)
    return RC_OUT_OF_MEMORY;

  self->richpresence->previous = previous;
  self->richpresence->owns_memrefs = 0;
  memcpy(self->richpresence->md5, md5, sizeof(md5));
  self->richpresence->buffer = malloc(size);

  if (!self->richpresence->buffer)
    return RC_OUT_OF_MEMORY;

  rc_init_parse_state(&parse, self->richpresence->buffer, L, funcs_idx);
  self->richpresence->richpresence = richpresence = RC_ALLOC(rc_richpresence_t, &parse);
  parse.first_memref = &self->memrefs;
  parse.variables = &self->variables;
  rc_parse_richpresence_internal(richpresence, script, &parse);
  rc_destroy_parse_state(&parse);

  if (parse.offset < 0) {
    free(self->richpresence->buffer);
    free(self->richpresence);
    self->richpresence = previous;
    *self->next_memref = NULL; /* disassociate any memrefs allocated by the failed parse */
    return parse.offset;
  }

  self->richpresence->owns_memrefs = rc_runtime_allocated_memrefs(self);

  richpresence->memrefs = NULL;
  richpresence->variables = NULL;

  if (!richpresence->first_display || !richpresence->first_display->display) {
    /* non-existant rich presence */
    self->richpresence->richpresence = NULL;
  }
  else {
    /* reset all of the conditions */
    rc_reset_richpresence(richpresence);
  }

  return RC_OK;
}

int rc_runtime_get_richpresence(const rc_runtime_t* self, char* buffer, size_t buffersize, rc_runtime_peek_t peek, void* peek_ud, lua_State* L) {
  if (self->richpresence && self->richpresence->richpresence)
    return rc_get_richpresence_display_string(self->richpresence->richpresence, buffer, buffersize, peek, peek_ud, L);

  *buffer = '\0';
  return 0;
}

void rc_runtime_do_frame(rc_runtime_t* self, rc_runtime_event_handler_t event_handler, rc_runtime_peek_t peek, void* ud, lua_State* L) {
  rc_runtime_event_t runtime_event;
  int i;

  runtime_event.value = 0;

  rc_update_memref_values(self->memrefs, peek, ud);
  rc_update_variables(self->variables, peek, ud, L);

  for (i = self->trigger_count - 1; i >= 0; --i) {
    rc_trigger_t* trigger = self->triggers[i].trigger;
    int old_state, new_state;
    uint32_t old_measured_value;

    if (!trigger)
      continue;

    if (self->triggers[i].invalid_memref) {
      runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED;
      runtime_event.id = self->triggers[i].id;
      runtime_event.value = self->triggers[i].invalid_memref->address;

      trigger->state = RC_TRIGGER_STATE_DISABLED;
      self->triggers[i].invalid_memref = NULL;

      event_handler(&runtime_event);

      runtime_event.value = 0; /* achievement loop expects this to stay at 0 */
      continue;
    }

    old_measured_value = trigger->measured_value;
    old_state = trigger->state;
    new_state = rc_evaluate_trigger(trigger, peek, ud, L);

    /* trigger->state doesn't actually change to RESET, RESET just serves as a notification.
     * handle the notification, then look at the actual state */
    if (new_state == RC_TRIGGER_STATE_RESET) {
      runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_RESET;
      runtime_event.id = self->triggers[i].id;
      event_handler(&runtime_event);

      new_state = trigger->state;
    }

    /* if the measured value changed and the achievement hasn't triggered, send a notification */
    if (trigger->measured_value != old_measured_value && old_measured_value != RC_MEASURED_UNKNOWN &&
        trigger->measured_target != 0 && trigger->measured_value <= trigger->measured_target &&
        new_state != RC_TRIGGER_STATE_TRIGGERED &&
        new_state != RC_TRIGGER_STATE_INACTIVE && new_state != RC_TRIGGER_STATE_WAITING) {

      runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_PROGRESS_UPDATED;
      runtime_event.id = self->triggers[i].id;

      if (trigger->measured_as_percent) {
        /* if reporting measured value as a percentage, only send the notification if the percentage changes */
        const int32_t old_percent = (int32_t)(((unsigned long long)old_measured_value * 100) / trigger->measured_target);
        const int32_t new_percent = (int32_t)(((unsigned long long)trigger->measured_value * 100) / trigger->measured_target);
        if (old_percent != new_percent) {
          runtime_event.value = new_percent;
          event_handler(&runtime_event);
        }
      }
      else {
        runtime_event.value = trigger->measured_value;
        event_handler(&runtime_event);
      }

      runtime_event.value = 0; /* achievement loop expects this to stay at 0 */
    }

    /* if the state hasn't changed, there won't be any events raised */
    if (new_state == old_state)
      continue;

    /* raise an UNPRIMED event when changing from PRIMED to anything else */
    if (old_state == RC_TRIGGER_STATE_PRIMED) {
      runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_UNPRIMED;
      runtime_event.id = self->triggers[i].id;
      event_handler(&runtime_event);
    }

    /* raise events for each of the possible new states */
    switch (new_state)
    {
      case RC_TRIGGER_STATE_TRIGGERED:
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED;
        runtime_event.id = self->triggers[i].id;
        event_handler(&runtime_event);
        break;

      case RC_TRIGGER_STATE_PAUSED:
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED;
        runtime_event.id = self->triggers[i].id;
        event_handler(&runtime_event);
        break;

      case RC_TRIGGER_STATE_PRIMED:
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED;
        runtime_event.id = self->triggers[i].id;
        event_handler(&runtime_event);
        break;

      case RC_TRIGGER_STATE_ACTIVE:
        /* only raise ACTIVATED event when transitioning from an inactive state.
         * note that inactive in this case means active but cannot trigger. */
        if (old_state == RC_TRIGGER_STATE_WAITING || old_state == RC_TRIGGER_STATE_PAUSED) {
          runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED;
          runtime_event.id = self->triggers[i].id;
          event_handler(&runtime_event);
        }
        break;
    }
  }

  for (i = self->lboard_count - 1; i >= 0; --i) {
    rc_lboard_t* lboard = self->lboards[i].lboard;
    int lboard_state;

    if (!lboard)
      continue;

    if (self->lboards[i].invalid_memref) {
      runtime_event.type = RC_RUNTIME_EVENT_LBOARD_DISABLED;
      runtime_event.id = self->lboards[i].id;
      runtime_event.value = self->lboards[i].invalid_memref->address;

      lboard->state = RC_LBOARD_STATE_DISABLED;
      self->lboards[i].invalid_memref = NULL;

      event_handler(&runtime_event);
      continue;
    }

    lboard_state = lboard->state;
    switch (rc_evaluate_lboard(lboard, &runtime_event.value, peek, ud, L))
    {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (lboard_state != RC_LBOARD_STATE_STARTED) {
          self->lboards[i].value = runtime_event.value;

          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_STARTED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        else if (runtime_event.value != self->lboards[i].value) {
          self->lboards[i].value = runtime_event.value;

          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_UPDATED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (lboard_state != RC_LBOARD_STATE_CANCELED) {
          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_CANCELED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (lboard_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          runtime_event.type = RC_RUNTIME_EVENT_LBOARD_TRIGGERED;
          runtime_event.id = self->lboards[i].id;
          event_handler(&runtime_event);
        }
        break;
    }
  }

  if (self->richpresence && self->richpresence->richpresence)
    rc_update_richpresence(self->richpresence->richpresence, peek, ud, L);
}

void rc_runtime_reset(rc_runtime_t* self) {
  rc_value_t* variable;
  uint32_t i;

  for (i = 0; i < self->trigger_count; ++i) {
    if (self->triggers[i].trigger)
      rc_reset_trigger(self->triggers[i].trigger);
  }

  for (i = 0; i < self->lboard_count; ++i) {
    if (self->lboards[i].lboard)
      rc_reset_lboard(self->lboards[i].lboard);
  }

  if (self->richpresence && self->richpresence->richpresence)
    rc_reset_richpresence(self->richpresence->richpresence);

  for (variable = self->variables; variable; variable = variable->next)
    rc_reset_value(variable);
}

static int rc_condset_contains_memref(const rc_condset_t* condset, const rc_memref_t* memref) {
  rc_condition_t* cond;
  if (!condset)
    return 0;

  for (cond = condset->conditions; cond; cond = cond->next) {
    if (rc_operand_is_memref(&cond->operand1) && cond->operand1.value.memref == memref)
      return 1;
    if (rc_operand_is_memref(&cond->operand2) && cond->operand2.value.memref == memref)
      return 1;
  }

  return 0;
}

int rc_value_contains_memref(const rc_value_t* value, const rc_memref_t* memref) {
  rc_condset_t* condset;
  if (!value)
    return 0;

  for (condset = value->conditions; condset; condset = condset->next) {
    if (rc_condset_contains_memref(condset, memref))
      return 1;
  }

  return 0;
}

int rc_trigger_contains_memref(const rc_trigger_t* trigger, const rc_memref_t* memref) {
  rc_condset_t* condset;
  if (!trigger)
    return 0;

  if (rc_condset_contains_memref(trigger->requirement, memref))
    return 1;

  for (condset = trigger->alternative; condset; condset = condset->next) {
    if (rc_condset_contains_memref(condset, memref))
      return 1;
  }

  return 0;
}

static void rc_runtime_invalidate_memref(rc_runtime_t* self, rc_memref_t* memref) {
  uint32_t i;

  /* disable any achievements dependent on the address */
  for (i = 0; i < self->trigger_count; ++i) {
    if (!self->triggers[i].invalid_memref && rc_trigger_contains_memref(self->triggers[i].trigger, memref))
      self->triggers[i].invalid_memref = memref;
  }

  /* disable any leaderboards dependent on the address */
  for (i = 0; i < self->lboard_count; ++i) {
    if (!self->lboards[i].invalid_memref) {
      rc_lboard_t* lboard = self->lboards[i].lboard;
      if (lboard) {
        if (rc_trigger_contains_memref(&lboard->start, memref)) {
          lboard->start.state = RC_TRIGGER_STATE_DISABLED;
          self->lboards[i].invalid_memref = memref;
        }

        if (rc_trigger_contains_memref(&lboard->cancel, memref)) {
          lboard->cancel.state = RC_TRIGGER_STATE_DISABLED;
          self->lboards[i].invalid_memref = memref;
        }

        if (rc_trigger_contains_memref(&lboard->submit, memref)) {
          lboard->submit.state = RC_TRIGGER_STATE_DISABLED;
          self->lboards[i].invalid_memref = memref;
        }

        if (rc_value_contains_memref(&lboard->value, memref))
          self->lboards[i].invalid_memref = memref;
      }
    }
  }
}

void rc_runtime_invalidate_address(rc_runtime_t* self, uint32_t address) {
  rc_memref_t** last_memref = &self->memrefs;
  rc_memref_t* memref = self->memrefs;

  while (memref) {
    if (memref->address == address && !memref->value.is_indirect) {
      /* remove the invalid memref from the chain so we don't try to evaluate it in the future.
       * it's still there, so anything referencing it will continue to fetch 0.
       */
      *last_memref = memref->next;

      rc_runtime_invalidate_memref(self, memref);
      break;
    }

    last_memref = &memref->next;
    memref = *last_memref;
  }
}

void rc_runtime_validate_addresses(rc_runtime_t* self, rc_runtime_event_handler_t event_handler, 
    rc_runtime_validate_address_t validate_handler) {
  rc_memref_t** last_memref = &self->memrefs;
  rc_memref_t* memref = self->memrefs;
  int num_invalid = 0;

  while (memref) {
    if (!memref->value.is_indirect && !validate_handler(memref->address)) {
      /* remove the invalid memref from the chain so we don't try to evaluate it in the future.
       * it's still there, so anything referencing it will continue to fetch 0.
       */
      *last_memref = memref->next;

      rc_runtime_invalidate_memref(self, memref);
      ++num_invalid;
    }
    else {
      last_memref = &memref->next;
    }

    memref = *last_memref;
  }

  if (num_invalid) {
    rc_runtime_event_t runtime_event;
    int i;

    for (i = self->trigger_count - 1; i >= 0; --i) {
      rc_trigger_t* trigger = self->triggers[i].trigger;
      if (trigger && self->triggers[i].invalid_memref) {
        runtime_event.type = RC_RUNTIME_EVENT_ACHIEVEMENT_DISABLED;
        runtime_event.id = self->triggers[i].id;
        runtime_event.value = self->triggers[i].invalid_memref->address;

        trigger->state = RC_TRIGGER_STATE_DISABLED;
        self->triggers[i].invalid_memref = NULL;

        event_handler(&runtime_event);
      }
    }

    for (i = self->lboard_count - 1; i >= 0; --i) {
      rc_lboard_t* lboard = self->lboards[i].lboard;
      if (lboard && self->lboards[i].invalid_memref) {
        runtime_event.type = RC_RUNTIME_EVENT_LBOARD_DISABLED;
        runtime_event.id = self->lboards[i].id;
        runtime_event.value = self->lboards[i].invalid_memref->address;

        lboard->state = RC_LBOARD_STATE_DISABLED;
        self->lboards[i].invalid_memref = NULL;

        event_handler(&runtime_event);
      }
    }
  }
}
