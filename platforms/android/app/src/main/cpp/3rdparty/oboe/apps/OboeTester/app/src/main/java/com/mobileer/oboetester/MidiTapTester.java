/*
 * Copyright 2015 The Android Open Source Project
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


import android.media.midi.MidiDeviceService;
import android.media.midi.MidiReceiver;
import android.util.Log;

import com.mobileer.miditools.MidiConstants;
import com.mobileer.miditools.MidiFramer;

import java.io.IOException;
import java.util.ArrayList;

/**
 * Measure the latency of various output paths by playing a blip.
 * Report the results back to the TestListeners.
 */
public class MidiTapTester extends MidiDeviceService {
    // These must match the values in service_device_info.xml
    public static final String PRODUCT_NAME = "MidiTapLatencyTester";
    public static final String MANUFACTURER_NAME = "Mobileer";

    // Sometimes the service can be run without the MainActivity being run!
    static {
        // Must match name in CMakeLists.txt
        System.loadLibrary("oboetester");
    }

    private ArrayList<NoteListener> mListeners = new ArrayList<NoteListener>();
    private MyMidiReceiver mReceiver = new MyMidiReceiver();
    private MidiFramer mMidiFramer = new MidiFramer(mReceiver);

    private static MidiTapTester mInstance;

    public static interface NoteListener {
        public void onNoteOn(int pitch);
    }

    /**
     * This is a Service so it is only created when a client requests the service.
     */
    public MidiTapTester() {
        mInstance = this;
    }

    public void addTestListener(NoteListener listener) {
        mListeners.add(listener);
    }

    public void removeTestListener(NoteListener listener) {
        mListeners.remove(listener);
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public void onDestroy() {
        // do stuff here
        super.onDestroy();
    }

    public static MidiTapTester getInstanceOrNull() {
        return mInstance;
    }

    class MyMidiReceiver extends MidiReceiver {
        public void onSend(byte[] data, int offset,
                           int count, long timestamp) throws IOException {
            // parse MIDI
            byte command = (byte) (data[0] & 0x0F0);
            if (command == MidiConstants.STATUS_NOTE_ON) {
                if (data[2] == 0) {
                    noteOff(data[1]);
                } else {
                    noteOn(data[1]);
                }
            } else if (command == MidiConstants.STATUS_NOTE_OFF) {
                noteOff(data[1]);
            }
            Log.i(TapToToneActivity.TAG, "MIDI command = " + command);
        }
    }

    private void noteOn(byte b) {
        fireNoteOn(b);
    }

    private void fireNoteOn(byte pitch) {
        for (NoteListener listener : mListeners) {
            listener.onNoteOn(pitch);
        }
    }

    private void noteOff(byte b) {}

    @Override
    public MidiReceiver[] onGetInputPortReceivers() {
        return new MidiReceiver[]{mMidiFramer};
    }

}
