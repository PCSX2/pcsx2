#include "rc_runtime.h"
#include "rc_internal.h"

#include "rc_util.h"
#include "../rhash/md5.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define RC_RUNTIME_MARKER             0x0A504152 /* RAP\n */

#define RC_RUNTIME_CHUNK_MEMREFS      0x4645524D /* MREF */
#define RC_RUNTIME_CHUNK_VARIABLES    0x53524156 /* VARS */
#define RC_RUNTIME_CHUNK_ACHIEVEMENT  0x56484341 /* ACHV */
#define RC_RUNTIME_CHUNK_LEADERBOARD  0x4452424C /* LBRD */
#define RC_RUNTIME_CHUNK_RICHPRESENCE 0x48434952 /* RICH */

#define RC_RUNTIME_CHUNK_DONE         0x454E4F44 /* DONE */

#define RC_RUNTIME_MIN_BUFFER_SIZE    4 + 8 + 16 /* RUNTIME_MARKER, CHUNK_DONE, MD5 */

typedef struct rc_runtime_progress_t {
  const rc_runtime_t* runtime;

  uint32_t offset;
  uint8_t* buffer;
  uint32_t buffer_size;

  uint32_t chunk_size_offset;
} rc_runtime_progress_t;

#define assert_chunk_size(expected_size) assert((uint32_t)(progress->offset - progress->chunk_size_offset - 4) == (uint32_t)(expected_size))

#define RC_TRIGGER_STATE_UNUPDATED 0x7F

#define RC_MEMREF_FLAG_CHANGED_THIS_FRAME 0x00010000

#define RC_VAR_FLAG_HAS_COND_DATA         0x01000000

#define RC_COND_FLAG_IS_TRUE_MASK                       0x00000003
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

static void rc_runtime_progress_init(rc_runtime_progress_t* progress, const rc_runtime_t* runtime)
{
  memset(progress, 0, sizeof(rc_runtime_progress_t));
  progress->runtime = runtime;
}

#define RC_RUNTIME_SERIALIZED_MEMREF_SIZE 16 /* 4x uint: address, flags, value, prior */

static int rc_runtime_progress_write_memrefs(rc_runtime_progress_t* progress)
{
  uint32_t count = rc_memrefs_count_memrefs(progress->runtime->memrefs);
  if (count == 0)
    return RC_OK;

  if (progress->offset + 8 + count * RC_RUNTIME_SERIALIZED_MEMREF_SIZE > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_MEMREFS);

  if (!progress->buffer) {
    progress->offset += count * RC_RUNTIME_SERIALIZED_MEMREF_SIZE;
  }
  else {
    uint32_t flags = 0;
    const rc_memref_list_t* memref_list = &progress->runtime->memrefs->memrefs;
    const rc_memref_t* memref;

    for (; memref_list; memref_list = memref_list->next) {
      const rc_memref_t* memref_end;

      memref = memref_list->items;
      memref_end = memref + memref_list->count;
      for (; memref < memref_end; ++memref) {
        flags = memref->value.size;
        if (memref->value.changed)
          flags |= RC_MEMREF_FLAG_CHANGED_THIS_FRAME;

        rc_runtime_progress_write_uint(progress, memref->address);
        rc_runtime_progress_write_uint(progress, flags);
        rc_runtime_progress_write_uint(progress, memref->value.value);
        rc_runtime_progress_write_uint(progress, memref->value.prior);
      }
    }
  }

  assert_chunk_size(count * RC_RUNTIME_SERIALIZED_MEMREF_SIZE);
  rc_runtime_progress_end_chunk(progress);
  return RC_OK;
}

