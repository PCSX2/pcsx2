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

package com.mobileer.oboetester;

import android.media.AudioAttributes;
import android.media.AudioManager;
import android.os.Build;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/**
 * Container for the properties of a Stream.
 *
 * This can be used to build a stream, or as a base class for a Stream,
 * or as a way to report the properties of a Stream.
 */

public class StreamConfiguration {
    public static final int UNSPECIFIED = 0;

    // These must match order in Spinner and in native code and in AAudio.h
    public static final int NATIVE_API_UNSPECIFIED = 0;
    public static final int NATIVE_API_OPENSLES = 1;
    public static final int NATIVE_API_AAUDIO = 2;

    public static final int SHARING_MODE_EXCLUSIVE = 0; // must match AAUDIO
    public static final int SHARING_MODE_SHARED = 1; // must match AAUDIO

    public static final int AUDIO_FORMAT_PCM_16 = 1; // must match AAUDIO
    public static final int AUDIO_FORMAT_PCM_FLOAT = 2; // must match AAUDIO
    public static final int AUDIO_FORMAT_PCM_24 = 3; // must match AAUDIO
    public static final int AUDIO_FORMAT_PCM_32 = 4; // must match AAUDIO
    public static final int AUDIO_FORMAT_IEC61937 = 5; // must match AAUDIO
    public static final int AUDIO_FORMAT_MP3 = 6; // must match AAUDIO

    public static final int DIRECTION_OUTPUT = 0; // must match AAUDIO
    public static final int DIRECTION_INPUT = 1; // must match AAUDIO

    public static final int SESSION_ID_NONE = -1; // must match AAUDIO
    public static final int SESSION_ID_ALLOCATE = 0; // must match AAUDIO

    public static final int PERFORMANCE_MODE_NONE = 10; // must match AAUDIO
    public static final int PERFORMANCE_MODE_POWER_SAVING = 11; // must match AAUDIO
    public static final int PERFORMANCE_MODE_LOW_LATENCY = 12; // must match AAUDIO
    public static final int PERFORMANCE_MODE_POWER_SAVING_OFFLOAD = 13; // must match AAUDIO

    public static final int RATE_CONVERSION_QUALITY_NONE = 0; // must match Oboe
    public static final int RATE_CONVERSION_QUALITY_FASTEST = 1; // must match Oboe
    public static final int RATE_CONVERSION_QUALITY_LOW = 2; // must match Oboe
    public static final int RATE_CONVERSION_QUALITY_MEDIUM = 3; // must match Oboe
    public static final int RATE_CONVERSION_QUALITY_HIGH = 4; // must match Oboe
    public static final int RATE_CONVERSION_QUALITY_BEST = 5; // must match Oboe

    public static final int STREAM_STATE_STARTING = 3; // must match Oboe
    public static final int STREAM_STATE_STARTED = 4; // must match Oboe

    public static final int INPUT_PRESET_GENERIC = 1; // must match Oboe
    public static final int INPUT_PRESET_CAMCORDER = 5; // must match Oboe
    public static final int INPUT_PRESET_VOICE_RECOGNITION = 6; // must match Oboe
    public static final int INPUT_PRESET_VOICE_COMMUNICATION = 7; // must match Oboe
    public static final int INPUT_PRESET_UNPROCESSED = 9; // must match Oboe
    public static final int INPUT_PRESET_VOICE_PERFORMANCE = 10; // must match Oboe

    public static final int SPATIALIZATION_BEHAVIOR_AUTO = 1; // must match Oboe
    public static final int SPATIALIZATION_BEHAVIOR_NEVER = 2; // must match Oboe

    public static final int ERROR_BASE = -900; // must match Oboe
    public static final int ERROR_DISCONNECTED = -899; // must match Oboe
    public static final int ERROR_ILLEGAL_ARGUMENT = -898; // must match Oboe
    public static final int ERROR_INTERNAL = -896; // must match Oboe
    public static final int ERROR_INVALID_STATE = -895; // must match Oboe
    public static final int ERROR_INVALID_HANDLE = -892; // must match Oboe
    public static final int ERROR_UNIMPLEMENTED = -890; // must match Oboe
    public static final int ERROR_UNAVAILABLE = -889; // must match Oboe
    public static final int ERROR_NO_FREE_HANDLES = -888; // must match Oboe
    public static final int ERROR_NO_MEMORY = -887; // must match Oboe
    public static final int ERROR_NULL = -886; // must match Oboe
    public static final int ERROR_TIMEOUT = -885; // must match Oboe
    public static final int ERROR_WOULD_BLOCK = -884; // must match Oboe
    public static final int ERROR_INVALID_FORMAT = -883; // must match Oboe
    public static final int ERROR_OUT_OF_RANGE = -882; // must match Oboe
    public static final int ERROR_NO_SERVICE = -881; // must match Oboe
    public static final int ERROR_INVALID_RATE = -880; // must match Oboe
    public static final int ERROR_CLOSED = -869; // must match Oboe
    public static final int ERROR_OK = 0; // must match Oboe

    public static final int USAGE_MEDIA = 1;
    public static final int USAGE_VOICE_COMMUNICATION = 2;
    public static final int USAGE_VOICE_COMMUNICATION_SIGNALLING = 3;
    public static final int USAGE_ALARM = 4;
    public static final int USAGE_NOTIFICATION = 5;
    public static final int USAGE_NOTIFICATION_RINGTONE = 6;
    public static final int USAGE_NOTIFICATION_EVENT = 10;
    public static final int USAGE_ASSISTANCE_ACCESSIBILITY = 11;
    public static final int USAGE_ASSISTANCE_NAVIGATION_GUIDANCE = 12;
    public static final int USAGE_ASSISTANCE_SONIFICATION = 13;
    public static final int USAGE_GAME = 14;
    public static final int USAGE_ASSISTANT = 16;

