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

#ifndef OBOE_AAUDIO_LOADER_H_
#define OBOE_AAUDIO_LOADER_H_

#include <unistd.h>
#include "oboe/Definitions.h"

// If the NDK is before O then define this in your build
// so that AAudio.h will not be included.
#ifdef OBOE_NO_INCLUDE_AAUDIO

// Define missing types from AAudio.h
typedef int32_t aaudio_stream_state_t;
typedef int32_t aaudio_direction_t;
typedef int32_t aaudio_format_t;
typedef int32_t aaudio_data_callback_result_t;
typedef int32_t aaudio_result_t;
typedef int32_t aaudio_sharing_mode_t;
typedef int32_t aaudio_performance_mode_t;

typedef struct AAudioStreamStruct         AAudioStream;
typedef struct AAudioStreamBuilderStruct  AAudioStreamBuilder;

typedef aaudio_data_callback_result_t (*AAudioStream_dataCallback)(
        AAudioStream *stream,
        void *userData,
        void *audioData,
        int32_t numFrames);

typedef void (*AAudioStream_errorCallback)(
        AAudioStream *stream,
        void *userData,
        aaudio_result_t error);

// These were defined in P
typedef int32_t aaudio_usage_t;
typedef int32_t aaudio_content_type_t;
typedef int32_t aaudio_input_preset_t;
typedef int32_t aaudio_session_id_t;

// There are a few definitions used by Oboe.
#define AAUDIO_OK                      static_cast<aaudio_result_t>(Result::OK)
#define AAUDIO_ERROR_TIMEOUT           static_cast<aaudio_result_t>(Result::ErrorTimeout)
#define AAUDIO_STREAM_STATE_STARTING   static_cast<aaudio_stream_state_t>(StreamState::Starting)
#define AAUDIO_STREAM_STATE_STARTED    static_cast<aaudio_stream_state_t>(StreamState::Started)
#else
#include <aaudio/AAudio.h>
#endif

#ifdef __NDK_MAJOR__
#define OBOE_USING_NDK 1
#else
#define __NDK_MAJOR__ 0
#define OBOE_USING_NDK 0
#endif

#if __NDK_MAJOR__ < 24
// Defined in SC_V2
typedef uint32_t aaudio_channel_mask_t;
typedef int32_t aaudio_spatialization_behavior_t;
#endif

#if OBOE_USING_NDK && __NDK_MAJOR__ < 29
// Defined in Android B
typedef void (*AAudioStream_presentationEndCallback)(
        AAudioStream* stream,
        void* userData);
#endif

#if OBOE_USING_NDK && __NDK_MAJOR__ < 30
// Defined in Android C
typedef void (*AAudioStream_routingChangedCallback)(
        AAudioStream* stream,
        void* userData,
        const int32_t* deviceIds,
        int32_t numDevices);
#endif

#ifndef __ANDROID_API_Q__
#define __ANDROID_API_Q__ 29
#endif

#ifndef __ANDROID_API_R__
#define __ANDROID_API_R__ 30
#endif

#ifndef __ANDROID_API_S__
#define __ANDROID_API_S__ 31
#endif

#ifndef __ANDROID_API_S_V2__
#define __ANDROID_API_S_V2__ 32
#endif

#ifndef __ANDROID_API_U__
#define __ANDROID_API_U__ 34
#endif

#ifndef __ANDROID_API_B__
#define __ANDROID_API_B__ 36
#endif

#ifndef __ANDROID_API_C__
#define __ANDROID_API_C__ 37
#endif

#if OBOE_USING_NDK && __NDK_MAJOR__ < 30
// These were defined in Android B
typedef int32_t AAudio_DeviceType;
typedef int32_t aaudio_policy_t;
#endif

// TODO: find the first NDK version containing the following values
#if OBOE_USING_NDK && __NDK_MAJOR__ <= 30
typedef enum AAudio_FallbackMode : int32_t {
    AAUDIO_FALLBACK_MODE_DEFAULT = 0,
    AAUDIO_FALLBACK_MODE_MUTE = 1,
    AAUDIO_FALLBACK_MODE_FAIL = 2,
} AAudio_FallbackMode;

