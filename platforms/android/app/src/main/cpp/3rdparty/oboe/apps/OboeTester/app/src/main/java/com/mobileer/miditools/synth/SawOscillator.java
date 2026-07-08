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

public class SawOscillator extends SynthUnit {
    private float mPhase = 0.0f;
    private float mPhaseIncrement = 0.01f;
    private float mFrequency = 0.0f;
    private float mFrequencyScaler = 1.0f;
    private float mAmplitude = 1.0f;

    public void setPitch(float pitch) {
        float freq = (float) pitchToFrequency(pitch);
        setFrequency(freq);
    }

    public void setFrequency(float frequency) {
        mFrequency = frequency;
        updatePhaseIncrement();
    }

    private void updatePhaseIncrement() {
        mPhaseIncrement = 2.0f * mFrequency * mFrequencyScaler / 48000.0f;
    }

    public void setAmplitude(float amplitude) {
        mAmplitude = amplitude;
    }

    public float getAmplitude() {
        return mAmplitude;
    }

    public float getFrequencyScaler() {
        return mFrequencyScaler;
    }

    public void setFrequencyScaler(float frequencyScaler) {
        mFrequencyScaler = frequencyScaler;
        updatePhaseIncrement();
    }

    float incrementWrapPhase() {
        mPhase += mPhaseIncrement;
        while (mPhase > 1.0) {
            mPhase -= 2.0;
        }
        while (mPhase < -1.0) {
            mPhase += 2.0;
        }
        return mPhase;
    }

    @Override
    public float render() {
        return incrementWrapPhase() * mAmplitude;
    }

}
