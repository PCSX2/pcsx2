/*
 * Copyright 2021 The Android Open Source Project
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

#ifndef SYNTHMARK_ADPF_WRAPPER_H
#define SYNTHMARK_ADPF_WRAPPER_H

#include <algorithm>
#include <functional>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>

#include "oboe/Definitions.h"

namespace oboe {

    struct APerformanceHintManager;
    struct APerformanceHintSession;

    typedef struct APerformanceHintManager APerformanceHintManager;
    typedef struct APerformanceHintSession APerformanceHintSession;

    class AdpfWrapper {
    public:
        /**
         * Create an ADPF session that can be used to boost performance.
         * @param threadId
         * @param targetDurationNanos - nominal period of isochronous task
         * @return zero or negative error
         */
        int open(pid_t threadId,
                 int64_t targetDurationNanos);

        bool isOpen() const {
            return (mHintSession != nullptr);
        }

        void close();

        /**
         * Call this at the beginning of the callback that you are measuring.
         */
        void onBeginCallback();

        /**
         * Call this at the end of the callback that you are measuring.
         * It is OK to skip this if you have a short callback.
         */
        void onEndCallback(double durationScaler);

        /**
         * For internal use only!
         * This is a hack for communicating with experimental versions of ADPF.
         * @param enabled
         */
        static void setUseAlternative(bool enabled) {
            sUseAlternativeHack = enabled;
        }

        /**
         * Report the measured duration of a callback.
         * This is normally called by onEndCallback().
         * On older Android devices (below Android API level 36), you may want to call this
         * directly in order to give an advance hint of a jump in workload.
         * On Android API level 36 and above, notifyWorkloadIncrease() is the preferred
         * API to call instead to signal an upcoming increase in workload.
         * @param actualDurationNanos
         */
        void reportActualDuration(int64_t actualDurationNanos);

        void reportWorkload(int32_t appWorkload);

        /**
        * Informs the framework of an upcoming increase in the workload of an audio callback
        * bound to this session. The user can specify whether the increase is expected to be
        * on the CPU, GPU, or both.
        *
        * Sending hints for both CPU and GPU counts as two separate hints for the purposes of the
        * rate limiter.
        *
        * This was introduced in Android API Level 36
        *
        * @param cpu Indicates if the workload increase is expected to affect the CPU.
        * @param gpu Indicates if the workload increase is expected to affect the GPU.
        * @param debugName A required string used to identify this specific hint during
        *        tracing. This debug string will only be held for the duration of the
        *        method, and can be safely discarded after.
        *
        * @return Result::OK on success.
        *         Result::ErrorClosed if open was not called.
        *         Result::ErrorUnimplemented if the API is not supported.
        *         Result::ErrorInvalidHandle if no hints were requested.
        *         Result::ErrorInvalidRate if the hint was rate limited.
        *         Result::ErrorNoService if communication with the system service has failed.
        *         Result::ErrorUnavailable if the hint is not supported.
        */
        oboe::Result notifyWorkloadIncrease(bool cpu, bool gpu, const char* debugName);

        /**
        * Informs the framework of an upcoming reset in the workload of an audio callback
        * bound to this session, or the imminent start of a new workload. The user can specify
        * whether the reset is expected to affect the CPU, GPU, or both.
        *
        * Sending hints for both CPU and GPU counts as two separate hints for the purposes of the
        * this load tracking.
        *
        * This was introduced in Android API Level 36
        *
        * @param cpu Indicates if the workload reset is expected to affect the CPU.
        * @param gpu Indicates if the workload reset is expected to affect the GPU.
        * @param debugName A required string used to identify this specific hint during
        *        tracing. This debug string will only be held for the duration of the
        *        method, and can be safely discarded after.
        *
        * @return Result::OK on success.
        *         Result::ErrorClosed if open was not called.
        *         Result::ErrorUnimplemented if the API is not supported.
        *         Result::ErrorInvalidHandle if no hints were requested.
        *         Result::ErrorInvalidRate if the hint was rate limited.
        *         Result::ErrorNoService if communication with the system service has failed.
        *         Result::ErrorUnavailable if the hint is not supported.
        */
        oboe::Result notifyWorkloadReset(bool cpu, bool gpu, const char* debugName);

        /**
        * Informs the framework of an upcoming one-off expensive frame for an audio callback
        * bound to this session. This frame will be treated as not representative of the workload as a
        * whole, and it will be discarded the purposes of load tracking. The user can specify
        * whether the workload spike is expected to be on the CPU, GPU, or both.
        *
        * Sending hints for both CPU and GPU counts as two separate hints for the purposes of the
        * rate limiter.
        *
        * This was introduced in Android API Level 36
        *
        * @param cpu Indicates if the workload spike is expected to affect the CPU.
        * @param gpu Indicates if the workload spike is expected to affect the GPU.
        * @param debugName A required string used to identify this specific hint during
        *        tracing. This debug string will only be held for the duration of the
        *        method, and can be safely discarded after.
        *
        * @return Result::OK on success.
        *         Result::ErrorClosed if open was not called.
        *         Result::ErrorUnimplemented if the API is not supported.
        *         Result::ErrorInvalidHandle if no hints were requested.
        *         Result::ErrorInvalidRate if the hint was rate limited.
        *         Result::ErrorNoService if communication with the system service has failed.
        *         Result::ErrorUnavailable if the hint is not supported.
        */
        oboe::Result notifyWorkloadSpike(bool cpu, bool gpu, const char* debugName);

    private:
        std::mutex mLock;
        APerformanceHintSession *mHintSession = nullptr;
        int64_t mBeginCallbackNanos = 0;
        static bool sUseAlternativeHack;
        int32_t mPreviousWorkload = 0;
        double mNanosPerWorkloadUnit = 0.0;
    };

}
#endif //SYNTHMARK_ADPF_WRAPPER_H
