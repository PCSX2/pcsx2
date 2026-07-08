/*
 * Copyright 2015 The Android Open Source Project
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
#include "InputStreamCallbackAnalyzer.h"

double InputStreamCallbackAnalyzer::getPeakLevel(int index) {
    if (mPeakDetectors == nullptr) {
        LOGE("%s() called before setup()", __func__);
        return -1.0;
    } else if (index < 0 || index >= mNumChannels) {
        LOGE("%s(), index out of range, 0 <= %d < %d", __func__, index, mNumChannels);
        return -2.0;
    }
    return mPeakDetectors[index].getLevel();
}

oboe::DataCallbackResult InputStreamCallbackAnalyzer::onAudioReady(
        oboe::AudioStream *audioStream,
        void *audioData,
        int numFrames) {
    int32_t channelCount = audioStream->getChannelCount();

    maybeHang(getNanoseconds());
    printScheduler();
    mInputConverter->convertToInternalOutput(numFrames * channelCount, audioData);
    float *floatData = (float *) mInputConverter->getOutputBuffer();
    if (mRecording != nullptr) {
        mRecording->write(floatData, numFrames);
    }
    int32_t sampleIndex = 0;
    for (int iFrame = 0; iFrame < numFrames; iFrame++) {
        for (int iChannel = 0; iChannel < channelCount; iChannel++) {
            float sample = floatData[sampleIndex++];
            mPeakDetectors[iChannel].process(sample);
        }
    }

    audioStream->waitForAvailableFrames(mMinimumFramesBeforeRead, oboe::kNanosPerSecond);

    return oboe::DataCallbackResult::Continue;
}
