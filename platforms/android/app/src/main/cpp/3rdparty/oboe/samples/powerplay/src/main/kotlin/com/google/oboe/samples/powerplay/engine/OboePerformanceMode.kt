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

package com.google.oboe.samples.powerplay.engine

/**
 * The performance mode of the audio stream. This is the kotlin version
 */
enum class OboePerformanceMode(val value: Int) {
    /**
     * No particular performance needs. Default.
     */
    None(0), // AAUDIO_PERFORMANCE_MODE_NONE,

    /**
     * Reducing latency is most important.
     */
    LowLatency(1), // AAUDIO_PERFORMANCE_MODE_LOW_LATENCY


    /**
     * Extending battery life is most important.
     */
    PowerSaving(2), // AAUDIO_PERFORMANCE_MODE_POWER_SAVING,

    /**
     * Extending battery life is more important than low latency.
     *
     * This mode is not supported in input streams.
     * This mode will play through the offloaded audio path to save battery life.
     * With the offload playback, the default data callback size will be large and it
     * allows data feeding thread to sleep longer time after sending enough data.
     */
    PowerSavingOffloaded(3); // AAUDIO_PERFORMANCE_MODE_POWER_SAVING_OFFLOADED

    companion object {
        fun fromInt(value: Int) = entries.first { it.value == value }
    }
}
