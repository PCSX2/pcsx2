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

package com.mobileer.miditools;

import android.media.midi.MidiReceiver;

import java.io.IOException;

/**
 * Add MIDI Events to an EventScheduler
 */
public class MidiEventScheduler extends EventScheduler {
    // Maintain a pool of scheduled events to reduce memory allocation.
    // This pool increases performance by about 14%.
    private final static int POOL_EVENT_SIZE = 16;
    private MidiReceiver mReceiver = new SchedulingReceiver();

    private class SchedulingReceiver extends MidiReceiver
    {
        /**
         * Store these bytes in the EventScheduler to be delivered at the specified
         * time.
         */
        @Override
        public void onSend(byte[] msg, int offset, int count, long timestamp)
                throws IOException {
            MidiEvent event = createScheduledEvent(msg, offset, count, timestamp);
            if (event != null) {
                add(event);
            }
        }
    }

    public static class MidiEvent extends SchedulableEvent {
        public int count = 0;
        public byte[] data;

        private MidiEvent(int count) {
            super(0);
            data = new byte[count];
        }

        private MidiEvent(byte[] msg, int offset, int count, long timestamp) {
            super(timestamp);
            data = new byte[count];
            System.arraycopy(msg, offset, data, 0, count);
            this.count = count;
        }

        @Override
        public String toString() {
            String text = "Event: ";
            for (int i = 0; i < count; i++) {
                text += data[i] + ", ";
            }
            return text;
        }
    }

    /**
     * Create an event that contains the message.
     */
    private MidiEvent createScheduledEvent(byte[] msg, int offset, int count,
            long timestamp) {
        MidiEvent event;
        if (count > POOL_EVENT_SIZE) {
            event = new MidiEvent(msg, offset, count, timestamp);
        } else {
            event = (MidiEvent) removeEventfromPool();
            if (event == null) {
                event = new MidiEvent(POOL_EVENT_SIZE);
            }
            System.arraycopy(msg, offset, event.data, 0, count);
            event.count = count;
            event.setTimestamp(timestamp);
        }
        return event;
    }

    /**
     * Return events to a pool so they can be reused.
     *
     * @param event
     */
    @Override
    public void addEventToPool(SchedulableEvent event) {
        // Make sure the event is suitable for the pool.
        if (event instanceof MidiEvent) {
            MidiEvent midiEvent = (MidiEvent) event;
            if (midiEvent.data.length == POOL_EVENT_SIZE) {
                super.addEventToPool(event);
            }
        }
    }

    /**
     * This MidiReceiver will write date to the scheduling buffer.
     * @return the MidiReceiver
     */
    public MidiReceiver getReceiver() {
        return mReceiver;
    }

}
