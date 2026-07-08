/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.mobileer.miditools.synth;

/**
 * Abstract control over the audio latency.
 */
public abstract class LatencyController {
    private boolean mLowLatencyEnabled;
    private boolean mAutoSizeEnabled;

    public void setLowLatencyEnabled(boolean enabled) {
        mLowLatencyEnabled = enabled;
    }

    public boolean isLowLatencyEnabled() {
        return mLowLatencyEnabled;
    }

    /**
     * If true then adjust latency to lowest value that does not produce underruns.
     *
     * @param enabled
     */
    public void setAutoSizeEnabled(boolean enabled) {
        mAutoSizeEnabled = enabled;
    }

    public boolean isAutoSizeEnabled() {
        return mAutoSizeEnabled;
    }

    /**
     * @return true if this version supports the LOW_LATENCY flag
     */
    public abstract boolean isLowLatencySupported();

    /**
     * The amount of the buffer capacity that is being used.
     * @return
     */
    public abstract int getBufferSizeInFrames();

    /**
     * The allocated size of the buffer.
     * @return
     */
    public abstract int getBufferCapacityInFrames();

    public abstract int getUnderrunCount();

    /**
     * When the output is running, the LOW_LATENCY flag cannot be set.
     * @return
     */
    public abstract boolean isRunning();

    /**
     * Calculate the percentage of time that the a CPU is calculating data.
     * @return percent CPU load
     */
    public abstract int getCpuLoad();
}
