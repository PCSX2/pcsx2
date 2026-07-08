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

#include "ReverseJniEngine.h"
#include "common/OboeDebug.h"

#define TAG "ReverseJniEngine"

ReverseJniEngine::ReverseJniEngine(JNIEnv *env, jobject thiz, int channelCount)
        : mChannelCount(channelCount) {
    env->GetJavaVM(&mJavaVM);
    mJavaObject = env->NewGlobalRef(thiz);

    jclass javaClass = env->GetObjectClass(mJavaObject);
    mOnAudioReadyId = env->GetMethodID(javaClass, "onAudioReady", "(II)V");
    env->DeleteLocalRef(javaClass);
}

ReverseJniEngine::~ReverseJniEngine() {
    JNIEnv *env;
    mJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    env->DeleteGlobalRef(mJavaObject);

    if (mAudioBuffer) {
        env->DeleteGlobalRef(mAudioBuffer);
    }
}

void ReverseJniEngine::setAudioBuffer(JNIEnv *env, jfloatArray buffer) {
    mAudioBuffer = (jfloatArray)env->NewGlobalRef(buffer);
}

void ReverseJniEngine::start(int bufferSizeInBursts, int sleepDurationUs) {
    LOGI("Starting audio stream.");
    mSleepDurationUs = sleepDurationUs;
    setupAudioStream();
    setBufferSizeInBursts(bufferSizeInBursts);
    if (mAudioStream) {
        mAudioStream->requestStart();
    }
}

void ReverseJniEngine::stop() {
    __android_log_print(ANDROID_LOG_INFO, TAG, "Stopping audio stream.");
    if (mAudioStream) {
        mAudioStream->requestStop();
        closeAudioStream();
    }
}

void ReverseJniEngine::setBufferSizeInBursts(int bufferSizeInBursts) {
    if (mAudioStream) {
        mAudioStream->setBufferSizeInFrames(bufferSizeInBursts * mAudioStream->getFramesPerBurst());
        LOGI("Buffer size set to %d frames.", mAudioStream->getBufferSizeInFrames());
    }
}

void ReverseJniEngine::setSleepDurationUs(int sleepDurationUs) {
    mSleepDurationUs = sleepDurationUs;
}

void ReverseJniEngine::setupAudioStream() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(mChannelCount)
            ->setDataCallback(this)
            ->setErrorCallback(this);

    oboe::Result result = builder.openStream(mAudioStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to create stream. Error: %s", oboe::convertToText(result));
    } else {
        LOGI("Successfully created stream with channel count: %d", mAudioStream->getChannelCount());
    }
}

void ReverseJniEngine::closeAudioStream() {
    if (mAudioStream) {
        mAudioStream->close();
    }
}

oboe::DataCallbackResult ReverseJniEngine::onAudioReady(oboe::AudioStream *oboeStream,
                                                        void *audioData,
                                                        int32_t numFrames) {
    JNIEnv *env;
    int getEnvStat = mJavaVM->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (mJavaVM->AttachCurrentThread(&env, nullptr) != 0) {
            LOGE("Failed to attach current thread to JVM");
            return oboe::DataCallbackResult::Stop;
        }
        mIsThreadAttached = true;
    }

    int xRunCount = mAudioStream->getXRunCount().value();

    env->CallVoidMethod(mJavaObject, mOnAudioReadyId, numFrames, xRunCount);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        LOGE("Exception thrown in Java onAudioReady");
        return oboe::DataCallbackResult::Stop;
    }

    jfloatArray javaBuffer = mAudioBuffer;
    jsize floatsToCopy = numFrames * mChannelCount;
    env->GetFloatArrayRegion(javaBuffer, 0, floatsToCopy, static_cast<jfloat*>(audioData));

    // Sleep to simulate an actual workload.
    if (mSleepDurationUs > 0) {
        usleep(mSleepDurationUs);
    }

    return oboe::DataCallbackResult::Continue;
}

void ReverseJniEngine::onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) {
    if (mIsThreadAttached.load()) {
        mJavaVM->DetachCurrentThread();
        mIsThreadAttached = false;
    }
    LOGI("Audio stream closed. Error: %s", oboe::convertToText(error));
}
