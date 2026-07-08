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

import java.util.Random;

/**
 * Try to crash in the native AAudio code by causing a routing change
 * while playing audio. The buffer may get deleted while we are writing to it!
 * See b/274815060
 */
public class TestRouteDuringCallbackActivity extends AppCompatActivity {

    private TextView mStatusView;
    private MyStreamSniffer mStreamSniffer;
    private AudioManager mAudioManager;
    private RadioButton mOutputButton;
    private RadioButton mInputButton;
    private Button mStartButton;
    private Button mStopButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_routing_crash);
        mStatusView = (TextView) findViewById(R.id.text_callback_status);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);

        mStartButton = (Button) findViewById(R.id.button_start_test);
        mStopButton = (Button) findViewById(R.id.button_stop_test);
        mOutputButton = (RadioButton) findViewById(R.id.direction_output);
        mInputButton = (RadioButton) findViewById(R.id.direction_input);
        setButtonsEnabled(false);
    }

    public void onStartRoutingTest(View view) {
        startRoutingTest();
    }

    public void onStopRoutingTest(View view) {
        stopRoutingTest();
    }

    private void setButtonsEnabled(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mOutputButton.setEnabled(!running);
        mInputButton.setEnabled(!running);
    }

    // Change routing while the stream is playing.
    // Keep trying until we crash.
    protected class MyStreamSniffer extends Thread {
        boolean enabled = true;
        int routingOption = 0;
        StringBuffer statusBuffer = new StringBuffer();
        int loopCount;

        @Override
        public void run() {
            routingOption = 0;
            changeRoute(routingOption);
            int result;
            Random random = new Random();
            while (enabled) {
                loopCount++;
                if (routingOption == 0) {
                    statusBuffer = new StringBuffer();
                }
                try {
                    sleep(100);
                    boolean useInput = mInputButton.isChecked();
                    result = startStream(useInput);
                    sleep(100);
                    log("-------#" + loopCount + ", " + (useInput ? "IN" : "OUT")
                            + "\nstartStream() returned " + result);
                    int sleepTimeMillis = 500 + random.nextInt(500);
                    sleep(sleepTimeMillis);
                    routingOption = (routingOption == 0) ? 1 : 0;
                    log( "changeRoute " + routingOption);
                    changeRoute(routingOption);
                    sleep(50);
                } catch (InterruptedException e) {
                } finally {
                    result = stopStream();
                    log("stopStream() returned " + result);
                }
            }
            changeRoute(0);
        }

        // Log to screen and logcat.
        private void log(String text) {
            Log.d(TAG, "RoutingCrash: " + text);
            statusBuffer.append(text + ", sleep " + getSleepTimeMicros() + " us\n");
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

    private void changeRoute(int option) {
        mAudioManager.setSpeakerphoneOn(option > 0);
    }

    private native int startStream(boolean useInput);
    private native int getSleepTimeMicros();
    private native int stopStream();

    @Override
    public void onPause() {
        super.onPause();
        stopRoutingTest();
    }

    private void startRoutingTest() {
        stopRoutingTest();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setButtonsEnabled(true);
        mStreamSniffer = new MyStreamSniffer();
        mStreamSniffer.start();
    }

    private void stopRoutingTest() {
        if (mStreamSniffer != null) {
            mStreamSniffer.finish();
            mStreamSniffer = null;
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            setButtonsEnabled(false);
        }
    }
}