    public static final int CONTENT_TYPE_SPEECH = 1;
    public static final int CONTENT_TYPE_MUSIC = 2;
    public static final int CONTENT_TYPE_MOVIE = 3;
    public static final int CONTENT_TYPE_SONIFICATION = 4;

    public static final int CHANNEL_FRONT_LEFT = 1 << 0;
    public static final int CHANNEL_FRONT_RIGHT = 1 << 1;
    public static final int CHANNEL_FRONT_CENTER = 1 << 2;
    public static final int CHANNEL_LOW_FREQUENCY = 1 << 3;
    public static final int CHANNEL_BACK_LEFT = 1 << 4;
    public static final int CHANNEL_BACK_RIGHT = 1 << 5;
    public static final int CHANNEL_FRONT_LEFT_OF_CENTER = 1 << 6;
    public static final int CHANNEL_FRONT_RIGHT_OF_CENTER = 1 << 7;
    public static final int CHANNEL_BACK_CENTER = 1 << 8;
    public static final int CHANNEL_SIDE_LEFT = 1 << 9;
    public static final int CHANNEL_SIDE_RIGHT = 1 << 10;
    public static final int CHANNEL_TOP_CENTER = 1 << 11;
    public static final int CHANNEL_TOP_FRONT_LEFT = 1 << 12;
    public static final int CHANNEL_TOP_FRONT_CENTER = 1 << 13;
    public static final int CHANNEL_TOP_FRONT_RIGHT = 1 << 14;
    public static final int CHANNEL_TOP_BACK_LEFT = 1 << 15;
    public static final int CHANNEL_TOP_BACK_CENTER = 1 << 16;
    public static final int CHANNEL_TOP_BACK_RIGHT = 1 << 17;
    public static final int CHANNEL_TOP_SIDE_LEFT = 1 << 18;
    public static final int CHANNEL_TOP_SIDE_RIGHT = 1 << 19;
    public static final int CHANNEL_BOTTOM_FRONT_LEFT = 1 << 20;
    public static final int CHANNEL_BOTTOM_FRONT_CENTER = 1 << 21;
    public static final int CHANNEL_BOTTOM_FRONT_RIGHT = 1 << 22;
    public static final int CHANNEL_LOW_FREQUENCY_2 = 1 << 23;
    public static final int CHANNEL_FRONT_WIDE_LEFT = 1 << 24;
    public static final int CHANNEL_FRONT_WIDE_RIGHT = 1 << 25;

