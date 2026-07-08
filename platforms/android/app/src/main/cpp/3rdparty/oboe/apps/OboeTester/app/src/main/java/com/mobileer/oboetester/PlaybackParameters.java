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

import java.util.Locale;

public class PlaybackParameters {
    public static final int FALLBACK_MODE_DEFAULT = 0;
    public static final int FALLBACK_MODE_MUTE = 1;
    public static final int FALLBACK_MODE_FAIL = 2;

    public static final int STRETCH_MODE_DEFAULT = 0;
    public static final int STRETCH_MODE_VOICE = 1;

    public int mFallbackMode;
    public int mStretchMode;
    public float mPitch;
    public float mSpeed;

    public PlaybackParameters(int fallbackMode, int stretchMode, float pitch, float speed) {
        mFallbackMode = fallbackMode;
        mStretchMode = stretchMode;
        mPitch = pitch;
        mSpeed = speed;
    }

    public String getFallbackModeAsStr() {
        switch (mFallbackMode) {
            case FALLBACK_MODE_DEFAULT: return "Default";
            case FALLBACK_MODE_FAIL: return "Fail";
            case FALLBACK_MODE_MUTE: return "Mute";
            default: return "Unknown";
        }
    }

    public String getStretchModeAsStr() {
        switch (mStretchMode) {
            case STRETCH_MODE_DEFAULT: return "Default";
            case STRETCH_MODE_VOICE: return "Voice";
            default: return "Unknown";
        }
    }

    @Override
    public String toString() {
        return String.format(Locale.getDefault(),
                "Fallback: %s, Stretch: %s, Pitch: %.2f, Speed: %.2f",
                getFallbackModeAsStr(), getFallbackModeAsStr(), mPitch, mSpeed);
    }
}
