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

#include <dlfcn.h>
#include <stdint.h>
#include <sys/types.h>

#include "oboe/AudioClock.h"
#include "oboe/Definitions.h"
#include "oboe/Utilities.h"
#include "AdpfWrapper.h"
#include "OboeDebug.h"
#include "Trace.h"

using namespace oboe;

typedef APerformanceHintManager* (*APH_getManager)();
typedef APerformanceHintSession* (*APH_createSession)(APerformanceHintManager*, const int32_t*,
                                                      size_t, int64_t);
typedef void (*APH_reportActualWorkDuration)(APerformanceHintSession*, int64_t);
typedef void (*APH_closeSession)(APerformanceHintSession* session);
typedef int (*APH_notifyWorkloadIncrease)(APerformanceHintSession*, bool, bool, const char*);
typedef int (*APH_notifyWorkloadSpike)(APerformanceHintSession*, bool, bool, const char*);
typedef int (*APH_notifyWorkloadReset)(APerformanceHintSession*, bool, bool, const char*);

static bool gAPerformanceHintBindingInitialized = false;
static APH_getManager gAPH_getManagerFn = nullptr;
static APH_createSession gAPH_createSessionFn = nullptr;
static APH_reportActualWorkDuration gAPH_reportActualWorkDurationFn = nullptr;
static APH_closeSession gAPH_closeSessionFn = nullptr;
static APH_notifyWorkloadIncrease gAPH_notifyWorkloadIncreaseFn = nullptr;
static APH_notifyWorkloadSpike gAPH_notifyWorkloadSpikeFn = nullptr;
static APH_notifyWorkloadReset gAPH_notifyWorkloadResetFn = nullptr;

#ifndef __ANDROID_API_B__
#define __ANDROID_API_B__ 36
#endif

static int loadAphFunctions() {
    if (gAPerformanceHintBindingInitialized) return true;

    void* handle_ = dlopen("libandroid.so", RTLD_NOW | RTLD_NODELETE);
    if (handle_ == nullptr) {
        return -1000;
    }

    gAPH_getManagerFn = (APH_getManager)dlsym(handle_, "APerformanceHint_getManager");
    if (gAPH_getManagerFn == nullptr) {
        return -1001;
    }

    gAPH_createSessionFn = (APH_createSession)dlsym(handle_, "APerformanceHint_createSession");
    if (gAPH_createSessionFn == nullptr) {
        return -1002;
    }

    gAPH_reportActualWorkDurationFn = (APH_reportActualWorkDuration)dlsym(
            handle_, "APerformanceHint_reportActualWorkDuration");
    if (gAPH_reportActualWorkDurationFn == nullptr) {
        return -1003;
    }

    gAPH_closeSessionFn = (APH_closeSession)dlsym(handle_, "APerformanceHint_closeSession");
    if (gAPH_closeSessionFn == nullptr) {
        return -1004;
    }

    // TODO: Remove pre-release check after Android B release
    if (getSdkVersion() >= __ANDROID_API_B__ || isAtLeastPreReleaseCodename("Baklava")) {
        gAPH_notifyWorkloadIncreaseFn = (APH_notifyWorkloadIncrease)dlsym(
                handle_, "APerformanceHint_notifyWorkloadIncrease");
        if (gAPH_notifyWorkloadIncreaseFn == nullptr) {
            return -1005;
        }
        gAPH_notifyWorkloadSpikeFn = (APH_notifyWorkloadSpike)dlsym(
            handle_, "APerformanceHint_notifyWorkloadSpike");
        if (gAPH_notifyWorkloadSpikeFn == nullptr) {
            return -1006;
        }
        gAPH_notifyWorkloadResetFn = (APH_notifyWorkloadReset)dlsym(
            handle_, "APerformanceHint_notifyWorkloadReset");
        if (gAPH_notifyWorkloadResetFn == nullptr) {
            return -1007;
        }
    }

    gAPerformanceHintBindingInitialized = true;

    return 0;
}

bool AdpfWrapper::sUseAlternativeHack = false; // TODO remove hack

int AdpfWrapper::open(pid_t threadId,
                      int64_t targetDurationNanos) {
    std::lock_guard<std::mutex> lock(mLock);
    int result = loadAphFunctions();
    if (result < 0) return result;

    // This is a singleton.
    APerformanceHintManager* manager = gAPH_getManagerFn();

    int32_t thread32 = threadId;
    if (sUseAlternativeHack) {
        // TODO Remove this hack when we finish experimenting with alternative algorithms.
        // The A5 is an arbitrary signal to a hacked version of ADPF to try an alternative
        // algorithm that is not based on PID.
        targetDurationNanos = (targetDurationNanos & ~0xFF) | 0xA5;
    }
    mHintSession = gAPH_createSessionFn(manager, &thread32, 1 /* size */, targetDurationNanos);
    if (mHintSession == nullptr) {
        return -1;
    }
    return 0;
}

void AdpfWrapper::reportActualDuration(int64_t actualDurationNanos) {
    //LOGD("ADPF Oboe %s(dur=%lld)", __func__, (long long)actualDurationNanos);
    std::lock_guard<std::mutex> lock(mLock);
    if (mHintSession != nullptr) {
        bool traceEnabled = Trace::getInstance().isEnabled();
        if (traceEnabled) {
            Trace::getInstance().beginSection("reportActualDuration");
            Trace::getInstance().setCounter("actualDurationNanos", actualDurationNanos);
        }
        gAPH_reportActualWorkDurationFn(mHintSession, actualDurationNanos);
        if (traceEnabled) {
            Trace::getInstance().endSection();
        }
    }
}