static void rc_runtime_progress_update_modified_memrefs(rc_runtime_progress_t* progress)
{
  rc_typed_value_t value, prior_value, modifier, prior_modifier;
  rc_modified_memref_list_t* modified_memref_list;
  rc_modified_memref_t* modified_memref;
  rc_operand_t prior_parent_operand, prior_modifier_operand;
  rc_memref_t prior_parent_memref, prior_modifier_memref;

  modified_memref_list = &progress->runtime->memrefs->modified_memrefs;
  for (; modified_memref_list; modified_memref_list = modified_memref_list->next) {
    const rc_modified_memref_t* modified_memref_end;
    modified_memref = modified_memref_list->items;
    modified_memref_end = modified_memref + modified_memref_list->count;
    for (; modified_memref < modified_memref_end; ++modified_memref) {
      modified_memref->memref.value.changed = 0;

      /* indirect memref values are stored in conditions */
      if (modified_memref->modifier_type == RC_OPERATOR_INDIRECT_READ)
        continue;

      /* non-indirect memref values can be reconstructed from the parents */
      memcpy(&prior_parent_operand, &modified_memref->parent, sizeof(prior_parent_operand));
      if (rc_operand_is_memref(&prior_parent_operand)) {
        memcpy(&prior_parent_memref, modified_memref->parent.value.memref, sizeof(prior_parent_memref));
        prior_parent_memref.value.value = prior_parent_memref.value.prior;
        modified_memref->memref.value.changed |= prior_parent_memref.value.changed;
        prior_parent_operand.value.memref = &prior_parent_memref;
      }

      memcpy(&prior_modifier_operand, &modified_memref->modifier, sizeof(prior_modifier_operand));
      if (rc_operand_is_memref(&prior_modifier_operand)) {
        memcpy(&prior_modifier_memref, modified_memref->modifier.value.memref, sizeof(prior_modifier_memref));
        prior_modifier_memref.value.value = prior_modifier_memref.value.prior;
        modified_memref->memref.value.changed |= prior_modifier_memref.value.changed;
        prior_modifier_operand.value.memref = &prior_modifier_memref;
      }

      rc_evaluate_operand(&value, &modified_memref->parent, NULL);
      rc_evaluate_operand(&modifier, &modified_memref->modifier, NULL);
      rc_evaluate_operand(&prior_value, &prior_parent_operand, NULL);
      rc_evaluate_operand(&prior_modifier, &prior_modifier_operand, NULL);

      if (modified_memref->modifier_type == RC_OPERATOR_SUB_PARENT) {
        rc_typed_value_negate(&value);
        rc_typed_value_add(&value, &modifier);

        rc_typed_value_negate(&prior_value);
        rc_typed_value_add(&prior_value, &prior_modifier);
      }
      else {
        rc_typed_value_combine(&value, &modifier, modified_memref->modifier_type);
        rc_typed_value_combine(&prior_value, &prior_modifier, modified_memref->modifier_type);
      }

      rc_typed_value_convert(&value, modified_memref->memref.value.type);
      modified_memref->memref.value.value = value.value.u32;

      rc_typed_value_convert(&prior_value, modified_memref->memref.value.type);
      modified_memref->memref.value.prior = prior_value.value.u32;
    }
  }
}

static int rc_runtime_progress_read_memrefs(rc_runtime_progress_t* progress)
{
  uint32_t entries;
  uint32_t address, flags, value, prior;
  uint8_t size;
  rc_memref_list_t* unmatched_memref_list = &progress->runtime->memrefs->memrefs;
  rc_memref_t* first_unmatched_memref = unmatched_memref_list->items;
  rc_memref_t* memref;

  /* re-read the chunk size to determine how many memrefs are present */
  progress->offset -= 4;
  entries = rc_runtime_progress_read_uint(progress) / RC_RUNTIME_SERIALIZED_MEMREF_SIZE;

  while (entries != 0) {
    address = rc_runtime_progress_read_uint(progress);
    flags = rc_runtime_progress_read_uint(progress);
    value = rc_runtime_progress_read_uint(progress);
    prior = rc_runtime_progress_read_uint(progress);

    size = flags & 0xFF;

    memref = first_unmatched_memref;
    if (memref->address == address && memref->value.size == size) {
      memref->value.value = value;
      memref->value.changed = (flags & RC_MEMREF_FLAG_CHANGED_THIS_FRAME) ? 1 : 0;
      memref->value.prior = prior;

      first_unmatched_memref++;
      if (first_unmatched_memref >= unmatched_memref_list->items + unmatched_memref_list->count) {
        unmatched_memref_list = unmatched_memref_list->next;
        if (!unmatched_memref_list)
          break;
        first_unmatched_memref = unmatched_memref_list->items;
      }
    }
    else {
      rc_memref_list_t* memref_list = unmatched_memref_list;
      do {
        ++memref;
        if (memref >= memref_list->items + memref_list->count) {
          memref_list = memref_list->next;
          if (!memref_list)
            break;

          memref = memref_list->items;
        }

        if (memref->address == address && memref->value.size == size) {
          memref->value.value = value;
          memref->value.changed = (flags & RC_MEMREF_FLAG_CHANGED_THIS_FRAME) ? 1 : 0;
          memref->value.prior = prior;
          break;
        }

      } while (1);
    }

    --entries;
  }

  rc_runtime_progress_update_modified_memrefs(progress);

  return RC_OK;
}

