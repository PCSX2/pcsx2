#include "rc_runtime.h"
#include "rc_internal.h"

#include "rc_util.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <string.h>

#define RC_RUNTIME_MARKER             0x0A504152 /* RAP\n */

#define RC_RUNTIME_CHUNK_MEMREFS      0x4645524D /* MREF */
#define RC_RUNTIME_CHUNK_VARIABLES    0x53524156 /* VARS */
#define RC_RUNTIME_CHUNK_ACHIEVEMENT  0x56484341 /* ACHV */
#define RC_RUNTIME_CHUNK_LEADERBOARD  0x4452424C /* LBRD */
#define RC_RUNTIME_CHUNK_RICHPRESENCE 0x48434952 /* RICH */

#define RC_RUNTIME_CHUNK_DONE         0x454E4F44 /* DONE */

typedef struct rc_runtime_progress_t {
  const rc_runtime_t* runtime;

  uint32_t offset;
  uint8_t* buffer;

  uint32_t chunk_size_offset;

  lua_State* L;
} rc_runtime_progress_t;

#define RC_TRIGGER_STATE_UNUPDATED 0x7F

#define RC_MEMREF_FLAG_CHANGED_THIS_FRAME 0x00010000

#define RC_VAR_FLAG_HAS_COND_DATA         0x01000000

#define RC_COND_FLAG_IS_TRUE                            0x00000001
#define RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF        0x00010000
#define RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME 0x00020000
#define RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF        0x00100000
#define RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME 0x00200000

static void rc_runtime_progress_write_uint(rc_runtime_progress_t* progress, uint32_t value)
{
  if (progress->buffer) {
    progress->buffer[progress->offset + 0] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 1] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 2] = value & 0xFF; value >>= 8;
    progress->buffer[progress->offset + 3] = value & 0xFF;
  }

  progress->offset += 4;
}

static uint32_t rc_runtime_progress_read_uint(rc_runtime_progress_t* progress)
{
  uint32_t value = progress->buffer[progress->offset + 0] |
      (progress->buffer[progress->offset + 1] << 8) |
      (progress->buffer[progress->offset + 2] << 16) |
      (progress->buffer[progress->offset + 3] << 24);

  progress->offset += 4;
  return value;
}

static void rc_runtime_progress_write_md5(rc_runtime_progress_t* progress, uint8_t* md5)
{
  if (progress->buffer)
    memcpy(&progress->buffer[progress->offset], md5, 16);

  progress->offset += 16;
}

static int rc_runtime_progress_match_md5(rc_runtime_progress_t* progress, uint8_t* md5)
{
  int result = 0;
  if (progress->buffer)
    result = (memcmp(&progress->buffer[progress->offset], md5, 16) == 0);

  progress->offset += 16;

  return result;
}

static void rc_runtime_progress_start_chunk(rc_runtime_progress_t* progress, uint32_t chunk_id)
{
  rc_runtime_progress_write_uint(progress, chunk_id);

  progress->chunk_size_offset = progress->offset;

  progress->offset += 4;
}

static void rc_runtime_progress_end_chunk(rc_runtime_progress_t* progress)
{
  uint32_t length;
  uint32_t offset;

  progress->offset = (progress->offset + 3) & ~0x03; /* align to 4 byte boundary */

  if (progress->buffer) {
    /* ignore chunk size field when calculating chunk size */
    length = (uint32_t)(progress->offset - progress->chunk_size_offset - 4);

    /* temporarily update the write pointer to write the chunk size field */
    offset = progress->offset;
    progress->offset = progress->chunk_size_offset;
    rc_runtime_progress_write_uint(progress, length);
    progress->offset = offset;
  }
}

static void rc_runtime_progress_init(rc_runtime_progress_t* progress, const rc_runtime_t* runtime, lua_State* L)
{
  memset(progress, 0, sizeof(rc_runtime_progress_t));
  progress->runtime = runtime;
  progress->L = L;
}

