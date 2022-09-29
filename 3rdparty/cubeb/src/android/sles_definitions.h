/*
 * Copyright (C) 2010 The Android Open Source Project
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

/**
 * This file is similar to the file "OpenSLES_AndroidConfiguration.h" found in
 * the Android NDK, but removes the #ifdef __cplusplus defines, so we can keep
 * using a C compiler in cubeb.
 */

#ifndef OPENSL_ES_ANDROIDCONFIGURATION_H_
#define OPENSL_ES_ANDROIDCONFIGURATION_H_

/*---------------------------------------------------------------------------*/
/* Android AudioRecorder configuration                                       */
/*---------------------------------------------------------------------------*/

/** Audio recording preset */
/** Audio recording preset key */
#define SL_ANDROID_KEY_RECORDING_PRESET                                        \
  ((const SLchar *)"androidRecordingPreset")
/** Audio recording preset values */
/**   preset "none" cannot be set, it is used to indicate the current settings
 *     do not match any of the presets. */
#define SL_ANDROID_RECORDING_PRESET_NONE ((SLuint32)0x00000000)
/**   generic recording configuration on the platform */
#define SL_ANDROID_RECORDING_PRESET_GENERIC ((SLuint32)0x00000001)
/**   uses the microphone audio source with the same orientation as the camera
 *     if available, the main device microphone otherwise */
#define SL_ANDROID_RECORDING_PRESET_CAMCORDER ((SLuint32)0x00000002)
/**   uses the main microphone tuned for voice recognition */
#define SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION ((SLuint32)0x00000003)
/**   uses the main microphone tuned for audio communications */
#define SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION ((SLuint32)0x00000004)
/**   uses the main microphone unprocessed */
#define SL_ANDROID_RECORDING_PRESET_UNPROCESSED ((SLuint32)0x00000005)

/*---------------------------------------------------------------------------*/
/* Android AudioPlayer configuration                                         */
/*---------------------------------------------------------------------------*/

/** Audio playback stream type */
/** Audio playback stream type key */
#define SL_ANDROID_KEY_STREAM_TYPE ((const SLchar *)"androidPlaybackStreamType")

/** Audio playback stream type  values */
/*      same as android.media.AudioManager.STREAM_VOICE_CALL */
#define SL_ANDROID_STREAM_VOICE ((SLint32)0x00000000)
/*      same as android.media.AudioManager.STREAM_SYSTEM */
#define SL_ANDROID_STREAM_SYSTEM ((SLint32)0x00000001)
/*      same as android.media.AudioManager.STREAM_RING */
#define SL_ANDROID_STREAM_RING ((SLint32)0x00000002)
/*      same as android.media.AudioManager.STREAM_MUSIC */
#define SL_ANDROID_STREAM_MEDIA ((SLint32)0x00000003)
/*      same as android.media.AudioManager.STREAM_ALARM */
#define SL_ANDROID_STREAM_ALARM ((SLint32)0x00000004)
/*      same as android.media.AudioManager.STREAM_NOTIFICATION */
#define SL_ANDROID_STREAM_NOTIFICATION ((SLint32)0x00000005)

/*---------------------------------------------------------------------------*/
/* Android AudioPlayer and AudioRecorder configuration                       */
/*---------------------------------------------------------------------------*/

/** Audio Performance mode.
 * Performance mode tells the framework how to configure the audio path
 * for a player or recorder according to application performance and
 * functional requirements.
 * It affects the output or input latency based on acceptable tradeoffs on
 * battery drain and use of pre or post processing effects.
 * Performance mode should be set before realizing the object and should be
 * read after realizing the object to check if the requested mode could be
 * granted or not.
 */
/** Audio Performance mode key */
#define SL_ANDROID_KEY_PERFORMANCE_MODE                                        \
  ((const SLchar *)"androidPerformanceMode")

/** Audio performance values */
/*      No specific performance requirement. Allows HW and SW pre/post
 * processing. */
#define SL_ANDROID_PERFORMANCE_NONE ((SLuint32)0x00000000)
/*      Priority given to latency. No HW or software pre/post processing.
 *      This is the default if no performance mode is specified. */
#define SL_ANDROID_PERFORMANCE_LATENCY ((SLuint32)0x00000001)
/*      Priority given to latency while still allowing HW pre and post
 * processing. */
#define SL_ANDROID_PERFORMANCE_LATENCY_EFFECTS ((SLuint32)0x00000002)
/*      Priority given to power saving if latency is not a concern.
 *      Allows HW and SW pre/post processing. */
#define SL_ANDROID_PERFORMANCE_POWER_SAVING ((SLuint32)0x00000003)

#endif /* OPENSL_ES_ANDROIDCONFIGURATION_H_ */
