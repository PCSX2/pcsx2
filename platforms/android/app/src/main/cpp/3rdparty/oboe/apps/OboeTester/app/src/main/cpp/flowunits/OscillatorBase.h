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

#ifndef NATIVEOBOE_OSCILLATORBASE_H
#define NATIVEOBOE_OSCILLATORBASE_H

#include "flowgraph/FlowGraphNode.h"

/**
 * Base class for various oscillators.
 * The oscillator has a phase that ranges from -1.0 to +1.0.
 * That makes it easier to implement simple algebraic waveforms.
 *
 * Subclasses must implement onProcess().
 *
 * This module has "frequency" and "amplitude" ports for control.
 */

class OscillatorBase : public oboe::flowgraph::FlowGraphNode {
public:
    OscillatorBase();

    virtual ~OscillatorBase() = default;

    void setSampleRate(float sampleRate) {
        mSampleRate = sampleRate;
        mFrequencyToPhaseIncrement = 2.0f / sampleRate; // -1 to +1 is a range of 2
    }

    float getSampleRate() {
        return mSampleRate;
    }

    /**
     * This can be used to set the initial phase of an oscillator before starting.
     * This is mostly used with an LFO.
     * Calling this while the oscillator is running will cause sharp pops.
     * @param phase between -1.0 and +1.0
     */
    void setPhase(float phase) {
        mPhase = phase;
    }

    float getPhase() {
        return mPhase;
    }

    /**
     * Control the frequency of the oscillator in Hz.
     */
    oboe::flowgraph::FlowGraphPortFloatInput  frequency;

    /**
     * Control the linear amplitude of the oscillator.
     * Silence is 0.0.
     * A typical full amplitude would be 1.0.
     */
    oboe::flowgraph::FlowGraphPortFloatInput  amplitude;

    oboe::flowgraph::FlowGraphPortFloatOutput output;

protected:
    /**
     * Increment phase based on frequency in Hz.
     * Frequency may be positive or negative.
     *
     * Frequency should not exceed +/- Nyquist Rate.
     * Nyquist Rate is sampleRate/2.
     */
    float incrementPhase(float frequency) {
        mPhase += frequency * mFrequencyToPhaseIncrement;
        // Wrap phase in the range of -1 to +1
        if (mPhase >= 1.0f) {
            mPhase -= 2.0f;
        } else if (mPhase < -1.0f) {
            mPhase += 2.0f;
        }
        return mPhase;
    }

    float   mPhase = 0.0f;  // phase that ranges from -1.0 to +1.0
    float   mSampleRate = 0.0f;
    float   mFrequencyToPhaseIncrement = 0.0f; // scaler for converting frequency to phase increment
};


#endif //NATIVEOBOE_OSCILLATORBASE_H
