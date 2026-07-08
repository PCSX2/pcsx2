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

#ifndef OBOETESTER_FULL_DUPLEX_STREAM_WITH_CONVERSION_H
#define OBOETESTER_FULL_DUPLEX_STREAM_WITH_CONVERSION_H

#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"
#include "FormatConverterBox.h"

class FullDuplexStreamWithConversion : public oboe::FullDuplexStream {
public:
    /**
     * Called when data is available on both streams.
     * Caller must override this method.
     */
    virtual oboe::DataCallbackResult onBothStreamsReadyFloat(
            const float *inputData,
            int numInputFrames,
            float *outputData,
            int numOutputFrames
    ) = 0;

    /**
     * Overrides the default onBothStreamsReady by converting to floats and then calling
     * onBothStreamsReadyFloat().
     */
    oboe::DataCallbackResult onBothStreamsReady(
            const void *inputData,
            int numInputFrames,
            void *outputData,
            int numOutputFrames
    ) override;

    oboe::ResultWithValue<int32_t> readInput(int32_t numFrames) override;

    virtual oboe::Result start() override;

private:
    std::unique_ptr<FormatConverterBox> mInputConverter;
    std::unique_ptr<FormatConverterBox> mOutputConverter;

};


#endif //OBOETESTER_FULL_DUPLEX_STREAM_WITH_CONVERSION_H
