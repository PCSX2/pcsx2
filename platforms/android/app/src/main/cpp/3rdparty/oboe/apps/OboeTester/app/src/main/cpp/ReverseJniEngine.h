/*
 * Copyright 2025 The Android Open Source Project
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


#ifndef OBOETESTER_REVERSEJNIENGINE_H
#define OBOETESTER_REVERSEJNIENGINE_H

#include <jni.h>
#include <oboe/Oboe.h>
#include <atomic>
#include <memory>

class ReverseJniEngine : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    ReverseJniEngine(JNIEnv *env, jobject thiz, int channelCount);
    ~ReverseJniEngine();

    void start(int bufferSizeInBursts, int sleepDurationNs);
    void stop();
    void setBufferSizeInBursts(int bufferSizeInBursts);
    void setSleepDurationUs(int sleepDurationUs);

    void setAudioBuffer(JNIEnv *env, jfloatArray buffer);

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
                                          void *audioData,
                                          int32_t numFrames) override;

    void onErrorAfterClose(oboe::AudioStream *oboeStream,
                           oboe::Result error) override;

private:
    JavaVM *mJavaVM = nullptr;
    jobject mJavaObject = nullptr;
    jmethodID mOnAudioReadyId = nullptr;
    std::atomic<bool> mIsThreadAttached{false};
    std::atomic<int> mSleepDurationUs{0};

    int mChannelCount;

    jfloatArray mAudioBuffer = nullptr;

    std::shared_ptr<oboe::AudioStream> mAudioStream;

    void setupAudioStream();
    void closeAudioStream();
};

#endif //OBOETESTER_REVERSEJNIENGINE_H
