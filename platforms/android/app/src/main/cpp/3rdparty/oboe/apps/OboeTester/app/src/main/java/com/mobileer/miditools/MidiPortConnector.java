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

import android.media.midi.MidiDevice;
import android.media.midi.MidiDevice.MidiConnection;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.os.Handler;
import android.util.Log;

import java.io.IOException;

/**
 * Tool for connecting MIDI ports on two remote devices.
 */
public class MidiPortConnector {
    private final MidiManager mMidiManager;
    private MidiDevice mSourceDevice;
    private MidiDevice mDestinationDevice;
    private MidiConnection mConnection;

    /**
     * @param midiManager
     */
    public MidiPortConnector(MidiManager midiManager) {
        mMidiManager = midiManager;
    }

    public void close() throws IOException {
        if (mConnection != null) {
            Log.i(MidiConstants.TAG,
                    "MidiPortConnector closing connection " + mConnection);
            mConnection.close();
            mConnection = null;
        }
        if (mSourceDevice != null) {
            mSourceDevice.close();
            mSourceDevice = null;
        }
        if (mDestinationDevice != null) {
            mDestinationDevice.close();
            mDestinationDevice = null;
        }
    }

    private void safeClose() {
        try {
            close();
        } catch (IOException e) {
            Log.e(MidiConstants.TAG, "could not close resources", e);
        }
    }

    /**
     * Listener class used for receiving the results of
     * {@link #connectToDevicePort}
     */
    public interface OnPortsConnectedListener {
        /**
         * Called to respond to a {@link #connectToDevicePort} request
         *
         * @param connection
         *            a {@link MidiConnection} that represents the connected
         *            ports, or null if connection failed
         */
        abstract public void onPortsConnected(MidiConnection connection);
    }

    /**
     * Open two devices and connect their ports.
     *
     * @param sourceDeviceInfo
     * @param sourcePortIndex
     * @param destinationDeviceInfo
     * @param destinationPortIndex
     */
    public void connectToDevicePort(final MidiDeviceInfo sourceDeviceInfo,
            final int sourcePortIndex,
            final MidiDeviceInfo destinationDeviceInfo,
            final int destinationPortIndex) {
        connectToDevicePort(sourceDeviceInfo, sourcePortIndex,
                destinationDeviceInfo, destinationPortIndex, null, null);
    }

    /**
     * Open two devices and connect their ports.
     * Then notify listener of the result.
     *
     * @param sourceDeviceInfo
     * @param sourcePortIndex
     * @param destinationDeviceInfo
     * @param destinationPortIndex
     * @param listener
     * @param handler
     */
    public void connectToDevicePort(final MidiDeviceInfo sourceDeviceInfo,
            final int sourcePortIndex,
            final MidiDeviceInfo destinationDeviceInfo,
            final int destinationPortIndex,
            final OnPortsConnectedListener listener, final Handler handler) {
        safeClose();
        mMidiManager.openDevice(destinationDeviceInfo,
                new MidiManager.OnDeviceOpenedListener() {
                    @Override
                    public void onDeviceOpened(MidiDevice destinationDevice) {
                        if (destinationDevice == null) {
                            Log.e(MidiConstants.TAG,
                                    "could not open " + destinationDeviceInfo);
                            if (listener != null) {
                                listener.onPortsConnected(null);
                            }
                        } else {
                            mDestinationDevice = destinationDevice;
                            Log.i(MidiConstants.TAG,
                                    "connectToDevicePort opened "
                                            + destinationDeviceInfo);
                            // Destination device was opened so go to next step.
                            MidiInputPort destinationInputPort = destinationDevice
                                    .openInputPort(destinationPortIndex);
                            if (destinationInputPort != null) {
                                Log.i(MidiConstants.TAG,
                                        "connectToDevicePort opened port on "
                                                + destinationDeviceInfo);
                                connectToDevicePort(sourceDeviceInfo,
                                        sourcePortIndex,
                                        destinationInputPort,
                                        listener, handler);
                            } else {
                                Log.e(MidiConstants.TAG,
                                        "could not open port on "
                                                + destinationDeviceInfo);
                                safeClose();
                                if (listener != null) {
                                    listener.onPortsConnected(null);
                                }
                            }
                        }
                    }
                }, handler);
    }


    /**
     * Open a source device and connect its output port to the
     * destinationInputPort.
     *
     * @param sourceDeviceInfo
     * @param sourcePortIndex
     * @param destinationInputPort
     */
    private void connectToDevicePort(final MidiDeviceInfo sourceDeviceInfo,
            final int sourcePortIndex,
            final MidiInputPort destinationInputPort,
            final OnPortsConnectedListener listener, final Handler handler) {
        mMidiManager.openDevice(sourceDeviceInfo,
                new MidiManager.OnDeviceOpenedListener() {
                    @Override
                    public void onDeviceOpened(MidiDevice device) {
                        if (device == null) {
                            Log.e(MidiConstants.TAG,
                                    "could not open " + sourceDeviceInfo);
                            safeClose();
                            if (listener != null) {
                                listener.onPortsConnected(null);
                            }
                        } else {
                            Log.i(MidiConstants.TAG,
                                    "connectToDevicePort opened "
                                            + sourceDeviceInfo);
                            // Device was opened so connect the ports.
                            mSourceDevice = device;
                            mConnection = device.connectPorts(
                                    destinationInputPort, sourcePortIndex);
                            if (mConnection == null) {
                                Log.e(MidiConstants.TAG, "could not connect to "
                                        + sourceDeviceInfo);
                                safeClose();
                            }
                            if (listener != null) {
                                listener.onPortsConnected(mConnection);
                            }
                        }
                    }
                }, handler);
    }

}