typedef enum AAudio_StretchMode : int32_t {
    AAUDIO_STRETCH_MODE_DEFAULT = 0,
    AAUDIO_STRETCH_MODE_VOICE = 1,
} AAudio_StretchMode;

typedef struct AAudioPlaybackParameters {
    AAudio_FallbackMode fallbackMode;
    AAudio_StretchMode stretchMode;
    float pitch;
    float speed;
} AAudioPlaybackParameters;

typedef int32_t (*AAudioStream_partialDataCallback)(
        AAudioStream* stream,
        void* userData,
        void* audioData,
        int32_t numFrames);
#endif

namespace oboe {

/**
 * The AAudio API was not available in early versions of Android.
 * To avoid linker errors, we dynamically link with the functions by name using dlsym().
 * On older versions this linkage will safely fail.
 */
class AAudioLoader {
  public:
    // Use signatures for common functions.
    // Key to letter abbreviations.
    // S = Stream
    // B = Builder
    // I = int32_t
    // L = int64_t
    // T = sTate
    // K = clocKid_t
    // P = Pointer to following data type
    // C = Const prefix
    // H = cHar
    // U = uint32_t
    // O = bOol
    // R = pResentation end callback
    // M = aaudioplaybackparaMeters
    // D = Datacallback/partialDatacallback

    typedef int32_t  (*signature_I_PPB)(AAudioStreamBuilder **builder);

    typedef const char * (*signature_CPH_I)(int32_t);

    typedef int32_t (*signature_I_PBPPS)(AAudioStreamBuilder *,
                                      AAudioStream **stream);  // AAudioStreamBuilder_open()

    typedef int32_t (*signature_I_PB)(AAudioStreamBuilder *);  // AAudioStreamBuilder_delete()
    // AAudioStreamBuilder_setSampleRate()
    typedef void    (*signature_V_PBI)(AAudioStreamBuilder *, int32_t);

    // AAudioStreamBuilder_setChannelMask()
    typedef void    (*signature_V_PBU)(AAudioStreamBuilder *, uint32_t);

    typedef void    (*signature_V_PBCPH)(AAudioStreamBuilder *, const char *);

    // AAudioStreamBuilder_setPrivacySensitive
    typedef void    (*signature_V_PBO)(AAudioStreamBuilder *, bool);

    typedef int32_t (*signature_I_PS)(AAudioStream *);  // AAudioStream_getSampleRate()
    typedef int64_t (*signature_L_PS)(AAudioStream *);  // AAudioStream_getFramesRead()
    // AAudioStream_setBufferSizeInFrames()
    typedef int32_t (*signature_I_PSI)(AAudioStream *, int32_t);

    typedef void    (*signature_V_PBPDPV)(AAudioStreamBuilder *,
                                          AAudioStream_dataCallback,
                                          void *);

    typedef void    (*signature_V_PBPEPV)(AAudioStreamBuilder *,
                                          AAudioStream_errorCallback,
                                          void *);

    typedef void (*signature_V_PBPRPV)(AAudioStreamBuilder *,
                                       AAudioStream_presentationEndCallback,
                                       void *);

    typedef void (*signature_V_PBRCCPV)(AAudioStreamBuilder *,
                                        AAudioStream_routingChangedCallback,
                                        void *);

    typedef aaudio_format_t (*signature_F_PS)(AAudioStream *stream);

    typedef int32_t (*signature_I_PSPVIL)(AAudioStream *, void *, int32_t, int64_t);
    typedef int32_t (*signature_I_PSCPVIL)(AAudioStream *, const void *, int32_t, int64_t);

    typedef int32_t (*signature_I_PSTPTL)(AAudioStream *,
                                          aaudio_stream_state_t,
                                          aaudio_stream_state_t *,
                                          int64_t);

    typedef int32_t (*signature_I_PSKPLPL)(AAudioStream *, clockid_t, int64_t *, int64_t *);

    typedef bool    (*signature_O_PS)(AAudioStream *);

    typedef uint32_t (*signature_U_PS)(AAudioStream *);

    typedef int32_t (*signature_I_II)(int32_t, int32_t);
    typedef int32_t (*signature_I_I)(int32_t);
    typedef int32_t (*signature_I)();
    typedef int32_t (*signature_I_PSII)(AAudioStream *, int32_t, int32_t);

