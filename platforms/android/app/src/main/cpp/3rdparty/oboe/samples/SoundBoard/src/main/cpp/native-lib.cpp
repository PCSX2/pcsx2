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

#include <jni.h>
#include <string>
#include <vector>

#include "SoundBoardEngine.h"

extern "C" {
/**
 * Start the audio engine
 *
 * @param env
 * @param instance
 * @param jCpuIds - CPU core IDs which the audio process should affine to
 * @return a pointer to the audio engine. This should be passed to other methods
 */
JNIEXPORT jlong JNICALL
Java_com_google_oboe_samples_soundboard_MainActivity_startEngine(JNIEnv *env, jobject /*unused*/,
         jint jNumSignals) {
    LOGD("numSignals : %d", static_cast<int>(jNumSignals));
    SoundBoardEngine  *engine = new SoundBoardEngine(jNumSignals);

    if (!engine->start()) {
        LOGE("Failed to start SoundBoard Engine");
        delete engine;
        engine = nullptr;
    } else  {
        LOGD("Engine Started");
    }
    return reinterpret_cast<jlong>(engine);
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_soundboard_MainActivity_stopEngine(JNIEnv *env, jobject instance,
        jlong jEngineHandle) {
    auto engine = reinterpret_cast<SoundBoardEngine*>(jEngineHandle);
    if (engine) {
        engine->stop();
        delete engine;
    } else {
        LOGD("Engine invalid, call startEngine() to create");
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_soundboard_MainActivity_native_1setDefaultStreamValues(JNIEnv *env,
                                                                            jclass type,
                                                                            jint sampleRate,
                                                                            jint framesPerBurst) {
    oboe::DefaultStreamValues::SampleRate = (int32_t) sampleRate;
    oboe::DefaultStreamValues::FramesPerBurst = (int32_t) framesPerBurst;
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_soundboard_NoteListener_noteOff(JNIEnv *env, jobject thiz,
                                                         jlong engine_handle, jint noteIndex) {
    auto *engine = reinterpret_cast<SoundBoardEngine*>(engine_handle);
    if (engine) {
        engine->noteOff(noteIndex);
    } else {
        LOGE("Engine handle is invalid, call createEngine() to create a new one");
    }
}

JNIEXPORT void JNICALL
Java_com_google_oboe_samples_soundboard_NoteListener_noteOn(JNIEnv *env, jobject thiz,
                                                         jlong engine_handle, jint noteIndex) {
    auto *engine = reinterpret_cast<SoundBoardEngine*>(engine_handle);
    if (engine) {
        engine->noteOn(noteIndex);
    } else {
        LOGE("Engine handle is invalid, call createEngine() to create a new one");
    }
}

} // extern "C"
