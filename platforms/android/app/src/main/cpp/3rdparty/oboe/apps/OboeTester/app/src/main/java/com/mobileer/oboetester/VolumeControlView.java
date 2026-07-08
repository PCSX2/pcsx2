/*
 * Copyright (C) 2025 The Android Open Source Project
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

import android.content.Context;
import android.media.AudioManager;
import android.os.Build;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * A custom compound view that encapsulates volume controls.
 * It inflates its own layout and manages its own logic, so it can be
 * dropped into any XML layout and work without any Java code in the Activity.
 */
public class VolumeControlView extends LinearLayout {

    private AudioManager mAudioManager;
    private int mCurrentStreamType = AudioManager.STREAM_MUSIC;
    private final Map<String, Integer> mStreamTypes = new LinkedHashMap<>();

    public VolumeControlView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        // Inflate the layout and attach it to this view
        LayoutInflater.from(context).inflate(R.layout.volume_buttons, this, true);

        // Get the AudioManager system service
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);

        // Populate the map of stream types
        mStreamTypes.put("Music", AudioManager.STREAM_MUSIC);
        mStreamTypes.put("Ring", AudioManager.STREAM_RING);
        mStreamTypes.put("Alarm", AudioManager.STREAM_ALARM);
        mStreamTypes.put("Notification", AudioManager.STREAM_NOTIFICATION);
        mStreamTypes.put("System", AudioManager.STREAM_SYSTEM);
        mStreamTypes.put("Voice Call", AudioManager.STREAM_VOICE_CALL);
        mStreamTypes.put("DTMF", AudioManager.STREAM_DTMF);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mStreamTypes.put("Accessibility", AudioManager.STREAM_ACCESSIBILITY);
        }

        // Set the orientation for the LinearLayout
        setOrientation(VERTICAL);

        setupControls();
    }

    private void setupControls() {
        // Find views within this component's layout
        Button volumeUpButton = findViewById(R.id.volume_up_button);
        Button volumeDownButton = findViewById(R.id.volume_down_button);
        Spinner streamTypeSpinner = findViewById(R.id.stream_type_spinner);

        // Setup Spinner
        if (streamTypeSpinner != null) {
            ArrayAdapter<String> adapter = new ArrayAdapter<>(
                    getContext(),
                    android.R.layout.simple_spinner_item,
                    new ArrayList<>(mStreamTypes.keySet())
            );
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            streamTypeSpinner.setAdapter(adapter);

            streamTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                @Override
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    String selectedStreamName = (String) parent.getItemAtPosition(position);
                    Integer streamType = mStreamTypes.get(selectedStreamName);
                    if (streamType != null) {
                        mCurrentStreamType = streamType;
                    }
                }

                @Override
                public void onNothingSelected(AdapterView<?> parent) {
                    // Do nothing
                }
            });
        }

        // Setup Volume Up Button
        if (volumeUpButton != null) {
            volumeUpButton.setOnClickListener(v -> {
                if (mAudioManager != null) {
                    mAudioManager.adjustStreamVolume(mCurrentStreamType, AudioManager.ADJUST_RAISE, AudioManager.FLAG_SHOW_UI);
                }
            });
        }

        // Setup Volume Down Button
        if (volumeDownButton != null) {
            volumeDownButton.setOnClickListener(v -> {
                if (mAudioManager != null) {
                    mAudioManager.adjustStreamVolume(mCurrentStreamType, AudioManager.ADJUST_LOWER, AudioManager.FLAG_SHOW_UI);
                }
            });
        }
    }
}
