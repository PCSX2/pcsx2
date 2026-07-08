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
 * Very simple Attack, Decay, Sustain, Release envelope with linear ramps.
 *
 * Times are in seconds.
 */
public class EnvelopeADSR extends SynthUnit {
    private static final int IDLE = 0;
    private static final int ATTACK = 1;
    private static final int DECAY = 2;
    private static final int SUSTAIN = 3;
    private static final int RELEASE = 4;
    private static final int FINISHED = 5;
    private static final float MIN_TIME = 0.001f;

    private float mAttackRate;
    private float mRreleaseRate;
    private float mSustainLevel;
    private float mDecayRate;
    private float mCurrent;
    private int mSstate = IDLE;
    private int mSamplerate;

    public EnvelopeADSR( int sampleRate) {
        mSamplerate = sampleRate;
        setAttackTime(0.003f);
        setDecayTime(0.08f);
        setSustainLevel(0.3f);
        setReleaseTime(1.0f);
    }



    public void setAttackTime(float time) {
        if (time < MIN_TIME)
            time = MIN_TIME;
        mAttackRate = 1.0f / (mSamplerate * time);
    }

    public void setDecayTime(float time) {
        if (time < MIN_TIME)
            time = MIN_TIME;
        mDecayRate = 1.0f / (mSamplerate * time);
    }

    public void setSustainLevel(float level) {
        if (level < 0.0f)
            level = 0.0f;
        mSustainLevel = level;
    }

    public void setReleaseTime(float time) {
        if (time < MIN_TIME)
            time = MIN_TIME;
        mRreleaseRate = 1.0f / (mSamplerate * time);
    }

    public void on() {
        mSstate = ATTACK;
    }

    public void off() {
        mSstate = RELEASE;
    }

    @Override
    public float render() {
        switch (mSstate) {
        case ATTACK:
            mCurrent += mAttackRate;
            if (mCurrent > 1.0f) {
                mCurrent = 1.0f;
                mSstate = DECAY;
            }
            break;
        case DECAY:
            mCurrent -= mDecayRate;
            if (mCurrent < mSustainLevel) {
                mCurrent = mSustainLevel;
                mSstate = SUSTAIN;
            }
            break;
        case RELEASE:
            mCurrent -= mRreleaseRate;
            if (mCurrent < 0.0f) {
                mCurrent = 0.0f;
                mSstate = FINISHED;
            }
            break;
        }
        return mCurrent;
    }

    public boolean isDone() {
        return mSstate == FINISHED;
    }

}