static int rc_runtime_progress_write_memrefs(rc_runtime_progress_t* progress)
{
  rc_memref_t* memref = progress->runtime->memrefs;
  uint32_t flags = 0;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_MEMREFS);

  if (!progress->buffer) {
    while (memref) {
      progress->offset += 16;
      memref = memref->next;
    }
  }
  else {
    while (memref) {
      flags = memref->value.size;
      if (memref->value.changed)
        flags |= RC_MEMREF_FLAG_CHANGED_THIS_FRAME;

      rc_runtime_progress_write_uint(progress, memref->address);
      rc_runtime_progress_write_uint(progress, flags);
      rc_runtime_progress_write_uint(progress, memref->value.value);
      rc_runtime_progress_write_uint(progress, memref->value.prior);

      memref = memref->next;
    }
  }

  rc_runtime_progress_end_chunk(progress);
  return RC_OK;
}

static int rc_runtime_progress_read_memrefs(rc_runtime_progress_t* progress)
{
  uint32_t entries;
  uint32_t address, flags, value, prior;
  uint8_t size;
  rc_memref_t* memref;
  rc_memref_t* first_unmatched_memref = progress->runtime->memrefs;

  /* re-read the chunk size to determine how many memrefs are present */
  progress->offset -= 4;
  entries = rc_runtime_progress_read_uint(progress) / 16;

  while (entries != 0) {
    address = rc_runtime_progress_read_uint(progress);
    flags = rc_runtime_progress_read_uint(progress);
    value = rc_runtime_progress_read_uint(progress);
    prior = rc_runtime_progress_read_uint(progress);

    size = flags & 0xFF;

    memref = first_unmatched_memref;
    while (memref) {
      if (memref->address == address && memref->value.size == size) {
        memref->value.value = value;
        memref->value.changed = (flags & RC_MEMREF_FLAG_CHANGED_THIS_FRAME) ? 1 : 0;
        memref->value.prior = prior;

        if (memref == first_unmatched_memref)
          first_unmatched_memref = memref->next;

        break;
      }

      memref = memref->next;
    }

    --entries;
  }

  return RC_OK;
}

static int rc_runtime_progress_is_indirect_memref(rc_operand_t* oper)
{
  switch (oper->type)
  {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_LUA:
      return 0;

    default:
      return oper->value.memref->value.is_indirect;
  }
}

static int rc_runtime_progress_write_condset(rc_runtime_progress_t* progress, rc_condset_t* condset)
{
  rc_condition_t* cond;
  uint32_t flags;

  rc_runtime_progress_write_uint(progress, condset->is_paused);

  cond = condset->conditions;
  while (cond) {
    flags = 0;
    if (cond->is_true)
      flags |= RC_COND_FLAG_IS_TRUE;

    if (rc_runtime_progress_is_indirect_memref(&cond->operand1)) {
      flags |= RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF;
      if (cond->operand1.value.memref->value.changed)
        flags |= RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME;
    }

    if (rc_runtime_progress_is_indirect_memref(&cond->operand2)) {
      flags |= RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF;
      if (cond->operand2.value.memref->value.changed)
        flags |= RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME;
    }

    rc_runtime_progress_write_uint(progress, cond->current_hits);
    rc_runtime_progress_write_uint(progress, flags);

    if (flags & RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF) {
      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.value);
      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.prior);
    }

    if (flags & RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF) {
      rc_runtime_progress_write_uint(progress, cond->operand2.value.memref->value.value);
      rc_runtime_progress_write_uint(progress, cond->operand2.value.memref->value.prior);
    }

    cond = cond->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_condset(rc_runtime_progress_t* progress, rc_condset_t* condset)
{
  rc_condition_t* cond;
  uint32_t flags;

  condset->is_paused = (char)rc_runtime_progress_read_uint(progress);

  cond = condset->conditions;
  while (cond) {
    cond->current_hits = rc_runtime_progress_read_uint(progress);
    flags = rc_runtime_progress_read_uint(progress);

    cond->is_true = (flags & RC_COND_FLAG_IS_TRUE) ? 1 : 0;

    if (flags & RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF) {
      if (!rc_operand_is_memref(&cond->operand1)) /* this should never happen, but better safe than sorry */
        return RC_INVALID_STATE;

      cond->operand1.value.memref->value.value = rc_runtime_progress_read_uint(progress);
      cond->operand1.value.memref->value.prior = rc_runtime_progress_read_uint(progress);
      cond->operand1.value.memref->value.changed = (flags & RC_COND_FLAG_OPERAND1_MEMREF_CHANGED_THIS_FRAME) ? 1 : 0;
    }

    if (flags & RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF) {
      if (!rc_operand_is_memref(&cond->operand2)) /* this should never happen, but better safe than sorry */
        return RC_INVALID_STATE;

      cond->operand2.value.memref->value.value = rc_runtime_progress_read_uint(progress);
      cond->operand2.value.memref->value.prior = rc_runtime_progress_read_uint(progress);
      cond->operand2.value.memref->value.changed = (flags & RC_COND_FLAG_OPERAND2_MEMREF_CHANGED_THIS_FRAME) ? 1 : 0;
    }

    cond = cond->next;
  }

  return RC_OK;
}

