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
 *
 * This code was translated from the JSyn Java code.
 * JSyn is Copyright 2009 Phil Burk, Mobileer Inc
 * JSyn is licensed under the Apache License, Version 2.0
 */

#ifndef SYNTHMARK_ENVELOPE_ADSR_H
#define SYNTHMARK_ENVELOPE_ADSR_H

#include <cstdint>
#include <math.h>
#include "SynthTools.h"
#include "UnitGenerator.h"

namespace marksynth {

/**
 * Generate a contour that can be used to control amplitude or
 * other parameters.
 */

class EnvelopeADSR  : public UnitGenerator
{
public:
    EnvelopeADSR()
    : mAttack(0.05)
    , mDecay(0.6)
    , mSustainLevel(0.4)
    , mRelease(2.5)
    {}

    virtual ~EnvelopeADSR() = default;

#define MIN_DURATION (1.0 / 100000.0)

    enum State {
        IDLE, ATTACKING, DECAYING, SUSTAINING, RELEASING
    };

    void setGate(bool gate) {
        triggered = gate;
    }

    bool isIdle() {
        return mState == State::IDLE;
    }

    /**
     * Time in seconds for the falling stage to go from 0 dB to -90 dB. The decay stage will stop at
     * the sustain level. But we calculate the time to fall to -90 dB so that the decay
     * <em>rate</em> will be unaffected by the sustain level.
     */
    void setDecayTime(synth_float_t time) {
        mDecay = time;
    }

    synth_float_t getDecayTime() {
        return mDecay;
    }

    /**
     * Time in seconds for the rising stage of the envelope to go from 0.0 to 1.0. The attack is a
     * linear ramp.
     */
    void setAttackTime(synth_float_t time) {
        mAttack = time;
    }

    synth_float_t getAttackTime() {
        return mAttack;
    }

    void generate(int32_t numSamples) {
        for (int i = 0; i < numSamples; i++) {
            switch (mState) {
                case IDLE:
                    for (; i < numSamples; i++) {
                        output[i] = mLevel;
                        if (triggered) {
                            startAttack();
                            break;
                        }
                    }
                    break;

                case ATTACKING:
                    for (; i < numSamples; i++) {
                        // Increment first so we can render fast attacks.
                        mLevel += increment;
                        if (mLevel >= 1.0) {
                            mLevel = 1.0;
                            output[i] = mLevel;
                            startDecay();
                            break;
                        } else {
                            output[i] = mLevel;
                            if (!triggered) {
                                startRelease();
                                break;
                            }
                        }
                    }
                    break;

                case DECAYING:
                    for (; i < numSamples; i++) {
                        output[i] = mLevel;
                        mLevel *= mScaler; // exponential decay
                        if (mLevel < kAmplitudeDb96) {
                            startIdle();
                            break;
                        } else if (!triggered) {
                            startRelease();
                            break;
                        } else if (mLevel < mSustainLevel) {
                            mLevel = mSustainLevel;
                            startSustain();
                            break;
                        }
                    }
                    break;

                case SUSTAINING:
                    for (; i < numSamples; i++) {
                        mLevel = mSustainLevel;
                        output[i] = mLevel;
                        if (!triggered) {
                            startRelease();
                            break;
                        }
                    }
                    break;

                case RELEASING:
                    for (; i < numSamples; i++) {
                        output[i] = mLevel;
                        mLevel *= mScaler; // exponential decay
                        if (triggered) {
                            startAttack();
                            break;
                        } else if (mLevel < kAmplitudeDb96) {
                            startIdle();
                            break;
                        }
                    }
                    break;
            }
        }
    }

private:

    void startIdle() {
        mState = State::IDLE;
        mLevel = 0.0;
    }

    void startAttack() {
        if (mAttack < MIN_DURATION) {
            mLevel = 1.0;
            startDecay();
        } else {
            increment = mSamplePeriod / mAttack;
            mState = State::ATTACKING;
        }
    }

    void startDecay() {
        double duration = mDecay;
        if (duration < MIN_DURATION) {
            startSustain();
        } else {
            mScaler = SynthTools::convertTimeToExponentialScaler(duration, mSampleRate);
            mState = State::DECAYING;
        }
    }

    void startSustain() {
        mState = State::SUSTAINING;
    }

    void startRelease() {
        double duration = mRelease;
        if (duration < MIN_DURATION) {
            duration = MIN_DURATION;
        }
        mScaler = SynthTools::convertTimeToExponentialScaler(duration, mSampleRate);
        mState = State::RELEASING;
    }

    synth_float_t mAttack;
    synth_float_t mDecay;
    /**
     * Level for the sustain stage. The envelope will hold here until the input goes to zero or
     * less. This should be set between 0.0 and 1.0.
     */
    synth_float_t mSustainLevel;
    /**
     * Time in seconds to go from 0 dB to -90 dB. This stage is triggered when the input goes to
     * zero or less. The release stage will start from the sustain level. But we calculate the time
     * to fall from full amplitude so that the release <em>rate</em> will be unaffected by the
     * sustain level.
     */
    synth_float_t mRelease;

    State mState = State::IDLE;
    synth_float_t mScaler = 1.0;
    synth_float_t mLevel = 0.0;
    synth_float_t increment = 0;
    bool triggered = false;

};

};
#endif // SYNTHMARK_ENVELOPE_ADSR_H
