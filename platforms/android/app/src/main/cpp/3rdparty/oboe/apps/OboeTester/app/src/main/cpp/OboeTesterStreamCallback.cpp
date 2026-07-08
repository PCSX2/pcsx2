/*
 * Copyright 2020 The Android Open Source Project
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

#include <sched.h>
#include <cstring>

#include "AudioStreamGateway.h"
#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include "OboeStreamCallbackProxy.h"
#include "OboeTesterStreamCallback.h"
#include "OboeTools.h"
#include "synth/IncludeMeOnce.h"

int32_t OboeTesterStreamCallback::mHangTimeMillis = 0;

// Print if scheduler changes.
void OboeTesterStreamCallback::printScheduler() {
#if OBOE_ENABLE_LOGGING
    int scheduler = sched_getscheduler(gettid());
    if (scheduler != mPreviousScheduler) {
        int schedulerType = scheduler & 0xFFFF; // mask off high flags
        LOGD("callback CPU scheduler = 0x%08x = %s",
             scheduler,
             ((schedulerType == SCHED_FIFO) ? "SCHED_FIFO" :
              ((schedulerType == SCHED_OTHER) ? "SCHED_OTHER" :
               ((schedulerType == SCHED_RR) ? "SCHED_RR" : "UNKNOWN")))
        );
        mPreviousScheduler = scheduler;
    }
#endif
}

// Sleep to cause an XRun. Then reschedule.
void OboeTesterStreamCallback::maybeHang(const int64_t startNanos) {
    if (mHangTimeMillis == 0) return;

    if (startNanos > mNextTimeToHang) {
        LOGD("%s() start sleeping", __func__);
        // Take short naps until it is time to wake up.
        int64_t nowNanos = startNanos;
        int64_t wakeupNanos = startNanos + (mHangTimeMillis * NANOS_PER_MILLISECOND);
        while (nowNanos < wakeupNanos && mHangTimeMillis > 0) {
            int32_t sleepTimeMicros = (int32_t) ((wakeupNanos - nowNanos) / 1000);
            if (sleepTimeMicros == 0) break;
            // The usleep() function can fail if it sleeps for more than one second.
            // So sleep for several small intervals.
            // This also allows us to exit the loop if mHangTimeMillis gets set to zero.
            const int32_t maxSleepTimeMicros =  100 * 1000;
            sleepTimeMicros = std::min(maxSleepTimeMicros, sleepTimeMicros);
            usleep(sleepTimeMicros);
            nowNanos = getNanoseconds();
        }
        // Calculate when we hang again.
        const int32_t minDurationMillis = 500;
        const int32_t maxDurationMillis = std::max(10000, mHangTimeMillis * 2);
        int32_t durationMillis = mHangTimeMillis * 10;
        durationMillis = std::max(minDurationMillis, std::min(maxDurationMillis, durationMillis));
        mNextTimeToHang = startNanos + (durationMillis * NANOS_PER_MILLISECOND);
        LOGD("%s() slept for %d msec, durationMillis = %d", __func__,
             (int)((nowNanos - startNanos) / 1e6L),
             durationMillis);
    }
}

int64_t OboeTesterStreamCallback::getNanoseconds(clockid_t clockId) {
    struct timespec time;
    int result = clock_gettime(clockId, &time);
    if (result < 0) {
        return result;
    }
    return (time.tv_sec * 1e9) + time.tv_nsec;
}