static uint32_t rc_runtime_progress_should_serialize_variable_condset(const rc_condset_t* conditions)
{
  const rc_condition_t* condition;

  /* predetermined presence of pause flag or indirect memrefs - must serialize */
  if (conditions->has_pause || conditions->has_indirect_memrefs)
    return RC_VAR_FLAG_HAS_COND_DATA;

  /* if any conditions has required hits, must serialize */
  /* ASSERT: Measured with comparison and no explicit target will set hit target to 0xFFFFFFFF */
  for (condition = conditions->conditions; condition; condition = condition->next) {
    if (condition->required_hits > 0)
      return RC_VAR_FLAG_HAS_COND_DATA;
  }

  /* can safely be reset without affecting behavior */
  return 0;
}

static int rc_runtime_progress_write_variable(rc_runtime_progress_t* progress, const rc_value_t* variable)
{
  uint32_t flags;

  flags = rc_runtime_progress_should_serialize_variable_condset(variable->conditions);
  if (variable->value.changed)
    flags |= RC_MEMREF_FLAG_CHANGED_THIS_FRAME;

  rc_runtime_progress_write_uint(progress, flags);
  rc_runtime_progress_write_uint(progress, variable->value.value);
  rc_runtime_progress_write_uint(progress, variable->value.prior);

  if (flags & RC_VAR_FLAG_HAS_COND_DATA) {
    int result = rc_runtime_progress_write_condset(progress, variable->conditions);
    if (result != RC_OK)
      return result;
  }

  return RC_OK;
}

static int rc_runtime_progress_write_variables(rc_runtime_progress_t* progress)
{
  uint32_t count = 0;
  const rc_value_t* variable;

  for (variable = progress->runtime->variables; variable; variable = variable->next)
    ++count;
  if (count == 0)
    return RC_OK;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_VARIABLES);
  rc_runtime_progress_write_uint(progress, count);

  for (variable = progress->runtime->variables; variable; variable = variable->next)
  {
    uint32_t djb2 = rc_djb2(variable->name);
    rc_runtime_progress_write_uint(progress, djb2);

    rc_runtime_progress_write_variable(progress, variable);
  }

  rc_runtime_progress_end_chunk(progress);
  return RC_OK;
}

static int rc_runtime_progress_read_variable(rc_runtime_progress_t* progress, rc_value_t* variable)
{
  uint32_t flags = rc_runtime_progress_read_uint(progress);
  variable->value.changed = (flags & RC_MEMREF_FLAG_CHANGED_THIS_FRAME) ? 1 : 0;
  variable->value.value = rc_runtime_progress_read_uint(progress);
  variable->value.prior = rc_runtime_progress_read_uint(progress);

  if (flags & RC_VAR_FLAG_HAS_COND_DATA) {
    int result = rc_runtime_progress_read_condset(progress, variable->conditions);
    if (result != RC_OK)
      return result;
  }
  else {
    rc_reset_condset(variable->conditions);
  }

  return RC_OK;
}