    public static final int CHANNEL_MONO = CHANNEL_FRONT_LEFT;
    public static final int CHANNEL_STEREO = CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT;
    public static final int CHANNEL_2POINT1 = CHANNEL_FRONT_LEFT |
                                              CHANNEL_FRONT_RIGHT |
                                              CHANNEL_LOW_FREQUENCY;
    public static final int CHANNEL_TRI = CHANNEL_FRONT_LEFT |
                                          CHANNEL_FRONT_RIGHT |
                                          CHANNEL_FRONT_CENTER;
    public static final int CHANNEL_TRI_BACK = CHANNEL_FRONT_LEFT |
                                               CHANNEL_FRONT_RIGHT |
                                               CHANNEL_BACK_CENTER;
    public static final int CHANNEL_3POINT1 = CHANNEL_FRONT_LEFT |
                                              CHANNEL_FRONT_RIGHT |
                                              CHANNEL_FRONT_CENTER |
                                              CHANNEL_LOW_FREQUENCY;
    public static final int CHANNEL_2POINT0POINT2 = CHANNEL_FRONT_LEFT |
                                                    CHANNEL_FRONT_RIGHT |
                                                    CHANNEL_TOP_SIDE_LEFT |
                                                    CHANNEL_TOP_SIDE_RIGHT;
    public static final int CHANNEL_2POINT1POINT2 = CHANNEL_2POINT0POINT2 | CHANNEL_LOW_FREQUENCY;
    public static final int CHANNEL_3POINT0POINT2 = CHANNEL_FRONT_LEFT |
                                                    CHANNEL_FRONT_RIGHT |
                                                    CHANNEL_FRONT_CENTER |
                                                    CHANNEL_TOP_SIDE_LEFT |
                                                    CHANNEL_TOP_SIDE_RIGHT;
    public static final int CHANNEL_3POINT1POINT2 = CHANNEL_3POINT0POINT2 | CHANNEL_LOW_FREQUENCY;
    public static final int CHANNEL_QUAD = CHANNEL_FRONT_LEFT |
                                           CHANNEL_FRONT_RIGHT |
                                           CHANNEL_BACK_LEFT |
                                           CHANNEL_BACK_RIGHT;
    public static final int CHANNEL_QUAD_SIDE = CHANNEL_FRONT_LEFT |
                                                CHANNEL_FRONT_RIGHT |
                                                CHANNEL_SIDE_LEFT |
                                                CHANNEL_SIDE_RIGHT;
    public static final int CHANNEL_SURROUND = CHANNEL_FRONT_LEFT |
                                               CHANNEL_FRONT_RIGHT |
                                               CHANNEL_FRONT_CENTER |
                                               CHANNEL_BACK_CENTER;
    public static final int CHANNEL_PENTA = CHANNEL_QUAD | CHANNEL_FRONT_CENTER;
    // aka 5POINT1_BACK
    public static final int CHANNEL_5POINT1 = CHANNEL_FRONT_LEFT |
                                              CHANNEL_FRONT_RIGHT |
                                              CHANNEL_FRONT_CENTER |
                                              CHANNEL_LOW_FREQUENCY |
                                              CHANNEL_BACK_LEFT |
                                              CHANNEL_BACK_RIGHT;
    public static final int CHANNEL_5POINT1_SIDE = CHANNEL_FRONT_LEFT |
                                                   CHANNEL_FRONT_RIGHT |
                                                   CHANNEL_FRONT_CENTER |
                                                   CHANNEL_LOW_FREQUENCY |
                                                   CHANNEL_SIDE_LEFT |
                                                   CHANNEL_SIDE_RIGHT;
    public static final int CHANNEL_6POINT1 = CHANNEL_FRONT_LEFT |
                                              CHANNEL_FRONT_RIGHT |
                                              CHANNEL_FRONT_CENTER |
                                              CHANNEL_LOW_FREQUENCY |
                                              CHANNEL_BACK_LEFT |
                                              CHANNEL_BACK_RIGHT |
                                              CHANNEL_BACK_CENTER;
    public static final int CHANNEL_7POINT1 = CHANNEL_5POINT1 |
                                              CHANNEL_SIDE_LEFT |
                                              CHANNEL_SIDE_RIGHT;
    public static final int CHANNEL_5POINT1POINT2 = CHANNEL_5POINT1 |
                                                    CHANNEL_TOP_SIDE_LEFT |
                                                    CHANNEL_TOP_SIDE_RIGHT;
    public static final int CHANNEL_5POINT1POINT4 = CHANNEL_5POINT1 |
                                                    CHANNEL_TOP_FRONT_LEFT |
                                                    CHANNEL_TOP_FRONT_RIGHT |
                                                    CHANNEL_TOP_BACK_LEFT |
                                                    CHANNEL_TOP_BACK_RIGHT;
    public static final int CHANNEL_7POINT1POINT2 = CHANNEL_7POINT1 |
                                                    CHANNEL_TOP_SIDE_LEFT |
                                                    CHANNEL_TOP_SIDE_RIGHT;
    public static final int CHANNEL_7POINT1POINT4 = CHANNEL_7POINT1 |
                                                    CHANNEL_TOP_FRONT_LEFT |
                                                    CHANNEL_TOP_FRONT_RIGHT |
                                                    CHANNEL_TOP_BACK_LEFT |
                                                    CHANNEL_TOP_BACK_RIGHT;
    public static final int CHANNEL_9POINT1POINT4 = CHANNEL_7POINT1POINT4 |
                                                    CHANNEL_FRONT_WIDE_LEFT |
                                                    CHANNEL_FRONT_WIDE_RIGHT;
    public static final int CHANNEL_9POINT1POINT6 = CHANNEL_9POINT1POINT4 |
                                                    CHANNEL_TOP_SIDE_LEFT |
                                                    CHANNEL_TOP_SIDE_RIGHT;
    public static final int CHANNEL_FRONT_BACK = CHANNEL_FRONT_CENTER | CHANNEL_BACK_CENTER;

    public static final int[] usages = {
            USAGE_MEDIA,
            USAGE_VOICE_COMMUNICATION,
            USAGE_VOICE_COMMUNICATION_SIGNALLING,
            USAGE_ALARM,
            USAGE_NOTIFICATION,
            USAGE_NOTIFICATION_RINGTONE,
            USAGE_NOTIFICATION_EVENT,
            USAGE_ASSISTANCE_ACCESSIBILITY,
            USAGE_ASSISTANCE_NAVIGATION_GUIDANCE,
            USAGE_ASSISTANCE_SONIFICATION,
            USAGE_GAME,
            USAGE_ASSISTANT};

    public static final int[] contentTypes = {
            CONTENT_TYPE_SPEECH,
            CONTENT_TYPE_MUSIC,
            CONTENT_TYPE_MOVIE,
            CONTENT_TYPE_SONIFICATION};

    public static final int[] channelMasks = {
            CHANNEL_MONO,
            CHANNEL_STEREO,
            CHANNEL_2POINT1,
            CHANNEL_TRI,
            CHANNEL_TRI_BACK,
            CHANNEL_3POINT1,
            CHANNEL_2POINT0POINT2,
            CHANNEL_2POINT1POINT2,
            CHANNEL_3POINT0POINT2,
            CHANNEL_3POINT1POINT2,
            CHANNEL_QUAD,
            CHANNEL_QUAD_SIDE,
            CHANNEL_SURROUND,
            CHANNEL_PENTA,
            CHANNEL_5POINT1,
            CHANNEL_5POINT1_SIDE,
            CHANNEL_6POINT1,
            CHANNEL_7POINT1,
            CHANNEL_5POINT1POINT2,
            CHANNEL_5POINT1POINT4,
            CHANNEL_7POINT1POINT2,
            CHANNEL_7POINT1POINT4,
            CHANNEL_9POINT1POINT4,
            CHANNEL_9POINT1POINT6,
            CHANNEL_FRONT_BACK
    };

    public static boolean isCompressedFormat(int format) {
        return format == AUDIO_FORMAT_MP3;
    }

