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

import android.media.midi.MidiReceiver;
import android.util.Log;

import com.mobileer.miditools.MidiConstants;
import com.mobileer.miditools.MidiEventScheduler;
import com.mobileer.miditools.MidiFramer;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Locale;

/**
 * Very simple polyphonic, single channel synthesizer. It runs a background
 * thread that processes MIDI events and synthesizes audio.
 */
public class SynthEngine extends MidiReceiver {

    private static final String TAG = "SynthEngine";
    // 64 is the greatest common divisor of 192 and 128
    private static final int DEFAULT_FRAMES_PER_BLOCK = 64;
    private static final int SAMPLES_PER_FRAME = 2;

    private volatile boolean mThreadEnabled;
    private Thread mThread;
    private float[] mBuffer = null;
    private float mFrequencyScaler = 1.0f;
    private float mBendRange = 2.0f; // semitones
    private int mProgram;

    private ArrayList<SynthVoice> mFreeVoices = new ArrayList<SynthVoice>();
    private Hashtable<Integer, SynthVoice>
            mVoices = new Hashtable<Integer, SynthVoice>();
    private MidiEventScheduler mEventScheduler;
    private MidiFramer mFramer;
    private MidiReceiver mReceiver = new MyReceiver();
    private SimpleAudioOutput mAudioOutput;
    private int mSampleRate;
    private int mFramesPerBlock = DEFAULT_FRAMES_PER_BLOCK;
    private int mMidiByteCount;

    public SynthEngine() {
        this(new SimpleAudioOutput());
    }

    public SynthEngine(SimpleAudioOutput audioOutput) {
        mAudioOutput = audioOutput;
        mReceiver = new MyReceiver();
        mFramer = new MidiFramer(mReceiver);
    }

    public SimpleAudioOutput getAudioOutput() {
        return mAudioOutput;
    }

    /* This will be called when MIDI data arrives. */
    @Override
    public void onSend(byte[] data, int offset, int count, long timestamp)
            throws IOException {
        if (mEventScheduler != null) {
            if (!MidiConstants.isAllActiveSensing(data, offset, count)) {
                mEventScheduler.getReceiver().send(data, offset, count,
                        timestamp);
            }
        }
        mMidiByteCount += count;
    }

    /**
     * Call this before the engine is started.
     * @param framesPerBlock
     */
    public void setFramesPerBlock(int framesPerBlock) {
        mFramesPerBlock = framesPerBlock;
    }


    private class MyReceiver extends MidiReceiver {
        @Override
        public void onSend(byte[] data, int offset, int count, long timestamp)
                throws IOException {
            byte command = (byte) (data[0] & MidiConstants.STATUS_COMMAND_MASK);
            int channel = (byte) (data[0] & MidiConstants.STATUS_CHANNEL_MASK);
            switch (command) {
            case MidiConstants.STATUS_NOTE_OFF:
                noteOff(channel, data[1], data[2]);
                break;
            case MidiConstants.STATUS_NOTE_ON:
                noteOn(channel, data[1], data[2]);
                break;
            case MidiConstants.STATUS_PITCH_BEND:
                int bend = (data[2] << 7) + data[1];
                pitchBend(channel, bend);
                break;
            case MidiConstants.STATUS_PROGRAM_CHANGE:
                mProgram = data[1];
                mFreeVoices.clear();
                break;
            default:
                logMidiMessage(data, offset, count);
                break;
            }
        }
    }

    class MyRunnable implements Runnable {
        @Override
        public void run() {
            try {
                mAudioOutput.start(mFramesPerBlock);
                mSampleRate = mAudioOutput.getFrameRate(); // rate is now valid
                if (mBuffer == null) {
                    mBuffer = new float[mFramesPerBlock * SAMPLES_PER_FRAME];
                }
                onLoopStarted();
                // The safest way to exit from a thread is to check a variable.
                while (mThreadEnabled) {
                    processMidiEvents();
                    generateBuffer();
                    float[] buffer = mBuffer;
                    mAudioOutput.write(buffer, 0, buffer.length);
                    onBufferCompleted(mFramesPerBlock);
                }
            } catch (Exception e) {
                Log.e(TAG, "SynthEngine background thread exception.", e);
            } finally {
                onLoopEnded();
                mAudioOutput.stop();
            }
        }
    }

    /**
     * This is called from the synthesis thread before it starts looping.
     */
    public void onLoopStarted() {
    }

