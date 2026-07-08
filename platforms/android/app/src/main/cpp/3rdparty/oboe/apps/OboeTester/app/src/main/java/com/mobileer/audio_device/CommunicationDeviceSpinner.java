package com.mobileer.audio_device;
/*
 * Copyright 2022 The Android Open Source Project
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

import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources.Theme;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatSpinner;

import com.mobileer.oboetester.R;

import java.util.List;

public class CommunicationDeviceSpinner extends AppCompatSpinner {
    private static final String TAG = CommunicationDeviceSpinner.class.getName();
    // menu positions
    public static final int POS_CLEAR = 0;
    public static final int POS_DEVICES = 1; // base position for device list
    private AudioDeviceAdapter mDeviceAdapter;
    private AudioManager mAudioManager;
    private Context mContext;
    AudioDeviceInfo[] mCommDeviceArray = null;

    public CommunicationDeviceSpinner(Context context){
        super(context);
        setup(context);
    }

    public CommunicationDeviceSpinner(Context context, int mode){
        super(context, mode);
        setup(context);
    }

    public CommunicationDeviceSpinner(Context context, AttributeSet attrs){
        super(context, attrs);
        setup(context);
    }

    public CommunicationDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr){
        super(context, attrs, defStyleAttr);
        setup(context);
    }

    public CommunicationDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr, int mode){
        super(context, attrs, defStyleAttr, mode);
        setup(context);
    }

    public CommunicationDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr,
                                      int mode, Theme popupTheme){
        super(context, attrs, defStyleAttr, mode, popupTheme);
        setup(context);
    }

    public AudioDeviceInfo[] getCommunicationsDevices() {
        return mCommDeviceArray;
    }

    private void setup(Context context){
        mContext = context;

        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);

        mDeviceAdapter = new AudioDeviceAdapter(context);
        setAdapter(mDeviceAdapter);

        // Add default entries to the list and select one.
        addDefaultDevicesOptions();
        setSelection(POS_CLEAR);
        setupCommunicationDeviceListener();
    }

    @TargetApi(31)
    private void setupCommunicationDeviceListener(){
        // Note that we will immediately receive a call to onDevicesAdded with the list of
        // devices which are currently connected.
        mAudioManager.registerAudioDeviceCallback(new AudioDeviceCallback() {
            @Override
            public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
                updateDeviceList();
            }

            public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
                updateDeviceList();
            }

            private void updateDeviceList() {
                mDeviceAdapter.clear();
                addDefaultDevicesOptions();
                setSelection(POS_CLEAR);
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
                    List<AudioDeviceInfo> commDeviceList = mAudioManager.getAvailableCommunicationDevices();
                    mCommDeviceArray = commDeviceList.toArray(new AudioDeviceInfo[0]);
                    // Communications Devices are always OUTPUTS.
                    List<AudioDeviceListEntry> deviceList =
                            AudioDeviceListEntry.createListFrom(
                                    mCommDeviceArray, AudioManager.GET_DEVICES_OUTPUTS);
                    mDeviceAdapter.addAll(deviceList);
                }
            }
        }, null);
    }

    private void addDefaultDevicesOptions() {
        mDeviceAdapter.add(new AudioDeviceListEntry(POS_CLEAR,
                mContext.getString(R.string.clear_comm)));
    }
}
