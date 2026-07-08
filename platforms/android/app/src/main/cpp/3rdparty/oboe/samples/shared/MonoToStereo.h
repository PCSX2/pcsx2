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

#ifndef SHARED_MONOTOSTEREO_H
#define SHARED_MONOTOSTEREO_H

#include "IRenderableAudio.h"


class MonoToStereo : public IRenderableAudio {

public:

    MonoToStereo(IRenderableAudio *input) : mInput(input){};

    void renderAudio(float *audioData, int32_t numFrames) override {

        constexpr int kChannelCountStereo = 2;

        mInput->renderAudio(audioData, numFrames);

        // We assume that audioData has sufficient frames to hold the stereo output, so copy each
        // frame in the input to the output twice, working our way backwards through the input array
        // e.g. 123 => 112233
        for (int i = numFrames - 1; i >= 0; --i) {

            audioData[i * kChannelCountStereo] = audioData[i];
            audioData[i * kChannelCountStereo + 1] = audioData[i];
        }
    }

    IRenderableAudio *mInput;
};


#endif //SHARED_MONOTOSTEREO_H