    // AAudioStream_getDeviceIds()
    typedef int32_t (*signature_I_PSPIPI)(AAudioStream *, int32_t *, int32_t *);

    typedef int32_t (*signature_I_PSIPL)(AAudioStream *, int32_t, int64_t *);

    typedef int32_t (*signature_I_PSPM)(AAudioStream *, AAudioPlaybackParameters *);
    typedef int32_t (*signature_I_PSCPM)(AAudioStream *, const AAudioPlaybackParameters *);

    static AAudioLoader* getInstance(); // singleton

    /**
     * Open the AAudio shared library and load the function pointers.
     * This can be called multiple times.
     * It should only be called from one thread.
     *
     * The destructor will clean up after the open.
     *
     * @return 0 if successful or negative error.
     */
    int open();

    void *getLibHandle() const { return mLibHandle; }

    // Function pointers into the AAudio shared library.
    signature_I_PPB   createStreamBuilder = nullptr;

    signature_I_PBPPS builder_openStream = nullptr;

    signature_V_PBI builder_setBufferCapacityInFrames = nullptr;
    signature_V_PBI builder_setChannelCount = nullptr;
    signature_V_PBI builder_setDeviceId = nullptr;
    signature_V_PBI builder_setDirection = nullptr;
    signature_V_PBI builder_setFormat = nullptr;
    signature_V_PBI builder_setFramesPerDataCallback = nullptr;
    signature_V_PBI builder_setPerformanceMode = nullptr;
    signature_V_PBI builder_setSampleRate = nullptr;
    signature_V_PBI builder_setSharingMode = nullptr;
    signature_V_PBU builder_setChannelMask = nullptr;

    signature_V_PBI builder_setUsage = nullptr;
    signature_V_PBI builder_setContentType = nullptr;
    signature_V_PBI builder_setInputPreset = nullptr;
    signature_V_PBI builder_setSessionId = nullptr;

    signature_V_PBO builder_setPrivacySensitive = nullptr;
    signature_V_PBI builder_setAllowedCapturePolicy = nullptr;

    signature_V_PBCPH builder_setPackageName = nullptr;
    signature_V_PBCPH builder_setAttributionTag = nullptr;

    signature_V_PBO builder_setIsContentSpatialized = nullptr;
    signature_V_PBI builder_setSpatializationBehavior = nullptr;

    signature_V_PBPDPV  builder_setDataCallback = nullptr;
    signature_V_PBPEPV  builder_setErrorCallback = nullptr;
    signature_V_PBPRPV  builder_setPresentationEndCallback = nullptr;
    signature_V_PBRCCPV builder_setRoutingChangedCallback = nullptr;
    signature_V_PBPDPV  builder_setPartialDataCallback = nullptr;

    signature_I_PB      builder_delete = nullptr;

    signature_F_PS      stream_getFormat = nullptr;

    signature_I_PSPVIL  stream_read = nullptr;
    signature_I_PSCPVIL stream_write = nullptr;

    signature_I_PSTPTL  stream_waitForStateChange = nullptr;

    signature_I_PSKPLPL stream_getTimestamp = nullptr;

    signature_I_PSPIPI  stream_getDeviceIds = nullptr;

    signature_I_PS   stream_release = nullptr;
    signature_I_PS   stream_close = nullptr;

    signature_I_PS   stream_getChannelCount = nullptr;
    signature_I_PS   stream_getDeviceId = nullptr;

    signature_I_PS   stream_getBufferSize = nullptr;
    signature_I_PS   stream_getBufferCapacity = nullptr;
    signature_I_PS   stream_getFramesPerBurst = nullptr;
    signature_I_PS   stream_getState = nullptr;
    signature_I_PS   stream_getPerformanceMode = nullptr;
    signature_I_PS   stream_getSampleRate = nullptr;
    signature_I_PS   stream_getSharingMode = nullptr;
    signature_I_PS   stream_getXRunCount = nullptr;

    signature_I_PSI  stream_setBufferSize = nullptr;
    signature_I_PS   stream_requestStart = nullptr;
    signature_I_PS   stream_requestPause = nullptr;
    signature_I_PS   stream_requestFlush = nullptr;
    signature_I_PS   stream_requestStop = nullptr;

