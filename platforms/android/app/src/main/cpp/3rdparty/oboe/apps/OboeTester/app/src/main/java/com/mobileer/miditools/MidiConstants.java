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

/**
 * MIDI related constants and static methods.
 * These values are defined in the MIDI Standard 1.0
 * available from the MIDI Manufacturers Association.
 */
public class MidiConstants {
    protected final static String TAG = "MidiTools";
    public static final byte STATUS_COMMAND_MASK = (byte) 0xF0;
    public static final byte STATUS_CHANNEL_MASK = (byte) 0x0F;

    // Channel voice messages.
    public static final byte STATUS_NOTE_OFF = (byte) 0x80;
    public static final byte STATUS_NOTE_ON = (byte) 0x90;
    public static final byte STATUS_POLYPHONIC_AFTERTOUCH = (byte) 0xA0;
    public static final byte STATUS_CONTROL_CHANGE = (byte) 0xB0;
    public static final byte STATUS_PROGRAM_CHANGE = (byte) 0xC0;
    public static final byte STATUS_CHANNEL_PRESSURE = (byte) 0xD0;
    public static final byte STATUS_PITCH_BEND = (byte) 0xE0;

    // System Common Messages.
    public static final byte STATUS_SYSTEM_EXCLUSIVE = (byte) 0xF0;
    public static final byte STATUS_MIDI_TIME_CODE = (byte) 0xF1;
    public static final byte STATUS_SONG_POSITION = (byte) 0xF2;
    public static final byte STATUS_SONG_SELECT = (byte) 0xF3;
    public static final byte STATUS_TUNE_REQUEST = (byte) 0xF6;
    public static final byte STATUS_END_SYSEX = (byte) 0xF7;

    // System Real-Time Messages
    public static final byte STATUS_TIMING_CLOCK = (byte) 0xF8;
    public static final byte STATUS_START = (byte) 0xFA;
    public static final byte STATUS_CONTINUE = (byte) 0xFB;
    public static final byte STATUS_STOP = (byte) 0xFC;
    public static final byte STATUS_ACTIVE_SENSING = (byte) 0xFE;
    public static final byte STATUS_RESET = (byte) 0xFF;

    /** Number of bytes in a message nc from 8c to Ec */
    public final static int CHANNEL_BYTE_LENGTHS[] = { 3, 3, 3, 3, 2, 2, 3 };

    /** Number of bytes in a message Fn from F0 to FF */
    public final static int SYSTEM_BYTE_LENGTHS[] = { 1, 2, 3, 2, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1 };

    /**
     * MIDI messages, except for SysEx, are 1,2 or 3 bytes long.
     * You can tell how long a MIDI message is from the first status byte.
     * Do not call this for SysEx, which has variable length.
     * @param statusByte
     * @return number of bytes in a complete message, zero if data byte passed
     */
    public static int getBytesPerMessage(byte statusByte) {
        // Java bytes are signed so we need to mask off the high bits
        // to get a value between 0 and 255.
        int statusInt = statusByte & 0xFF;
        if (statusInt >= 0xF0) {
            // System messages use low nibble for size.
            return SYSTEM_BYTE_LENGTHS[statusInt & 0x0F];
        } else if(statusInt >= 0x80) {
            // Channel voice messages use high nibble for size.
            return CHANNEL_BYTE_LENGTHS[(statusInt >> 4) - 8];
        } else {
            return 0; // data byte
        }
    }

    /**
     * @param msg
     * @param offset
     * @param count
     * @return true if the entire message is ActiveSensing commands
     */
    public static boolean isAllActiveSensing(byte[] msg, int offset,
            int count) {
        // Count bytes that are not active sensing.
        int goodBytes = 0;
        for (int i = 0; i < count; i++) {
            byte b = msg[offset + i];
            if (b != MidiConstants.STATUS_ACTIVE_SENSING) {
                goodBytes++;
            }
        }
        return (goodBytes == 0);
    }

}
