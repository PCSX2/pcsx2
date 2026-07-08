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

#ifndef SHARED_MIXER_H
#define SHARED_MIXER_H

#include <array>
#include "IRenderableAudio.h"

/**
 * A Mixer object which sums the output from multiple tracks into a single output. The number of
 * input channels on each track must match the number of output channels (default 1=mono). This can
 * be changed by calling `setChannelCount`.
 * The inputs to the mixer are not owned by the mixer, they should not be deleted while rendering.
 */
class Mixer : public IRenderableAudio {

public:
    void renderAudio(float *audioData, int32_t numFrames) {

        int numSamples = numFrames * mChannelCount;
        if (numSamples > mBufferSize) {
            mMixingBuffer = std::make_unique<float[]>(numSamples);
            mBufferSize = numSamples;
        }

        // Zero out the incoming container array
        memset(audioData, 0, sizeof(float) * numSamples);

        for (int i = 0; i < mTracks.size(); ++i) {
            mTracks[i]->renderAudio(mMixingBuffer.get(), numFrames);

            for (int j = 0; j < numSamples; ++j) {
                audioData[j] += mMixingBuffer[j];
            }
        }
    }

    void addTrack(IRenderableAudio *renderer){
        mTracks.push_back(renderer);
    }

    void setChannelCount(int32_t channelCount){ mChannelCount = channelCount; }

    void removeAllTracks(){
        mTracks.clear();
    }

private:
    int32_t mBufferSize = 0;
    std::unique_ptr<float[]> mMixingBuffer;
    std::vector<IRenderableAudio*> mTracks;
    int32_t mChannelCount = 1; // Default to mono
};


#endif //SHARED_MIXER_H
