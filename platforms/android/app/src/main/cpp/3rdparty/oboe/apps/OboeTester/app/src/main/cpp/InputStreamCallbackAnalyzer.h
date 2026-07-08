/*
 * Copyright 2016 The Android Open Source Project
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


#ifndef NATIVEOBOE_INPUTSTREAMCALLBACKANALYZER_H
#define NATIVEOBOE_INPUTSTREAMCALLBACKANALYZER_H

#include <unistd.h>
#include <sys/types.h>

// TODO #include "flowgraph/FlowGraph.h"
#include "oboe/Oboe.h"

#include "analyzer/PeakDetector.h"
#include "FormatConverterBox.h"
#include "MultiChannelRecording.h"
#include "OboeTesterStreamCallback.h"

class InputStreamCallbackAnalyzer : public OboeTesterStreamCallback {
public:

    void reset() {
        for (int iChannel = 0; iChannel < mNumChannels; iChannel++) {
            mPeakDetectors[iChannel].reset();
        }
        OboeTesterStreamCallback::reset();
    }

    void setup(int32_t maxFramesPerCallback,
               int32_t channelCount,
               oboe::AudioFormat inputFormat) {
        mNumChannels = channelCount;
        mPeakDetectors = std::make_unique<PeakDetector[]>(channelCount);
        int32_t bufferSize = maxFramesPerCallback * channelCount;
        mInputConverter = std::make_unique<FormatConverterBox>(bufferSize,
                                                               inputFormat,
                                                               oboe::AudioFormat::Float);
    }

    /**
     * Called by Oboe when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames) override;

    void setRecording(MultiChannelRecording *recording) {
        mRecording = recording;
    }

    double getPeakLevel(int index);

    void setMinimumFramesBeforeRead(int32_t numFrames) {
        mMinimumFramesBeforeRead = numFrames;
    }

    int32_t getMinimumFramesBeforeRead() {
        return mMinimumFramesBeforeRead;
    }

public:
    int32_t                         mNumChannels = 0;
    std::unique_ptr<PeakDetector[]> mPeakDetectors;
    MultiChannelRecording          *mRecording = nullptr;

private:
    std::unique_ptr<FormatConverterBox> mInputConverter;
    int32_t                             mMinimumFramesBeforeRead = 0;
};

#endif //NATIVEOBOE_INPUTSTREAMCALLBACKANALYZER_H
