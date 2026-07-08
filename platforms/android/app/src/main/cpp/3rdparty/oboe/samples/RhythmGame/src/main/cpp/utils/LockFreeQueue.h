/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef RHYTHMGAME_LOCKFREEQUEUE_H
#define RHYTHMGAME_LOCKFREEQUEUE_H

#include <cstdint>
#include <atomic>

/**
 * A lock-free queue for single consumer, single producer. Not thread-safe when using multiple
 * consumers or producers.
 *
 * Example code:
 *
 * LockFreeQueue<int, 1024> myQueue;
 * int value = 123;
 * myQueue.push(value);
 * myQueue.pop(value);
 *
 * @tparam T - The item type
 * @tparam CAPACITY - Maximum number of items which can be held in the queue. Must be a power of 2.
 * Must be less than the maximum value permissible in INDEX_TYPE
 * @tparam INDEX_TYPE - The internal index type, defaults to uint32_t. Changing this will affect
 * the maximum capacity. Included for ease of unit testing because testing queue lengths of
 * UINT32_MAX can be time consuming and is not always possible.
 */

template <typename T, uint32_t CAPACITY, typename INDEX_TYPE = uint32_t>
class LockFreeQueue {
public:

    /**
     * Implementation details:
     *
     * We have 2 counters: readCounter and writeCounter. Each will increment until it reaches
     * INDEX_TYPE_MAX, then wrap to zero. Unsigned integer overflow is defined behaviour in C++.
     *
     * Each time we need to access our data array we call mask() which gives us the index into the
     * array. This approach avoids having a "dead item" in the buffer to distinguish between full
     * and empty states. It also allows us to have a size() method which is easily calculated.
     *
     * IMPORTANT: This implementation is only thread-safe with a single reader thread and a single
     * writer thread. Have more than one of either will result in Bad Things™.
     */

    static constexpr bool isPowerOfTwo(uint32_t n) { return (n & (n - 1)) == 0; }
    static_assert(isPowerOfTwo(CAPACITY), "Capacity must be a power of 2");
    static_assert(std::is_unsigned<INDEX_TYPE>::value, "Index type must be unsigned");

    /**
     * Pop a value off the head of the queue
     *
     * @param val - element will be stored in this variable
     * @return true if value was popped successfully, false if the queue is empty
     */
    bool pop(T &val) {
        if (isEmpty()){
            return false;
        } else {
            val = buffer[mask(readCounter)];
            ++readCounter;
            return true;
        }
    }

    /**
     * Add an item to the back of the queue
     *
     * @param item - The item to add
     * @return true if item was added, false if the queue was full
     */
    bool push(const T& item) {
        if (isFull()){
            return false;
        } else {
            buffer[mask(writeCounter)] = item;
            ++writeCounter;
            return true;
        }
    }

    /**
     * Get the item at the front of the queue but do not remove it
     *
     * @param item - item will be stored in this variable
     * @return true if item was stored, false if the queue was empty
     */
    bool peek(T &item) const {
        if (isEmpty()){
            return false;
        } else {
            item = buffer[mask(readCounter)];
            return true;
        }
    }

    /**
     * Get the number of items in the queue
     *
     * @return number of items in the queue
     */
    INDEX_TYPE size() const {

        /**
         * This is worth some explanation:
         *
         * Whilst writeCounter is greater than readCounter the result of (write - read) will always
         * be positive. Simple.
         *
         * But when writeCounter is equal to INDEX_TYPE_MAX (e.g. UINT32_MAX) the next push will
         * wrap it around to zero, the start of the buffer, making writeCounter less than
         * readCounter so the result of (write - read) will be negative.
         *
         * But because we're returning an unsigned type return value will be as follows:
         *
         * returnValue = INDEX_TYPE_MAX - (write - read)
         *
         * e.g. if write is 0, read is 150 and the INDEX_TYPE is uint8_t where the max value is
         * 255 the return value will be (255 - (0 - 150)) = 105.
         *
         */
        return writeCounter - readCounter;
    };

private:

    bool isEmpty() const { return readCounter == writeCounter; }

    bool isFull() const { return size() == CAPACITY; }

    INDEX_TYPE mask(INDEX_TYPE n) const { return static_cast<INDEX_TYPE>(n & (CAPACITY - 1)); }

    T buffer[CAPACITY];
    std::atomic<INDEX_TYPE> writeCounter { 0 };
    std::atomic<INDEX_TYPE> readCounter { 0 };

};

#endif //RHYTHMGAME_LOCKFREEQUEUE_H