    private static HashMap<String,Integer> mUsageStringToIntegerMap;
    private static HashMap<String,Integer> mContentTypeStringToIntegerMap;
    private static HashMap<String,Integer> mChannelMaskStringToIntegerMap;
    private static List<String> mChannelMaskStrings = new ArrayList<>();

    private int mNativeApi;
    private int mBufferCapacityInFrames;
    private int mChannelCount;
    private int mDeviceId;
    @Nullable private int[] mDeviceIds;
    private int mSessionId;
    private int mDirection; // does not get reset
    private int mFormat;
    private int mSampleRate;
    private int mSharingMode;
    private int mPerformanceMode;
    private boolean mFormatConversionAllowed;
    private boolean mChannelConversionAllowed;
    private int mRateConversionQuality;
    private int mInputPreset;
    private int mUsage;
    private int mContentType;
    private int mFramesPerBurst;
    private boolean mMMap;
    private int mChannelMask;
    private int mHardwareChannelCount;
    private int mHardwareSampleRate;
    private int mHardwareFormat;
    private int mSpatializationBehavior;
    private String mPackageName;
    private String mAttributionTag;

    public StreamConfiguration() {
        reset();
    }

    static {
        // Build map for Usage string-to-int conversion.
        mUsageStringToIntegerMap = new HashMap<String,Integer>();
        mUsageStringToIntegerMap.put(convertUsageToText(UNSPECIFIED), UNSPECIFIED);
        for (int usage : usages) {
            mUsageStringToIntegerMap.put(convertUsageToText(usage), usage);
        }

        // Build map for Content Type string-to-int conversion.
        mContentTypeStringToIntegerMap = new HashMap<String,Integer>();
        mContentTypeStringToIntegerMap.put(convertContentTypeToText(UNSPECIFIED), UNSPECIFIED);
        for (int contentType : contentTypes) {
            mContentTypeStringToIntegerMap.put(convertContentTypeToText(contentType), contentType);
        }

        // Build map for Channel Mask string-to-int conversion.
        mChannelMaskStringToIntegerMap = new HashMap<String, Integer>();
        String channelMaskStr = convertChannelMaskToText(UNSPECIFIED);
        mChannelMaskStringToIntegerMap.put(channelMaskStr, UNSPECIFIED);
        mChannelMaskStrings.add(channelMaskStr);
        for (int channelMask : channelMasks) {
            channelMaskStr = convertChannelMaskToText(channelMask);
            mChannelMaskStringToIntegerMap.put(channelMaskStr, channelMask);
            mChannelMaskStrings.add(channelMaskStr);
        }
    }

    public void reset() {
        mNativeApi = NATIVE_API_UNSPECIFIED;
        mBufferCapacityInFrames = UNSPECIFIED;
        mChannelCount = UNSPECIFIED;
        mChannelMask = UNSPECIFIED;
        mDeviceId = UNSPECIFIED;
        mDeviceIds = new int[0];
        mSessionId = -1;
        mFormat = AUDIO_FORMAT_PCM_FLOAT;
        mSampleRate = UNSPECIFIED;
        mSharingMode = SHARING_MODE_EXCLUSIVE;
        mPerformanceMode = PERFORMANCE_MODE_LOW_LATENCY;
        mInputPreset = INPUT_PRESET_VOICE_RECOGNITION;
        mUsage = UNSPECIFIED;
        mContentType = UNSPECIFIED;
        mFormatConversionAllowed = false;
        mChannelConversionAllowed = false;
        mRateConversionQuality = RATE_CONVERSION_QUALITY_NONE;
        mMMap = NativeEngine.isMMapSupported();
        mHardwareChannelCount = UNSPECIFIED;
        mHardwareSampleRate = UNSPECIFIED;
        mHardwareFormat = UNSPECIFIED;
        mSpatializationBehavior = UNSPECIFIED;
        mPackageName = "";
        mAttributionTag = "";
    }

    public int getFramesPerBurst() {
        return mFramesPerBurst;
    }

    public void setFramesPerBurst(int framesPerBurst) {
        this.mFramesPerBurst = framesPerBurst;
    }

    public int getBufferCapacityInFrames() {
        return mBufferCapacityInFrames;
    }

    public void setBufferCapacityInFrames(int bufferCapacityInFrames) {
        this.mBufferCapacityInFrames = bufferCapacityInFrames;
    }

    public int getFormat() {
        return mFormat;
    }

    public void setFormat(int format) {
        this.mFormat = format;
    }

    public int getDirection() {
        return mDirection;
    }

    public void setDirection(int direction) {
        this.mDirection = direction;
    }

    public int getPerformanceMode() {
        return mPerformanceMode;
    }

    public void setPerformanceMode(int performanceMode) {
        this.mPerformanceMode = performanceMode;
    }

    static String convertPerformanceModeToText(int performanceMode) {
        switch(performanceMode) {
            case PERFORMANCE_MODE_NONE:
                return "NO";
            case PERFORMANCE_MODE_POWER_SAVING:
                return "PS";
            case PERFORMANCE_MODE_LOW_LATENCY:
                return "LL";
            case PERFORMANCE_MODE_POWER_SAVING_OFFLOAD:
                return "PSO";
            default:
                return "??";
        }
    }

    public int getInputPreset() { return mInputPreset; }
    public void setInputPreset(int inputPreset) {
        this.mInputPreset = inputPreset;
    }

    public int getSpatializationBehavior() { return mSpatializationBehavior; }
    public void setSpatializationBehavior(int spatializationBehavior) {
        this.mSpatializationBehavior = spatializationBehavior;
    }

