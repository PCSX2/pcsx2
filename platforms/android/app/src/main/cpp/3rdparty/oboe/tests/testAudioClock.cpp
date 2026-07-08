/*
 * Copyright 2022 The Android Open Source Project
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

/*
 * Test FlowGraph
 */

#include "math.h"
#include "stdio.h"

#include <gtest/gtest.h>
#include <oboe/Oboe.h>

using namespace oboe;

#define NANOS_PER_MICROSECOND    ((int64_t) 1000)

constexpr int64_t kSleepTimeMicroSec = 50 * 1000;
constexpr double kMaxLatenessMicroSec = 20 * 1000;

TEST(TestAudioClock, GetNanosecondsMonotonic) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    usleep(kSleepTimeMicroSec);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}

TEST(TestAudioClock, GetNanosecondsRealtime) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    usleep(kSleepTimeMicroSec);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}

TEST(TestAudioClock, SleepUntilNanoTimeMonotonic) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    AudioClock::sleepUntilNanoTime(startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond, CLOCK_MONOTONIC);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}

TEST(TestAudioClock, SleepUntilNanoTimeRealtime) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    AudioClock::sleepUntilNanoTime(startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond, CLOCK_REALTIME);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}

TEST(TestAudioClock, SleepForNanosMonotonic) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    AudioClock::sleepForNanos(kSleepTimeMicroSec * kNanosPerMicrosecond, CLOCK_MONOTONIC);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_MONOTONIC);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}

TEST(TestAudioClock, SleepForNanosRealtime) {

    int64_t startNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    AudioClock::sleepForNanos(kSleepTimeMicroSec * kNanosPerMicrosecond, CLOCK_REALTIME);
    int64_t endNanos = AudioClock::getNanoseconds(CLOCK_REALTIME);
    ASSERT_GE(endNanos, startNanos + kSleepTimeMicroSec * kNanosPerMicrosecond);
    ASSERT_LT(endNanos, startNanos + ((kSleepTimeMicroSec + kMaxLatenessMicroSec)
            * kNanosPerMicrosecond));
}
