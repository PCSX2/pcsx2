/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.mobileer.miditools;

import android.media.midi.MidiReceiver;
import android.media.midi.MidiSender;

import java.io.IOException;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Utility class for dispatching MIDI data to a list of {@link android.media.midi.MidiReceiver}s.
 * This class subclasses {@link android.media.midi.MidiReceiver} and dispatches any data it receives
 * to its receiver list. Any receivers that throw an exception upon receiving data will
 * be automatically removed from the receiver list, but no IOException will be returned
 * from the dispatcher's {@link android.media.midi.MidiReceiver#onReceive} in that case.
 */
public final class MidiDispatcher extends MidiReceiver {

    private final CopyOnWriteArrayList<MidiReceiver> mReceivers
            = new CopyOnWriteArrayList<MidiReceiver>();

    private final MidiSender mSender = new MidiSender() {
        /**
         * Called to connect a {@link android.media.midi.MidiReceiver} to the sender
         *
         * @param receiver the receiver to connect
         */
        @Override
        public void onConnect(MidiReceiver receiver) {
            mReceivers.add(receiver);
        }

        /**
         * Called to disconnect a {@link android.media.midi.MidiReceiver} from the sender
         *
         * @param receiver the receiver to disconnect
         */
        @Override
        public void onDisconnect(MidiReceiver receiver) {
            mReceivers.remove(receiver);
        }
    };

    /**
     * Returns the number of {@link android.media.midi.MidiReceiver}s this dispatcher contains.
     * @return the number of receivers
     */
    public int getReceiverCount() {
        return mReceivers.size();
    }

    /**
     * Returns a {@link android.media.midi.MidiSender} which is used to add and remove
     * {@link android.media.midi.MidiReceiver}s
     * to the dispatcher's receiver list.
     * @return the dispatcher's MidiSender
     */
    public MidiSender getSender() {
        return mSender;
    }

    @Override
    public void onSend(byte[] msg, int offset, int count, long timestamp) throws IOException {
       for (MidiReceiver receiver : mReceivers) {
            try {
                receiver.send(msg, offset, count, timestamp);
            } catch (IOException e) {
                // if the receiver fails we remove the receiver but do not propagate the exception
                mReceivers.remove(receiver);
            }
        }
    }

    @Override
    public void flush() throws IOException {
       for (MidiReceiver receiver : mReceivers) {
            receiver.flush();
       }
    }
}