    signature_L_PS   stream_getFramesRead = nullptr;
    signature_L_PS   stream_getFramesWritten = nullptr;

    signature_CPH_I  convertResultToText = nullptr;

    signature_I_PS   stream_getUsage = nullptr;
    signature_I_PS   stream_getContentType = nullptr;
    signature_I_PS   stream_getInputPreset = nullptr;
    signature_I_PS   stream_getSessionId = nullptr;

    signature_O_PS   stream_isPrivacySensitive = nullptr;
    signature_I_PS   stream_getAllowedCapturePolicy = nullptr;

    signature_U_PS   stream_getChannelMask = nullptr;

    signature_O_PS   stream_isContentSpatialized = nullptr;
    signature_I_PS   stream_getSpatializationBehavior = nullptr;

    signature_I_PS   stream_getHardwareChannelCount = nullptr;
    signature_I_PS   stream_getHardwareSampleRate = nullptr;
    signature_F_PS   stream_getHardwareFormat = nullptr;


    signature_I_II   aaudio_getPlatformMMapPolicy = nullptr;
    signature_I_II   aaudio_getPlatformMMapExclusivePolicy = nullptr;
    signature_I_I    aaudio_setMMapPolicy = nullptr;
    signature_I      aaudio_getMMapPolicy = nullptr;
    signature_O_PS   stream_isMMapUsed = nullptr;

    signature_I_PSII stream_setOffloadDelayPadding = nullptr;
    signature_I_PS   stream_getOffloadDelay = nullptr;
    signature_I_PS   stream_getOffloadPadding = nullptr;
    signature_I_PS   stream_setOffloadEndOfStream = nullptr;

    signature_I_PSIPL stream_flushFromFrame = nullptr;

    signature_I_PSPM stream_getPlaybackParameters = nullptr;
    signature_I_PSCPM stream_setPlaybackParameters = nullptr;

  private:
    AAudioLoader() {}
    ~AAudioLoader();

    // Load function pointers for specific signatures.
    signature_I_PPB     load_I_PPB(const char *name);
    signature_CPH_I     load_CPH_I(const char *name);
    signature_V_PBI     load_V_PBI(const char *name);
    signature_V_PBCPH   load_V_PBCPH(const char *name);
    signature_V_PBPDPV  load_V_PBPDPV(const char *name);
    signature_V_PBPEPV  load_V_PBPEPV(const char *name);
    signature_I_PB      load_I_PB(const char *name);
    signature_I_PBPPS   load_I_PBPPS(const char *name);
    signature_I_PS      load_I_PS(const char *name);
    signature_L_PS      load_L_PS(const char *name);
    signature_F_PS      load_F_PS(const char *name);
    signature_O_PS      load_O_PS(const char *name);
    signature_I_PSI     load_I_PSI(const char *name);
    signature_I_PSPVIL  load_I_PSPVIL(const char *name);
    signature_I_PSCPVIL load_I_PSCPVIL(const char *name);
    signature_I_PSTPTL  load_I_PSTPTL(const char *name);
    signature_I_PSKPLPL load_I_PSKPLPL(const char *name);
    signature_V_PBU     load_V_PBU(const char *name);
    signature_U_PS      load_U_PS(const char *name);
    signature_V_PBO     load_V_PBO(const char *name);
    signature_I_II      load_I_II(const char *name);
    signature_I_I       load_I_I(const char *name);
    signature_I         load_I(const char *name);
    signature_V_PBPRPV  load_V_PBPRPV(const char *name);
    signature_V_PBRCCPV load_V_PBRCCPV(const char *name);
    signature_I_PSII    load_I_PSII(const char *name);
    signature_I_PSPIPI  load_I_PSPIPI(const char *name);
    signature_I_PSIPL   load_I_PSIPL(const char *name);
    signature_I_PSPM    load_I_PSPM(const char *name);
    signature_I_PSCPM   load_I_PSCPM(const char *name);

    void *mLibHandle = nullptr;
};

} // namespace oboe

#endif //OBOE_AAUDIO_LOADER_H_
