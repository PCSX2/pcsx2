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
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Try to hang streams by rapidly opening and closing.
 * See b/348615156
 */
public class TestRapidCycleActivity extends AppCompatActivity {

    private TextView mStatusView;
    private MyStreamSniffer mStreamSniffer;
    private AudioManager mAudioManager;
    private RadioButton mApiOpenSLButton;
    private RadioButton mApiAAudioButton;
    private Button mStartButton;
    private Button mStopButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_rapid_cycle);
        mStatusView = (TextView) findViewById(R.id.text_callback_status);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);

        mStartButton = (Button) findViewById(R.id.button_start_test);
        mStopButton = (Button) findViewById(R.id.button_stop_test);
        mApiOpenSLButton = (RadioButton) findViewById(R.id.audio_api_opensl);
        mApiOpenSLButton.setChecked(true);
        mApiAAudioButton = (RadioButton) findViewById(R.id.audio_api_aaudio);
        setButtonsEnabled(false);
    }

    public void onStartCycleTest(View view) { startCycleTest(); }
    public void onStopCycleTest(View view) {
        stopCycleTest();
    }

    private void setButtonsEnabled(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mApiOpenSLButton.setEnabled(!running);
        mApiAAudioButton.setEnabled(!running);
    }

    // Change routing while the stream is playing.
    // Keep trying until we crash.
    protected class MyStreamSniffer extends Thread {
        boolean enabled = true;
        StringBuffer statusBuffer = new StringBuffer();

        @Override
        public void run() {
            int lastCycleCount = -1;
            boolean useOpenSL = mApiOpenSLButton.isChecked();
            startRapidCycleTest(useOpenSL);
            try {
                while (enabled) {
                    statusBuffer = new StringBuffer();
                    sleep(100);
                    int cycleCount = getCycleCount();
                    if (cycleCount > lastCycleCount) { // reduce spam
                        log("#" + cycleCount + " open/close cycles\n");
                        lastCycleCount = cycleCount;
                    }
                }
            } catch (InterruptedException e) {
            } finally {
                stopRapidCycleTest();
            }
        }

        // Log to screen and logcat.
        private void log(String text) {
            Log.d(TAG, "RapidCycle: " + text);
            statusBuffer.append(text);
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

    private native int startRapidCycleTest(boolean useOpenSL);
    private native int stopRapidCycleTest();
    private native int getCycleCount();

    @Override
    public void onPause() {
        super.onPause();
        Log.i(TAG, "onPause() called so stop the test =========================");
        stopCycleTest();
    }

    private void startCycleTest() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setButtonsEnabled(true);
        mStreamSniffer = new MyStreamSniffer();
        mStreamSniffer.start();
    }

    private void stopCycleTest() {
        if (mStreamSniffer != null) {
            mStreamSniffer.finish();
            mStreamSniffer = null;
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            setButtonsEnabled(false);
        }
    }
}
