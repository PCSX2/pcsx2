/*
 * Copyright (C) 2017 The Android Open Source Project
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


#ifndef ANALYZER_PSEUDORANDOM_H
#define ANALYZER_PSEUDORANDOM_H

#include <cstdint>

class PseudoRandom {
public:
    PseudoRandom(int64_t seed = 99887766)
            :    mSeed(seed)
    {}

    /**
     * Returns the next random double from -1.0 to 1.0
     *
     * @return value from -1.0 to 1.0
     */
    double nextRandomDouble() {
        return nextRandomInteger() * (0.5 / (((int32_t)1) << 30));
    }

    /** Calculate random 32 bit number using linear-congruential method
     * with known real-time performance.
     */
    int32_t nextRandomInteger() {
#if __has_builtin(__builtin_mul_overflow) && __has_builtin(__builtin_add_overflow)
        int64_t prod;
        // Use values for 64-bit sequence from MMIX by Donald Knuth.
        __builtin_mul_overflow(mSeed, (int64_t)6364136223846793005, &prod);
        __builtin_add_overflow(prod, (int64_t)1442695040888963407, &mSeed);
#else
        mSeed = (mSeed * (int64_t)6364136223846793005) + (int64_t)1442695040888963407;
#endif
        return (int32_t) (mSeed >> 32); // The higher bits have a longer sequence.
    }

private:
    int64_t mSeed;
};

#endif //ANALYZER_PSEUDORANDOM_H
