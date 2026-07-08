package com.google.oboe.samples.audio_device;
/*
 * Copyright 2017 The Android Open Source Project
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
import android.widget.Spinner;

import java.util.List;

public class AudioDeviceSpinner extends Spinner {

    private static final int AUTO_SELECT_DEVICE_ID = 0;
    private static final String TAG = AudioDeviceSpinner.class.getName();
    private int mDirectionType;
    private AudioDeviceAdapter mDeviceAdapter;
    private AudioManager mAudioManager;

    public AudioDeviceSpinner(Context context){
        super(context);
        setup(context);
    }

    public AudioDeviceSpinner(Context context, int mode){
        super(context, mode);
        setup(context);
    }

    public AudioDeviceSpinner(Context context, AttributeSet attrs){
        super(context, attrs);
        setup(context);
    }

    public AudioDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr){
        super(context, attrs, defStyleAttr);
        setup(context);
    }

    public AudioDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr, int mode){
        super(context, attrs, defStyleAttr, mode);
        setup(context);
    }

    public AudioDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr,
                              int defStyleRes, int mode){
        super(context, attrs, defStyleAttr, defStyleRes, mode);
        setup(context);
    }
    public AudioDeviceSpinner(Context context, AttributeSet attrs, int defStyleAttr,
                              int defStyleRes, int mode, Theme popupTheme){
        super(context, attrs, defStyleAttr, defStyleRes, mode, popupTheme);
        setup(context);
    }

    private void setup(Context context){
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);

        mDeviceAdapter = new AudioDeviceAdapter(context);
        setAdapter(mDeviceAdapter);

        // Add a default entry to the list and select it
        mDeviceAdapter.add(new AudioDeviceListEntry(AUTO_SELECT_DEVICE_ID,
                context.getString(R.string.auto_select)));
        setSelection(0);
    }

    @TargetApi(23)
    public void setDirectionType(int directionType){
        this.mDirectionType = directionType;
        setupAudioDeviceCallback();
    }

    @TargetApi(23)
    private void setupAudioDeviceCallback(){

        // Note that we will immediately receive a call to onDevicesAdded with the list of
        // devices which are currently connected.
        mAudioManager.registerAudioDeviceCallback(new AudioDeviceCallback() {
            @Override
            public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {

                List<AudioDeviceListEntry> deviceList =
                        AudioDeviceListEntry.createListFrom(addedDevices, mDirectionType);
                if (deviceList.size() > 0){
                    mDeviceAdapter.addAll(deviceList);
                }
            }

            public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {

                List<AudioDeviceListEntry> deviceList =
                        AudioDeviceListEntry.createListFrom(removedDevices, mDirectionType);
                for (AudioDeviceListEntry entry : deviceList){
                    mDeviceAdapter.remove(entry);
                }
            }
        }, null);
    }
}