static int rc_runtime_progress_is_indirect_memref(rc_operand_t* oper)
{
  switch (oper->type)
  {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_RECALL:
    case RC_OPERAND_FUNC:
      return 0;

    default:
      if (oper->value.memref->value.memref_type != RC_MEMREF_TYPE_MODIFIED_MEMREF)
        return 0;

      return ((const rc_modified_memref_t*)oper->value.memref)->modifier_type == RC_OPERATOR_INDIRECT_READ;
  }
}

static int rc_runtime_progress_write_condset(rc_runtime_progress_t* progress, rc_condset_t* condset)
{
  rc_condition_t* cond;
  uint32_t flags;

  if (progress->offset + 4 > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

  rc_runtime_progress_write_uint(progress, condset->is_paused);

  cond = condset->conditions;
  while (cond) {
    flags = (cond->is_true & RC_COND_FLAG_IS_TRUE_MASK);

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

    if (progress->offset + 8 > progress->buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    rc_runtime_progress_write_uint(progress, cond->current_hits);
    rc_runtime_progress_write_uint(progress, flags);

    if (flags & RC_COND_FLAG_OPERAND1_IS_INDIRECT_MEMREF) {
      if (progress->offset + 8 > progress->buffer_size)
        return RC_INSUFFICIENT_BUFFER;

      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.value);
      rc_runtime_progress_write_uint(progress, cond->operand1.value.memref->value.prior);
    }

    if (flags & RC_COND_FLAG_OPERAND2_IS_INDIRECT_MEMREF) {
      if (progress->offset + 8 > progress->buffer_size)
        return RC_INSUFFICIENT_BUFFER;

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

    cond->is_true = (flags & RC_COND_FLAG_IS_TRUE_MASK);

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

  /* predetermined presence of pause flag - must serialize */
  if (conditions->has_pause)
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

  if (progress->offset + 12 > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

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
  uint32_t count;
  const rc_value_t* value;
  int result;

  if (!progress->runtime->richpresence || !progress->runtime->richpresence->richpresence)
    return RC_OK;

  value = progress->runtime->richpresence->richpresence->values;
  count = rc_count_values(value);
  if (count == 0)
    return RC_OK;

  /* header + count + count(djb2,flags,value,prior,?cond) */
  if (progress->offset + 8 + 4 + count * 16 > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

  rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_VARIABLES);
  rc_runtime_progress_write_uint(progress, count);

  for (; value; value = value->next) {
    const uint32_t djb2 = rc_djb2(value->name);
    if (progress->offset + 16 > progress->buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    rc_runtime_progress_write_uint(progress, djb2);

    result = rc_runtime_progress_write_variable(progress, value);
    if (result != RC_OK)
      return result;
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
  rc_value_t* value;
  uint32_t count, serialized_count;
  int result;
  int32_t i;

  serialized_count = rc_runtime_progress_read_uint(progress);
  if (serialized_count == 0)
    return RC_OK;

  if (!progress->runtime->richpresence || !progress->runtime->richpresence->richpresence)
    return RC_OK;

  value = progress->runtime->richpresence->richpresence->values;
  count = rc_count_values(value);
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

  i = (int32_t)count;
  for (; value; value = value->next) {
    --i;
    pending_variables[i].variable = value;
    pending_variables[i].djb2 = rc_djb2(value->name);
  }

  result = RC_OK;
  for (; serialized_count > 0 && result == RC_OK; --serialized_count) {
    uint32_t djb2 = rc_runtime_progress_read_uint(progress);
    for (i = (int32_t)count - 1; i >= 0; --i) {
      if (pending_variables[i].djb2 == djb2) {
        value = pending_variables[i].variable;
        result = rc_runtime_progress_read_variable(progress, value);
        if (result == RC_OK) {
          if (i < (int32_t)count - 1)
            memcpy(&pending_variables[i], &pending_variables[count - 1], sizeof(struct rc_pending_value_t));
          count--;
        }
        break;
      }
    }
  }

  /* VS raises a C6385 warning here because it thinks count can exceed the size of the local_pending_variables array.
   * When count is larger, pending_variables points to allocated memory, so the warning is wrong. */
#if defined (_MSC_VER)
 #pragma warning(push)
 #pragma warning(disable:6385)
#endif
  while (count > 0)
    rc_reset_value(pending_variables[--count].variable);
#if defined (_MSC_VER)
 #pragma warning(pop)
#endif

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
  int initial_offset = 0;
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

      initial_offset = progress->offset;
    } else {
      if (progress->offset + runtime_trigger->serialized_size > progress->buffer_size)
        return RC_INSUFFICIENT_BUFFER;
    }

    rc_runtime_progress_start_chunk(progress, RC_RUNTIME_CHUNK_ACHIEVEMENT);
    rc_runtime_progress_write_uint(progress, runtime_trigger->id);
    rc_runtime_progress_write_md5(progress, runtime_trigger->md5);

    result = rc_runtime_progress_write_trigger(progress, runtime_trigger->trigger);
    if (result != RC_OK)
      return result;

    if (runtime_trigger->serialized_size) {
      /* runtime_trigger->serialized_size includes the header */
      assert_chunk_size(runtime_trigger->serialized_size - 8);
    }

    rc_runtime_progress_end_chunk(progress);

    if (!progress->buffer)
      runtime_trigger->serialized_size = progress->offset - initial_offset;
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
  int initial_offset = 0;
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

      initial_offset = progress->offset;
    } else {
      if (progress->offset + runtime_lboard->serialized_size > progress->buffer_size)
        return RC_INSUFFICIENT_BUFFER;
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

    if (runtime_lboard->serialized_size) {
      /* runtime_lboard->serialized_size includes the header */
      assert_chunk_size(runtime_lboard->serialized_size - 8);
    }

    rc_runtime_progress_end_chunk(progress);

    if (!progress->buffer)
      runtime_lboard->serialized_size = progress->offset - initial_offset;
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

  if (progress->offset + 8 + 16 > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

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
    rc_reset_richpresence_triggers(progress->runtime->richpresence->richpresence);
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

  if (progress->buffer_size < RC_RUNTIME_MIN_BUFFER_SIZE)
    return RC_INSUFFICIENT_BUFFER;

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

  if (progress->offset + 8 + 16 > progress->buffer_size)
    return RC_INSUFFICIENT_BUFFER;

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

uint32_t rc_runtime_progress_size(const rc_runtime_t* runtime, void* unused_L)
{
  rc_runtime_progress_t progress;
  int result;

  (void)unused_L;

  rc_runtime_progress_init(&progress, runtime);
  progress.buffer_size = 0xFFFFFFFF;

  result = rc_runtime_progress_serialize_internal(&progress);
  if (result != RC_OK)
    return 0;

  return progress.offset;
}

int rc_runtime_serialize_progress(void* buffer, const rc_runtime_t* runtime, void* unused_L)
{
  return rc_runtime_serialize_progress_sized((uint8_t*)buffer, 0xFFFFFFFF, runtime, unused_L);
}

int rc_runtime_serialize_progress_sized(uint8_t* buffer, uint32_t buffer_size, const rc_runtime_t* runtime, void* unused_L)
{
  rc_runtime_progress_t progress;

  (void)unused_L;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_runtime_progress_init(&progress, runtime);
  progress.buffer = (uint8_t*)buffer;
  progress.buffer_size = buffer_size;

  return rc_runtime_progress_serialize_internal(&progress);
}

int rc_runtime_deserialize_progress(rc_runtime_t* runtime, const uint8_t* serialized, void* unused_L)
{
  return rc_runtime_deserialize_progress_sized(runtime, serialized, 0xFFFFFFFF, unused_L);
}

int rc_runtime_deserialize_progress_sized(rc_runtime_t* runtime, const uint8_t* serialized, uint32_t serialized_size, void* unused_L)
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

  (void)unused_L;

  if (!serialized || serialized_size < RC_RUNTIME_MIN_BUFFER_SIZE) {
    rc_runtime_reset(runtime);
    return RC_INSUFFICIENT_BUFFER;
  }

  rc_runtime_progress_init(&progress, runtime);
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
    if (progress.offset + 8 >= serialized_size) {
      result = RC_INSUFFICIENT_BUFFER;
      break;
    }

    chunk_id = rc_runtime_progress_read_uint(&progress);
    chunk_size = rc_runtime_progress_read_uint(&progress);
    next_chunk_offset = progress.offset + chunk_size;

    if (next_chunk_offset > serialized_size) {
      result = RC_INSUFFICIENT_BUFFER;
      break;
    }

    switch (chunk_id) {
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
      rc_reset_richpresence_triggers(runtime->richpresence->richpresence);
  }

  return result;
}