static int rc_runtime_progress_read_variables(rc_runtime_progress_t* progress)
{
  struct rc_pending_value_t
  {
    rc_value_t* variable;
    uint32_t djb2;
  };
  struct rc_pending_value_t local_pending_variables[32];
  struct rc_pending_value_t* pending_variables;
  rc_value_t* variable;
  uint32_t count, serialized_count;
  int result;
  uint32_t i;

  serialized_count = rc_runtime_progress_read_uint(progress);
  if (serialized_count == 0)
    return RC_OK;

  count = 0;
  for (variable = progress->runtime->variables; variable; variable = variable->next)
    ++count;

  if (count == 0)
    return RC_OK;

  if (count <= sizeof(local_pending_variables) / sizeof(local_pending_variables[0])) {
    pending_variables = local_pending_variables;
  }
  else {
    pending_variables = (struct rc_pending_value_t*)malloc(count * sizeof(struct rc_pending_value_t));
    if (pending_variables == NULL)
      return RC_OUT_OF_MEMORY;
  }

  count = 0;
  for (variable = progress->runtime->variables; variable; variable = variable->next) {
    pending_variables[count].variable = variable;
    pending_variables[count].djb2 = rc_djb2(variable->name);
    ++count;
  }

  result = RC_OK;
  for (; serialized_count > 0 && result == RC_OK; --serialized_count) {
    uint32_t djb2 = rc_runtime_progress_read_uint(progress);
    for (i = 0; i < count; ++i) {
      if (pending_variables[i].djb2 == djb2) {
        variable = pending_variables[i].variable;
        result = rc_runtime_progress_read_variable(progress, variable);
        if (result == RC_OK) {
          if (i < count - 1)
            memcpy(&pending_variables[i], &pending_variables[count - 1], sizeof(struct rc_pending_value_t));
          count--;
        }
        break;
      }
    }
  }

  while (count > 0)
    rc_reset_value(pending_variables[--count].variable);

  if (pending_variables != local_pending_variables)
    free(pending_variables);

  return result;
}

