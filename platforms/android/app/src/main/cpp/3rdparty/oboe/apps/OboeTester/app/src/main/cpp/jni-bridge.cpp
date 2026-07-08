/*
 * Copyright 2015 The Android Open Source Project
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

#define MODULE_NAME "OboeTester"

#include <cassert>
#include <cstring>
#include <jni.h>
#include <memory>
#include <stdint.h>
#include <sys/sysinfo.h>
#include <thread>

#include "common/AdpfWrapper.h"
#include "common/OboeDebug.h"
#include "oboe/Oboe.h"

#include "NativeAudioContext.h"
#include "TestColdStartLatency.h"
#include "TestErrorCallback.h"
#include "TestRoutingCrash.h"
#include "TestRapidCycle.h"
#include "cpu/AudioWorkloadTest.h"
#include "cpu/AudioWorkloadTestRunner.h"
#include "ReverseJniEngine.h"

static NativeAudioContext engine;

/*********************************************************************************/
/**********************  JNI  Prototypes *****************************************/
/*********************************************************************************/
extern "C" {

// --- Cached JNI IDs ---
static jclass g_callbackStatusClass = nullptr;
static jmethodID g_callbackStatusConstructor = nullptr;

static jclass g_arrayListClass = nullptr;
static jmethodID g_arrayListConstructor = nullptr;
static jmethodID g_arrayListAddMethod = nullptr;

static jclass g_playbackParametersClass = nullptr;
static jmethodID g_playbackParametersConstructor = nullptr;
static jfieldID g_fallbackModeField = nullptr;
static jfieldID g_stretchModeField = nullptr;
static jfieldID g_pitchField = nullptr;
static jfieldID g_speedField = nullptr;

static jclass g_recordingStatsClass = nullptr;
static jmethodID g_recordingStatsConstructor = nullptr;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_openNative(JNIEnv *env, jobject,
                                                       jint nativeApi,
                                                       jint sampleRate,
                                                       jint channelCount,
                                                       jint channelMask,
                                                       jint format,
                                                       jint sharingMode,
                                                       jint performanceMode,
                                                       jint inputPreset,
                                                       jint usage,
                                                       jint contentType,
                                                       jint bufferCapacityInFrames,
                                                       jint deviceId,
                                                       jint sessionId,
                                                       jboolean channelConversionAllowed,
                                                       jboolean formatConversionAllowed,
                                                       jint rateConversionQuality,
                                                       jboolean isMMap,
                                                       jboolean isInput,
                                                       jint spatializationBehavior,
                                                       jstring packageName,
                                                       jstring attributionTag);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_close(JNIEnv *env, jobject, jint);

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setThresholdInFrames(JNIEnv *env, jobject, jint, jint);
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getThresholdInFrames(JNIEnv *env, jobject, jint);
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getBufferCapacityInFrames(JNIEnv *env, jobject, jint);
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setNativeApi(JNIEnv *env, jobject, jint, jint);

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setUseCallback(JNIEnv *env, jclass type,
                                                                      jboolean useCallback);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setCallbackReturnStop(JNIEnv *env,
                                                                             jclass type,
                                                                             jboolean b);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setCallbackSize(JNIEnv *env, jclass type,
                                                            jint callbackSize);

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setUsePartialCallbackNative(
        JNIEnv *env, jclass type, jboolean usePartialCallback);

// ================= OboeAudioOutputStream ================================

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_trigger(JNIEnv *env, jobject);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setToneType(JNIEnv *env, jobject, jint);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setAmplitude(JNIEnv *env, jobject, jfloat);
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setDuck(JNIEnv *env, jobject, jboolean);

/*********************************************************************************/
/**********************  JNI Implementations *************************************/
/*********************************************************************************/

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_NativeEngine_isMMapSupported(JNIEnv *env, jclass type) {
    return oboe::AAudioExtensions::getInstance().isMMapSupported();
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_NativeEngine_isMMapExclusiveSupported(JNIEnv *env, jclass type) {
    return oboe::AAudioExtensions::getInstance().isMMapExclusiveSupported();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_NativeEngine_setWorkaroundsEnabled(JNIEnv *env, jclass type,
                                                                          jboolean enabled) {
    oboe::OboeGlobals::setWorkaroundsEnabled(enabled);
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_NativeEngine_areWorkaroundsEnabled(JNIEnv *env,
        jclass type) {
    return oboe::OboeGlobals::areWorkaroundsEnabled();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_NativeEngine_getCpuCount(JNIEnv *env, jclass type) {
    return sysconf(_SC_NPROCESSORS_CONF);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_NativeEngine_setCpuAffinityMask(JNIEnv *env,
                                                                     jclass type,
                                                                     jint mask) {
    engine.getCurrentActivity()->setCpuAffinityMask(mask);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_NativeEngine_setWorkloadReportingEnabled(JNIEnv *env,
                                                             jclass type,
                                                             jboolean enabled) {
    engine.getCurrentActivity()->setWorkloadReportingEnabled(enabled);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_NativeEngine_setNotifyWorkloadIncreaseEnabled(JNIEnv *env,
                                                                      jclass type,
                                                                      jboolean enabled) {
    engine.getCurrentActivity()->setNotifyWorkloadIncreaseEnabled(enabled);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_openNative(
        JNIEnv *env, jobject synth,
        jint nativeApi,
        jint sampleRate,
        jint channelCount,
        jint channelMask,
        jint format,
        jint sharingMode,
        jint performanceMode,
        jint inputPreset,
        jint usage,
        jint contentType,
        jint bufferCapacityInFrames,
        jint deviceId,
        jint sessionId,
        jboolean channelConversionAllowed,
        jboolean formatConversionAllowed,
        jint rateConversionQuality,
        jboolean isMMap,
        jboolean isInput,
        jint spatializationBehavior,
        jstring packageName,
        jstring attributionTag) {
    LOGD("OboeAudioStream_openNative: sampleRate = %d", sampleRate);

    const char *packageNameStr = env->GetStringUTFChars(packageName, nullptr);
    const char *attributionTagStr = env->GetStringUTFChars(attributionTag, nullptr);

    int result = engine.getCurrentActivity()->open(nativeApi,
                                      sampleRate,
                                      channelCount,
                                      channelMask,
                                      format,
                                      sharingMode,
                                      performanceMode,
                                      inputPreset,
                                      usage,
                                      contentType,
                                      bufferCapacityInFrames,
                                      deviceId,
                                      sessionId,
                                      channelConversionAllowed,
                                      formatConversionAllowed,
                                      rateConversionQuality,
                                      isMMap,
                                      isInput,
                                      spatializationBehavior,
                                      packageNameStr,
                                      attributionTagStr);

    env->ReleaseStringUTFChars(packageName, packageNameStr);
    env->ReleaseStringUTFChars(attributionTag, attributionTagStr);

    return (jint) result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_startNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->start();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_pauseNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->pause();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_flushNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->flush();
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_flushFromFrameNative(
        JNIEnv * /*env*/, jobject, jint accuracy, jlong frames) {
    return (jlong) engine.getCurrentActivity()->flushFromFrame(
            accuracy, static_cast<int64_t>(frames));
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_stopNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->stop();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_releaseNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->release();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_getFramesPerCallback(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->getFramesPerCallback();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setupMemoryBuffer(JNIEnv *env, jobject thiz,
                                                                 jbyteArray buffer, jint offset,
                                                                 jint length) {
    auto buf = std::make_unique<uint8_t[]>(length);

    env->GetByteArrayRegion(buffer, offset, length, reinterpret_cast<jbyte *>(buf.get()));
    engine.getCurrentActivity()->setupMemoryBuffer(buf, length);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setPlaybackParametersNative(
        JNIEnv *env, jobject, jobject playbackParameters) {
    oboe::PlaybackParameters params{};
    params.fallbackMode = static_cast<oboe::FallbackMode>(
            env->GetIntField(playbackParameters, g_fallbackModeField));
    params.stretchMode = static_cast<oboe::StretchMode>(
            env->GetIntField(playbackParameters, g_stretchModeField));
    params.pitch = static_cast<float>(env->GetFloatField(playbackParameters, g_pitchField));
    params.speed = static_cast<float>(env->GetFloatField(playbackParameters, g_speedField));

    return static_cast<jint>(engine.getCurrentActivity()->setPlaybackParameters(params));
}

JNIEXPORT jobject JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_getPlaybackParametersNative(
        JNIEnv *env, jobject) {
    oboe::ResultWithValue<oboe::PlaybackParameters> result =
            engine.getCurrentActivity()->getPlaybackParameters();
    if (!result) {
        return nullptr;
    }
    oboe::PlaybackParameters params = result.value();

    return env->NewObject(g_playbackParametersClass, g_playbackParametersConstructor,
                          (jint)params.fallbackMode,
                          (jint)params.stretchMode, params.pitch, params.speed);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_startPlaybackNative(JNIEnv *env, jobject) {
    return (jint) engine.getCurrentActivity()->startPlayback();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_close(JNIEnv *env, jobject, jint streamIndex) {
    engine.getCurrentActivity()->close(streamIndex);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setUseAlternativeAdpf(JNIEnv *env, jobject, jboolean enabled) {
    oboe::AdpfWrapper::setUseAlternative(enabled);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setBufferSizeInFrames(
        JNIEnv *env, jobject, jint streamIndex, jint threshold) {
    return (jint) engine.getCurrentActivity()->setBufferSizeInFrames(streamIndex, threshold);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getBufferSizeInFrames(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getBufferSizeInFrames();
    }
    return result;
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setPartialCallbackPercentage(
        JNIEnv * /*env*/, jobject /*thiz*/, jint percentage) {
    engine.getCurrentActivity()->setPartialCallbackPercentage(percentage);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setPerformanceHintEnabled(
        JNIEnv *env, jobject, jint streamIndex, jboolean enabled) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        oboeStream->setPerformanceHintEnabled(enabled);
    }
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getBufferCapacityInFrames(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getBufferCapacityInFrames();
    }
    return result;
}

static int convertAudioApiToNativeApi(oboe::AudioApi audioApi) {
    switch(audioApi) {
        case oboe::AudioApi::Unspecified:
            return NATIVE_MODE_UNSPECIFIED;
        case oboe::AudioApi::OpenSLES:
            return NATIVE_MODE_OPENSLES;
        case oboe::AudioApi::AAudio:
            return NATIVE_MODE_AAUDIO;
        default:
            return -1;
    }
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getNativeApi(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        oboe::AudioApi audioApi = oboeStream->getAudioApi();
        result = convertAudioApiToNativeApi(audioApi);
        LOGD("OboeAudioStream_getNativeApi got %d", result);
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getSampleRate(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getSampleRate();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getSharingMode(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getSharingMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getPerformanceMode(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getPerformanceMode();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getInputPreset(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getInputPreset();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getSpatializationBehavior(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getSpatializationBehavior();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getFramesPerBurst(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesPerBurst();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getChannelCount(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getChannelCount();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getChannelMask(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getChannelMask();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getFormat(JNIEnv *env, jobject instance, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getFormat();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getHardwareChannelCount(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getHardwareChannelCount();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getHardwareFormat(JNIEnv *env, jobject instance, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getHardwareFormat();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getHardwareSampleRate(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getHardwareSampleRate();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getUsage(JNIEnv *env, jobject instance, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getUsage();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getContentType(JNIEnv *env, jobject instance, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = (jint) oboeStream->getContentType();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getDeviceId(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getDeviceId();
    }
    return result;
}

JNIEXPORT jintArray JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getDeviceIds(
        JNIEnv *env, jobject, jint streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        std::vector<int32_t> deviceIds = oboeStream->getDeviceIds();
        jsize length = deviceIds.size();
        jintArray result = env->NewIntArray(length);

        if (result == nullptr) {
            return nullptr;
        }

        if (length > 0) {
            env->SetIntArrayRegion(result, 0, length,
                                   reinterpret_cast<jint*>(deviceIds.data()));
        }

        return result;
    }
    return nullptr;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getSessionId(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getSessionId();
    }
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getPackageName(
        JNIEnv *env, jobject, jint streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        return env->NewStringUTF(oboeStream->getPackageName().c_str());
    }
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getAttributionTag(
        JNIEnv *env, jobject, jint streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        return env->NewStringUTF(oboeStream->getAttributionTag().c_str());
    }
    return env->NewStringUTF("");
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getFramesWritten(
        JNIEnv *env, jobject, jint streamIndex) {
    jlong result = (jint) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesWritten();
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getFramesRead(
        JNIEnv *env, jobject, jint streamIndex) {
    jlong result = (jlong) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        result = oboeStream->getFramesRead();
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getXRunCount(
        JNIEnv *env, jobject, jint streamIndex) {
    jint result = (jlong) oboe::Result::ErrorNull;
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto oboeResult  = oboeStream->getXRunCount();
        if (!oboeResult) {
            result = (jint) oboeResult.error();
        } else {
            result = oboeResult.value();
        }
    }
    return result;
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getCallbackCount(
        JNIEnv *env, jobject) {
    return engine.getCurrentActivity()->getCallbackCount();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getLastErrorCallbackResult(
        JNIEnv *env, jobject, jint streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        return (jint) oboeStream->getLastErrorCallbackResult();
    }
    return 0;
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getTimestampLatency(JNIEnv *env,
        jobject instance,
        jint streamIndex) {
    return engine.getCurrentActivity()->getTimestampLatency(streamIndex);
}

JNIEXPORT jfloat JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getCpuLoad(JNIEnv *env, jobject instance, jint streamIndex) {
    return engine.getCurrentActivity()->getCpuLoad();
}

JNIEXPORT jfloat JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getAndResetMaxCpuLoad(JNIEnv *env, jobject instance, jint streamIndex) {
    return engine.getCurrentActivity()->getAndResetMaxCpuLoad();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getAndResetCpuMask(JNIEnv *env, jobject instance, jint streamIndex) {
    return (jint) engine.getCurrentActivity()->getAndResetCpuMask();
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getCallbackTimeString(JNIEnv *env, jobject instance) {
    return env->NewStringUTF(engine.getCurrentActivity()->getCallbackTimeString().c_str());
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setWorkload(
        JNIEnv *env, jobject, jint workload) {
    engine.getCurrentActivity()->setWorkload(workload);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setHearWorkload(
        JNIEnv *env, jobject, jint streamIndex, jboolean enabled) {
    engine.getCurrentActivity()->setHearWorkload(enabled);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getState(JNIEnv *env, jobject instance, jint streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = engine.getCurrentActivity()->getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto state = oboeStream->getState();
        if (state != oboe::StreamState::Starting && state != oboe::StreamState::Started
                && state != oboe::StreamState::Disconnected) {
            oboe::Result result = oboeStream->waitForStateChange(
                    oboe::StreamState::Uninitialized,
                    &state, 0);

            if (result != oboe::Result::OK){
                if (result == oboe::Result::ErrorClosed) {
                    state = oboe::StreamState::Closed;
                } else if (result == oboe::Result::ErrorDisconnected){
                    state = oboe::StreamState::Disconnected;
                } else {
                    state = oboe::StreamState::Unknown;
                }
            }
        }
        return (jint) state;
    }
    return -1;
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_AudioInputTester_getPeakLevel(JNIEnv *env,
                                                          jobject instance,
                                                          jint index) {
    return engine.getCurrentActivity()->getPeakLevel(index);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setUseCallback(JNIEnv *env, jclass type,
                                                                      jboolean useCallback) {
    ActivityContext::mUseCallback = useCallback;
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setCallbackReturnStop(JNIEnv *env, jclass type,
                                                                      jboolean b) {
    OboeStreamCallbackProxy::setCallbackReturnStop(b);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setHangTimeMillis(JNIEnv *env, jclass type,
                                                                   jint hangTimeMillis) {
    OboeTesterStreamCallback::setHangTimeMillis(hangTimeMillis);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setCallbackSize(JNIEnv *env, jclass type,
                                                            jint callbackSize) {
    ActivityContext::callbackSize = callbackSize;
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_setUsePartialDataCallbackNative(
        JNIEnv *env, jclass type, jboolean usePartialDataCallback) {
    ActivityContext::mUsePartialDataCallback = usePartialDataCallback;
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_isMMap(JNIEnv *env, jobject instance, jint streamIndex) {
    return engine.getCurrentActivity()->isMMapUsed(streamIndex);
}

// ================= OboeAudioOutputStream ================================

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_trigger(
        JNIEnv *env, jobject) {
    engine.getCurrentActivity()->trigger();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setChannelEnabled(
        JNIEnv *env, jobject, jint channelIndex, jboolean enabled) {
    engine.getCurrentActivity()->setChannelEnabled(channelIndex, enabled);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setSignalType(
        JNIEnv *env, jobject, jint signalType) {
    engine.getCurrentActivity()->setSignalType(signalType);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_OboeAudioOutputStream_setAmplitude(JNIEnv *env, jobject, jfloat amplitude) {
    engine.getCurrentActivity()->setAmplitude(amplitude);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_OboeAudioStream_getOboeVersionNumber(JNIEnv *env,
                                                                          jclass type) {
    return OBOE_VERSION_NUMBER;
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setActivityType(JNIEnv *env,
                                                                         jobject instance,
                                                                         jint activityType) {
    engine.setActivityType(activityType);
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestInputActivity_saveWaveFile(JNIEnv *env,
                                                                        jobject instance,
                                                                        jstring fileName) {
    const char *str = env->GetStringUTFChars(fileName, nullptr);
    LOGD("nativeSaveFile(%s)", str);
    jint result = engine.getCurrentActivity()->saveWaveFile(str);
    env->ReleaseStringUTFChars(fileName, str);
    return result;
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestInputActivity_setMinimumFramesBeforeRead(JNIEnv *env,
                                                                      jobject instance,
                                                                      jint numFrames) {
    engine.getCurrentActivity()->setMinimumFramesBeforeRead(numFrames);
}

// ==========================================================================
JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_EchoActivity_setDelayTime(JNIEnv *env,
                                                                         jobject instance,
                                                                         jdouble delayTimeSeconds) {
    engine.setDelayTime(delayTimeSeconds);
}

JNIEXPORT int JNICALL
Java_com_mobileer_oboetester_EchoActivity_getColdStartInputMillis(JNIEnv *env,
        jobject instance) {
    return engine.getCurrentActivity()->getColdStartInputMillis();
}

JNIEXPORT int JNICALL
Java_com_mobileer_oboetester_EchoActivity_getColdStartOutputMillis(JNIEnv *env,
                                                                            jobject instance) {
    return engine.getCurrentActivity()->getColdStartOutputMillis();
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getAnalyzerProgress(JNIEnv *env,
                                                                                    jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getProgress();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getMeasuredLatency(JNIEnv *env,
                                                                                   jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getMeasuredLatency();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getMeasuredConfidence(JNIEnv *env,
                                                                            jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getMeasuredConfidence();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getMeasuredCorrelation(JNIEnv *env,
                                                                            jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getMeasuredCorrelation();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_measureTimestampLatency(JNIEnv *env,
                                                                            jobject instance) {
    return engine.mActivityRoundTripLatency.measureTimestampLatency();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getBackgroundRMS(JNIEnv *env,
                                                                                 jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getBackgroundRMS();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_RoundTripLatencyActivity_getSignalRMS(JNIEnv *env,
                                                                                 jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getSignalRMS();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AnalyzerActivity_getMeasuredResult(JNIEnv *env,
                                                                          jobject instance) {
    return engine.mActivityRoundTripLatency.getLatencyAnalyzer()->getResult();
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AnalyzerActivity_getAnalyzerState(JNIEnv *env,
                                                                         jobject instance) {
    return ((ActivityFullDuplex *)engine.getCurrentActivity())->getState();
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_AnalyzerActivity_isAnalyzerDone(JNIEnv *env,
                                                                       jobject instance) {
    return ((ActivityFullDuplex *)engine.getCurrentActivity())->isAnalyzerDone();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AnalyzerActivity_getResetCount(JNIEnv *env,
                                                                          jobject instance) {
    auto activity = (ActivityFullDuplex *)engine.getCurrentActivity();
    if (activity == nullptr) {
        return -1;
    }
    return activity->getResetCount();
}

// ==========================================================================
JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getGlitchCount(JNIEnv *env,
                                                           jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getGlitchCount();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getGlitchLength(JNIEnv *env,
                                                           jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getGlitchLength();
}

JNIEXPORT double JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getPhase(JNIEnv *env,
                                                           jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getPhaseOffset();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getStateFrameCount(JNIEnv *env,
                                                                     jobject instance,
                                                                     jint state) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getStateFrameCount(state);
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getSignalToNoiseDB(JNIEnv *env,
                                                                         jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getSignalToNoiseDB();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getPeakAmplitude(JNIEnv *env,
                                                                       jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getPeakAmplitude();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getSineAmplitude(JNIEnv *env,
                                                             jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getSineAmplitude();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_GlitchActivity_getSinePeriod(JNIEnv *env,
                                                             jobject instance) {
    return engine.mActivityGlitches.getGlitchAnalyzer()->getSinePeriod();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getMagnitude(JNIEnv *env,
                                                                          jobject instance) {
    return engine.mActivityDataPath.getDataPathAnalyzer()->getMagnitude();
}

JNIEXPORT jdouble JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getMaxMagnitude(JNIEnv *env,
                                                                          jobject instance) {
    return engine.mActivityDataPath.getDataPathAnalyzer()->getMaxMagnitude();
}

JNIEXPORT double JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getPhaseDataPaths(JNIEnv *env,
                                                     jobject instance) {
    return engine.mActivityDataPath.getDataPathAnalyzer()->getPhaseOffset();
}

JNIEXPORT double JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getAveragePhaseError(
    JNIEnv* env, jobject instance) {
  return engine.mActivityDataPath.getDataPathAnalyzer()->getAveragePhaseError();
}

JNIEXPORT bool JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_isPhaseJitterValid(
    JNIEnv* env, jobject instance) {
  return engine.mActivityDataPath.getDataPathAnalyzer()->isPhaseJitterValid();
}

JNIEXPORT int JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getPhaseCount(JNIEnv *env,
                                                     jobject instance) {
    return engine.mActivityDataPath.getDataPathAnalyzer()->getPhaseCount();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_setSignalType(JNIEnv *env,
                                                                          jobject instance,
                                                                          jint signalType) {
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        engine.mActivityDataPath.getDataPathAnalyzer()->setSignalType(signalType);
    }
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getFrequencyResponse(JNIEnv *env,
                                                                          jobject instance) {
    std::string report = "";
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        report = engine.mActivityDataPath.getDataPathAnalyzer()->getFrequencyResponse();
    }
    return env->NewStringUTF(report.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getDistortionReport(JNIEnv *env,
                                                                          jobject instance) {
    std::string report = "";
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        report = engine.mActivityDataPath.getDataPathAnalyzer()->getDistortionReport();
    }
    return env->NewStringUTF(report.c_str());
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestDataPathsActivity_getAnalysisResult(JNIEnv *env,
                                                                          jobject instance) {
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        return engine.mActivityDataPath.getDataPathAnalyzer()->getAnalysisResult();
    }
    return 0;
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_GlitchActivity_setTolerance(JNIEnv *env,
                                                                   jobject instance,
                                                                   jfloat tolerance) {
    if (engine.mActivityGlitches.getGlitchAnalyzer()) {
        engine.mActivityGlitches.getGlitchAnalyzer()->setTolerance(tolerance);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_GlitchActivity_setForcedGlitchDuration(JNIEnv *env,
                                                                  jobject instance,
                                                                  jint frames) {
    if (engine.mActivityGlitches.getGlitchAnalyzer()) {
        engine.mActivityGlitches.getGlitchAnalyzer()->setForcedGlitchDuration(frames);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_GlitchActivity_setInputChannelNative(JNIEnv *env,
                                                                   jobject instance,
                                                                   jint channel) {
    if (engine.mActivityGlitches.getGlitchAnalyzer()) {
        engine.mActivityGlitches.getGlitchAnalyzer()->setInputChannel(channel);
    }
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        engine.mActivityDataPath.getDataPathAnalyzer()->setInputChannel(channel);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_GlitchActivity_setOutputChannelNative(JNIEnv *env,
                                                                       jobject instance,
                                                                       jint channel) {
    if (engine.mActivityGlitches.getGlitchAnalyzer()) {
        engine.mActivityGlitches.getGlitchAnalyzer()->setOutputChannel(channel);
    }
    if (engine.mActivityDataPath.getDataPathAnalyzer()) {
        engine.mActivityDataPath.getDataPathAnalyzer()->setOutputChannel(channel);
    }
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_ManualGlitchActivity_getGlitch(JNIEnv *env, jobject instance,
                                                                      jfloatArray waveform_) {
    float *waveform = env->GetFloatArrayElements(waveform_, nullptr);
    jsize length = env->GetArrayLength(waveform_);
    jsize numSamples = 0;
    auto *analyzer = engine.mActivityGlitches.getGlitchAnalyzer();
    if (analyzer) {
        numSamples = analyzer->getLastGlitch(waveform, length);
    }

    env->ReleaseFloatArrayElements(waveform_, waveform, 0);
    return numSamples;
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_ManualGlitchActivity_getRecentSamples(JNIEnv *env, jobject instance,
                                                            jfloatArray waveform_) {
    float *waveform = env->GetFloatArrayElements(waveform_, nullptr);
    jsize length = env->GetArrayLength(waveform_);
    jsize numSamples = 0;
    auto *analyzer = engine.mActivityGlitches.getGlitchAnalyzer();
    if (analyzer) {
        numSamples = analyzer->getRecentSamples(waveform, length);
    }

    env->ReleaseFloatArrayElements(waveform_, waveform, 0);
    return numSamples;
}

JNIEXPORT jobject JNICALL
Java_com_mobileer_oboetester_RecorderActivity_getRecordingStatsJni(JNIEnv *env, jobject) {

    ActivityRecording::RecordingStats params = engine.mActivityRecording.getRecordingStats();

    return env->NewObject(g_recordingStatsClass, g_recordingStatsConstructor,
                          (jfloat)params.peakAbs,
                          (params.n > 0) ? (float)std::sqrt(params.sumSq / (float)params.n) : 0.0F);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setDefaultAudioValues(JNIEnv *env, jclass clazz,
                                                                     jint audio_manager_sample_rate,
                                                                     jint audio_manager_frames_per_burst) {
    oboe::DefaultStreamValues::SampleRate = audio_manager_sample_rate;
    oboe::DefaultStreamValues::FramesPerBurst = audio_manager_frames_per_burst;
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestAudioActivity_setDuck(JNIEnv *env, jobject, jboolean isDucked) {
    engine.getCurrentActivity()->setDuck(isDucked);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TapToToneActivity_useNoisePulse(JNIEnv *env,
                                                             jclass clazz,
                                                             jboolean enabled) {
    engine.mActivityTapToTone.useNoisePulse(enabled);
}

static TestErrorCallback sErrorCallbackTester;

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestErrorCallbackActivity_testDeleteCrash(
        JNIEnv *env, jobject instance) {
    sErrorCallbackTester.test();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestErrorCallbackActivity_getCallbackMagic(
        JNIEnv *env, jobject instance) {
    return sErrorCallbackTester.getCallbackMagic();
}

static TestRoutingCrash sRoutingCrash;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRouteDuringCallbackActivity_startStream(
        JNIEnv *env, jobject instance,
        jboolean useInput) {
    return sRoutingCrash.start(useInput);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRouteDuringCallbackActivity_stopStream(
        JNIEnv *env, jobject instance) {
    return sRoutingCrash.stop();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRouteDuringCallbackActivity_getSleepTimeMicros(
        JNIEnv *env, jobject instance) {
    return sRoutingCrash.getSleepTimeMicros();
}

static TestColdStartLatency sColdStartLatency;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_openStream(
        JNIEnv *env, jobject instance,
        jboolean useInput, jboolean useLowLatency, jboolean useMmap, jboolean useExclusive) {
    return sColdStartLatency.open(useInput, useLowLatency, useMmap, useExclusive);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_startStream(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.start();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_closeStream(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.close();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_getOpenTimeMicros(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.getOpenTimeMicros();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_getStartTimeMicros(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.getStartTimeMicros();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_getColdStartTimeMicros(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.getColdStartTimeMicros();
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_waitForValidTimestamp(
        JNIEnv *env, jobject instance) {
    sColdStartLatency.waitForValidTimestamp();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestColdStartLatencyActivity_getAudioDeviceId(
        JNIEnv *env, jobject instance) {
    return sColdStartLatency.getDeviceId();
}

static TestRapidCycle sRapidCycle;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRapidCycleActivity_startRapidCycleTest(JNIEnv *env, jobject thiz,
                                                                        jboolean use_open_sl) {
    return sRapidCycle.start(use_open_sl);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRapidCycleActivity_stopRapidCycleTest(JNIEnv *env, jobject thiz) {
    return sRapidCycle.stop();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestRapidCycleActivity_getCycleCount(JNIEnv *env, jobject thiz) {
    return sRapidCycle.getCycleCount();
}

static AudioWorkloadTest sAudioWorkload;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_open(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.open();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getFramesPerBurst(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getFramesPerBurst();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getSampleRate(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getSampleRate();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getBufferSizeInFrames(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getBufferSizeInFrames();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_start(JNIEnv *env, jobject thiz,
        jint targetDurationMs, jint numBursts, jint numVoices, jint numAlternateVoices,
        jint alternatingPeriodMs, jboolean adpfEnabled, jboolean adpfWorkloadIncreaseEnabled,
        jboolean hearWorkload) {
    return sAudioWorkload.start(targetDurationMs, numBursts, numVoices,
                                numAlternateVoices, alternatingPeriodMs, adpfEnabled,
                                adpfWorkloadIncreaseEnabled, hearWorkload);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getCpuCount(JNIEnv *env, jobject thiz) {
    return AudioWorkloadTest::getCpuCount();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_setCpuAffinityForCallback(JNIEnv *env, jobject thiz,
                                                                                 jint mask) {
    return AudioWorkloadTest::setCpuAffinityForCallback(mask);
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getXRunCount(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getXRunCount();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getCallbackCount(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getCallbackCount();
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getLastDurationNs(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.getLastDurationNs();
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_isRunning(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.isRunning();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_stop(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.stop();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_close(JNIEnv *env, jobject thiz) {
    return sAudioWorkload.close();
}

// Store the JavaVM pointer to get JNIEnv in JNI_OnLoad/OnUnload
static JavaVM* g_javaVM = nullptr;

// Cache jni classes and methods for getCallbackStatistics
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_javaVM = vm; // Cache the JavaVM pointer
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JNI_OnLoad: Failed to get JNIEnv.");
        return JNI_ERR;
    }

    const char* callbackStatusClassName =
            "com/mobileer/oboetester/AudioWorkloadTestActivity$CallbackStatus";
    jclass localCallbackStatusClass = env->FindClass(callbackStatusClassName);
    if (localCallbackStatusClass == nullptr) {
        LOGE("JNI_OnLoad: Could not find class %s", callbackStatusClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }
    // Create a global reference for the class
    g_callbackStatusClass = (jclass)env->NewGlobalRef(localCallbackStatusClass);
    env->DeleteLocalRef(localCallbackStatusClass); // Clean up the local reference
    if (g_callbackStatusClass == nullptr) {
        LOGE("JNI_OnLoad: Could not create global ref for %s", callbackStatusClassName);
        return JNI_ERR;
    }

    g_callbackStatusConstructor = env->GetMethodID(g_callbackStatusClass, "<init>", "(IJJII)V");
    if (g_callbackStatusConstructor == nullptr) {
        LOGE("JNI_OnLoad: Could not find constructor for %s", callbackStatusClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }

    const char* arrayListClassName = "java/util/ArrayList";
    jclass localArrayListClass = env->FindClass(arrayListClassName);
    if (localArrayListClass == nullptr) {
        LOGE("JNI_OnLoad: Could not find class %s", arrayListClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }
    g_arrayListClass = (jclass)env->NewGlobalRef(localArrayListClass);
    env->DeleteLocalRef(localArrayListClass); // Clean up local reference
    if (g_arrayListClass == nullptr) {
        LOGE("JNI_OnLoad: Could not create global ref for %s", arrayListClassName);
        return JNI_ERR;
    }

    g_arrayListConstructor = env->GetMethodID(g_arrayListClass, "<init>", "()V");
    if (g_arrayListConstructor == nullptr) {
        LOGE("JNI_OnLoad: Could not find constructor for %s", arrayListClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }

    g_arrayListAddMethod = env->GetMethodID(g_arrayListClass, "add", "(Ljava/lang/Object;)Z");
    if (g_arrayListAddMethod == nullptr) {
        LOGE("JNI_OnLoad: Could not find 'add' method for %s", arrayListClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }

    const char* playbackParametersClassName = "com/mobileer/oboetester/PlaybackParameters";
    jclass localPlaybackParametersClass = env->FindClass(playbackParametersClassName);
    if (localPlaybackParametersClass == nullptr) {
        LOGE("JNI_OnLoad: Could not find class %s", playbackParametersClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }
    g_playbackParametersClass = (jclass)env->NewGlobalRef(localPlaybackParametersClass);
    env->DeleteLocalRef(localPlaybackParametersClass);
    if (g_playbackParametersClass == nullptr) {
        LOGE("JNI_OnLoad: Could not create global ref for %s", playbackParametersClassName);
        return JNI_ERR;
    }

    g_playbackParametersConstructor = env->GetMethodID(
            g_playbackParametersClass, "<init>", "(IIFF)V");
    if (g_playbackParametersConstructor == nullptr) {
        LOGE("JNI_OnLoad: Could not find constructor for %s", playbackParametersClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }

    const char* recordingStatsClassName = "com/mobileer/oboetester/RecordingStats";
    jclass localRecordingStatsClass = env->FindClass(recordingStatsClassName);
    if (localRecordingStatsClass == nullptr) {
        LOGE("JNI_OnLoad: Could not find class %s", recordingStatsClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }
    g_recordingStatsClass = (jclass)env->NewGlobalRef(localRecordingStatsClass);
    env->DeleteLocalRef(localRecordingStatsClass);
    if (g_recordingStatsClass == nullptr) {
        LOGE("JNI_OnLoad: Could not create global ref for %s", recordingStatsClassName);
        return JNI_ERR;
    }

    g_recordingStatsConstructor = env->GetMethodID(
            g_recordingStatsClass, "<init>", "(FF)V");
    if (g_recordingStatsConstructor == nullptr) {
        LOGE("JNI_OnLoad: Could not find constructor for %s", recordingStatsClassName);
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return JNI_ERR;
    }

    g_fallbackModeField = env->GetFieldID(g_playbackParametersClass, "mFallbackMode", "I");
    g_stretchModeField = env->GetFieldID(g_playbackParametersClass, "mStretchMode", "I");
    g_pitchField = env->GetFieldID(g_playbackParametersClass, "mPitch", "F");
    g_speedField = env->GetFieldID(g_playbackParametersClass, "mSpeed", "F");

    std::cout << "JNI_OnLoad: Successfully cached JNI class and method IDs." << std::endl;
    return JNI_VERSION_1_6;
}

// Unload the jni classes and methods for getCallbackStatistics
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("JNI_OnUnload: Failed to get JNIEnv.");
        return;
    }

    // Delete global references
    if (g_callbackStatusClass != nullptr) {
        env->DeleteGlobalRef(g_callbackStatusClass);
        g_callbackStatusClass = nullptr;
    }
    if (g_arrayListClass != nullptr) {
        env->DeleteGlobalRef(g_arrayListClass);
        g_arrayListClass = nullptr;
    }
    if (g_playbackParametersClass != nullptr) {
        env->DeleteGlobalRef(g_playbackParametersClass);
        g_playbackParametersClass = nullptr;
    }

    g_callbackStatusConstructor = nullptr;
    g_arrayListConstructor = nullptr;
    g_arrayListAddMethod = nullptr;
    g_playbackParametersConstructor = nullptr;
    g_fallbackModeField = nullptr;
    g_stretchModeField = nullptr;
    g_pitchField = nullptr;
    g_speedField = nullptr;

    g_javaVM = nullptr;
    std::cout << "JNI_OnUnload: Released global JNI references." << std::endl;
}

JNIEXPORT jobject JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestActivity_getCallbackStatistics(JNIEnv *env,
                                                                             jobject obj) {
    if (g_callbackStatusClass == nullptr || g_callbackStatusConstructor == nullptr ||
        g_arrayListClass == nullptr || g_arrayListConstructor == nullptr ||
        g_arrayListAddMethod == nullptr) {
        LOGE("Error: JNI IDs not cached. Initialization in JNI_OnLoad might have failed.");
        return nullptr;
    }

    std::vector<AudioWorkloadTest::CallbackStatus> cppCallbackStats =
            sAudioWorkload.getCallbackStatistics();

    jobject javaList = env->NewObject(g_arrayListClass, g_arrayListConstructor);
    if (javaList == nullptr) {
        LOGE("Error: Could not create new ArrayList object.");
        if (env->ExceptionCheck()) env->ExceptionDescribe();
        return nullptr;
    }

    for (const auto& status : cppCallbackStats) {
        jobject javaStatus = env->NewObject(
                g_callbackStatusClass,
                g_callbackStatusConstructor,
                (jint)status.numVoices,
                (jlong)status.beginTimeNs,
                (jlong)status.finishTimeNs,
                (jint)status.xRunCount,
                (jint)status.cpuIndex
        );
        if (javaStatus == nullptr) {
            LOGE("Error: Could not create new CallbackStatus object.");
            if (env->ExceptionCheck()) env->ExceptionDescribe();
            env->DeleteLocalRef(javaList);
            return nullptr;
        }

        env->CallBooleanMethod(javaList, g_arrayListAddMethod, javaStatus);
        env->DeleteLocalRef(javaStatus);
    }

    return javaList;
}

static AudioWorkloadTestRunner sAudioWorkloadRunner;

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_start(JNIEnv *env, jobject thiz,
                                                   jint targetDurationMs,
                                                   jint numBursts,
                                                   jint numVoices,
                                                   jint alternateNumVoices,
                                                   jint alternatingPeriodMillis,
                                                   jboolean adpfEnabled,
                                                   jboolean adpfWorkloadIncreaseEnabled,
                                                   jboolean hearWorkload) {
    return sAudioWorkloadRunner.start(targetDurationMs, numBursts, numVoices,
                                      alternateNumVoices, alternatingPeriodMillis, adpfEnabled,
                                      adpfWorkloadIncreaseEnabled, hearWorkload);
}

JNIEXPORT jboolean JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_stopIfDone(JNIEnv *env, jobject thiz) {
    return sAudioWorkloadRunner.stopIfDone();
}

JNIEXPORT jstring JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_getStatus(JNIEnv *env, jobject thiz) {
    std::string status = sAudioWorkloadRunner.getStatus();
    return env->NewStringUTF(status.c_str());
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_stop(JNIEnv *env, jobject thiz) {
    return sAudioWorkloadRunner.stop();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_getResult(JNIEnv *env, jobject thiz) {
    return sAudioWorkloadRunner.getResult();
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_AudioWorkloadTestRunnerActivity_getXRunCount(JNIEnv *env, jobject thiz) {
    return sAudioWorkloadRunner.getXRunCount();
}

JNIEXPORT jlong JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_createEngine(JNIEnv *env, jobject thiz, jint channelCount) {
    ReverseJniEngine *reverseJniEngine = new ReverseJniEngine(env, thiz, channelCount);
    return reinterpret_cast<jlong>(reverseJniEngine);
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_startEngine(JNIEnv *env, jobject thiz, jlong enginePtr, jint bufferSizeInBursts, jint sleepDurationUs) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        reverseJniEngine->start(bufferSizeInBursts, sleepDurationUs);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_stopEngine(JNIEnv *env, jobject thiz, jlong enginePtr) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        reverseJniEngine->stop();
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_deleteEngine(JNIEnv *env, jobject thiz, jlong enginePtr) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        delete reverseJniEngine;
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_setBufferSizeInBursts(JNIEnv *env, jobject thiz, jlong enginePtr, jint bufferSizeInBursts) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        reverseJniEngine->setBufferSizeInBursts(bufferSizeInBursts);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_setSleepDurationUs(JNIEnv *env, jobject thiz, jlong enginePtr, jint sleepDurationUs) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        reverseJniEngine->setSleepDurationUs(sleepDurationUs);
    }
}

JNIEXPORT void JNICALL
Java_com_mobileer_oboetester_ReverseJniEngine_setAudioBuffer(JNIEnv *env, jobject thiz, jlong enginePtr, jfloatArray buffer) {
    ReverseJniEngine *reverseJniEngine = reinterpret_cast<ReverseJniEngine *>(enginePtr);
    if (reverseJniEngine) {
        reverseJniEngine->setAudioBuffer(env, buffer);
    }
}

JNIEXPORT jint JNICALL
Java_com_mobileer_oboetester_TestDisconnectActivity_getRoutingChangedCount(JNIEnv *env, jobject) {
    return engine.mActivityTestDisconnect.getRoutingChangedCount();
}

} // extern "C"
