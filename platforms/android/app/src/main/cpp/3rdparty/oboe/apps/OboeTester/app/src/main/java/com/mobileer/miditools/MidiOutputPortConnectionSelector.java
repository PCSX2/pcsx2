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

import android.app.Activity;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.util.Log;

import java.io.IOException;

/**
 * Select an output port and connect it to a destination input port.
 */
public class MidiOutputPortConnectionSelector extends MidiPortSelector {
    public final static String TAG = "MidiOutputPortConnectionSelector";
    private MidiPortConnector mSynthConnector;
    private MidiDeviceInfo mDestinationDeviceInfo;
    private int mDestinationPortIndex;
    private MidiPortWrapper mLastWrapper;
    private MidiPortConnector.OnPortsConnectedListener mConnectedListener;

    /**
     * Create a selector for connecting to the destination input port.
     *
     * @param midiManager
     * @param activity
     * @param spinnerId
     * @param destinationDeviceInfo
     * @param destinationPortIndex
     */
    public MidiOutputPortConnectionSelector(MidiManager midiManager,
            Activity activity, int spinnerId,
            MidiDeviceInfo destinationDeviceInfo, int destinationPortIndex) {
        super(midiManager, activity, spinnerId,
                MidiDeviceInfo.PortInfo.TYPE_OUTPUT);
        mDestinationDeviceInfo = destinationDeviceInfo;
        mDestinationPortIndex = destinationPortIndex;
    }

    @Override
    public void onPortSelected(final MidiPortWrapper wrapper) {
        if(!wrapper.equals(mLastWrapper)) {
            onClose();
            if (wrapper.getDeviceInfo() != null) {
                mSynthConnector = new MidiPortConnector(mMidiManager);
                mSynthConnector.connectToDevicePort(wrapper.getDeviceInfo(),
                        wrapper.getPortIndex(), mDestinationDeviceInfo,
                        mDestinationPortIndex,
                        // not safe on UI thread
                        mConnectedListener, null);
            }
        }
        mLastWrapper = wrapper;
    }

    @Override
    public void onClose() {
        try {
            if (mSynthConnector != null) {
                mSynthConnector.close();
                mSynthConnector = null;
            }
        } catch (IOException e) {
            Log.e(MidiConstants.TAG, "Exception in closeSynthResources()", e);
        }
        super.onClose();
    }

    /**
     * @param connectedListener
     */
    public void setConnectedListener(
            MidiPortConnector.OnPortsConnectedListener connectedListener) {
        mConnectedListener = connectedListener;
    }
}
