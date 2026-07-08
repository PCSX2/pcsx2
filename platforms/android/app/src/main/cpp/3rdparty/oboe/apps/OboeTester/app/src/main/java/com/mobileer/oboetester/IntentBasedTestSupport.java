/*
 * Copyright 2022 The Android Open Source Project
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

import android.media.AudioManager;
import android.os.Bundle;

public class IntentBasedTestSupport {

    public static final String KEY_IN_SHARING = "in_sharing";
    public static final String KEY_OUT_SHARING = "out_sharing";
    public static final String VALUE_SHARING_EXCLUSIVE = "exclusive";
    public static final String VALUE_SHARING_SHARED = "shared";

    public static final String KEY_IN_PERF = "in_perf";
    public static final String KEY_OUT_PERF = "out_perf";
    public static final String VALUE_PERF_LOW_LATENCY = "lowlat";
    public static final String VALUE_PERF_POWERSAVE = "powersave";
    public static final String VALUE_PERF_NONE = "none";
    public static final String VALUE_PERF_POWERSAVE_OFFLOAD = "powersave_offload";

    public static final String KEY_IN_CHANNELS = "in_channels";
    public static final String KEY_OUT_CHANNELS = "out_channels";
    public static final int VALUE_DEFAULT_CHANNELS = 2;

    public static final String KEY_IN_USE_MMAP = "in_use_mmap";
    public static final String KEY_OUT_USE_MMAP = "out_use_mmap";
    public static final boolean VALUE_DEFAULT_USE_MMAP = NativeEngine.isMMapSupported();

    public static final String KEY_IN_PRESET = "in_preset";
    public static final String KEY_SAMPLE_RATE = "sample_rate";
    public static final int VALUE_DEFAULT_SAMPLE_RATE = 48000;
    public static final String VALUE_UNSPECIFIED = "unspecified";

    public static final String KEY_OUT_USAGE = "out_usage";
    public static final String VALUE_USAGE_MEDIA = "media";
    public static final String VALUE_USAGE_VOICE_COMMUNICATION = "voice_communication";
    public static final String VALUE_USAGE_ALARM = "alarm";
    public static final String VALUE_USAGE_NOTIFICATION = "notification";
    public static final String VALUE_USAGE_GAME = "game";

    public static final String KEY_IN_API = "in_api";
    public static final String KEY_OUT_API = "out_api";
    public static final String VALUE_API_AAUDIO = "aaudio";
    public static final String VALUE_API_OPENSLES = "opensles";

    public static final String KEY_FILE_NAME = "file";
    public static final String KEY_BUFFER_BURSTS = "buffer_bursts";
    public static final String KEY_BUFFER_FRAMES = "buffer_frames";
    public static final String KEY_BACKGROUND = "background";
    public static final String KEY_FOREGROUND_SERVICE = "foreground_service";
    public static final String KEY_VOLUME = "volume";
    public static final String KEY_RESTART_STREAM_IF_CLOSED = "restart_if_closed";
    public static final String KEY_AUDIO_FOCUS = "audio_focus";

    public static final String KEY_VOLUME_TYPE = "volume_type";
    public static final float VALUE_VOLUME_INVALID = -1.0f;
    public static final String VALUE_VOLUME_TYPE_ACCESSIBILITY = "accessibility";
    public static final String VALUE_VOLUME_TYPE_ALARM = "alarm";
    public static final String VALUE_VOLUME_TYPE_DTMF = "dtmf";
    public static final String VALUE_VOLUME_TYPE_MUSIC = "music";
    public static final String VALUE_VOLUME_TYPE_NOTIFICATION = "notification";
    public static final String VALUE_VOLUME_TYPE_RING = "ring";
    public static final String VALUE_VOLUME_TYPE_SYSTEM = "system";
    public static final String VALUE_VOLUME_TYPE_VOICE_CALL = "voice_call";

    public static final String KEY_IN_CHANNEL_MASK = "in_channel_mask";
    public static final String KEY_OUT_CHANNEL_MASK = "out_channel_mask";
    public static final String VALUE_CHANNEL_MONO = "mono";
    public static final String VALUE_CHANNEL_STEREO = "stereo";
    public static final String VALUE_CHANNEL_2POINT1 = "2.1";
    public static final String VALUE_CHANNEL_TRI = "tri";
    public static final String VALUE_CHANNEL_TRI_BACK = "triBack";
    public static final String VALUE_CHANNEL_TRI_BACK_LOWERCASE = "triback";
    public static final String VALUE_CHANNEL_3POINT1 = "3.1";
    public static final String VALUE_CHANNEL_2POINT0POINT2 = "2.0.2";
    public static final String VALUE_CHANNEL_2POINT1POINT2 = "2.1.2";
    public static final String VALUE_CHANNEL_3POINT0POINT2 = "3.0.2";
    public static final String VALUE_CHANNEL_3POINT1POINT2 = "3.1.2";
    public static final String VALUE_CHANNEL_QUAD = "quad";
    public static final String VALUE_CHANNEL_QUAD_SIDE = "quadSide";
    public static final String VALUE_CHANNEL_QUAD_SIDE_LOWERCASE = "quadside";
    public static final String VALUE_CHANNEL_SURROUND = "surround";
    public static final String VALUE_CHANNEL_PENTA = "penta";
    public static final String VALUE_CHANNEL_5POINT1 = "5.1";
    public static final String VALUE_CHANNEL_5POINT1_SIDE = "5.1Side";
    public static final String VALUE_CHANNEL_5POINT1_SIDE_LOWERCASE = "5.1side";
    public static final String VALUE_CHANNEL_6POINT1 = "6.1";
    public static final String VALUE_CHANNEL_7POINT1 = "7.1";
    public static final String VALUE_CHANNEL_5POINT1POINT2 = "5.1.2";
    public static final String VALUE_CHANNEL_5POINT1POINT4 = "5.1.4";
    public static final String VALUE_CHANNEL_7POINT1POINT2 = "7.1.2";
    public static final String VALUE_CHANNEL_7POINT1POINT4 = "7.1.4";
    public static final String VALUE_CHANNEL_9POINT1POINT4 = "9.1.4";
    public static final String VALUE_CHANNEL_9POINT1POINT6 = "9.1.6";
    public static final String VALUE_CHANNEL_FRONT_BACK = "frontBack";
    public static final String VALUE_CHANNEL_FRONT_BACK_LOWERCASE = "frontback";

    public static final String KEY_SIGNAL_TYPE = "signal_type";
    public static final String VALUE_SIGNAL_SINE = "sine";
    public static final String VALUE_SIGNAL_SAWTOOTH = "sawtooth";
    public static final String VALUE_SIGNAL_FREQ_SWEEP = "freq_sweep";
    public static final String VALUE_SIGNAL_PITCH_SWEEP = "pitch_sweep";
    public static final String VALUE_SIGNAL_WHITE_NOISE = "white_noise";

    public static final String KEY_DURATION = "duration";
    public static final int VALUE_DEFAULT_DURATION = 10;

    public static final String KEY_OUT_FORMAT = "out_format";
    public static final String KEY_IN_FORMAT = "in_format";
    public static final String VALUE_FORMAT_PCM_16_BIT = "pcm_16_bit";
    public static final String VALUE_FORMAT_PCM_FLOAT = "pcm_float";
    public static final String VALUE_FORMAT_PCM_24_BIT = "pcm_24_bit";
    public static final String VALUE_FORMAT_PCM_32_BIT = "pcm_32_bit";
    public static final String VALUE_FORMAT_IEC61937 = "iec61937";
    public static final String VALUE_FORMAT_MP3 = "mp3";

    public static final String KEY_BUFFER_CAPACITY = "buffer_capacity";

    public static int getApiFromText(String text) {
        if (VALUE_API_AAUDIO.equals(text)) {
            return StreamConfiguration.NATIVE_API_AAUDIO;
        } else if (VALUE_API_OPENSLES.equals(text)) {
            return StreamConfiguration.NATIVE_API_OPENSLES;
        } else {
            return StreamConfiguration.NATIVE_API_UNSPECIFIED;
        }
    }

    public static int getPerfFromText(String text) {
        if (VALUE_PERF_NONE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_NONE;
        } else if (VALUE_PERF_POWERSAVE.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_POWER_SAVING;
        } else if (VALUE_PERF_LOW_LATENCY.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY;
        } else if (VALUE_PERF_POWERSAVE_OFFLOAD.equals(text)) {
            return StreamConfiguration.PERFORMANCE_MODE_POWER_SAVING_OFFLOAD;
        } else {
            throw new IllegalArgumentException("perf mode invalid: " + text);
        }
    }

    public static int getSharingFromText(String text) {
        if (VALUE_SHARING_SHARED.equals(text)) {
            return StreamConfiguration.SHARING_MODE_SHARED;
        } else {
            return StreamConfiguration.SHARING_MODE_EXCLUSIVE;
        }
    }
    public static int getUsageFromText(String text) {
        if (VALUE_USAGE_GAME.equals(text)) {
            return StreamConfiguration.USAGE_GAME;
        } else if (VALUE_USAGE_VOICE_COMMUNICATION.equals(text)) {
            return StreamConfiguration.USAGE_VOICE_COMMUNICATION;
        } else if (VALUE_USAGE_MEDIA.equals(text)) {
            return StreamConfiguration.USAGE_MEDIA;
        } else if (VALUE_USAGE_ALARM.equals(text)) {
            return StreamConfiguration.USAGE_ALARM;
        } else if (VALUE_USAGE_NOTIFICATION.equals(text)) {
            return StreamConfiguration.USAGE_NOTIFICATION;
        } else {
            return StreamConfiguration.UNSPECIFIED;
        }
    }

    public static void configureStreamsFromBundle(Bundle bundle,
                                                  StreamConfiguration requestedInConfig,
                                                  StreamConfiguration requestedOutConfig) {
        configureInputStreamFromBundle(bundle, requestedInConfig);
        configureOutputStreamFromBundle(bundle, requestedOutConfig);
    }

    public static float getNormalizedVolumeFromBundle(Bundle bundle) {
        return bundle.getFloat(KEY_VOLUME, VALUE_VOLUME_INVALID);
    }

    /**
     * @param bundle
     * @return AudioManager.STREAM type or throw IllegalArgumentException
     */
    public static int getVolumeStreamTypeFromBundle(Bundle bundle) {
        String typeText = bundle.getString(KEY_VOLUME_TYPE, VALUE_VOLUME_TYPE_MUSIC);
        switch (typeText) {
            case VALUE_VOLUME_TYPE_ACCESSIBILITY:
                return AudioManager.STREAM_ACCESSIBILITY;
            case VALUE_VOLUME_TYPE_ALARM:
                return AudioManager.STREAM_ALARM;
            case VALUE_VOLUME_TYPE_DTMF:
                return AudioManager.STREAM_DTMF;
            case VALUE_VOLUME_TYPE_MUSIC:
                return AudioManager.STREAM_MUSIC;
            case VALUE_VOLUME_TYPE_NOTIFICATION:
                return AudioManager.STREAM_NOTIFICATION;
            case VALUE_VOLUME_TYPE_RING:
                return AudioManager.STREAM_RING;
            case VALUE_VOLUME_TYPE_SYSTEM:
                return AudioManager.STREAM_SYSTEM;
            case VALUE_VOLUME_TYPE_VOICE_CALL:
                return AudioManager.STREAM_VOICE_CALL;
            default:
               throw new IllegalArgumentException(KEY_VOLUME_TYPE + " invalid: " + typeText);
        }
    }

    public static int getChannelMaskFromBundle(Bundle bundle, String channelMaskKey) {
        String channelMaskText = bundle.getString(channelMaskKey);
        if (channelMaskText == null) {
            return StreamConfiguration.UNSPECIFIED;
        }
        switch (channelMaskText) {
            case VALUE_CHANNEL_MONO:
                return StreamConfiguration.CHANNEL_MONO;
            case VALUE_CHANNEL_STEREO:
                return StreamConfiguration.CHANNEL_STEREO;
            case VALUE_CHANNEL_2POINT1:
                return StreamConfiguration.CHANNEL_2POINT1;
            case VALUE_CHANNEL_TRI:
                return StreamConfiguration.CHANNEL_TRI;
            case VALUE_CHANNEL_TRI_BACK:
            case VALUE_CHANNEL_TRI_BACK_LOWERCASE:
                return StreamConfiguration.CHANNEL_TRI_BACK;
            case VALUE_CHANNEL_3POINT1:
                return StreamConfiguration.CHANNEL_3POINT1;
            case VALUE_CHANNEL_2POINT0POINT2:
                return StreamConfiguration.CHANNEL_2POINT0POINT2;
            case VALUE_CHANNEL_2POINT1POINT2:
                return StreamConfiguration.CHANNEL_2POINT1POINT2;
            case VALUE_CHANNEL_3POINT0POINT2:
                return StreamConfiguration.CHANNEL_3POINT0POINT2;
            case VALUE_CHANNEL_3POINT1POINT2:
                return StreamConfiguration.CHANNEL_3POINT1POINT2;
            case VALUE_CHANNEL_QUAD:
                return StreamConfiguration.CHANNEL_QUAD;
            case VALUE_CHANNEL_QUAD_SIDE:
            case VALUE_CHANNEL_QUAD_SIDE_LOWERCASE:
                return StreamConfiguration.CHANNEL_QUAD_SIDE;
            case VALUE_CHANNEL_SURROUND:
                return StreamConfiguration.CHANNEL_SURROUND;
            case VALUE_CHANNEL_PENTA:
                return StreamConfiguration.CHANNEL_PENTA;
            case VALUE_CHANNEL_5POINT1:
                return StreamConfiguration.CHANNEL_5POINT1;
            case VALUE_CHANNEL_5POINT1_SIDE:
            case VALUE_CHANNEL_5POINT1_SIDE_LOWERCASE:
                return StreamConfiguration.CHANNEL_5POINT1_SIDE;
            case VALUE_CHANNEL_6POINT1:
                return StreamConfiguration.CHANNEL_6POINT1;
            case VALUE_CHANNEL_7POINT1:
                return StreamConfiguration.CHANNEL_7POINT1;
            case VALUE_CHANNEL_5POINT1POINT2:
                return StreamConfiguration.CHANNEL_5POINT1POINT2;
            case VALUE_CHANNEL_5POINT1POINT4:
                return StreamConfiguration.CHANNEL_5POINT1POINT4;
            case VALUE_CHANNEL_7POINT1POINT2:
                return StreamConfiguration.CHANNEL_7POINT1POINT2;
            case VALUE_CHANNEL_7POINT1POINT4:
                return StreamConfiguration.CHANNEL_7POINT1POINT4;
            case VALUE_CHANNEL_9POINT1POINT4:
                return StreamConfiguration.CHANNEL_9POINT1POINT4;
            case VALUE_CHANNEL_9POINT1POINT6:
                return StreamConfiguration.CHANNEL_9POINT1POINT6;
            case VALUE_CHANNEL_FRONT_BACK:
            case VALUE_CHANNEL_FRONT_BACK_LOWERCASE:
                return StreamConfiguration.CHANNEL_FRONT_BACK;
            default:
                throw new IllegalArgumentException(
                        channelMaskKey + " invalid: " + channelMaskText);
        }
    }

    public static int getFormatFromText(String text) {
        if (VALUE_FORMAT_PCM_16_BIT.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_PCM_16;
        } else if (VALUE_FORMAT_PCM_FLOAT.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_PCM_FLOAT;
        } else if (VALUE_FORMAT_PCM_24_BIT.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_PCM_24;
        } else if (VALUE_FORMAT_PCM_32_BIT.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_PCM_32;
        } else if (VALUE_FORMAT_IEC61937.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_IEC61937;
        } else if (VALUE_FORMAT_MP3.equals(text)) {
            return StreamConfiguration.AUDIO_FORMAT_MP3;
        } else {
            return StreamConfiguration.UNSPECIFIED;
        }
    }

    public static void configureOutputStreamFromBundle(Bundle bundle,
                                                        StreamConfiguration requestedOutConfig) {
        int audioApi;
        String text;

        requestedOutConfig.reset();

        int sampleRate = bundle.getInt(KEY_SAMPLE_RATE, VALUE_DEFAULT_SAMPLE_RATE);
        requestedOutConfig.setSampleRate(sampleRate);

        text = bundle.getString(KEY_OUT_API, VALUE_UNSPECIFIED);
        audioApi = getApiFromText(text);
        requestedOutConfig.setNativeApi(audioApi);

        int outChannels = bundle.getInt(KEY_OUT_CHANNELS, VALUE_DEFAULT_CHANNELS);
        int channelMask = getChannelMaskFromBundle(bundle, KEY_OUT_CHANNEL_MASK);
        // Respect channel mask when it is specified.
        if (channelMask != StreamConfiguration.UNSPECIFIED) {
            requestedOutConfig.setChannelMask(channelMask);
        } else {
            requestedOutConfig.setChannelCount(outChannels);
        }

        boolean outMMAP = bundle.getBoolean(KEY_OUT_USE_MMAP, VALUE_DEFAULT_USE_MMAP);
        requestedOutConfig.setMMap(outMMAP);

        text = bundle.getString(KEY_OUT_PERF, VALUE_PERF_LOW_LATENCY);
        int perfMode = getPerfFromText(text);
        requestedOutConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_OUT_SHARING, VALUE_SHARING_EXCLUSIVE);
        int sharingMode = getSharingFromText(text);
        requestedOutConfig.setSharingMode(sharingMode);

        text = bundle.getString(KEY_OUT_USAGE, VALUE_USAGE_MEDIA);
        int usage = getUsageFromText(text);
        requestedOutConfig.setUsage(usage);

        text = bundle.getString(KEY_OUT_FORMAT, "");
        requestedOutConfig.setFormat(getFormatFromText(text));

        int bufferCapacity = bundle.getInt(KEY_BUFFER_CAPACITY, 0);
        requestedOutConfig.setBufferCapacityInFrames(bufferCapacity);
    }

    public static void configureInputStreamFromBundle(Bundle bundle,
                                                       StreamConfiguration requestedInConfig) {
        int audioApi;
        String text;

        requestedInConfig.reset();

        int sampleRate = bundle.getInt(KEY_SAMPLE_RATE, VALUE_DEFAULT_SAMPLE_RATE);
        requestedInConfig.setSampleRate(sampleRate);

        text = bundle.getString(KEY_IN_API, VALUE_UNSPECIFIED);
        audioApi = getApiFromText(text);
        requestedInConfig.setNativeApi(audioApi);

        int inChannels = bundle.getInt(KEY_IN_CHANNELS, VALUE_DEFAULT_CHANNELS);
        int channelMask = getChannelMaskFromBundle(bundle, KEY_IN_CHANNEL_MASK);
        // Respect channel mask when it is specified.
        if (channelMask != StreamConfiguration.UNSPECIFIED) {
            requestedInConfig.setChannelMask(channelMask);
        } else {
            requestedInConfig.setChannelCount(inChannels);
        }

        boolean inMMAP = bundle.getBoolean(KEY_IN_USE_MMAP, VALUE_DEFAULT_USE_MMAP);
        requestedInConfig.setMMap(inMMAP);

        text = bundle.getString(KEY_IN_PERF, VALUE_PERF_LOW_LATENCY);
        int perfMode = getPerfFromText(text);
        requestedInConfig.setPerformanceMode(perfMode);

        text = bundle.getString(KEY_IN_SHARING, VALUE_SHARING_EXCLUSIVE);
        int sharingMode = getSharingFromText(text);
        requestedInConfig.setSharingMode(sharingMode);

        String defaultText = StreamConfiguration.convertInputPresetToText(
                StreamConfiguration.INPUT_PRESET_VOICE_RECOGNITION);
        text = bundle.getString(KEY_IN_PRESET, defaultText);
        int inputPreset = StreamConfiguration.convertTextToInputPreset(text);
        if (inputPreset < 0) throw new IllegalArgumentException(KEY_IN_PRESET + " invalid: " + text);
        requestedInConfig.setInputPreset(inputPreset);

        text = bundle.getString(KEY_IN_FORMAT, "");
        requestedInConfig.setFormat(getFormatFromText(text));

        int bufferCapacity = bundle.getInt(KEY_BUFFER_CAPACITY, 0);
        requestedInConfig.setBufferCapacityInFrames(bufferCapacity);
    }

    public static int getSignalTypeFromBundle(Bundle bundle) {
        String signalTypeText = bundle.getString(KEY_SIGNAL_TYPE);
        if (signalTypeText == null) {
            return 0;
        }
        switch (signalTypeText) {
            case VALUE_SIGNAL_SINE:
                return 0;
            case VALUE_SIGNAL_SAWTOOTH:
                return 1;
            case VALUE_SIGNAL_FREQ_SWEEP:
                return 2;
            case VALUE_SIGNAL_PITCH_SWEEP:
                return 3;
            case VALUE_SIGNAL_WHITE_NOISE:
                return 4;
            default:
                throw new IllegalArgumentException(
                        KEY_SIGNAL_TYPE + " invalid: " + signalTypeText);
        }
    }

    public static int getDurationSeconds(Bundle bundle) {
        return bundle.getInt(KEY_DURATION, VALUE_DEFAULT_DURATION);
    }

    public static int getBurstCount(Bundle bundle) {
        return bundle.getInt(KEY_BUFFER_BURSTS, 0);
    }

    public static int getBufferFrameCount(Bundle bundle) {
        return bundle.getInt(KEY_BUFFER_FRAMES, 0);
    }
}
