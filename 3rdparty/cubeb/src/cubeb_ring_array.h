/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_RING_ARRAY_H
#define CUBEB_RING_ARRAY_H

#include "cubeb_utils.h"
#include <CoreAudio/CoreAudioTypes.h>

/** Ring array of pointers is used to hold buffers. In case that
    asynchronous producer/consumer callbacks do not arrive in a
    repeated order the ring array stores the buffers and fetch
    them in the correct order. */

typedef struct {
  AudioBuffer * buffer_array; /**< Array that hold pointers of the allocated
                                 space for the buffers. */
  unsigned int tail;     /**< Index of the last element (first to deliver). */
  unsigned int count;    /**< Number of elements in the array. */
  unsigned int capacity; /**< Total length of the array. */
} ring_array;

static int
single_audiobuffer_init(AudioBuffer * buffer, uint32_t bytesPerFrame,
                        uint32_t channelsPerFrame, uint32_t frames)
{
  assert(buffer);
  assert(bytesPerFrame > 0 && channelsPerFrame && frames > 0);

  size_t size = bytesPerFrame * frames;
  buffer->mData = operator new(size);
  if (buffer->mData == NULL) {
    return CUBEB_ERROR;
  }
  PodZero(static_cast<char *>(buffer->mData), size);

  buffer->mNumberChannels = channelsPerFrame;
  buffer->mDataByteSize = size;

  return CUBEB_OK;
}

/** Initialize the ring array.
    @param ra The ring_array pointer of allocated structure.
    @retval 0 on success. */
static int
ring_array_init(ring_array * ra, uint32_t capacity, uint32_t bytesPerFrame,
                uint32_t channelsPerFrame, uint32_t framesPerBuffer)
{
  assert(ra);
  if (capacity == 0 || bytesPerFrame == 0 || channelsPerFrame == 0 ||
      framesPerBuffer == 0) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }
  ra->capacity = capacity;
  ra->tail = 0;
  ra->count = 0;

  ra->buffer_array = new AudioBuffer[ra->capacity];
  PodZero(ra->buffer_array, ra->capacity);
  if (ra->buffer_array == NULL) {
    return CUBEB_ERROR;
  }

  for (unsigned int i = 0; i < ra->capacity; ++i) {
    if (single_audiobuffer_init(&ra->buffer_array[i], bytesPerFrame,
                                channelsPerFrame,
                                framesPerBuffer) != CUBEB_OK) {
      return CUBEB_ERROR;
    }
  }

  return CUBEB_OK;
}

/** Destroy the ring array.
    @param ra The ring_array pointer.*/
static void
ring_array_destroy(ring_array * ra)
{
  assert(ra);
  if (ra->buffer_array == NULL) {
    return;
  }
  for (unsigned int i = 0; i < ra->capacity; ++i) {
    if (ra->buffer_array[i].mData) {
      operator delete(ra->buffer_array[i].mData);
    }
  }
  delete[] ra->buffer_array;
}

/** Get the allocated buffer to be stored with fresh data.
    @param ra The ring_array pointer.
    @retval Pointer of the allocated space to be stored with fresh data or NULL
   if full. */
static AudioBuffer *
ring_array_get_free_buffer(ring_array * ra)
{
  assert(ra && ra->buffer_array);
  assert(ra->buffer_array[0].mData != NULL);
  if (ra->count == ra->capacity) {
    return NULL;
  }

  assert(ra->count == 0 || (ra->tail + ra->count) % ra->capacity != ra->tail);
  AudioBuffer * ret = &ra->buffer_array[(ra->tail + ra->count) % ra->capacity];

  ++ra->count;
  assert(ra->count <= ra->capacity);

  return ret;
}

/** Get the next available buffer with data.
    @param ra The ring_array pointer.
    @retval Pointer of the next in order data buffer or NULL if empty. */
static AudioBuffer *
ring_array_get_data_buffer(ring_array * ra)
{
  assert(ra && ra->buffer_array);
  assert(ra->buffer_array[0].mData != NULL);

  if (ra->count == 0) {
    return NULL;
  }
  AudioBuffer * ret = &ra->buffer_array[ra->tail];

  ra->tail = (ra->tail + 1) % ra->capacity;
  assert(ra->tail < ra->capacity);

  assert(ra->count > 0);
  --ra->count;

  return ret;
}

#endif // CUBEB_RING_ARRAY_H
