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

#include <dlfcn.h>
#include <cstdio>
#include "Trace.h"
#include "OboeDebug.h"

using namespace oboe;

typedef void *(*fp_ATrace_beginSection)(const char *sectionName);

typedef void *(*fp_ATrace_endSection)();

typedef void *(*fp_ATrace_setCounter)(const char *counterName, int64_t counterValue);

typedef bool *(*fp_ATrace_isEnabled)(void);


bool Trace::isEnabled() const {
    return ATrace_isEnabled != nullptr && ATrace_isEnabled();
}

void Trace::beginSection(const char *format, ...) {
    char buffer[256];
    va_list va;
    va_start(va, format);
    vsprintf(buffer, format, va);
    ATrace_beginSection(buffer);
    va_end(va);
}

void Trace::endSection() const {
    ATrace_endSection();
}

void Trace::setCounter(const char *counterName, int64_t counterValue) const {
    ATrace_setCounter(counterName, counterValue);
}

Trace::Trace() {
    // Using dlsym allows us to use tracing on API 21+ without needing android/trace.h which wasn't
    // published until API 23
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    LOGD("Trace():  dlopen(%s) returned %p", "libandroid.so", lib);
    if (lib == nullptr) {
        LOGE("Trace() could not open libandroid.so to dynamically load tracing symbols");
    } else {
        ATrace_beginSection =
                reinterpret_cast<fp_ATrace_beginSection>(
                        dlsym(lib, "ATrace_beginSection"));
        if (ATrace_beginSection == nullptr) {
            LOGE("Trace::beginSection() not supported");
            return;
        }

        ATrace_endSection =
                reinterpret_cast<fp_ATrace_endSection>(
                        dlsym(lib, "ATrace_endSection"));
        if (ATrace_endSection == nullptr) {
            LOGE("Trace::endSection() not supported");
            return;
        }

        ATrace_setCounter =
                reinterpret_cast<fp_ATrace_setCounter>(
                        dlsym(lib, "ATrace_setCounter"));
        if (ATrace_setCounter == nullptr) {
            LOGE("Trace::setCounter() not supported");
            return;
        }

        // If any of the previous functions are null then ATrace_isEnabled will be null.
        ATrace_isEnabled =
                reinterpret_cast<fp_ATrace_isEnabled>(
                        dlsym(lib, "ATrace_isEnabled"));
        if (ATrace_isEnabled == nullptr) {
            LOGE("Trace::isEnabled() not supported");
        }
    }
}
