/*
 * Copyright (C) 2014 The Android Open Source Project
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
 * Band limited sawtooth oscillator.
 * This will have very little aliasing at high frequencies.
 */
public class SawOscillatorDPW extends SawOscillator {
    private float mZ1 = 0.0f; // delayed values
    private float mZ2 = 0.0f;
    private float mScaler; // frequency dependent scaler
    private final static float VERY_LOW_FREQ = 0.0000001f;

    @Override
    public void setFrequency(float freq) {
        /* Calculate scaling based on frequency. */
        freq = Math.abs(freq);
        super.setFrequency(freq);
        if (freq < VERY_LOW_FREQ) {
            mScaler = (float) (0.125 * 44100 / VERY_LOW_FREQ);
        } else {
            mScaler = (float) (0.125 * 44100 / freq);
        }
    }

    @Override
    public float render() {
        float phase = incrementWrapPhase();
        /* Square the raw sawtooth. */
        float squared = phase * phase;
        float diffed = squared - mZ2;
        mZ2 = mZ1;
        mZ1 = squared;
        return diffed * mScaler * getAmplitude();
    }

}
