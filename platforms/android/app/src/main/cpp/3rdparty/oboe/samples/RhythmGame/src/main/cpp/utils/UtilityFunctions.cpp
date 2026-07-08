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

#include <chrono>
#include <ui/OpenGLFunctions.h>
#include <GameConstants.h>
#include "UtilityFunctions.h"
#include "logging.h"

int64_t nowUptimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void renderEvent(TapResult r){
    switch (r) {
        case TapResult::Success:
            SetGLScreenColor(kTapSuccessColor);
            break;
        case TapResult::Early:
            SetGLScreenColor(kTapEarlyColor);
            break;
        case TapResult::Late:
            SetGLScreenColor(kTapLateColor);
            break;
    }
}
