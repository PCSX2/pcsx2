/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_RING_BUFFER_H
#define CUBEB_RING_BUFFER_H

#include "cubeb_utils.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

/**
 * Single producer single consumer lock-free and wait-free ring buffer.
 *
 * This data structure allows producing data from one thread, and consuming it
 * on another thread, safely and without explicit synchronization. If used on
 * two threads, this data structure uses atomics for thread safety. It is
 * possible to disable the use of atomics at compile time and only use this data
 * structure on one thread.
 *
 * The role for the producer and the consumer must be constant, i.e., the
 * producer should always be on one thread and the consumer should always be on
 * another thread.
 *
 * Some words about the inner workings of this class:
 * - Capacity is fixed. Only one allocation is performed, in the constructor.
 *   When reading and writing, the return value of the method allows checking if
 *   the ring buffer is empty or full.
 * - We always keep the read index at least one element ahead of the write
 *   index, so we can distinguish between an empty and a full ring buffer: an
 *   empty ring buffer is when the write index is at the same position as the
 *   read index. A full buffer is when the write index is exactly one position
 *   before the read index.
 * - We synchronize updates to the read index after having read the data, and
 *   the write index after having written the data. This means that the each
 *   thread can only touch a portion of the buffer that is not touched by the
 *   other thread.
 * - Callers are expected to provide buffers. When writing to the queue,
 *   elements are copied into the internal storage from the buffer passed in.
 *   When reading from the queue, the user is expected to provide a buffer.
 *   Because this is a ring buffer, data might not be contiguous in memory,
 *   providing an external buffer to copy into is an easy way to have linear
 *   data for further processing.
 */
