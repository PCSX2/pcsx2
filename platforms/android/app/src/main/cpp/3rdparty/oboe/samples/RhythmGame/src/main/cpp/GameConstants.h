/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef SAMPLES_GAMECONSTANTS_H
#define SAMPLES_GAMECONSTANTS_H

#include "ui/OpenGLFunctions.h"

constexpr int kBufferSizeInBursts = 2; // Use 2 bursts as the buffer size (double buffer)
constexpr int kMaxQueueItems = 4; // Must be power of 2

// Colors for game states and visual feedback for taps
constexpr ScreenColor kPlayingColor = GREY;
constexpr ScreenColor kLoadingColor = YELLOW;
constexpr ScreenColor kLoadingFailedColor = RED;
constexpr ScreenColor kTapSuccessColor = GREEN;
constexpr ScreenColor kTapEarlyColor = ORANGE;
constexpr ScreenColor kTapLateColor = PURPLE;

// This defines the size of the tap window in milliseconds. For example, if defined at 100ms the
// player will have 100ms before and after the centre of the tap window to tap on the screen and
// be successful
constexpr int kWindowCenterOffsetMs = 100;

// Filename for clap sound asset (in assets folder)
constexpr char kClapFilename[] { "CLAP.mp3" };

// Filename for the backing track asset (in assets folder)
constexpr char kBackingTrackFilename[] { "FUNKY_HOUSE.mp3" };

// The game will first demonstrate the pattern which the user should copy. It does this by
// "clapping" (playing a clap sound) at certain times during the song. We can specify these times
// here in milliseconds. Our backing track has a tempo of 120 beats per minute, which is 2 beats per
// second. This means a pattern of 3 claps starting on the first beat of the first bar would mean
// playing claps at 0ms, 500ms and 1000ms
constexpr int64_t kClapEvents[] { 0, 500, 1000 };

// We then want the user to tap on the screen exactly 4 beats after the first clap so we add clap
// windows at 2000ms, 2500ms and 3000ms (or 2, 2.5 and 3 seconds). @see getTapResult for more info.
constexpr int64_t kClapWindows[] { 2000, 2500, 3000 };

struct AudioProperties {
    int32_t channelCount;
    int32_t sampleRate;
};

#endif //SAMPLES_GAMECONSTANTS_H
