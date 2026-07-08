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

#ifndef SYNTHMARK_SIMPLE_VOICE_H
#define SYNTHMARK_SIMPLE_VOICE_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"
#include "VoiceBase.h"
#include "SawtoothOscillator.h"
#include "SawtoothOscillatorDPW.h"
#include "SquareOscillatorDPW.h"
#include "SineOscillator.h"
#include "EnvelopeADSR.h"
#include "PitchToFrequency.h"
#include "BiquadFilter.h"

namespace marksynth {
/**
 * Classic subtractive synthesizer voice with
 * 2 LFOs, 2 audio oscillators, filter and envelopes.
 */
class SimpleVoice : public VoiceBase
{
public:
    SimpleVoice()
    : VoiceBase()
    , mLfo1()
    , mOsc1()
    , mOsc2()
    , mPitchToFrequency()
    , mFilter()
    , mFilterEnvelope()
    , mAmplitudeEnvelope()
      // The following values are arbitrary but typical values.
    , mDetune(1.0001f) // slight phasing
    , mVibratoDepth(0.03f)
    , mVibratoRate(6.0f)
    , mFilterEnvDepth(3000.0f)
    , mFilterCutoff(400.0f)
    {
        mFilter.setQ(2.0);
        // Randomize attack times to smooth out CPU load for envelope state transitions.
        mFilterEnvelope.setAttackTime(0.05 + (0.2 * SynthTools::nextRandomDouble()));
        mFilterEnvelope.setDecayTime(7.0 + (1.0 * SynthTools::nextRandomDouble()));
        mAmplitudeEnvelope.setAttackTime(0.02 + (0.05 * SynthTools::nextRandomDouble()));
        mAmplitudeEnvelope.setDecayTime(1.0 + (0.2 * SynthTools::nextRandomDouble()));
    }

    virtual ~SimpleVoice() = default;

    void setPitch(synth_float_t pitch) {
        mPitch = pitch;
    }

    void noteOn(synth_float_t pitch, synth_float_t velocity) {
        (void) velocity; // TODO use velocity?
        mPitch = pitch;
        mFilterEnvelope.setGate(true);
        mAmplitudeEnvelope.setGate(true);
    }

    void noteOff() {
        mFilterEnvelope.setGate(false);
        mAmplitudeEnvelope.setGate(false);
    }

    void generate(int32_t numFrames) {
        assert(numFrames <= kSynthmarkFramesPerRender);

        // LFO #1 - vibrato
        mLfo1.generate(mVibratoRate, numFrames);
        synth_float_t *pitches = mBuffer1;
        SynthTools::scaleOffsetBuffer(mLfo1.output, pitches, numFrames, mVibratoDepth, mPitch);
        synth_float_t *frequencies = mBuffer2;
        mPitchToFrequency.generate(pitches, frequencies, numFrames);

        // OSC #1 - sawtooth
        mOsc1.generate(frequencies, numFrames);

        // OSC #2 - detuned square wave oscillator
        SynthTools::scaleBuffer(frequencies, frequencies, numFrames, mDetune);
        mOsc2.generate(frequencies, numFrames);

        // Mix the two oscillators
        synth_float_t *mixed = frequencies;
        SynthTools::mixBuffers(mOsc1.output, 0.6, mOsc2.output, 0.4, mixed, numFrames);

        // Filter envelope
        mFilterEnvelope.generate(numFrames);
        synth_float_t *cutoffFrequencies = pitches;  // reuse unneeded buffer
        SynthTools::scaleOffsetBuffer(mFilterEnvelope.output, cutoffFrequencies, numFrames,
                                      mFilterEnvDepth, mFilterCutoff);

        // Biquad resonant low-pass filter
        mFilter.generate(mixed, cutoffFrequencies, numFrames);

        // Amplitude ADSR
        mAmplitudeEnvelope.generate(numFrames);
        SynthTools::multiplyBuffers(mFilter.output, mAmplitudeEnvelope.output, output, numFrames);
    }

private:
    SineOscillator mLfo1;
    SawtoothOscillatorDPW mOsc1;
    SquareOscillatorDPW mOsc2;
    PitchToFrequency mPitchToFrequency;
    BiquadFilter mFilter;
    EnvelopeADSR mFilterEnvelope;
    EnvelopeADSR mAmplitudeEnvelope;

    synth_float_t mDetune;          // frequency scaler
    synth_float_t mVibratoDepth;    // in semitones
    synth_float_t mVibratoRate;     // in Hertz
    synth_float_t mFilterEnvDepth;  // in Hertz
    synth_float_t mFilterCutoff;    // in Hertz

    // Buffers for storing signals that are being passed between units.
    synth_float_t mBuffer1[kSynthmarkFramesPerRender];
    synth_float_t mBuffer2[kSynthmarkFramesPerRender];
};
};
#endif // SYNTHMARK_SIMPLE_VOICE_H
