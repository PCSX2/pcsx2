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

import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiDeviceInfo.PortInfo;

public class MidiPortWrapper {
    private MidiDeviceInfo mInfo;
    private int mPortIndex;
    private int mType;
    private String mString;

    /**
     * Wrapper for a MIDI device and port description.
     * @param info
     * @param portType
     * @param portIndex
     */
    public MidiPortWrapper(MidiDeviceInfo info, int portType, int portIndex) {
        mInfo = info;
        mType = portType;
        mPortIndex = portIndex;
    }

    private void updateString() {
        if (mInfo == null) {
            mString = "- - - - - -";
        } else {
            StringBuilder sb = new StringBuilder();
            String name = mInfo.getProperties()
                    .getString(MidiDeviceInfo.PROPERTY_NAME);
            if (name == null) {
                name = mInfo.getProperties()
                        .getString(MidiDeviceInfo.PROPERTY_MANUFACTURER) + ", "
                        + mInfo.getProperties()
                                .getString(MidiDeviceInfo.PROPERTY_PRODUCT);
            }
            sb.append("#" + mInfo.getId());
            sb.append(", ").append(name);
            PortInfo portInfo = findPortInfo();
            sb.append("[" + mPortIndex + "]");
            if (portInfo != null) {
                sb.append(", ").append(portInfo.getName());
            } else {
                sb.append(", null");
            }
            mString = sb.toString();
        }
    }

    /**
     * @param info
     * @param portIndex
     * @return
     */
    private PortInfo findPortInfo() {
        PortInfo[] ports = mInfo.getPorts();
        for (PortInfo portInfo : ports) {
            if (portInfo.getPortNumber() == mPortIndex
                    && portInfo.getType() == mType) {
                return portInfo;
            }
        }
        return null;
    }

    public int getPortIndex() {
        return mPortIndex;
    }

    public MidiDeviceInfo getDeviceInfo() {
        return mInfo;
    }

    @Override
    public String toString() {
        if (mString == null) {
            updateString();
        }
        return mString;
    }

    @Override
    public boolean equals(Object other) {
        if (other == null)
            return false;
        if (!(other instanceof MidiPortWrapper))
            return false;
        MidiPortWrapper otherWrapper = (MidiPortWrapper) other;
        if (mPortIndex != otherWrapper.mPortIndex)
            return false;
        if (mType != otherWrapper.mType)
            return false;
        if (mInfo == null)
            return (otherWrapper.mInfo == null);
        return mInfo.equals(otherWrapper.mInfo);
    }

    @Override
    public int hashCode() {
        int hashCode = 1;
        hashCode = 31 * hashCode + mPortIndex;
        hashCode = 31 * hashCode + mType;
        hashCode = 31 * hashCode + mInfo.hashCode();
        return hashCode;
    }

}
