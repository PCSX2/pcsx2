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

#ifndef NATIVEOBOE_PLAY_RECORDING_CALLBACK_H
#define NATIVEOBOE_PLAY_RECORDING_CALLBACK_H

#include "oboe/Oboe.h"

#include "MultiChannelRecording.h"

class PlayRecordingCallback : public oboe::AudioStreamCallback {
public:
    PlayRecordingCallback() {}
    ~PlayRecordingCallback() = default;

    void setRecording(MultiChannelRecording *recording) {
        mRecording = recording;
    }

    /**
     * Called when the stream is ready to process audio.
     */
    oboe::DataCallbackResult onAudioReady(
            oboe::AudioStream *audioStream,
            void *audioData,
            int numFrames);

private:
    MultiChannelRecording *mRecording = nullptr;
};


#endif //NATIVEOBOE_PLAYRECORDINGCALLBACK_H
