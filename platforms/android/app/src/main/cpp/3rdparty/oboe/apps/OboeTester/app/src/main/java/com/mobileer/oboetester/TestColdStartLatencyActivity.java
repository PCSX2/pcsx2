/*
 * Copyright 2023 The Android Open Source Project
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

import static com.mobileer.oboetester.TestAudioActivity.TAG;

import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.RadioButton;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Test for getting the cold start latency
 */
public class TestColdStartLatencyActivity extends AppCompatActivity {

    private TextView mStatusView;
    private MyStreamSniffer mStreamSniffer;
    private AudioManager mAudioManager;
    private RadioButton mOutputButton;
    private RadioButton mInputButton;
    private CheckBox mLowLatencyCheckBox;
    private CheckBox mMmapCheckBox;
    private CheckBox mExclusiveCheckBox;
    private Spinner mStartStabilizeDelaySpinner;
    private Spinner mCloseOpenDelaySpinner;
    private Spinner mOpenStartDelaySpinner;
    private Button mStartButton;
    private Button mStopButton;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_cold_start_latency);
        mStatusView = (TextView) findViewById(R.id.text_status);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);

        mStartButton = (Button) findViewById(R.id.button_start_test);
        mStopButton = (Button) findViewById(R.id.button_stop_test);
        mOutputButton = (RadioButton) findViewById(R.id.direction_output);
        mInputButton = (RadioButton) findViewById(R.id.direction_input);
        mMmapCheckBox = (CheckBox) findViewById(R.id.checkbox_mmap);
        mExclusiveCheckBox = (CheckBox) findViewById(R.id.checkbox_exclusive);
        mLowLatencyCheckBox = (CheckBox) findViewById(R.id.checkbox_low_latency);
        mStartStabilizeDelaySpinner = (Spinner) findViewById(R.id.spinner_start_stabilize_time);
        mStartStabilizeDelaySpinner.setSelection(7); // Set to 1000 ms by default
        mCloseOpenDelaySpinner = (Spinner) findViewById(R.id.spinner_close_open_time);
        mOpenStartDelaySpinner = (Spinner) findViewById(R.id.spinner_open_start_time);

        setButtonsEnabled(false);
    }

    public void onStartColdStartLatencyTest(View view) {
        keepScreenOn(true);
        stopSniffer();
        mStreamSniffer = new MyStreamSniffer();
        mStreamSniffer.start();
        setButtonsEnabled(true);
    }

    public void onStopColdStartLatencyTest(View view) {
        keepScreenOn(false);
        stopSniffer();
        setButtonsEnabled(false);
    }

    private void setButtonsEnabled(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mOutputButton.setEnabled(!running);
        mInputButton.setEnabled(!running);
        mLowLatencyCheckBox.setEnabled(!running);
        mMmapCheckBox.setEnabled(!running);
        mExclusiveCheckBox.setEnabled(!running);
        mStartStabilizeDelaySpinner.setEnabled(!running);
        mCloseOpenDelaySpinner.setEnabled(!running);
        mOpenStartDelaySpinner.setEnabled(!running);
    }

    protected class MyStreamSniffer extends Thread {
        boolean enabled = true;
        StringBuffer statusBuffer = new StringBuffer();
        int loopCount;

        @Override
        public void run() {
            boolean useInput = mInputButton.isChecked();
            boolean useLowLatency = mLowLatencyCheckBox.isChecked();
            boolean useMmap = mMmapCheckBox.isChecked();
            boolean useExclusive = mExclusiveCheckBox.isChecked();
            Log.d(TAG,(useInput ? "IN" : "OUT")
                    + ", " + (useLowLatency ? "LOW_LATENCY" : "NOT LOW_LATENCY")
                    + ", " + (useMmap ? "MMAP" : "NOT MMAP")
                    + ", " + (useExclusive ? "EXCLUSIVE" : "SHARED"));
            String closeSleepTimeText =
                    (String) mCloseOpenDelaySpinner.getAdapter().getItem(
                            mCloseOpenDelaySpinner.getSelectedItemPosition());
            int closedSleepTimeMillis = Integer.parseInt(closeSleepTimeText);
            Log.d(TAG, "Sleep before open time = " + closedSleepTimeMillis + " msec");
            String openSleepTimeText = (String) mOpenStartDelaySpinner.getAdapter().getItem(
                    mOpenStartDelaySpinner.getSelectedItemPosition());
            int openSleepTimeMillis = Integer.parseInt(openSleepTimeText);
            Log.d(TAG, "Sleep after open Time = " + openSleepTimeMillis + " msec");
            String startStabilizeTimeText = (String) mStartStabilizeDelaySpinner.getAdapter().getItem(
                    mStartStabilizeDelaySpinner.getSelectedItemPosition());
            int startSleepTimeMillis = Integer.parseInt(startStabilizeTimeText);
            Log.d(TAG, "Sleep after start Time = " + startSleepTimeMillis + " msec");
            while (enabled) {
                loopCount++;
                try {
                    sleep(closedSleepTimeMillis);
                    int result = openStream(useInput, useLowLatency, useMmap, useExclusive);
                    if (result != 0) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                Toast.makeText(TestColdStartLatencyActivity.this,
                                        "Error opening stream. Error: " + result,
                                        Toast.LENGTH_SHORT).show();
                            }
                        });
                        break;
                    }
                    log("-------#" + loopCount + " Device Id: " + getAudioDeviceId());
                    log("open() Latency: " + getOpenTimeMicros() / 1000 + " msec");
                    sleep(openSleepTimeMillis);
                    startStream();
                    log("requestStart() Latency: " + getStartTimeMicros() / 1000 + " msec");
                    sleep(startSleepTimeMillis);
                    waitForValidTimestamp();
                    log("Cold Start Latency: " + getColdStartTimeMicros() / 1000 + " msec");
                    closeStream();
                } catch (InterruptedException e) {
                    enabled = false;
                } finally {
                    closeStream();
                }
            }
        }

        // Log to screen and logcat.
        private void log(String text) {
            statusBuffer.append(text + "\n");
            showStatus(statusBuffer.toString());
        }

        // Stop the test thread.
        void finish() {
            enabled = false;
            interrupt();
            try {
                join(2000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    protected void showStatus(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStatusView.setText(message);
            }
        });
    }

    private native int openStream(boolean useInput, boolean useLowLatency, boolean useMmap,
                                  boolean useExclusive);
    private native int startStream();
    private native int closeStream();
    private native void waitForValidTimestamp();
    private native int getOpenTimeMicros();
    private native int getStartTimeMicros();
    private native int getColdStartTimeMicros();
    private native int getAudioDeviceId();

    @Override
    public void onPause() {
        super.onPause();
        stopSniffer();
    }

    private void stopSniffer() {
        if (mStreamSniffer != null) {
            mStreamSniffer.finish();
            mStreamSniffer = null;
        }
    }

    protected void keepScreenOn(boolean on) {
        if (on) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }
}