    public int getUsage() { return mUsage; }
    public void setUsage(int usage) {
        this.mUsage = usage;
    }

    public int getContentType() { return mContentType; }
    public void setContentType(int contentType) {
        this.mContentType = contentType;
    }

    static String convertUsageToText(int usage) {
        switch(usage) {
            case UNSPECIFIED:
                return "Unspecified";
            case USAGE_MEDIA:
                return "Media";
            case USAGE_VOICE_COMMUNICATION:
                return "VoiceComm";
            case USAGE_VOICE_COMMUNICATION_SIGNALLING:
                return "VoiceCommSig";
            case USAGE_ALARM:
                return "Alarm";
            case USAGE_NOTIFICATION:
                return "Notification";
            case USAGE_NOTIFICATION_RINGTONE:
                return "Ringtone";
            case USAGE_NOTIFICATION_EVENT:
                return "Event";
            case USAGE_ASSISTANCE_ACCESSIBILITY:
                return "Accessability";
            case USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
                return "Navigation";
            case USAGE_ASSISTANCE_SONIFICATION:
                return "Sonification";
            case USAGE_GAME:
                return "Game";
            case USAGE_ASSISTANT:
                return "Assistant";
            default:
                return "?=" + usage;
        }
    }

    static int convertUsageToAudioAttributeUsage(int usage) {
        switch(usage) {
            case USAGE_MEDIA:
                return AudioAttributes.USAGE_MEDIA;
            case USAGE_VOICE_COMMUNICATION:
                return AudioAttributes.USAGE_VOICE_COMMUNICATION;
            case USAGE_VOICE_COMMUNICATION_SIGNALLING:
                return AudioAttributes.USAGE_VOICE_COMMUNICATION_SIGNALLING;
            case USAGE_ALARM:
                return AudioAttributes.USAGE_ALARM;
            case USAGE_NOTIFICATION:
                return AudioAttributes.USAGE_NOTIFICATION;
            case USAGE_NOTIFICATION_RINGTONE:
                return AudioAttributes.USAGE_NOTIFICATION_RINGTONE;
            case USAGE_NOTIFICATION_EVENT:
                return AudioAttributes.USAGE_NOTIFICATION_EVENT;
            case USAGE_ASSISTANCE_ACCESSIBILITY:
                return AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY;
            case USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
                return AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE;
            case USAGE_ASSISTANCE_SONIFICATION:
                return AudioAttributes.USAGE_ASSISTANCE_SONIFICATION;
            case USAGE_GAME:
                return AudioAttributes.USAGE_GAME;
            case USAGE_ASSISTANT:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    return AudioAttributes.USAGE_ASSISTANT;
                }
                return AudioAttributes.USAGE_UNKNOWN;
            case UNSPECIFIED:
            default:
                return AudioAttributes.USAGE_UNKNOWN;
        }
    }

    // Used to convert Usages to legacy Stream types
    // See https://source.android.com/docs/core/audio/attributes#compatibility
    static int convertUsageToStreamType(int usage) {
        switch(usage) {
            case USAGE_VOICE_COMMUNICATION:
            case USAGE_VOICE_COMMUNICATION_SIGNALLING:
                return AudioManager.STREAM_VOICE_CALL;
            case USAGE_ASSISTANCE_SONIFICATION:
                return AudioManager.STREAM_SYSTEM;
            case USAGE_NOTIFICATION_RINGTONE:
                return AudioManager.STREAM_RING;
            case USAGE_ALARM:
                return AudioManager.STREAM_ALARM;
            case USAGE_NOTIFICATION:
            case USAGE_NOTIFICATION_EVENT:
                return AudioManager.STREAM_NOTIFICATION;
            case UNSPECIFIED:
            case USAGE_GAME:
            case USAGE_MEDIA:
            case USAGE_ASSISTANCE_ACCESSIBILITY:
            case USAGE_ASSISTANCE_NAVIGATION_GUIDANCE:
            case USAGE_ASSISTANT:
            default:
                return AudioManager.STREAM_MUSIC;
        }
    }

    public static int convertTextToUsage(String text) {
        return mUsageStringToIntegerMap.get(text);
    }

    static String convertContentTypeToText(int contentType) {
        switch(contentType) {
            case CONTENT_TYPE_SPEECH:
                return "Speech";
            case CONTENT_TYPE_MUSIC:
                return "Music";
            case CONTENT_TYPE_MOVIE:
                return "Movie";
            case CONTENT_TYPE_SONIFICATION:
                return "Sonification";
            case UNSPECIFIED:
                return "Unspecified";
            default:
                return "?=" + contentType;
        }
    }

    static int convertContentTypeAudioAttributesContentType(int contentType) {
        switch(contentType) {
            case CONTENT_TYPE_SPEECH:
                return AudioAttributes.CONTENT_TYPE_SPEECH;
            case CONTENT_TYPE_MUSIC:
                return AudioAttributes.CONTENT_TYPE_MUSIC;
            case CONTENT_TYPE_MOVIE:
                return AudioAttributes.CONTENT_TYPE_MOVIE;
            case CONTENT_TYPE_SONIFICATION:
                return AudioAttributes.CONTENT_TYPE_SONIFICATION;
            case UNSPECIFIED:
            default:
                return AudioAttributes.CONTENT_TYPE_UNKNOWN;
        }
    }

    public static int convertTextToContentType(String text) {
        return mContentTypeStringToIntegerMap.get(text);
    }

    public int getSharingMode() {
        return mSharingMode;
    }

    public void setSharingMode(int sharingMode) {
        this.mSharingMode = sharingMode;
    }

    static String convertSharingModeToText(int sharingMode) {
        switch(sharingMode) {
            case SHARING_MODE_SHARED:
                return "SH";
            case SHARING_MODE_EXCLUSIVE:
                return "EX";
            default:
                return "??";
        }
    }

    public static String convertFormatToText(int format) {
        switch(format) {
            case UNSPECIFIED:
                return "Unspecified";
            case AUDIO_FORMAT_PCM_16:
                return "I16";
            case AUDIO_FORMAT_PCM_24:
                return "I24";
            case AUDIO_FORMAT_PCM_32:
                return "I32";
            case AUDIO_FORMAT_PCM_FLOAT:
                return "Float";
            case AUDIO_FORMAT_IEC61937:
                return "IEC61937";
            case AUDIO_FORMAT_MP3:
                return "MP3";
            default:
                return "Invalid";
        }
    }

    public static String convertNativeApiToText(int api) {
        switch(api) {
            case NATIVE_API_UNSPECIFIED:
                return "Unspec";
            case NATIVE_API_AAUDIO:
                return "AAudio";
            case NATIVE_API_OPENSLES:
                return "OpenSL";
            default:
                return "Invalid";
        }
    }

    public static String convertChannelMaskToText(int channelMask) {
        switch (channelMask) {
            case UNSPECIFIED:
                return "Unspecified";
            case CHANNEL_MONO:
                return "Mono";
            case CHANNEL_STEREO:
                return "Stereo";
            case CHANNEL_2POINT1:
                return "2.1";
            case CHANNEL_TRI:
                return "Tri";
            case CHANNEL_TRI_BACK:
                return "TriBack";
            case CHANNEL_3POINT1:
                return "3.1";
            case CHANNEL_2POINT0POINT2:
                return "2.0.2";
            case CHANNEL_2POINT1POINT2:
                return "2.1.2";
            case CHANNEL_3POINT0POINT2:
                return "3.0.2";
            case CHANNEL_3POINT1POINT2:
                return "3.1.2";
            case CHANNEL_QUAD:
                return "Quad";
            case CHANNEL_QUAD_SIDE:
                return "QuadSide";
            case CHANNEL_SURROUND:
                return "Surround";
            case CHANNEL_PENTA:
                return "Penta";
            case CHANNEL_5POINT1:
                return "5.1";
            case CHANNEL_5POINT1_SIDE:
                return "5.1Side";
            case CHANNEL_6POINT1:
                return "6.1";
            case CHANNEL_7POINT1:
                return "7.1";
            case CHANNEL_5POINT1POINT2:
                return "5.1.2";
            case CHANNEL_5POINT1POINT4:
                return "5.1.4";
            case CHANNEL_7POINT1POINT2:
                return "7.1.2";
            case CHANNEL_7POINT1POINT4:
                return "7.1.4";
            case CHANNEL_9POINT1POINT4:
                return "9.1.4";
            case CHANNEL_9POINT1POINT6:
                return "9.1.6";
            case CHANNEL_FRONT_BACK:
                return "FrontBack";
            default:
                return "?=" + Integer.toHexString(channelMask);
        }
    }

    static String convertRateConversionQualityToText(int quality) {
        switch(quality) {
            case RATE_CONVERSION_QUALITY_NONE:
                return "None";
            case RATE_CONVERSION_QUALITY_FASTEST:
                return "Fastest";
            case RATE_CONVERSION_QUALITY_LOW:
                return "Low";
            case RATE_CONVERSION_QUALITY_MEDIUM:
                return "Medium";
            case RATE_CONVERSION_QUALITY_HIGH:
                return "High";
            case RATE_CONVERSION_QUALITY_BEST:
                return "Best";
            default:
                return "?=" + quality;
        }
    }

    public static int convertTextToChannelMask(String text) {
        return mChannelMaskStringToIntegerMap.get(text);
    }

    public String getPackageName() { return mPackageName; }
    public void setPackageName(String packageName) {
        this.mPackageName = packageName;
    }

    public String getAttributionTag() { return mAttributionTag; }
    public void setAttributionTag(String attributionTag) {
        this.mAttributionTag = attributionTag;
    }

    public String dump() {
        String prefix = (getDirection() == DIRECTION_INPUT) ? "in" : "out";
        StringBuffer message = new StringBuffer();
        message.append(String.format(Locale.getDefault(), "%s.channels = %d\n", prefix, mChannelCount));
        message.append(String.format(Locale.getDefault(), "%s.perf = %s\n", prefix,
                convertPerformanceModeToText(mPerformanceMode).toLowerCase(Locale.getDefault())));
        if (getDirection() == DIRECTION_INPUT) {
            message.append(String.format(Locale.getDefault(), "%s.preset = %s\n", prefix,
                    convertInputPresetToText(mInputPreset).toLowerCase(Locale.getDefault())));
        } else {
            message.append(String.format(Locale.getDefault(), "%s.usage = %s\n", prefix,
                    convertUsageToText(mUsage).toLowerCase(Locale.getDefault())));
            message.append(String.format(Locale.getDefault(), "%s.contentType = %s\n", prefix,
                    convertContentTypeToText(mContentType).toLowerCase(Locale.getDefault())));
        }
        message.append(String.format(Locale.getDefault(), "%s.sharing = %s\n", prefix,
                convertSharingModeToText(mSharingMode).toLowerCase(Locale.getDefault())));
        message.append(String.format(Locale.getDefault(), "%s.api = %s\n", prefix,
                convertNativeApiToText(getNativeApi()).toLowerCase(Locale.getDefault())));
        message.append(String.format(Locale.getDefault(), "%s.rate = %d\n", prefix, mSampleRate));
        message.append(String.format(Locale.getDefault(), "%s.device = %d\n", prefix, mDeviceId));
        message.append(String.format(Locale.getDefault(), "%s.devices = %s\n", prefix, convertDeviceIdsToText(mDeviceIds)));
        message.append(String.format(Locale.getDefault(), "%s.mmap = %s\n", prefix, isMMap() ? "yes" : "no"));
        message.append(String.format(Locale.getDefault(), "%s.rate.conversion.quality = %d\n", prefix, mRateConversionQuality));
        message.append(String.format(Locale.getDefault(), "%s.hardware.channels = %d\n", prefix, mHardwareChannelCount));
        message.append(String.format(Locale.getDefault(), "%s.hardware.sampleRate = %d\n", prefix, mHardwareSampleRate));
        message.append(String.format(Locale.getDefault(), "%s.hardware.format = %s\n", prefix,
                convertFormatToText(mHardwareFormat).toLowerCase(Locale.getDefault())));
        message.append(String.format(Locale.getDefault(), "%s.spatializationBehavior = %s\n", prefix,
                convertSpatializationBehaviorToText(mSpatializationBehavior).toLowerCase(Locale.getDefault())));
        message.append(String.format(Locale.getDefault(), "%s.packageName = %s\n", prefix,
                getPackageName()));
        message.append(String.format(Locale.getDefault(), "%s.attributionTag = %s\n", prefix,
                getAttributionTag()));
        return message.toString();
    }

    // text must match menu values
    public static final String NAME_INPUT_PRESET_GENERIC = "Generic";
    public static final String NAME_INPUT_PRESET_CAMCORDER = "Camcorder";
    public static final String NAME_INPUT_PRESET_VOICE_RECOGNITION = "VoiceRec";
    public static final String NAME_INPUT_PRESET_VOICE_COMMUNICATION = "VoiceComm";
    public static final String NAME_INPUT_PRESET_UNPROCESSED = "Unprocessed";
    public static final String NAME_INPUT_PRESET_VOICE_PERFORMANCE = "Performance";

    public static String convertInputPresetToText(int inputPreset) {
        switch(inputPreset) {
            case INPUT_PRESET_GENERIC:
                return NAME_INPUT_PRESET_GENERIC;
            case INPUT_PRESET_CAMCORDER:
                return NAME_INPUT_PRESET_CAMCORDER;
            case INPUT_PRESET_VOICE_RECOGNITION:
                return NAME_INPUT_PRESET_VOICE_RECOGNITION;
            case INPUT_PRESET_VOICE_COMMUNICATION:
                return NAME_INPUT_PRESET_VOICE_COMMUNICATION;
            case INPUT_PRESET_UNPROCESSED:
                return NAME_INPUT_PRESET_UNPROCESSED;
            case INPUT_PRESET_VOICE_PERFORMANCE:
                return NAME_INPUT_PRESET_VOICE_PERFORMANCE;
            default:
                return "Invalid";
        }
    }

    private static boolean matchInputPreset(String text, int preset) {
        return convertInputPresetToText(preset).toLowerCase(Locale.getDefault()).equals(text);
    }

    /**
     * Case insensitive.
     * @param text
     * @return inputPreset, eg. INPUT_PRESET_CAMCORDER
     */
    public static int convertTextToInputPreset(String text) {
        text = text.toLowerCase(Locale.getDefault());
        if (matchInputPreset(text, INPUT_PRESET_GENERIC)) {
            return INPUT_PRESET_GENERIC;
        } else if (matchInputPreset(text, INPUT_PRESET_CAMCORDER)) {
            return INPUT_PRESET_CAMCORDER;
        } else if (matchInputPreset(text, INPUT_PRESET_VOICE_RECOGNITION)) {
            return INPUT_PRESET_VOICE_RECOGNITION;
        } else if (matchInputPreset(text, INPUT_PRESET_VOICE_COMMUNICATION)) {
            return INPUT_PRESET_VOICE_COMMUNICATION;
        } else if (matchInputPreset(text, INPUT_PRESET_UNPROCESSED)) {
            return INPUT_PRESET_UNPROCESSED;
        } else if (matchInputPreset(text, INPUT_PRESET_VOICE_PERFORMANCE)) {
            return INPUT_PRESET_VOICE_PERFORMANCE;
        }
        return -1;
    }

    // text must match menu values
    public static final String NAME_SPATIALIZATION_BEHAVIOR_UNSPECIFIED = "Unspecified";
    public static final String NAME_SPATIALIZATION_BEHAVIOR_AUTO = "Auto";
    public static final String NAME_SPATIALIZATION_BEHAVIOR_NEVER = "Never";

    public static String convertSpatializationBehaviorToText(int spatializationBehavior) {
        switch(spatializationBehavior) {
            case UNSPECIFIED:
                return NAME_SPATIALIZATION_BEHAVIOR_UNSPECIFIED;
            case SPATIALIZATION_BEHAVIOR_AUTO:
                return NAME_SPATIALIZATION_BEHAVIOR_AUTO;
            case SPATIALIZATION_BEHAVIOR_NEVER:
                return NAME_SPATIALIZATION_BEHAVIOR_NEVER;
            default:
                return "Invalid";
        }
    }

    private static boolean matchSpatializationBehavior(String text, int spatializationBehavior) {
        return convertSpatializationBehaviorToText(spatializationBehavior).toLowerCase(Locale.getDefault()).equals(text);
    }

    /**
     * Case insensitive.
     * @param text
     * @return spatializationBehavior, eg. SPATIALIZATION_BEHAVIOR_NEVER
     */
    public static int convertTextToSpatializationBehavior(String text) {
        text = text.toLowerCase(Locale.getDefault());
        if (matchSpatializationBehavior(text, UNSPECIFIED)) {
            return UNSPECIFIED;
        } else if (matchSpatializationBehavior(text, SPATIALIZATION_BEHAVIOR_AUTO)) {
            return SPATIALIZATION_BEHAVIOR_AUTO;
        } else if (matchSpatializationBehavior(text, SPATIALIZATION_BEHAVIOR_NEVER)) {
            return SPATIALIZATION_BEHAVIOR_NEVER;
        }
        return -1;
    }

    public int getChannelCount() {
        return mChannelCount;
    }

    public void setChannelCount(int channelCount) {
        this.mChannelCount = channelCount;
    }

    public int getSampleRate() {
        return mSampleRate;
    }

    public void setSampleRate(int sampleRate) {
        this.mSampleRate = sampleRate;
    }

    public int getDeviceId() {
        return mDeviceId;
    }

    public void setDeviceId(int deviceId) {
        this.mDeviceId = deviceId;
    }

    public int[] getDeviceIds() {
        return mDeviceIds;
    }

    public void setDeviceIds(int[] deviceIds) {
        this.mDeviceIds = deviceIds;
    }

    public int getSessionId() {
        return mSessionId;
    }

    public void setSessionId(int sessionId) {
        mSessionId = sessionId;
    }

    public boolean isMMap() {
        return mMMap;
    }

    public void setMMap(boolean b) { mMMap = b; }

    public int getNativeApi() {
        return mNativeApi;
    }

    public void setNativeApi(int nativeApi) {
        mNativeApi = nativeApi;
    }

    public void setChannelConversionAllowed(boolean b) { mChannelConversionAllowed = b; }

    public boolean getChannelConversionAllowed() {
        return mChannelConversionAllowed;
    }

    public void setFormatConversionAllowed(boolean b) {
        mFormatConversionAllowed = b;
    }

    public boolean getFormatConversionAllowed() {
        return mFormatConversionAllowed;
    }

    public void setRateConversionQuality(int quality) { mRateConversionQuality = quality; }

    public int getRateConversionQuality() {
        return mRateConversionQuality;
    }

    public int getChannelMask() {
        return mChannelMask;
    }

    public void setChannelMask(int channelMask) {
        this.mChannelMask = channelMask;
    }

    public static List<String> getAllChannelMasks() {
        return mChannelMaskStrings;
    }

    public int getHardwareChannelCount() {
        return mHardwareChannelCount;
    }

    public void setHardwareChannelCount(int hardwareChannelCount) {
        this.mHardwareChannelCount = hardwareChannelCount;
    }

    public int getHardwareSampleRate() {
        return mHardwareSampleRate;
    }

    public void setHardwareSampleRate(int hardwareSampleRate) {
        this.mHardwareSampleRate = hardwareSampleRate;
    }

    public int getHardwareFormat() {
        return mHardwareFormat;
    }

    public void setHardwareFormat(int hardwareFormat) {
        this.mHardwareFormat = hardwareFormat;
    }

    static String convertErrorToText(int error) {
        switch (error) {
            case ERROR_BASE:
                return "ErrorBase";
            case ERROR_DISCONNECTED:
                return "ErrorDisconnected";
            case ERROR_ILLEGAL_ARGUMENT:
                return "ErrorIllegalArgument";
            case ERROR_INTERNAL:
                return "ErrorInternal";
            case ERROR_INVALID_STATE:
                return "ErrorInvalidState";
            case ERROR_INVALID_HANDLE:
                return "ErrorInvalidHandle";
            case ERROR_UNIMPLEMENTED:
                return "ErrorUnimplemented";
            case ERROR_UNAVAILABLE:
                return "ErrorUnavailable";
            case ERROR_NO_FREE_HANDLES:
                return "ErrorNoFreeHandles";
            case ERROR_NO_MEMORY:
                return "ErrorNoMemory";
            case ERROR_NULL:
                return "ErrorNull";
            case ERROR_TIMEOUT:
                return "ErrorTimeout";
            case ERROR_WOULD_BLOCK:
                return "ErrorWouldBlock";
            case ERROR_INVALID_FORMAT:
                return "ErrorInvalidFormat";
            case ERROR_OUT_OF_RANGE:
                return "ErrorOutOfRange";
            case ERROR_NO_SERVICE:
                return "ErrorNoService";
            case ERROR_INVALID_RATE:
                return "ErrorInvalidRate";
            case ERROR_CLOSED:
                return "ErrorClosed";
            case ERROR_OK:
                return "ErrorOk";
            default:
                return "?=" + error;
        }
    }

    public static String convertDeviceIdsToText(int[] deviceIds) {
        if (deviceIds == null || deviceIds.length == 0) {
            return "[]";
        }

        List<String> deviceIdStrings = new ArrayList<>();
        for (int deviceId : deviceIds) {
            deviceIdStrings.add(String.valueOf(deviceId));
        }

        String joinedIds = TextUtils.join(",", deviceIdStrings);
        return "[" + joinedIds + "]";
    }
}
