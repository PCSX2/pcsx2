/*
 * Copyright 2025 The Android Open Source Project
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

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Test for testing reverse JNI.
 * This test lets you configure the buffer size and the sleep duration to simulate a workload and
 * lets you hear a sine wave and see the XRun count.
 * This will inform us the viability of creating a Java/Kotlin only interface that uses Oboe.
 */
public class ReverseJniActivity extends AppCompatActivity {

    private ReverseJniEngine mAudioEngine;
    private Button mStartButton;
    private Button mStopButton;
    TextView mStatusTextView;
    private ExponentialSliderView mSleepDurationUsSlider;
    private ExponentialSliderView mBufferSizeInBurstsSlider;
    private boolean mIsPlaying = false;

    private Handler mUiUpdateHandler;
    private Runnable mUiUpdateRunnable;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_reverse_jni); // Assumes a basic layout with two buttons

        mAudioEngine = new ReverseJniEngine();
        mStartButton = findViewById(R.id.startButton);
        mStopButton = findViewById(R.id.stopButton);
        mStatusTextView = findViewById(R.id.text_status);
        mSleepDurationUsSlider = findViewById(R.id.sleep_duration_us);
        mBufferSizeInBurstsSlider = findViewById(R.id.num_bursts);

        mStartButton.setOnClickListener(v -> {
            if (!mIsPlaying) {
                startAudio();
            }
        });

        mStopButton.setOnClickListener(v -> {
            if (mIsPlaying) {
                stopAudio();
            }
        });

        mSleepDurationUsSlider.setOnValueChangedListener(new ExponentialSliderView.OnValueChangedListener() {
            @Override
            public void onValueChanged(ExponentialSliderView view, int value) {
                mAudioEngine.setSleepDurationUs(value);
            }
        });

        mBufferSizeInBurstsSlider.setOnValueChangedListener(new ExponentialSliderView.OnValueChangedListener() {
            @Override
            public void onValueChanged(ExponentialSliderView view, int value) {
                mAudioEngine.setBufferSizeInBursts(value);
            }
        });

        mUiUpdateHandler = new Handler(Looper.getMainLooper());
        mUiUpdateRunnable = new Runnable() {
            @Override
            public void run() {
                if (mIsPlaying && mAudioEngine != null) {
                    int xruns = mAudioEngine.getXRunCount();
                    // This must be run on the UI thread
                    mStatusTextView.setText(String.format("XRuns: %d", xruns));
                    mUiUpdateHandler.postDelayed(this, 50); // Update every 50ms
                }
            }
        };

        updateButtonStates();
        mStatusTextView.setText("XRuns: 0");
    }

    private void startAudio() {
        mAudioEngine.create();
        mAudioEngine.start(mBufferSizeInBurstsSlider.getValue(), mSleepDurationUsSlider.getValue());
        mIsPlaying = true;
        mUiUpdateHandler.post(mUiUpdateRunnable);
        updateButtonStates();
    }

    private void stopAudio() {
        mUiUpdateHandler.removeCallbacks(mUiUpdateRunnable);
        mAudioEngine.stop();
        mAudioEngine.destroy();
        mIsPlaying = false;
        updateButtonStates();
    }

    private void updateButtonStates() {
        mStartButton.setEnabled(!mIsPlaying);
        mStopButton.setEnabled(mIsPlaying);
    }

    @Override
    protected void onDestroy() {
        mUiUpdateHandler.removeCallbacks(mUiUpdateRunnable);
        if (mIsPlaying) {
            stopAudio();
        }
        super.onDestroy();
    }
}