void AdpfWrapper::close() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mHintSession != nullptr) {
        gAPH_closeSessionFn(mHintSession);
        mHintSession = nullptr;
    }
}

void AdpfWrapper::onBeginCallback() {
    if (isOpen()) {
        mBeginCallbackNanos = oboe::AudioClock::getNanoseconds();
    }
}

void AdpfWrapper::onEndCallback(double durationScaler) {
    if (isOpen()) {
        int64_t endCallbackNanos = oboe::AudioClock::getNanoseconds();
        int64_t actualDurationNanos = endCallbackNanos - mBeginCallbackNanos;
        int64_t scaledDurationNanos = static_cast<int64_t>(actualDurationNanos * durationScaler);
        reportActualDuration(scaledDurationNanos);
        // When the workload is non-zero, update the conversion factor from workload
        // units to nanoseconds duration.
        if (mPreviousWorkload > 0) {
            mNanosPerWorkloadUnit = ((double) scaledDurationNanos) / mPreviousWorkload;
        }
    }
}

void AdpfWrapper::reportWorkload(int32_t appWorkload) {
    if (isOpen()) {
        // Compare with previous workload. If we think we will need more
        // time to render the callback then warn ADPF as soon as possible.
        if (appWorkload > mPreviousWorkload && mNanosPerWorkloadUnit > 0.0) {
            int64_t predictedDuration = (int64_t) (appWorkload * mNanosPerWorkloadUnit);
            reportActualDuration(predictedDuration);
        }
        mPreviousWorkload = appWorkload;
    }
}

oboe::Result AdpfWrapper::notifyWorkloadIncrease(bool cpu, bool gpu, const char* debugName) {
    std::lock_guard<std::mutex> lock(mLock);
    bool traceEnabled = Trace::getInstance().isEnabled();
    if (traceEnabled) {
        Trace::getInstance().beginSection("notifyWorkloadIncrease");
    }
    if (gAPH_notifyWorkloadIncreaseFn == nullptr) {
        return Result::ErrorUnimplemented;
    }
    if (mHintSession == nullptr) {
        return Result::ErrorClosed;
    }
    int result = gAPH_notifyWorkloadIncreaseFn(mHintSession, cpu, gpu, debugName);
    if (result == 0) {
        return Result::OK;
    } else if (result == EINVAL) { // no hints were requested
        return Result::ErrorInvalidHandle;
    } else if (result == EBUSY) { // the hint was rate limited
        return Result::ErrorInvalidRate;
    } else if (result == EPIPE) { // communication with the system service has failed
        return Result::ErrorNoService;
    } else if (result == ENOTSUP) { // the hint is not supported
        return Result::ErrorUnavailable;
    } else {
        return Result::ErrorInternal; // Unknown error
    }
    if (traceEnabled) {
        Trace::getInstance().endSection();
    }
}

oboe::Result AdpfWrapper::notifyWorkloadSpike(bool cpu, bool gpu, const char* debugName) {
    std::lock_guard<std::mutex> lock(mLock);
    bool traceEnabled = Trace::getInstance().isEnabled();
    if (traceEnabled) {
        Trace::getInstance().beginSection("notifyWorkloadSpike");
    }
    if (gAPH_notifyWorkloadSpikeFn == nullptr) {
        return Result::ErrorUnimplemented;
    }
    if (mHintSession == nullptr) {
        return Result::ErrorClosed;
    }
    int result = gAPH_notifyWorkloadSpikeFn(mHintSession, cpu, gpu, debugName);
    if (result == 0) {
        return Result::OK;
    } else if (result == EINVAL) { // no hints were requested
        return Result::ErrorInvalidHandle;
    } else if (result == EBUSY) { // the hint was rate limited
        return Result::ErrorInvalidRate;
    } else if (result == EPIPE) { // communication with the system service has failed
        return Result::ErrorNoService;
    } else if (result == ENOTSUP) { // the hint is not supported
        return Result::ErrorUnavailable;
    } else {
        return Result::ErrorInternal; // Unknown error
    }
    if (traceEnabled) {
        Trace::getInstance().endSection();
    }
}

oboe::Result AdpfWrapper::notifyWorkloadReset(bool cpu, bool gpu, const char* debugName) {
    std::lock_guard<std::mutex> lock(mLock);
    bool traceEnabled = Trace::getInstance().isEnabled();
    if (traceEnabled) {
        Trace::getInstance().beginSection("notifyWorkloadReset");
    }
    if (gAPH_notifyWorkloadResetFn == nullptr) {
        return Result::ErrorUnimplemented;
    }
    if (mHintSession == nullptr) {
        return Result::ErrorClosed;
    }
    int result = gAPH_notifyWorkloadResetFn(mHintSession, cpu, gpu, debugName);
    if (result == 0) {
        return Result::OK;
    } else if (result == EINVAL) { // no hints were requested
        return Result::ErrorInvalidHandle;
    } else if (result == EBUSY) { // the hint was rate limited
        return Result::ErrorInvalidRate;
    } else if (result == EPIPE) { // communication with the system service has failed
        return Result::ErrorNoService;
    } else if (result == ENOTSUP) { // the hint is not supported
        return Result::ErrorUnavailable;
    } else {
        return Result::ErrorInternal; // Unknown error
    }
    if (traceEnabled) {
        Trace::getInstance().endSection();
    }
}