static int rc_runtime_progress_write_trigger(rc_runtime_progress_t* progress, const rc_trigger_t* trigger)
{
  rc_condset_t* condset;
  int result;

  rc_runtime_progress_write_uint(progress, trigger->state);
  rc_runtime_progress_write_uint(progress, trigger->measured_value);

  if (trigger->requirement) {
    result = rc_runtime_progress_write_condset(progress, trigger->requirement);
    if (result != RC_OK)
      return result;
  }

  condset = trigger->alternative;
  while (condset) {
    result = rc_runtime_progress_write_condset(progress, condset);
    if (result != RC_OK)
      return result;

    condset = condset->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_trigger(rc_runtime_progress_t* progress, rc_trigger_t* trigger)
{
  rc_condset_t* condset;
  int result;

  trigger->state = (char)rc_runtime_progress_read_uint(progress);
  trigger->measured_value = rc_runtime_progress_read_uint(progress);

  if (trigger->requirement) {
    result = rc_runtime_progress_read_condset(progress, trigger->requirement);
    if (result != RC_OK)
      return result;
  }

  condset = trigger->alternative;
  while (condset) {
    result = rc_runtime_progress_read_condset(progress, condset);
    if (result != RC_OK)
      return result;

    condset = condset->next;
  }

  return RC_OK;
}

static int rc_runtime_progress_write_achievements(rc_runtime_progress_t* progress)
{
  uint32_t i;
  int offset = 0;
  int result;

  for (i = 0; i < progress->runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &progress->runtime->triggers[i];
    if (!runtime_trigger->trigger)
      continue;

    /* don't store state for inactive or triggered achievements */
    if (!rc_trigger_state_active(runtime_trigger->trigger->state))
      continue;

    if (!progress->buffer) {
      if (runtime_trigger->serialized_size) {
        progress->offset += runtime_trigger->serialized_size;
        continue;
      }

      offset = progress->offset;
    }

    rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_ACHIEVEMENT);
    rc_runtime_progress_write_uint(progress, runtime_trigger->id);
    rc_runtime_progress_write_md5(progress, runtime_trigger->md5);

    result = rc_runtime_progress_write_trigger(progress, runtime_trigger->trigger);
    if (result != RC_OK)
      return result;

    rc_runtime_progress_end_chunk(progress);

    if (!progress->buffer)
      runtime_trigger->serialized_size = progress->offset - offset;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_achievement(rc_runtime_progress_t* progress)
{
  uint32_t id = rc_runtime_progress_read_uint(progress);
  uint32_t i;

  for (i = 0; i < progress->runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &progress->runtime->triggers[i];
    if (runtime_trigger->id == id && runtime_trigger->trigger != NULL) {
      /* ignore triggered and waiting achievements */
      if (runtime_trigger->trigger->state == RC_TRIGGER_STATE_UNUPDATED) {
        /* only update state if definition hasn't changed (md5 matches) */
        if (rc_runtime_progress_match_md5(progress, runtime_trigger->md5))
          return rc_runtime_progress_read_trigger(progress, runtime_trigger->trigger);
        break;
      }
    }
  }

  return RC_OK;
}

static int rc_runtime_progress_write_leaderboards(rc_runtime_progress_t* progress)
{
  uint32_t i;
  uint32_t flags;
  int offset = 0;
  int result;

  for (i = 0; i < progress->runtime->lboard_count; ++i) {
    rc_runtime_lboard_t* runtime_lboard = &progress->runtime->lboards[i];
    if (!runtime_lboard->lboard)
      continue;

    /* don't store state for inactive leaderboards */
    if (!rc_lboard_state_active(runtime_lboard->lboard->state))
      continue;

    if (!progress->buffer) {
      if (runtime_lboard->serialized_size) {
        progress->offset += runtime_lboard->serialized_size;
        continue;
      }

      offset = progress->offset;
    }

    rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_LEADERBOARD);
    rc_runtime_progress_write_uint(progress, runtime_lboard->id);
    rc_runtime_progress_write_md5(progress, runtime_lboard->md5);

    flags = runtime_lboard->lboard->state;
    rc_runtime_progress_write_uint(progress, flags);

    result = rc_runtime_progress_write_trigger(progress, &runtime_lboard->lboard->start);
    if (result != RC_OK)
      return result;

    result = rc_runtime_progress_write_trigger(progress, &runtime_lboard->lboard->submit);
    if (result != RC_OK)
      return result;

    result = rc_runtime_progress_write_trigger(progress, &runtime_lboard->lboard->cancel);
    if (result != RC_OK)
      return result;

    result = rc_runtime_progress_write_variable(progress, &runtime_lboard->lboard->value);
    if (result != RC_OK)
      return result;

    rc_runtime_progress_end_chunk(progress);

    if (!progress->buffer)
      runtime_lboard->serialized_size = progress->offset - offset;
  }

  return RC_OK;
}

static int rc_runtime_progress_read_leaderboard(rc_runtime_progress_t* progress)
{
  uint32_t id = rc_runtime_progress_read_uint(progress);
  uint32_t i;
  int result;

  for (i = 0; i < progress->runtime->lboard_count; ++i) {
    rc_runtime_lboard_t* runtime_lboard = &progress->runtime->lboards[i];
    if (runtime_lboard->id == id && runtime_lboard->lboard != NULL) {
      /* ignore triggered and waiting achievements */
      if (runtime_lboard->lboard->state == RC_TRIGGER_STATE_UNUPDATED) {
        /* only update state if definition hasn't changed (md5 matches) */
        if (rc_runtime_progress_match_md5(progress, runtime_lboard->md5)) {
          uint32_t flags = rc_runtime_progress_read_uint(progress);

          result = rc_runtime_progress_read_trigger(progress, &runtime_lboard->lboard->start);
          if (result != RC_OK)
            return result;

          result = rc_runtime_progress_read_trigger(progress, &runtime_lboard->lboard->submit);
          if (result != RC_OK)
            return result;

          result = rc_runtime_progress_read_trigger(progress, &runtime_lboard->lboard->cancel);
          if (result != RC_OK)
            return result;

          result = rc_runtime_progress_read_variable(progress, &runtime_lboard->lboard->value);
          if (result != RC_OK)
            return result;

          runtime_lboard->lboard->state = (char)(flags & 0x7F);
        }
        break;
      }
    }
  }

  return RC_OK;
}

static int rc_runtime_progress_write_rich_presence(rc_runtime_progress_t* progress)
{
  const rc_richpresence_display_t* display;
  int result;

  if (!progress->runtime->richpresence || !progress->runtime->richpresence->richpresence)
    return RC_OK;

  /* if there are no conditional display strings, there's nothing to capture */
  display = progress->runtime->richpresence->richpresence->first_display;
  if (!display->next)
    return RC_OK;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_RICHPRESENCE);
  rc_runtime_progress_write_md5(progress, progress->runtime->richpresence->md5);

  for (; display->next; display = display->next) {
    result = rc_runtime_progress_write_trigger(progress, &display->trigger);
    if (result != RC_OK)
      return result;
  }

  rc_runtime_progress_end_chunk(progress);
  return RC_OK;
}

static int rc_runtime_progress_read_rich_presence(rc_runtime_progress_t* progress)
{
  rc_richpresence_display_t* display;
  int result;

  if (!progress->runtime->richpresence || !progress->runtime->richpresence->richpresence)
    return RC_OK;

  if (!rc_runtime_progress_match_md5(progress, progress->runtime->richpresence->md5)) {
    rc_reset_richpresence(progress->runtime->richpresence->richpresence);
    return RC_OK;
  }

  display = progress->runtime->richpresence->richpresence->first_display;
  for (; display->next; display = display->next) {
    result = rc_runtime_progress_read_trigger(progress, &display->trigger);
    if (result != RC_OK)
      return result;
  }

  return RC_OK;
}

static int rc_runtime_progress_serialize_internal(rc_runtime_progress_t* progress)
{
  md5_state_t state;
  uint8_t md5[16];
  int result;

  rc_runtime_progress_write_uint(progress, RC_RUNTIME_MARKER);

  if ((result = rc_runtime_progress_write_memrefs(progress)) != RC_OK)
    return result;

  if ((result = rc_runtime_progress_write_variables(progress)) != RC_OK)
    return result;

  if ((result = rc_runtime_progress_write_achievements(progress)) != RC_OK)
    return result;

  if ((result = rc_runtime_progress_write_leaderboards(progress)) != RC_OK)
    return result;

  if ((result = rc_runtime_progress_write_rich_presence(progress)) != RC_OK)
    return result;

  rc_runtime_progress_write_uint(progress, RC_RUNTIME_CHUNK_DONE);
  rc_runtime_progress_write_uint(progress, 16);

  if (progress->buffer) {
    md5_init(&state);
    md5_append(&state, progress->buffer, progress->offset);
    md5_finish(&state, md5);
  }

  rc_runtime_progress_write_md5(progress, md5);

  return RC_OK;
}

int rc_runtime_progress_size(const rc_runtime_t* runtime, lua_State* L)
{
  rc_runtime_progress_t progress;
  int result;

  rc_runtime_progress_init(&progress, runtime, L);

  result = rc_runtime_progress_serialize_internal(&progress);
  if (result != RC_OK)
    return result;

  return progress.offset;
}

int rc_runtime_serialize_progress(void* buffer, const rc_runtime_t* runtime, lua_State* L)
{
  rc_runtime_progress_t progress;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_runtime_progress_init(&progress, runtime, L);
  progress.buffer = (uint8_t*)buffer;

  return rc_runtime_progress_serialize_internal(&progress);
}

int rc_runtime_deserialize_progress(rc_runtime_t* runtime, const uint8_t* serialized, lua_State* L)
{
  rc_runtime_progress_t progress;
  md5_state_t state;
  uint8_t md5[16];
  uint32_t chunk_id;
  uint32_t chunk_size;
  uint32_t next_chunk_offset;
  uint32_t i;
  int seen_rich_presence = 0;
  int result = RC_OK;

  if (!serialized) {
    rc_runtime_reset(runtime);
    return RC_INVALID_STATE;
  }

  rc_runtime_progress_init(&progress, runtime, L);
  progress.buffer = (uint8_t*)serialized;

  if (rc_runtime_progress_read_uint(&progress) != RC_RUNTIME_MARKER) {
    rc_runtime_reset(runtime);
    return RC_INVALID_STATE;
  }

  for (i = 0; i < runtime->trigger_count; ++i) {
    rc_runtime_trigger_t* runtime_trigger = &runtime->triggers[i];
    if (runtime_trigger->trigger) {
      /* don't update state for inactive or triggered achievements */
      if (rc_trigger_state_active(runtime_trigger->trigger->state)) {
        /* mark active achievements as unupdated. anything that's still unupdated
         * after deserializing the progress will be reset to waiting */
        runtime_trigger->trigger->state = RC_TRIGGER_STATE_UNUPDATED;
      }
    }
  }

  for (i = 0; i < runtime->lboard_count; ++i) {
    rc_runtime_lboard_t* runtime_lboard = &runtime->lboards[i];
    if (runtime_lboard->lboard) {
      /* don't update state for inactive or triggered achievements */
      if (rc_lboard_state_active(runtime_lboard->lboard->state)) {
        /* mark active achievements as unupdated. anything that's still unupdated
         * after deserializing the progress will be reset to waiting */
          runtime_lboard->lboard->state = RC_TRIGGER_STATE_UNUPDATED;
      }
    }
  }

  do {
    chunk_id = rc_runtime_progress_read_uint(&progress);
    chunk_size = rc_runtime_progress_read_uint(&progress);
    next_chunk_offset = progress.offset + chunk_size;

    switch (chunk_id)
    {
      case RC_RUNTIME_CHUNK_MEMREFS:
        result = rc_runtime_progress_read_memrefs(&progress);
        break;

      case RC_RUNTIME_CHUNK_VARIABLES:
        result = rc_runtime_progress_read_variables(&progress);
        break;

      case RC_RUNTIME_CHUNK_ACHIEVEMENT:
        result = rc_runtime_progress_read_achievement(&progress);
        break;

      case RC_RUNTIME_CHUNK_LEADERBOARD:
        result = rc_runtime_progress_read_leaderboard(&progress);
        break;

      case RC_RUNTIME_CHUNK_RICHPRESENCE:
        seen_rich_presence = 1;
        result = rc_runtime_progress_read_rich_presence(&progress);
        break;

      case RC_RUNTIME_CHUNK_DONE:
        md5_init(&state);
        md5_append(&state, progress.buffer, progress.offset);
        md5_finish(&state, md5);
        if (!rc_runtime_progress_match_md5(&progress, md5))
          result = RC_INVALID_STATE;
        break;

      default:
        if (chunk_size & 0xFFFF0000)
          result = RC_INVALID_STATE; /* assume unknown chunk > 64KB is invalid */
        break;
    }

    progress.offset = next_chunk_offset;
  } while (result == RC_OK && chunk_id != RC_RUNTIME_CHUNK_DONE);

  if (result != RC_OK) {
    rc_runtime_reset(runtime);
  }
  else {
    for (i = 0; i < runtime->trigger_count; ++i) {
      rc_trigger_t* trigger = runtime->triggers[i].trigger;
      if (trigger && trigger->state == RC_TRIGGER_STATE_UNUPDATED)
        rc_reset_trigger(trigger);
    }

    for (i = 0; i < runtime->lboard_count; ++i) {
      rc_lboard_t* lboard = runtime->lboards[i].lboard;
      if (lboard && lboard->state == RC_TRIGGER_STATE_UNUPDATED)
        rc_reset_lboard(lboard);
    }

    if (!seen_rich_presence && runtime->richpresence && runtime->richpresence->richpresence)
      rc_reset_richpresence(runtime->richpresence->richpresence);
  }

  return result;
}
