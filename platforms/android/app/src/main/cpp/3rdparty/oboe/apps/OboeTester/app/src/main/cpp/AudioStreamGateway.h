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
#ifndef NATIVEOBOE_AUDIOGRAPHRUNNER_H
#define NATIVEOBOE_AUDIOGRAPHRUNNER_H

#include <unistd.h>
#include <sys/types.h>

#include "flowgraph/FlowGraphNode.h"
#include "oboe/Oboe.h"
#include "OboeTesterStreamCallback.h"

using namespace oboe::flowgraph;

/**
 * Bridge between an audio flowgraph and an audio device.
 * Pass in an AudioSink and then pass
 * this object to the AudioStreamBuilder as a callback.
 */
class AudioStreamGateway : public OboeTesterStreamCallback {
public:
    virtual ~AudioStreamGateway() = default;

    void setAudioSink(std::shared_ptr<oboe::flowgraph::FlowGraphSink>  sink) {
        mAudioSink = sink;
    }

    /**
     * Called by Oboe when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames) override;

private:

    std::shared_ptr<oboe::flowgraph::FlowGraphSink>  mAudioSink;
};


#endif //NATIVEOBOE_AUDIOGRAPHRUNNER_H
