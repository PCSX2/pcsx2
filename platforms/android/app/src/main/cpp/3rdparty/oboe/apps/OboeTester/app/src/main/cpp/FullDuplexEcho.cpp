/*
 * Copyright 2019 The Android Open Source Project
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

#include "common/OboeDebug.h"
#include "FullDuplexEcho.h"

oboe::Result  FullDuplexEcho::start() {
    int32_t delayFrames = (int32_t) (kMaxDelayTimeSeconds * getOutputStream()->getSampleRate());
    mDelayLine = std::make_unique<InterpolatingDelayLine>(delayFrames);
    // Use peak detector for input streams
    mNumChannels = getInputStream()->getChannelCount();
    mPeakDetectors = std::make_unique<PeakDetector[]>(mNumChannels);
    return FullDuplexStreamWithConversion::start();
}

double FullDuplexEcho::getPeakLevel(int index) {
    if (mPeakDetectors == nullptr) {
        LOGE("%s() called before setup()", __func__);
        return -1.0;
    } else if (index < 0 || index >= mNumChannels) {
        LOGE("%s(), index out of range, 0 <= %d < %d", __func__, index, mNumChannels.load());
        return -2.0;
    }
    return mPeakDetectors[index].getLevel();
}

oboe::DataCallbackResult FullDuplexEcho::onBothStreamsReadyFloat(
        const float *inputData,
        int   numInputFrames,
        float *outputData,
        int   numOutputFrames) {
    int32_t framesToEcho = std::min(numInputFrames, numOutputFrames);
    auto *inputFloat = const_cast<float *>(inputData);
    float *outputFloat = outputData;
    // zero out entire output array
    memset(outputFloat, 0, static_cast<size_t>(numOutputFrames)
            * static_cast<size_t>(getOutputStream()->getBytesPerFrame()));

    int32_t inputStride = getInputStream()->getChannelCount();
    int32_t outputStride = getOutputStream()->getChannelCount();
    float delayFrames = mDelayTimeSeconds * getOutputStream()->getSampleRate();
    while (framesToEcho-- > 0) {
        *outputFloat = mDelayLine->process(delayFrames, *inputFloat); // mono delay

        for (int iChannel = 0; iChannel < inputStride; iChannel++) {
            float sample = * (inputFloat + iChannel);
            mPeakDetectors[iChannel].process(sample);
        }

        inputFloat += inputStride;
        outputFloat += outputStride;
    }
    return oboe::DataCallbackResult::Continue;
};
