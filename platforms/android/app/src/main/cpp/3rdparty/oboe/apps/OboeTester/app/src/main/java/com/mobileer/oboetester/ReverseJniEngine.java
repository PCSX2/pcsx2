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

package com.mobileer.oboetester;

import android.util.Log;

public class ReverseJniEngine {

    private static final String TAG = "ReverseJniEngine";
    private long mNativeEngineHandle = 0;

    private static final int MAX_BUFFER_SIZE = 1920; // e.g., 10 bursts of 192 frames
    private final float[] mAudioBuffer;

    private static final int CHANNEL_COUNT = 2;
    private double[] mPhase;
    private double[] mPhaseIncrement;
    private final double[] mFrequencies = {440.0, 523.25}; // A4 and C5 for stereo

    private int mXRunCount = 0;
    private final int mSampleRate = 48000;


    // Load the native library
    static {
        System.loadLibrary("oboetester");
    }

    public ReverseJniEngine() {
        // Use two buffers to synchronize between java and native. This lets us
        mAudioBuffer = new float[MAX_BUFFER_SIZE * CHANNEL_COUNT];
        mPhase = new double[CHANNEL_COUNT];
        mPhaseIncrement = new double[CHANNEL_COUNT];
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            mPhase[i] = 0.0;
            double frequency = (i < mFrequencies.length) ? mFrequencies[i] : mFrequencies[0] * (i + 1);
            mPhaseIncrement[i] = 2 * Math.PI * frequency / mSampleRate;
        }
    }

    public void create() {
        if (mNativeEngineHandle == 0) {
            mNativeEngineHandle = createEngine(CHANNEL_COUNT);
            Log.i(TAG, "Created native engine with handle: " + mNativeEngineHandle);
            setAudioBuffer(mNativeEngineHandle, mAudioBuffer);
            Log.i(TAG, "Passed audio buffers to native engine.");
        }
    }

    public void start(int bufferSizeInBursts, int sleepDurationUs) {
        if (mNativeEngineHandle != 0) {
            startEngine(mNativeEngineHandle, bufferSizeInBursts, sleepDurationUs);
            Log.i(TAG, "Started native engine.");
        }
    }

    public void stop() {
        if (mNativeEngineHandle != 0) {
            stopEngine(mNativeEngineHandle);
            Log.i(TAG, "Stopped native engine.");
        }
    }

    public void destroy() {
        if (mNativeEngineHandle != 0) {
            deleteEngine(mNativeEngineHandle);
            mNativeEngineHandle = 0;
            Log.i(TAG, "Destroyed native engine.");
        }
    }

    public void setBufferSizeInBursts(int bufferSizeInBursts) {
        if (mNativeEngineHandle != 0) {
            setBufferSizeInBursts(mNativeEngineHandle, bufferSizeInBursts);
            Log.i(TAG, "SetBufferSizeInBursts:" + bufferSizeInBursts);
        }
    }

    public int getXRunCount() {
        return mXRunCount;
    }

    public void setSleepDurationUs(int sleepDurationUs) {
        if (mNativeEngineHandle != 0) {
            setSleepDurationUs(mNativeEngineHandle, sleepDurationUs);
            Log.i(TAG, "setSleepDurationUs:" + sleepDurationUs);
        }
    }

    /**
     * Called from JNI. Fills the internal buffer with stereo audio data.
     *
     * @param numFrames The number of frames the native side needs.
     * @param totalXRunCount The current x-run count from Oboe.
     */
    @SuppressWarnings("unused") // Called from JNI
    private void onAudioReady(int numFrames, int totalXRunCount) {
        for (int i = 0; i < numFrames; i++) {
            for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
                mAudioBuffer[(i * CHANNEL_COUNT) + ch] = (float) Math.sin(mPhase[ch]);
                mPhase[ch] += mPhaseIncrement[ch];
                if (mPhase[ch] > 2 * Math.PI) {
                    mPhase[ch] -= 2 * Math.PI;
                }
            }
        }
        mXRunCount = totalXRunCount;
    }

    // Native methods that are implemented in jni-bridge.cpp
    private native long createEngine(int channelCount);
    private native void startEngine(long enginePtr, int bufferSizeInBursts, int sleepDurationUs);
    private native void stopEngine(long enginePtr);
    private native void deleteEngine(long enginePtr);
    private native void setBufferSizeInBursts(long enginePtr, int bufferSizeInBursts);
    private native void setSleepDurationUs(long enginePtr, int sleepDurationUs);
    private native void setAudioBuffer(long enginePtr, float[] buffer);
}
