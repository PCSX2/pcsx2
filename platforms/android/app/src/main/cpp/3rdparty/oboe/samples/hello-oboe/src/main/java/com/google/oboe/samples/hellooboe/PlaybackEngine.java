package com.google.oboe.samples.hellooboe;
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

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;

public class PlaybackEngine {
    // Load native library
    static {
        System.loadLibrary("hello-oboe");
    }

    static void setDefaultStreamValues(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1){
            AudioManager myAudioMgr = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            String sampleRateStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int defaultSampleRate = Integer.parseInt(sampleRateStr);
            String framesPerBurstStr = myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int defaultFramesPerBurst = Integer.parseInt(framesPerBurstStr);

            setDefaultStreamValues(defaultSampleRate, defaultFramesPerBurst);
        }
    }


    // Native methods that require audioserver calls and might take several seconds.
    static native int startEngine(int audioApi, int deviceId, int channelCount);
    static native int stopEngine();

    // Native methods that only talk to the native client code.
    static native void setToneOn(boolean isToneOn);
    static native void setBufferSizeInBursts(int bufferSizeInBursts);
    static native double getCurrentOutputLatencyMillis();
    static native boolean isLatencyDetectionSupported();
    static native boolean isAAudioRecommended();
    static native void setDefaultStreamValues(int sampleRate, int framesPerBurst);
}
