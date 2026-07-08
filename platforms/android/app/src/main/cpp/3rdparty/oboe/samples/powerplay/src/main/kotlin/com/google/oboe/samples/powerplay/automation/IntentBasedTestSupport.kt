/*
 * Copyright 2026 The Android Open Source Project
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

package com.google.oboe.samples.powerplay.automation

import android.os.Bundle
import com.google.oboe.samples.powerplay.engine.OboePerformanceMode

/**
 * Constants and utilities for Intent-based automation of the PowerPlay sample app.
 *
 * This enables ADB command-line control for testing power-efficient audio features.
 *
 * Example usage:
 * ```
 * adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
 *     --es command play \
 *     --es perf_mode offload \
 *     --ez background true
 * ```
 */
object IntentBasedTestSupport {

    // ============================================================================
    // Command Keys - Intent extra names for ADB commands
    // ============================================================================

    /** Command to execute: "play", "pause", "stop" */
    const val KEY_COMMAND = "command"

    /** Song index to play (0-2 for built-in playlist) */
    const val KEY_SONG_INDEX = "song_index"

    /** Performance mode: "none", "lowlat", "powersave", "offload" */
    const val KEY_PERF_MODE = "perf_mode"

    /** Volume percentage (0-100) */
    const val KEY_VOLUME = "volume"

    /** Move activity to background after starting playback */
    const val KEY_BACKGROUND = "background"

    /** Duration in milliseconds - auto-stop after this time */
    const val KEY_DURATION_MS = "duration_ms"

    /** Enable/disable MMAP audio path */
    const val KEY_USE_MMAP = "use_mmap"

    /** Buffer size in frames (only applicable in PCM Offload mode) */
    const val KEY_BUFFER_FRAMES = "buffer_frames"

    // ============================================================================
    // Command Values
    // ============================================================================

    const val COMMAND_PLAY = "play"
    const val COMMAND_PAUSE = "pause"
    const val COMMAND_STOP = "stop"

    // ============================================================================
    // Performance Mode Values
    // ============================================================================

    const val PERF_MODE_NONE = "none"
    const val PERF_MODE_LOW_LATENCY = "lowlat"
    const val PERF_MODE_POWER_SAVING = "powersave"
    const val PERF_MODE_OFFLOAD = "offload"

    // ============================================================================
    // Default Values
    // ============================================================================

    const val DEFAULT_SONG_INDEX = 0
    const val DEFAULT_VOLUME = 100
    const val DEFAULT_DURATION_MS = -1 // No auto-stop

    // ============================================================================
    // Status Log Tags for Machine Parsing
    // ============================================================================

    /** Log tag for PowerPlay automation */
    const val LOG_TAG = "PowerPlay"

    /** Prefix for machine-readable status logs */
    const val STATUS_PREFIX = "POWERPLAY_STATUS:"

    // Status values
    const val STATUS_PLAYING = "PLAYING"
    const val STATUS_PAUSED = "PAUSED"
    const val STATUS_STOPPED = "STOPPED"
    const val STATUS_ERROR = "ERROR"
    const val STATUS_OFFLOAD_REVOKED = "OFFLOAD_REVOKED"

    // ============================================================================
    // Utility Functions
    // ============================================================================

    /**
     * Parse performance mode string from Intent extra to OboePerformanceMode enum.
     *
     * @param text Performance mode string ("none", "lowlat", "powersave", "offload")
     * @return Corresponding OboePerformanceMode, defaults to None if invalid
     */
    fun getPerformanceModeFromText(text: String?): OboePerformanceMode {
        return when (text?.lowercase()) {
            PERF_MODE_NONE -> OboePerformanceMode.None
            PERF_MODE_LOW_LATENCY -> OboePerformanceMode.LowLatency
            PERF_MODE_POWER_SAVING -> OboePerformanceMode.PowerSaving
            PERF_MODE_OFFLOAD -> OboePerformanceMode.PowerSavingOffloaded
            else -> OboePerformanceMode.None
        }
    }

    /**
     * Get song index from bundle with bounds checking.
     *
     * @param bundle Intent extras bundle
     * @param maxIndex Maximum valid index (exclusive)
     * @return Valid song index, clamped to valid range
     */
    fun getSongIndex(bundle: Bundle, maxIndex: Int): Int {
        val index = bundle.getInt(KEY_SONG_INDEX, DEFAULT_SONG_INDEX)
        return index.coerceIn(0, maxIndex - 1)
    }

    /**
     * Get volume from bundle, normalized to 0.0-1.0 range.
     *
     * @param bundle Intent extras bundle
     * @return Volume as float (0.0 to 1.0)
     */
    fun getNormalizedVolume(bundle: Bundle): Float {
        val volume = bundle.getInt(KEY_VOLUME, DEFAULT_VOLUME)
        return (volume.coerceIn(0, 100) / 100.0f)
    }

    /**
     * Get command from bundle.
     *
     * @param bundle Intent extras bundle
     * @return Command string or null if not present
     */
    fun getCommand(bundle: Bundle): String? {
        return bundle.getString(KEY_COMMAND)
    }

    /**
     * Get performance mode from bundle.
     *
     * @param bundle Intent extras bundle
     * @return OboePerformanceMode from bundle, defaults to None
     */
    fun getPerformanceMode(bundle: Bundle): OboePerformanceMode {
        return getPerformanceModeFromText(bundle.getString(KEY_PERF_MODE))
    }

    /**
     * Check if background mode is requested.
     *
     * @param bundle Intent extras bundle
     * @return true if app should move to background after starting
     */
    fun isBackgroundRequested(bundle: Bundle): Boolean {
        return bundle.getBoolean(KEY_BACKGROUND, false)
    }

    /**
     * Get duration in milliseconds for auto-stop.
     *
     * @param bundle Intent extras bundle
     * @return Duration in ms, or DEFAULT_DURATION_MS (-1) if not set
     */
    fun getDurationMs(bundle: Bundle): Long {
        return bundle.getInt(KEY_DURATION_MS, DEFAULT_DURATION_MS).toLong()
    }

    /**
     * Get MMAP preference from bundle.
     *
     * @param bundle Intent extras bundle
     * @param currentValue Current MMAP enabled state
     * @return MMAP preference, or current value if not specified
     */
    fun getMMapEnabled(bundle: Bundle, currentValue: Boolean): Boolean {
        return if (bundle.containsKey(KEY_USE_MMAP)) {
            bundle.getBoolean(KEY_USE_MMAP)
        } else {
            currentValue
        }
    }

    /**
     * Get buffer size in frames from bundle.
     *
     * @param bundle Intent extras bundle
     * @return Buffer size in frames, or 0 if not specified
     */
    fun getBufferFrames(bundle: Bundle): Int {
        return bundle.getInt(KEY_BUFFER_FRAMES, 0)
    }

    /**
     * Format a machine-readable status log message.
     *
     * @param status Status value (e.g., STATUS_PLAYING)
     * @param extras Additional key=value pairs to include
     * @return Formatted log message
     */
    fun formatStatusLog(status: String, vararg extras: Pair<String, Any>): String {
        val extraStr = if (extras.isNotEmpty()) {
            " | " + extras.joinToString(" | ") { "${it.first}=${it.second}" }
        } else {
            ""
        }
        return "$STATUS_PREFIX $status$extraStr"
    }
}
