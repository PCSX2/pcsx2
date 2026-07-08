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

#ifndef RHYTHMGAME_UTILITYFUNCTIONS_H
#define RHYTHMGAME_UTILITYFUNCTIONS_H

#include <stdint.h>

constexpr int64_t kMillisecondsInSecond = 1000;
constexpr int64_t kNanosecondsInMillisecond = 1000000;

enum class TapResult {
    Early,
    Late,
    Success
};

int64_t nowUptimeMillis();

constexpr int64_t convertFramesToMillis(const int64_t frames, const int sampleRate){
    return static_cast<int64_t>((static_cast<double>(frames)/ sampleRate) * kMillisecondsInSecond);
}

TapResult getTapResult(int64_t tapTimeInMillis, int64_t tapWindowInMillis);

void renderEvent(TapResult r);


#endif //RHYTHMGAME_UTILITYFUNCTIONS_H
