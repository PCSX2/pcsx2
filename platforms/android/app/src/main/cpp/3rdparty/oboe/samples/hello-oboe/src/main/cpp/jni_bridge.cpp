/*
 * Copyright 2017 The Android Open Source Project
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
#include <oboe/Oboe.h>
#include "HelloOboeEngine.h"
#include "logging_macros.h"

extern "C" {
/* We only need one HelloOboeEngine and it is needed for the entire time.
 * So just allocate one statically. */
static HelloOboeEngine sEngine;

JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_startEngine(
        JNIEnv *env,
        jclass,
        int audioApi, int deviceId, int channelCount) {
    return static_cast<jint>(sEngine.start((oboe::AudioApi)audioApi, deviceId, channelCount));
}

JNIEXPORT jint JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_stopEngine(
        JNIEnv *env,
        jclass) {
    return static_cast<jint>(sEngine.stop());
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_setToneOn(
        JNIEnv *env,
        jclass,
        jboolean isToneOn) {

    sEngine.tap(isToneOn);
}


JNIEXPORT void JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_setBufferSizeInBursts(
        JNIEnv *env,
        jclass,
        jint bufferSizeInBursts) {
    sEngine.setBufferSizeInBursts(bufferSizeInBursts);
}

JNIEXPORT jdouble JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_getCurrentOutputLatencyMillis(
        JNIEnv *env,
        jclass) {
    return static_cast<jdouble>(sEngine.getCurrentOutputLatencyMillis());
}

JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_isLatencyDetectionSupported(
        JNIEnv *env,
        jclass) {
    return (sEngine.isLatencyDetectionSupported() ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT jboolean JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_isAAudioRecommended(
        JNIEnv *env,
        jclass) {
    return (sEngine.isAAudioRecommended() ? JNI_TRUE : JNI_FALSE);
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_hellooboe_PlaybackEngine_setDefaultStreamValues(
        JNIEnv *env,
        jclass,
        jint sampleRate,
        jint framesPerBurst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
}
} // extern "C"
