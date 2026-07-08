/*
 * Copyright 2019 The Android Open Source Project
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

import android.annotation.TargetApi;
import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.widget.TextView;

import com.mobileer.audio_device.AudioDeviceInfoConverter;

import java.io.IOException;
import java.util.HashMap;

/**
 * Tests the latency of plugging in or unplugging an audio device.
 */
public class TestPlugLatencyActivity extends TestAudioActivity {

    public static final int POLL_DURATION_MILLIS = 1;
    public static final int TIMEOUT_MILLIS = 1000;

    private TextView     mInstructionsTextView;
    private TextView     mPlugTextView;
    private TextView     mAutoTextView;
    MyAudioDeviceCallback mDeviceCallback = new MyAudioDeviceCallback();
    private AudioManager mAudioManager;

    private volatile int mPlugCount = 0;
    private long         mTimeoutAtMillis;

    private AudioOutputTester   mAudioOutTester;

    class MyAudioDeviceCallback extends AudioDeviceCallback {
        private HashMap<Integer, AudioDeviceInfo> mDevices
                = new HashMap<Integer, AudioDeviceInfo>();

        @Override
        public void onAudioDevicesAdded(AudioDeviceInfo[] addedDevices) {
            boolean isBootingUp = mDevices.isEmpty();
            AudioDeviceInfo outputDeviceInfo = null;
            for (AudioDeviceInfo info : addedDevices) {
                mDevices.put(info.getId(), info);
                if (!isBootingUp)
                {
                    log("====== Device Added =======");
                    log(adiToString(info));
                    // Only process OUTPUT devices because that is what we are testing.
                    if (info.isSink()) {
                        outputDeviceInfo = info;
                    }
                }

            }

            if (isBootingUp) {
                log("Starting stream with existing audio devices");
            }
            if (outputDeviceInfo != null) {
                updateLatency(false /* wasDeviceRemoved */);
            }
        }

        public void onAudioDevicesRemoved(AudioDeviceInfo[] removedDevices) {
            AudioDeviceInfo outputDeviceInfo = null;
            for (AudioDeviceInfo info : removedDevices) {
                mDevices.remove(info.getId());
                log("====== Device Removed =======");
                log(adiToString(info));
                // Only process OUTPUT devices because that is what we are testing.
                if (info.isSink()) {
                    outputDeviceInfo = info;
                }
            }

            if (outputDeviceInfo != null) {
                updateLatency(true /* wasDeviceRemoved */);
            }
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_plug_latency);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mInstructionsTextView = (TextView) findViewById(R.id.text_instructions);
        mPlugTextView = (TextView) findViewById(R.id.text_plug_events);
        mAutoTextView = (TextView) findViewById(R.id.text_log_device_report);

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    }

    @Override
    protected void onStart() {
        super.onStart();
        keepScreenOn(true);
        addAudioDeviceCallback();
    }

    @Override
    protected void onStop() {
        removeAudioDeviceCallback();
        keepScreenOn(false);
        super.onStop();
    }

    @TargetApi(23)
    private void addAudioDeviceCallback(){
        // Note that we will immediately receive a call to onDevicesAdded with the list of
        // devices which are currently connected.
        mAudioManager.registerAudioDeviceCallback(mDeviceCallback, null);
    }

    @TargetApi(23)
    private void removeAudioDeviceCallback(){
        mAudioManager.unregisterAudioDeviceCallback(mDeviceCallback);
    }

    @Override
    public String getTestName() {
        return "Plug Latency";
    }

    int getActivityType() {
        return ACTIVITY_TEST_DISCONNECT;
    }

    @Override
    boolean isOutput() {
        return true;
    }

