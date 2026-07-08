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

package com.mobileer.oboetester;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.util.AttributeSet;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;

import com.mobileer.audio_device.CommunicationDeviceSpinner;

public class CommunicationDeviceView extends LinearLayout {

    private AudioManager mAudioManager;
    private CheckBox mSpeakerphoneCheckbox;
    private CheckBox mScoCheckbox;
    private TextView mSpeakerStatusView;
    private TextView mScoStatusView;
    private BroadcastReceiver mScoStateReceiver;
    private boolean mScoStateReceiverRegistered = false;
    private CommunicationDeviceSpinner mDeviceSpinner;
    private int mScoState;
    private CommDeviceSniffer mCommDeviceSniffer = new CommDeviceSniffer();;

    protected class CommDeviceSniffer extends NativeSniffer {
        @Override
        public void updateStatusText() {
            showCommDeviceStatus();
        }
    }

    public CommunicationDeviceView(Context context) {
        super(context);
        initializeViews(context);
    }

    public CommunicationDeviceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context);
    }

    public CommunicationDeviceView(Context context,
                          AttributeSet attrs,
                          int defStyle) {
        super(context, attrs, defStyle);
        initializeViews(context);
    }

    /**
     * Inflates the views in the layout.
     *
     * @param context the current context for the view.
     */
    private void initializeViews(Context context) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.comm_device_view, this);

        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mSpeakerphoneCheckbox = (CheckBox) findViewById(R.id.setSpeakerphoneOn);
        mSpeakerphoneCheckbox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onSetSpeakerphoneOn(view);
            }
        });
        mSpeakerStatusView = (TextView) findViewById(R.id.spkr_status_view);

        mScoCheckbox = (CheckBox) findViewById(R.id.setBluetoothScoOn);
        mScoCheckbox.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                onStartStopBluetoothSco(view);
            }
        });
        mScoStatusView = (TextView) findViewById(R.id.sco_status_view);
        mScoStateReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                mScoState = intent.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, -1);
                showCommDeviceStatus();
            }
        };

        mDeviceSpinner = (CommunicationDeviceSpinner) findViewById(R.id.comm_devices_spinner);
        mDeviceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id)
            {
                AudioDeviceInfo[] commDeviceArray = mDeviceSpinner.getCommunicationsDevices();
                if (commDeviceArray != null) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        if (position == CommunicationDeviceSpinner.POS_CLEAR) {
                            mAudioManager.clearCommunicationDevice();
                        } else {
                            AudioDeviceInfo selectedDevice = commDeviceArray[position - CommunicationDeviceSpinner.POS_DEVICES]; // skip "Clear"
                            mAudioManager.setCommunicationDevice(selectedDevice);
                        }
                    }
                }
                showCommDeviceStatus();
            }
            public void onNothingSelected(AdapterView<?> parent) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    mAudioManager.clearCommunicationDevice();
                }
                showCommDeviceStatus();
            }
        });

        showCommDeviceStatus();
    }

    public void onStart() {
        registerScoStateReceiver();
        mCommDeviceSniffer.startSniffer();
    }


    public void onStop() {
        mCommDeviceSniffer.stopSniffer();
        mSpeakerphoneCheckbox.setChecked(false);
        setSpeakerPhoneOn(false);
        mScoCheckbox.setChecked(false);
        mAudioManager.stopBluetoothSco();
        unregisterScoStateReceiver();
    }

    public void onSetSpeakerphoneOn(View view) {
        Log.d(TestAudioActivity.TAG, "onSetSpeakerphoneOn() called from Checkbox");
        CheckBox checkBox = (CheckBox) view;
        boolean enabled = checkBox.isChecked();
        setSpeakerPhoneOn(enabled);
        showCommDeviceStatus();
    }

    private void setSpeakerPhoneOn(boolean enabled) {
        Log.d(TestAudioActivity.TAG, "call setSpeakerphoneOn(" + enabled + ")");
        mAudioManager.setSpeakerphoneOn(enabled);
    }

    private void showCommDeviceStatus() {
        boolean enabled = mAudioManager.isSpeakerphoneOn();
        String text = ":" + (enabled ? "ON" : "OFF");
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            AudioDeviceInfo commDeviceInfo = mAudioManager.getCommunicationDevice();
            if (commDeviceInfo != null) {
                text += ", #" + commDeviceInfo.getId();
            }
        }
        mSpeakerStatusView.setText(text + ",");

        if (mScoState == AudioManager.SCO_AUDIO_STATE_CONNECTING) {
            text = ":WAIT";
        } else if (mScoState == AudioManager.SCO_AUDIO_STATE_CONNECTED) {
            text = ":CON";
        } else if (mScoState == AudioManager.SCO_AUDIO_STATE_DISCONNECTED) {
            text = ":DISCON";
        }
        mScoStatusView.setText(text);
    }

    public void onStartStopBluetoothSco(View view) {
        CheckBox checkBox = (CheckBox) view;
        if (checkBox.isChecked()) {
            mAudioManager.startBluetoothSco();
        } else {
            mAudioManager.stopBluetoothSco();
        }
    }

    private synchronized void registerScoStateReceiver() {
        if (!mScoStateReceiverRegistered) {
            getContext().registerReceiver(mScoStateReceiver,
                    new IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED));
            mScoStateReceiverRegistered = true;
        }
    }

    private synchronized void unregisterScoStateReceiver() {
        if (mScoStateReceiverRegistered) {
            getContext().unregisterReceiver(mScoStateReceiver);
            mScoStateReceiverRegistered = false;
        }
    }

}
