/*
 * Copyright © 2022 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/**
 * Adapted and ported to C++ from https://crates.io/crates/triple_buffer
 */

#ifndef CUBEB_TRIPLE_BUFFER
#define CUBEB_TRIPLE_BUFFER

#include <atomic>

// Single producer / single consumer wait-free triple buffering
// implementation, for when a producer wants to publish data to a consumer
// without blocking, but when a queue is wastefull, because it's OK for the
// consumer to miss data updates.
template <typename T> class triple_buffer {
public:
  // Write a new value into the triple buffer. Returns true if a value was
  // overwritten.
  // Producer-side only.
  bool write(T & input)
  {
    storage[input_idx] = input;
    return publish();
  }
  // Get the latest value from the triple buffer.
  // Consumer-side only.
  T & read()
  {
    update();
    return storage[output_idx];
  }
  // Returns true if a new value has been published by the consumer without
  // having been consumed yet.
  // Consumer-side only.
  bool updated()
  {
    return (shared_state.load(std::memory_order_relaxed) & BACK_DIRTY_BIT) != 0;
  }

private:
  // Publish a value to the consumer. Returns true if the data was overwritten
  // without having been read.
  bool publish()
  {
    auto former_back_idx = shared_state.exchange(input_idx | BACK_DIRTY_BIT,
                                                 std::memory_order_acq_rel);
    input_idx = former_back_idx & BACK_INDEX_MASK;
    return (former_back_idx & BACK_DIRTY_BIT) != 0;
  }
  // Get a new value from the producer, if a new value has been produced.
  bool update()
  {
    bool was_updated = updated();
    if (was_updated) {
      auto former_back_idx =
          shared_state.exchange(output_idx, std::memory_order_acq_rel);
      output_idx = former_back_idx & BACK_INDEX_MASK;
    }
    return was_updated;
  }
  T storage[3];
  // Mask used to extract back-buffer index
  const uint8_t BACK_INDEX_MASK = 0b11;
  // Bit set by producer to signal updates
  const uint8_t BACK_DIRTY_BIT = 0b100;
  // Shared state: a dirty bit, and an index.
  std::atomic<uint8_t> shared_state = {0};
  // Output index, private to the consumer.
  uint8_t output_idx = 1;
  // Input index, private to the producer.
  uint8_t input_idx = 2;
};

#endif // CUBEB_TRIPLE_BUFFER