    // Write to status and command view
    private void setInstructionsText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mInstructionsTextView.setText(text);
            }
        });
    }

    public void startAudioTest() throws IOException {
        startAudio();
    }

    private void setupTimeout() {
        mTimeoutAtMillis = System.currentTimeMillis() + TIMEOUT_MILLIS;
    }

    private void sleepOrTimeout(String message) throws InterruptedException {
        Thread.sleep(POLL_DURATION_MILLIS);
        if (System.currentTimeMillis() >= mTimeoutAtMillis) {
            throw new InterruptedException(message);
        }
    }

    private long calculateLatencyMs(boolean wasDeviceRemoved) {
        long testStartMillis = System.currentTimeMillis();
        long frameReadMillis = -1;
        final int TIMEOUT_MAX = 100;
        int timeout;
        try {
            long callbackMillis = -1;
            if (wasDeviceRemoved && (mAudioOutTester != null)) {
                log("Wait for error callback != 0");
                // Keep querying as long as error is ok
                setupTimeout();
                while (mAudioOutTester.getLastErrorCallbackResult() == 0) {
                    sleepOrTimeout("timed out waiting while error==0");
                }
                callbackMillis = System.currentTimeMillis();
                log("Error callback at " + (callbackMillis - testStartMillis) + " ms. " +
                        "WAIT -> CALLBACK = took " + (callbackMillis - testStartMillis) + " ms");
            }
            closeAudio();
            long closedMillis = System.currentTimeMillis();
            if (callbackMillis == -1) {
                log("Audio closed at " + (closedMillis - testStartMillis) + " ms");
            } else {
                log("Audio closed at " + (closedMillis - testStartMillis) + " ms. " +
                        "CALLBACK -> CLOSED took " + (closedMillis - callbackMillis) + " ms");
            }

            clearStreamContexts();
            mAudioOutTester = addAudioOutputTester();
            openAudio();
            long openedMillis = System.currentTimeMillis();
            log("Audio opened at " + (openedMillis - testStartMillis) + " ms. " +
                    "CLOSED -> OPENED took " + (openedMillis - closedMillis) + " ms");
            AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
            startAudioTest();
            long startingMillis = System.currentTimeMillis();
            log("Audio starting at " + (startingMillis - testStartMillis) + " ms. " +
                    "OPENED -> STARTING took " + (startingMillis - openedMillis) + " ms");

            setupTimeout();
            while (stream.getState() == StreamConfiguration.STREAM_STATE_STARTING) {
                sleepOrTimeout("timed out waiting while STATE_STARTING");
            }
            long startedMillis = System.currentTimeMillis();
            log("Audio started at " + (startedMillis - testStartMillis) + " ms. " +
                    "STARTING -> STARTED took " + (startedMillis - startingMillis) + " ms");

            setupTimeout();
            while (mAudioOutTester.getFramesRead() == 0) {
                sleepOrTimeout("timed out waiting while framesRead()==0");
            }
            frameReadMillis = System.currentTimeMillis();
            log("First frame read at " + (frameReadMillis - testStartMillis) + " ms. " +
                    "STARTED -> READ took " + (frameReadMillis - startedMillis) + " ms");
        } catch (IOException | InterruptedException e) {
            log("EXCEPTION: " + e);
            e.printStackTrace();
            closeAudio();
            return -1;
        }

        return frameReadMillis - testStartMillis;
    }

    public static String adiToString(AudioDeviceInfo adi) {

        StringBuilder sb = new StringBuilder();
        sb.append("Id: ");
        sb.append(adi.getId());

        sb.append("\nProduct name: ");
        sb.append(adi.getProductName());

        sb.append("\nType: ");
        sb.append(AudioDeviceInfoConverter.typeToString(adi.getType()));

        sb.append("\nIsSource: ");
        sb.append(String.valueOf(adi.isSource()));
        sb.append(", IsSink: ");
        sb.append(String.valueOf(adi.isSink()));

        return sb.toString();
    }

    // Write to scrollable TextView
    private void log(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mAutoTextView.append(text);
                mAutoTextView.append("\n");
            }
        });
    }

    private void updateLatency(boolean wasDeviceRemoved) {
        mPlugCount++;
        log("\nOperation #" + mPlugCount + " starting");
        long latencyMs = calculateLatencyMs(wasDeviceRemoved);
        String message = "Operation #" + mPlugCount + " latency: "+ latencyMs + " ms\n";
        log(message);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mPlugTextView.setText(message);
            }
        });
    }
}