template <typename T> class ring_buffer_base {
public:
  /**
   * Constructor for a ring buffer.
   *
   * This performs an allocation, but is the only allocation that will happen
   * for the life time of a `ring_buffer_base`.
   *
   * @param capacity The maximum number of element this ring buffer will hold.
   */
  ring_buffer_base(int capacity)
      /* One more element to distinguish from empty and full buffer. */
      : capacity_(capacity + 1)
  {
    assert(storage_capacity() < std::numeric_limits<int>::max() / 2 &&
           "buffer too large for the type of index used.");
    assert(capacity_ > 0);

    data_.reset(new T[storage_capacity()]);
    /* If this queue is using atomics, initializing those members as the last
     * action in the constructor acts as a full barrier, and allow capacity() to
     * be thread-safe. */
    write_index_ = 0;
    read_index_ = 0;
  }
  /**
   * Push `count` zero or default constructed elements in the array.
   *
   * Only safely called on the producer thread.
   *
   * @param count The number of elements to enqueue.
   * @return The number of element enqueued.
   */
  int enqueue_default(int count) { return enqueue(nullptr, count); }
  /**
   * @brief Put an element in the queue
   *
   * Only safely called on the producer thread.
   *
   * @param element The element to put in the queue.
   *
   * @return 1 if the element was inserted, 0 otherwise.
   */
  int enqueue(T & element) { return enqueue(&element, 1); }
  /**
   * Push `count` elements in the ring buffer.
   *
   * Only safely called on the producer thread.
   *
   * @param elements a pointer to a buffer containing at least `count` elements.
   * If `elements` is nullptr, zero or default constructed elements are
   * enqueued.
   * @param count The number of elements to read from `elements`
   * @return The number of elements successfully coped from `elements` and
   * inserted into the ring buffer.
   */
  int enqueue(T * elements, int count)
  {
#ifndef NDEBUG
    assert_correct_thread(producer_id);
#endif

    int wr_idx = write_index_.load(std::memory_order_relaxed);
    int rd_idx = read_index_.load(std::memory_order_acquire);

    if (full_internal(rd_idx, wr_idx)) {
      return 0;
    }

    int to_write = std::min(available_write_internal(rd_idx, wr_idx), count);

    /* First part, from the write index to the end of the array. */
    int first_part = std::min(storage_capacity() - wr_idx, to_write);
    /* Second part, from the beginning of the array */
    int second_part = to_write - first_part;

    if (elements) {
      Copy(data_.get() + wr_idx, elements, first_part);
      Copy(data_.get(), elements + first_part, second_part);
    } else {
      ConstructDefault(data_.get() + wr_idx, first_part);
      ConstructDefault(data_.get(), second_part);
    }

    write_index_.store(increment_index(wr_idx, to_write),
                       std::memory_order_release);

    return to_write;
  }
  /**
   * Retrieve at most `count` elements from the ring buffer, and copy them to
   * `elements`, if non-null.
   *
   * Only safely called on the consumer side.
   *
   * @param elements A pointer to a buffer with space for at least `count`
   * elements. If `elements` is `nullptr`, `count` element will be discarded.
   * @param count The maximum number of elements to dequeue.
   * @return The number of elements written to `elements`.
   */
  int dequeue(T * elements, int count)
  {
#ifndef NDEBUG
    assert_correct_thread(consumer_id);
#endif

    int rd_idx = read_index_.load(std::memory_order_relaxed);
    int wr_idx = write_index_.load(std::memory_order_acquire);

    if (empty_internal(rd_idx, wr_idx)) {
      return 0;
    }

    int to_read = std::min(available_read_internal(rd_idx, wr_idx), count);

    int first_part = std::min(storage_capacity() - rd_idx, to_read);
    int second_part = to_read - first_part;

    if (elements) {
      Copy(elements, data_.get() + rd_idx, first_part);
      Copy(elements + first_part, data_.get(), second_part);
    }

    read_index_.store(increment_index(rd_idx, to_read),
                      std::memory_order_release);

    return to_read;
  }
  /**
   * Get the number of available element for consuming.
   *
   * Only safely called on the consumer thread.
   *
   * @return The number of available elements for reading.
   */
  int available_read() const
  {
#ifndef NDEBUG
    assert_correct_thread(consumer_id);
#endif
    return available_read_internal(
        read_index_.load(std::memory_order_relaxed),
        write_index_.load(std::memory_order_acquire));
  }
  /**
   * Get the number of available elements for consuming.
   *
   * Only safely called on the producer thread.
   *
   * @return The number of empty slots in the buffer, available for writing.
   */
  int available_write() const
  {
#ifndef NDEBUG
    assert_correct_thread(producer_id);
#endif
    return available_write_internal(
        read_index_.load(std::memory_order_acquire),
        write_index_.load(std::memory_order_relaxed));
  }
  /**
   * Get the total capacity, for this ring buffer.
   *
   * Can be called safely on any thread.
   *
   * @return The maximum capacity of this ring buffer.
   */
  int capacity() const { return storage_capacity() - 1; }
  /**
   * Reset the consumer and producer thread identifier, in case the thread are
   * being changed. This has to be externally synchronized. This is no-op when
   * asserts are disabled.
   */
  void reset_thread_ids()
  {
#ifndef NDEBUG
    consumer_id = producer_id = std::thread::id();
#endif
  }

private:
  /** Return true if the ring buffer is empty.
   *
   * @param read_index the read index to consider
   * @param write_index the write index to consider
   * @return true if the ring buffer is empty, false otherwise.
   **/
  bool empty_internal(int read_index, int write_index) const
  {
    return write_index == read_index;
  }
  /** Return true if the ring buffer is full.
   *
   * This happens if the write index is exactly one element behind the read
   * index.
   *
   * @param read_index the read index to consider
   * @param write_index the write index to consider
   * @return true if the ring buffer is full, false otherwise.
   **/
  bool full_internal(int read_index, int write_index) const
  {
    return (write_index + 1) % storage_capacity() == read_index;
  }
  /**
   * Return the size of the storage. It is one more than the number of elements
   * that can be stored in the buffer.
   *
   * @return the number of elements that can be stored in the buffer.
   */
  int storage_capacity() const { return capacity_; }
  /**
   * Returns the number of elements available for reading.
   *
   * @return the number of available elements for reading.
   */
  int available_read_internal(int read_index, int write_index) const
  {
    if (write_index >= read_index) {
      return write_index - read_index;
    } else {
      return write_index + storage_capacity() - read_index;
    }
  }
  /**
   * Returns the number of empty elements, available for writing.
   *
   * @return the number of elements that can be written into the array.
   */
  int available_write_internal(int read_index, int write_index) const
  {
    /* We substract one element here to always keep at least one sample
     * free in the buffer, to distinguish between full and empty array. */
    int rv = read_index - write_index - 1;
    if (write_index >= read_index) {
      rv += storage_capacity();
    }
    return rv;
  }
  /**
   * Increments an index, wrapping it around the storage.
   *
   * @param index a reference to the index to increment.
   * @param increment the number by which `index` is incremented.
   * @return the new index.
   */
  int increment_index(int index, int increment) const
  {
    assert(increment >= 0);
    return (index + increment) % storage_capacity();
  }
  /**
   * @brief This allows checking that enqueue (resp. dequeue) are always called
   * by the right thread.
   *
   * @param id the id of the thread that has called the calling method first.
   */
#ifndef NDEBUG
  static void assert_correct_thread(std::thread::id & id)
  {
    if (id == std::thread::id()) {
      id = std::this_thread::get_id();
      return;
    }
    assert(id == std::this_thread::get_id());
  }
#endif
  /** Index at which the oldest element is at, in samples. */
  std::atomic<int> read_index_;
  /** Index at which to write new elements. `write_index` is always at
   * least one element ahead of `read_index_`. */
  std::atomic<int> write_index_;
  /** Maximum number of elements that can be stored in the ring buffer. */
  const int capacity_;
  /** Data storage */
  std::unique_ptr<T[]> data_;
#ifndef NDEBUG
  /** The id of the only thread that is allowed to read from the queue. */
  mutable std::thread::id consumer_id;
  /** The id of the only thread that is allowed to write from the queue. */
  mutable std::thread::id producer_id;
#endif
};

