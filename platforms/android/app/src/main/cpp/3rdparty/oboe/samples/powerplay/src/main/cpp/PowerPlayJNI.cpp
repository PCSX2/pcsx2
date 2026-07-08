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

#include <jni.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <android/log.h>

// parselib includes
#include <stream/MemInputStream.h>
#include <wav/WavStreamReader.h>

#include <player/OneShotSampleSource.h>
#include "PowerPlayMultiPlayer.h"

static const char *TAG = "PowerPlayJNI";

#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

using namespace iolib;
using namespace parselib;
using namespace oboe;

// Global static player instance.
// For more complex scenarios, consider passing this as a native peer (long) from Java.
static PowerPlayMultiPlayer player;

oboe::PerformanceMode getPerformanceMode(JNIEnv *env, jobject performanceModeObj) {
    if (performanceModeObj == nullptr) {
        LOG_ERROR("performanceModeObj is null in getPerformanceMode");
        return PerformanceMode::None;
    }

    jclass performanceModeClass = env->GetObjectClass(performanceModeObj);
    if (performanceModeClass == nullptr) {
        LOG_ERROR("Failed to get class for performanceModeObj");
        if (env->ExceptionCheck()) {
            env->ExceptionClear(); // Clear it if we are returning a default.
        }
        return PerformanceMode::None;
    }

    jmethodID ordinalMethod = env->GetMethodID(performanceModeClass, "ordinal", "()I");
    env->DeleteLocalRef(performanceModeClass);

    if (ordinalMethod == nullptr) {
        LOG_ERROR("Failed to get 'ordinal' method ID");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return PerformanceMode::None;
    }

    jint ordinal = env->CallIntMethod(performanceModeObj, ordinalMethod);
    if (env->ExceptionCheck()) {
        LOG_ERROR("Exception occurred calling 'ordinal' method.");
        env->ExceptionClear();
        return PerformanceMode::None;
    }

    // Mapping based on Kotlin enum ordinals to Oboe PerformanceMode values
    switch (ordinal) {
        case 0:
            return PerformanceMode::None;
        case 1:
            return PerformanceMode::LowLatency;
        case 2:
            return PerformanceMode::PowerSaving;
        case 3:
            return PerformanceMode::PowerSavingOffloaded;
        default:
            LOG_ERROR("Unknown performance mode ordinal: %d", ordinal);
            return PerformanceMode::None;
    }
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.setupAudioStreamNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_setupAudioStreamNative(
        JNIEnv *env,
        jobject,
        jint channels) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "setupAudioStreamNative()");

    //TODO - Read channel number from file instead of passing it in.
    player.setupAudioStream(channels, oboe::PerformanceMode::None);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.startAudioStreamNative()
 */
JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_startAudioStreamNative(
        JNIEnv *,
        jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "startAudioStreamNative()");
    return (jint) player.startStream();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.teardownAudioStreamNative()
 */
JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_teardownAudioStreamNative(
        JNIEnv *,
        jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", "teardownAudioStreamNative()");
    player.teardownAudioStream();

    //TODO - Actually handle a return here.
    return true;
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.loadAssetNative()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_loadAssetNative(
        JNIEnv *env,
        jobject,
        jbyteArray bytearray,
        jint index) {
    const int32_t len = env->GetArrayLength(bytearray);
    auto *buf = new unsigned char[len];

    env->GetByteArrayRegion(bytearray, 0, len, reinterpret_cast<jbyte *>(buf));

    MemInputStream stream(buf, len);
    WavStreamReader reader(&stream);
    reader.parse();

    auto *sampleBuffer = new SampleBuffer();
    sampleBuffer->loadSampleData(&reader);

    const auto source = new OneShotSampleSource(sampleBuffer, 0);
    player.addSampleSource(source, sampleBuffer);

    delete[] buf;
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.unloadWavAssetsNative()
 */
JNIEXPORT void JNICALL Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_unloadAssetsNative(
        JNIEnv *env,
        jobject) {
    player.unloadSampleData();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.getOutputReset()
 */
JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_getOutputResetNative(
        JNIEnv *,
        jobject) {
    return player.getOutputReset();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.clearOutputReset()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_clearOutputResetNative(
        JNIEnv *,
        jobject) {
    player.clearOutputReset();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.startPlayingNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_startPlayingNative(
        JNIEnv *env, jobject,
        jint index,
        jobject mode) {
    auto performanceMode = getPerformanceMode(env, mode);
    player.triggerDown(index, performanceMode);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.stopPlayingNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_stopPlayingNative(
        JNIEnv *env,
        jobject,
        jint index) {
    player.triggerUp(index);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.updatePerformanceModeNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_updatePerformanceModeNative(
        JNIEnv *env,
        jobject,
        jobject mode) {
    auto performanceMode = getPerformanceMode(env, mode);
    player.updatePerformanceMode(performanceMode);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.setLoopingNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_setLoopingNative(
        JNIEnv *env,
        jobject,
        jint index,
        jboolean looping) {
    player.setLoopMode(index, looping);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.setMMapEnabledNative()
 */
JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_setMMapEnabledNative(
        JNIEnv *env,
        jobject,
        jboolean enable) {
    return PowerPlayMultiPlayer::setMMapEnabled(enable);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.isMMapEnabledNative()
 */
JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_isMMapEnabledNative(
        JNIEnv *env,
        jobject) {
    return PowerPlayMultiPlayer::isMMapEnabled();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.isMMapSupportedNative()
 */
JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_isMMapSupportedNative(
        JNIEnv *env,
        jobject) {
    return PowerPlayMultiPlayer::isMMapSupported();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.setBufferSizeInFramesNative()
 */
JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_setBufferSizeInFramesNative(
        JNIEnv *env,
        jobject,
        jint bufferSizeInFrames) {
    return player.setBufferSizeInFrames(bufferSizeInFrames);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.getBufferSizeInFramesNative()
 */
JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_getBufferCapacityInFramesNative(
        JNIEnv *env,
        jobject) {
    return player.getBufferCapacityInFrames();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.setVolumeNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_setVolumeNative(
        JNIEnv *env,
        jobject,
        jfloat volume) {
    int32_t currentIndex = player.getCurrentlyPlayingIndex();
    if (currentIndex >= 0) {
        player.setGain(currentIndex, volume);
        return;
    }

    // Set gain for all tracks if nothing is playing.
    for (int i = 0; i < 3; ++i) {
        player.setGain(i, volume);
    }
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.isOffloadedNative()
 */
JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_isOffloadedNative(
        JNIEnv *env,
        jobject) {
    return player.isOffloaded();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.getCurrentlyPlayingIndexNative()
 */
JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_getCurrentlyPlayingIndexNative(
        JNIEnv *env,
        jobject) {
    return player.getCurrentlyPlayingIndex();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.getPlaybackPositionMillisNative()
 */
JNIEXPORT jlong JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_getPlaybackPositionMillisNative(
        JNIEnv *env,
        jobject) {
    return (jlong) player.getPlaybackPositionMillis();
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.seekToNative()
 */
JNIEXPORT void JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_seekToNative(
        JNIEnv *env,
        jobject,
        jint positionMillis) {
    player.seekTo(positionMillis);
}

/**
 * Native (JNI) implementation of PowerPlayAudioPlayer.getDurationMillisNative()
 */
JNIEXPORT jlong JNICALL
Java_com_google_oboe_samples_powerplay_engine_PowerPlayAudioPlayer_getDurationMillisNative(
        JNIEnv *env,
        jobject,
        jint index) {
    return (jlong) player.getDurationMillis(index);
}

#ifdef __cplusplus
}
#endif

