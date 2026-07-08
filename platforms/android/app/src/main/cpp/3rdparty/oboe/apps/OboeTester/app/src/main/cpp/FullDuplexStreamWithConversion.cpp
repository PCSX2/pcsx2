/*
 * Copyright 2023 The Android Open Source Project
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
#include "FullDuplexStreamWithConversion.h"

oboe::Result FullDuplexStreamWithConversion::start() {
    // Determine maximum size that could possibly be called.
    int32_t maxFrames = getOutputStream()->getBufferCapacityInFrames();
    int32_t inputBufferSize = maxFrames * getInputStream()->getChannelCount();
    int32_t outputBufferSize = maxFrames * getOutputStream()->getChannelCount();
    mInputConverter = std::make_unique<FormatConverterBox>(inputBufferSize,
                                                           getInputStream()->getFormat(),
                                                           oboe::AudioFormat::Float);
    mOutputConverter = std::make_unique<FormatConverterBox>(outputBufferSize,
                                                            oboe::AudioFormat::Float,
                                                            getOutputStream()->getFormat());
    return FullDuplexStream::start();
}

oboe::ResultWithValue<int32_t> FullDuplexStreamWithConversion::readInput(int32_t numFrames) {
    oboe::ResultWithValue<int32_t> result = getInputStream()->read(
            mInputConverter->getInputBuffer(),
            numFrames,
            0 /* timeout */);
    if (result == oboe::Result::OK) {
        int32_t numSamples = result.value() * getInputStream()->getChannelCount();
        mInputConverter->convertInternalBuffers(numSamples);
    }
    return result;
}

oboe::DataCallbackResult FullDuplexStreamWithConversion::onBothStreamsReady(
        const void *inputData,
        int numInputFrames,
        void *outputData,
        int numOutputFrames
) {
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;
    callbackResult = onBothStreamsReadyFloat(
            static_cast<const float *>(mInputConverter->getOutputBuffer()),
            numInputFrames,
            static_cast<float *>(mOutputConverter->getInputBuffer()),
            numOutputFrames);
    mOutputConverter->convertFromInternalInput( outputData,
                                                numOutputFrames * getOutputStream()->getChannelCount());
    return callbackResult;
}
