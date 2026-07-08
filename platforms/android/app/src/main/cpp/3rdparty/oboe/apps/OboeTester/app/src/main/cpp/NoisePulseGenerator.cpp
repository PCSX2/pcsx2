/*
 * Copyright 2025 The Android Open Source Project
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

#include <unistd.h>
#include "common/OboeDebug.h"
#include "oboe/Definitions.h"
#include "NoisePulseGenerator.h"

using namespace oboe::flowgraph;

NoisePulseGenerator::NoisePulseGenerator()
        : amplitude(*this, 1)
        , output(*this, 1)
        , mRequestCount(0)
        , mAcknowledgeCount(0)
        , mLevel(0.0f) {
}

NoisePulseGenerator::~NoisePulseGenerator() { }

void NoisePulseGenerator::reset() {
    FlowGraphNode::reset();
    mAcknowledgeCount.store(mRequestCount.load());
}

int32_t NoisePulseGenerator::onProcess(int numFrames) {
    const float *amplitudes = amplitude.getBuffer();
    float *buffer = output.getBuffer();

    if (mRequestCount.load() > mAcknowledgeCount.load()) {
        mLevel = 1.0;
        mAcknowledgeCount++;
    }

    // Check level to prevent numeric underflow.
    if (mLevel > 0.000001) {
        for (int i = 0; i < numFrames; i++) {
            *buffer++ = (float) (mLevel * amplitudes[i]);
            mLevel *= 0.999;
        }
    } else {
        for (int i = 0; i < numFrames; i++) {
            *buffer++ = 0.0f;
        }
    }

    return numFrames;
}

void NoisePulseGenerator::trigger() {
    mRequestCount++;
}
