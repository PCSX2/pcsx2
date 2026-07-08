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

#ifndef OBOETESTER_FULL_DUPLEX_ANALYZER_H
#define OBOETESTER_FULL_DUPLEX_ANALYZER_H

#include <unistd.h>
#include <sys/types.h>

#include "oboe/Oboe.h"
#include "analyzer/LatencyAnalyzer.h"
#include "FullDuplexStreamWithConversion.h"
#include "MultiChannelRecording.h"

class FullDuplexAnalyzer : public FullDuplexStreamWithConversion {
public:
    FullDuplexAnalyzer(LoopbackProcessor *processor)
            : mLoopbackProcessor(processor) {
    }

    /**
     * Called when data is available on both streams.
     * Caller should override this method.
     */
    oboe::DataCallbackResult onBothStreamsReadyFloat(
            const float *inputData,
            int   numInputFrames,
            float *outputData,
            int   numOutputFrames
    ) override;

    oboe::Result start() override;

    LoopbackProcessor *getLoopbackProcessor() {
        return mLoopbackProcessor;
    }

    void setRecording(MultiChannelRecording *recording) {
        mRecording = recording;
    }

    bool isWriteReadDeltaValid() {
        return mWriteReadDeltaValid;
    }

    int64_t getWriteReadDelta() {
        return mWriteReadDelta;
    }

private:
    MultiChannelRecording  *mRecording = nullptr;

    LoopbackProcessor * const mLoopbackProcessor;

    std::atomic<bool> mWriteReadDeltaValid{false};
    std::atomic<int64_t> mWriteReadDelta{0};
};


#endif //OBOETESTER_FULL_DUPLEX_ANALYZER_H