/**
 * Adapter for `ring_buffer_base` that exposes an interface in frames.
 */
template <typename T> class audio_ring_buffer_base {
public:
  /**
   * @brief Constructor.
   *
   * @param channel_count       Number of channels.
   * @param capacity_in_frames  The capacity in frames.
   */
  audio_ring_buffer_base(int channel_count, int capacity_in_frames)
      : channel_count(channel_count),
        ring_buffer(frames_to_samples(capacity_in_frames))
  {
    assert(channel_count > 0);
  }
  /**
   * @brief Enqueue silence.
   *
   * Only safely called on the producer thread.
   *
   * @param frame_count The number of frames of silence to enqueue.
   * @return  The number of frames of silence actually written to the queue.
   */
  int enqueue_default(int frame_count)
  {
    return samples_to_frames(
        ring_buffer.enqueue(nullptr, frames_to_samples(frame_count)));
  }
  /**
   * @brief Enqueue `frames_count` frames of audio.
   *
   * Only safely called from the producer thread.
   *
   * @param [in] frames If non-null, the frames to enqueue.
   *                    Otherwise, silent frames are enqueued.
   * @param frame_count The number of frames to enqueue.
   *
   * @return The number of frames enqueued
   */

  int enqueue(T * frames, int frame_count)
  {
    return samples_to_frames(
        ring_buffer.enqueue(frames, frames_to_samples(frame_count)));
  }

  /**
   * @brief Removes `frame_count` frames from the buffer, and
   *        write them to `frames` if it is non-null.
   *
   * Only safely called on the consumer thread.
   *
   * @param frames      If non-null, the frames are copied to `frames`.
   *                    Otherwise, they are dropped.
   * @param frame_count The number of frames to remove.
   *
   * @return  The number of frames actually dequeud.
   */
  int dequeue(T * frames, int frame_count)
  {
    return samples_to_frames(
        ring_buffer.dequeue(frames, frames_to_samples(frame_count)));
  }
  /**
   * Get the number of available frames of audio for consuming.
   *
   * Only safely called on the consumer thread.
   *
   * @return The number of available frames of audio for reading.
   */
  int available_read() const
  {
    return samples_to_frames(ring_buffer.available_read());
  }
  /**
   * Get the number of available frames of audio for consuming.
   *
   * Only safely called on the producer thread.
   *
   * @return The number of empty slots in the buffer, available for writing.
   */
  int available_write() const
  {
    return samples_to_frames(ring_buffer.available_write());
  }
  /**
   * Get the total capacity, for this ring buffer.
   *
   * Can be called safely on any thread.
   *
   * @return The maximum capacity of this ring buffer.
   */
  int capacity() const { return samples_to_frames(ring_buffer.capacity()); }

private:
  /**
   * @brief Frames to samples conversion.
   *
   * @param frames The number of frames.
   *
   * @return  A number of samples.
   */
  int frames_to_samples(int frames) const { return frames * channel_count; }
  /**
   * @brief Samples to frames conversion.
   *
   * @param samples The number of samples.
   *
   * @return  A number of frames.
   */
  int samples_to_frames(int samples) const { return samples / channel_count; }
  /** Number of channels of audio that will stream through this ring buffer. */
  int channel_count;
  /** The underlying ring buffer that is used to store the data. */
  ring_buffer_base<T> ring_buffer;
};

/**
 * Lock-free instantiation of the `ring_buffer_base` type. This is safe to use
 * from two threads, one producer, one consumer (that never change role),
 * without explicit synchronization.
 */
template <typename T> using lock_free_queue = ring_buffer_base<T>;
/**
 * Lock-free instantiation of the `audio_ring_buffer` type. This is safe to use
 * from two threads, one producer, one consumer (that never change role),
 * without explicit synchronization.
 */
template <typename T>
using lock_free_audio_ring_buffer = audio_ring_buffer_base<T>;

#endif // CUBEB_RING_BUFFER_H
