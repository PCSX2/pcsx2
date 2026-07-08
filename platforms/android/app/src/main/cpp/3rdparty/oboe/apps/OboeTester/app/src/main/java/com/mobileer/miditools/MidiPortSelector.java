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
import android.media.midi.MidiDeviceStatus;
import android.media.midi.MidiManager;
import android.media.midi.MidiManager.DeviceCallback;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import java.util.HashSet;

/**
 * Base class that uses a Spinner to select available MIDI ports.
 */
public abstract class MidiPortSelector extends DeviceCallback {
    private int mType = MidiDeviceInfo.PortInfo.TYPE_INPUT;
    protected ArrayAdapter<MidiPortWrapper> mAdapter;
    protected HashSet<MidiPortWrapper> mBusyPorts = new HashSet<MidiPortWrapper>();
    private Spinner mSpinner;
    protected MidiManager mMidiManager;
    protected Activity mActivity;
    private MidiPortWrapper mCurrentWrapper;

    /**
     * @param midiManager
     * @param activity
     * @param spinnerId
     *            ID from the layout resource
     * @param type
     *            TYPE_INPUT or TYPE_OUTPUT
     */
    public MidiPortSelector(MidiManager midiManager, Activity activity,
            int spinnerId, int type) {
        mMidiManager = midiManager;
        mActivity = activity;
        mType = type;
        mAdapter = new ArrayAdapter<MidiPortWrapper>(activity,
                android.R.layout.simple_spinner_item);
        mAdapter.setDropDownViewResource(
                android.R.layout.simple_spinner_dropdown_item);
        mAdapter.add(new MidiPortWrapper(null, 0, 0));

        mSpinner = (Spinner) activity.findViewById(spinnerId);
        mSpinner.setOnItemSelectedListener(
                new AdapterView.OnItemSelectedListener() {

                    public void onItemSelected(AdapterView<?> parent, View view,
                            int pos, long id) {
                        mCurrentWrapper = mAdapter.getItem(pos);
                        onPortSelected(mCurrentWrapper);
                    }

                    public void onNothingSelected(AdapterView<?> parent) {
                        onPortSelected(null);
                        mCurrentWrapper = null;
                    }
                });
        mSpinner.setAdapter(mAdapter);

        MidiDeviceMonitor.getInstance(mMidiManager).registerDeviceCallback(this,
                new Handler(Looper.getMainLooper()));

        MidiDeviceInfo[] infos = mMidiManager.getDevices();
        for (MidiDeviceInfo info : infos) {
            onDeviceAdded(info);
        }
    }

    /**
     * Set to no port selected.
     */
    public void clearSelection() {
        mSpinner.setSelection(0);
    }

    private int getInfoPortCount(final MidiDeviceInfo info) {
        int portCount = (mType == MidiDeviceInfo.PortInfo.TYPE_INPUT)
                ? info.getInputPortCount() : info.getOutputPortCount();
        return portCount;
    }

    @Override
    public void onDeviceAdded(final MidiDeviceInfo info) {
        int portCount = getInfoPortCount(info);
        for (int i = 0; i < portCount; ++i) {
            MidiPortWrapper wrapper = new MidiPortWrapper(info, mType, i);
            mAdapter.add(wrapper);
            Log.i(MidiConstants.TAG, wrapper + " was added to " + this);
            mAdapter.notifyDataSetChanged();
        }
    }

    @Override
    public void onDeviceRemoved(final MidiDeviceInfo info) {
        int portCount = getInfoPortCount(info);
        for (int i = 0; i < portCount; ++i) {
            MidiPortWrapper wrapper = new MidiPortWrapper(info, mType, i);
            MidiPortWrapper currentWrapper = mCurrentWrapper;
            mAdapter.remove(wrapper);
            // If the currently selected port was removed then select no port.
            if (wrapper.equals(currentWrapper)) {
                clearSelection();
            }
            mAdapter.notifyDataSetChanged();
            Log.i(MidiConstants.TAG, wrapper + " was removed");
        }
    }

    @Override
    public void onDeviceStatusChanged(final MidiDeviceStatus status) {
        // If an input port becomes busy then remove it from the menu.
        // If it becomes free then add it back to the menu.
        if (mType == MidiDeviceInfo.PortInfo.TYPE_INPUT) {
            MidiDeviceInfo info = status.getDeviceInfo();
            Log.i(MidiConstants.TAG, "MidiPortSelector.onDeviceStatusChanged status = " + status
                    + ", mType = " + mType
                    + ", activity = " + mActivity.getPackageName()
                    + ", info = " + info);
            // Look for transitions from free to busy.
            int portCount = info.getInputPortCount();
            for (int i = 0; i < portCount; ++i) {
                MidiPortWrapper wrapper = new MidiPortWrapper(info, mType, i);
                if (!wrapper.equals(mCurrentWrapper)) {
                    if (status.isInputPortOpen(i)) { // busy?
                        if (!mBusyPorts.contains(wrapper)) {
                            // was free, now busy
                            mBusyPorts.add(wrapper);
                            mAdapter.remove(wrapper);
                            mAdapter.notifyDataSetChanged();
                        }
                    } else {
                        if (mBusyPorts.remove(wrapper)) {
                            // was busy, now free
                            mAdapter.add(wrapper);
                            mAdapter.notifyDataSetChanged();
                        }
                    }
                }
            }
        }
    }

    /**
     * Implement this method to handle the user selecting a port on a device.
     *
     * @param wrapper
     */
    public abstract void onPortSelected(MidiPortWrapper wrapper);

    /**
     * Implement this method to clean up any open resources.
     */
    public void onClose() {
    }

    /**
     * Implement this method to clean up any open resources.
     */
    public void onDestroy() {
        MidiDeviceMonitor.getInstance(mMidiManager).unregisterDeviceCallback(this);
    }

    /**
     *
     */
    public void close() {
        onClose();
    }
}
