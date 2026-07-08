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
 * Base class for a polyphonic synthesizer voice.
 */
public abstract class SynthVoice {
    private int mNoteIndex;
    private float mAmplitude;
    public static final int STATE_OFF = 0;
    public static final int STATE_ON = 1;
    private int mState = STATE_OFF;

    public SynthVoice() {
        mNoteIndex = -1;
    }

    public void noteOn(int noteIndex, int velocity) {
        mState = STATE_ON;
        this.mNoteIndex = noteIndex;
        setAmplitude(velocity / 128.0f);
    }

    public void noteOff() {
        mState = STATE_OFF;
    }

    /**
     * Add the output of this voice to an output buffer.
     *
     * @param outputBuffer
     * @param samplesPerFrame
     * @param level
     */
    public void mix(float[] outputBuffer, int samplesPerFrame, float level) {
        int numFrames = outputBuffer.length / samplesPerFrame;
        for (int i = 0; i < numFrames; i++) {
            float output = render();
            int offset = i * samplesPerFrame;
            for (int jf = 0; jf < samplesPerFrame; jf++) {
                outputBuffer[offset + jf] += output * level;
            }
        }
    }

    public abstract float render();

    public boolean isDone() {
        return mState == STATE_OFF;
    }

    public int getNoteIndex() {
        return mNoteIndex;
    }

    public float getAmplitude() {
        return mAmplitude;
    }

    public void setAmplitude(float amplitude) {
        this.mAmplitude = amplitude;
    }

    /**
     * @param scaler
     */
    public void setFrequencyScaler(float scaler) {
    }

}