    /**
     * This is called once at the end of each synthesis loop.
     *
     * @param framesPerBuffer
     */
    public void onBufferCompleted(int framesPerBuffer) {
    }

    /**
     * This is called from the synthesis thread when it stops looping.
     */
    public void onLoopEnded() {
    }

    /**
     * Assume message has been aligned to the start of a MIDI message.
     *
     * @param data
     * @param offset
     * @param count
     */
    public void logMidiMessage(byte[] data, int offset, int count) {
        String text = "Received: ";
        for (int i = 0; i < count; i++) {
            text += String.format(Locale.getDefault(), "0x%02X, ", data[offset + i]);
        }
        Log.i(TAG, text);
    }

    /**
     * @throws IOException
     *
     */
    private void processMidiEvents() throws IOException {
        long now = System.nanoTime(); // TODO use audio presentation time
        MidiEventScheduler.MidiEvent event = (MidiEventScheduler.MidiEvent) mEventScheduler.getNextEvent(now);
        while (event != null) {
            mFramer.send(event.data, 0, event.count, event.getTimestamp());
            mEventScheduler.addEventToPool(event);
            event = (MidiEventScheduler.MidiEvent) mEventScheduler.getNextEvent(now);
        }
    }

    /**
     * Mix the output of each active voice into a buffer.
     */
    private void generateBuffer() {
        float[] buffer = mBuffer;
        for (int i = 0; i < buffer.length; i++) {
            buffer[i] = 0.0f;
        }
        Iterator<SynthVoice> iterator = mVoices.values().iterator();
        while (iterator.hasNext()) {
            SynthVoice voice = iterator.next();
            if (voice.isDone()) {
                iterator.remove();
                // mFreeVoices.add(voice);
            } else {
                voice.mix(buffer, SAMPLES_PER_FRAME, 0.25f);
            }
        }
    }

    public void noteOff(int channel, int noteIndex, int velocity) {
        SynthVoice voice = mVoices.get(noteIndex);
        if (voice != null) {
            voice.noteOff();
        }
    }

    public void allNotesOff() {
        Iterator<SynthVoice> iterator = mVoices.values().iterator();
        while (iterator.hasNext()) {
            SynthVoice voice = iterator.next();
            voice.noteOff();
        }
    }

    /**
     * Create a SynthVoice.
     */
    public SynthVoice createVoice(int program) {
        // For every odd program number use a sine wave.
        if ((program & 1) == 1) {
            return new SineVoice(mSampleRate);
        } else {
            return new SawVoice(mSampleRate);
        }
    }

    /**
     *
     * @param channel
     * @param noteIndex
     * @param velocity
     */
    public void noteOn(int channel, int noteIndex, int velocity) {
        if (velocity == 0) {
            noteOff(channel, noteIndex, velocity);
        } else {
            mVoices.remove(noteIndex);
            SynthVoice voice;
            if (mFreeVoices.size() > 0) {
                voice = mFreeVoices.remove(mFreeVoices.size() - 1);
            } else {
                voice = createVoice(mProgram);
            }
            voice.setFrequencyScaler(mFrequencyScaler);
            voice.noteOn(noteIndex, velocity);
            mVoices.put(noteIndex, voice);
        }
    }

    public void pitchBend(int channel, int bend) {
        double semitones = (mBendRange * (bend - 0x2000)) / 0x2000;
        mFrequencyScaler = (float) Math.pow(2.0, semitones / 12.0);
        Iterator<SynthVoice> iterator = mVoices.values().iterator();
        while (iterator.hasNext()) {
            SynthVoice voice = iterator.next();
            voice.setFrequencyScaler(mFrequencyScaler);
        }
    }

    /**
     * Start the synthesizer.
     */
    public void start() {
        stop();
        mThreadEnabled = true;
        mThread = new Thread(new MyRunnable());
        mEventScheduler = new MidiEventScheduler();
        mThread.start();
    }

    /**
     * Stop the synthesizer.
     */
    public void stop() {
        mThreadEnabled = false;
        if (mThread != null) {
            try {
                mThread.interrupt();
                mThread.join(500);
            } catch (InterruptedException e) {
                // OK, just stopping safely.
            }
            mThread = null;
            mEventScheduler = null;
        }
    }

    public LatencyController getLatencyController() {
        return mAudioOutput.getLatencyController();
    }

    public int getMidiByteCount() {
        return mMidiByteCount;
    }
}
