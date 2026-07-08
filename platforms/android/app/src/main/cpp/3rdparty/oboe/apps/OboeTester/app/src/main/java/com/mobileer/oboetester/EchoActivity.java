/*
 * Copyright 2018 The Android Open Source Project
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
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

import java.io.IOException;

/**
 * Activity to capture audio and then send a delayed copy to output.
 * There is a fader for setting delay time
 */
public class EchoActivity extends TestInputActivity {

    AudioOutputTester mAudioOutTester;

    protected TextView mTextDelayTime;
    protected SeekBar mFaderDelayTime;
    protected ExponentialTaper mTaperDelayTime;
    private static final double MIN_DELAY_TIME_SECONDS = 0.0;
    private static final double MAX_DELAY_TIME_SECONDS = 3.0;
    private double mDelayTime;
    private Button mStartButton;
    private Button mStopButton;
    private TextView mStatusTextView;

    private ColdStartSniffer mNativeSniffer = new ColdStartSniffer();

    protected static final int MAX_DELAY_TIME_PROGRESS = 1000;

    private SeekBar.OnSeekBarChangeListener mDelayListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setDelayTimeByPosition(progress);
        }

        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }

        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };

    // Periodically query for cold start latency from the native code until it is ready.
    protected class ColdStartSniffer extends NativeSniffer {

        private int stableCallCount = 0;
        private static final int STABLE_CALLS_NEEDED = 20;
        private int mInputLatency;
        private int mOutputLatency;

        @Override
        public void startSniffer() {
            stableCallCount = 0;
            mInputLatency = -1;
            mOutputLatency = -1;
            super.startSniffer();
        }

        @Override
        public boolean isComplete() {
            mInputLatency = getColdStartInputMillis();
            mOutputLatency = getColdStartOutputMillis();
            if (mInputLatency > 0 && mOutputLatency > 0) {
                stableCallCount++;
            }
            return stableCallCount > STABLE_CALLS_NEEDED;
        }

        private String getCurrentStatusReport() {
            StringBuffer message = new StringBuffer();
            message.append("cold.start.input.msec = " +
                    ((mInputLatency > 0)
                            ? mInputLatency
                            : "?")
                    + "\n");
            message.append("cold.start.output.msec = " +
                    ((mOutputLatency > 0)
                            ? mOutputLatency
                            : "?")
                    + "\n");
            message.append("stable.call.count = " + stableCallCount +  "\n");
            return message.toString();
        }

        @Override
        public void updateStatusText() {
            String message = getCurrentStatusReport();
            mStatusTextView.setText(message);
        }

    }

    private native int getColdStartInputMillis();
    private native int getColdStartOutputMillis();

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_echo);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        updateEnabledWidgets();

        mAudioOutTester = addAudioOutputTester();

        mStartButton = (Button) findViewById(R.id.button_start_echo);
        mStopButton = (Button) findViewById(R.id.button_stop_echo);
        mStopButton.setEnabled(false);

        mStatusTextView = (TextView) findViewById(R.id.text_status);

        mTextDelayTime = (TextView) findViewById(R.id.text_delay_time);
        mFaderDelayTime = (SeekBar) findViewById(R.id.fader_delay_time);
        mFaderDelayTime.setOnSeekBarChangeListener(mDelayListener);
        mTaperDelayTime = new ExponentialTaper(
                MIN_DELAY_TIME_SECONDS,
                MAX_DELAY_TIME_SECONDS,
                100.0);
        mFaderDelayTime.setProgress(MAX_DELAY_TIME_PROGRESS / 2);

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);
        hideSettingsViews();
    }

    private void setDelayTimeByPosition(int progress) {
        mDelayTime = mTaperDelayTime.linearToExponential(
                ((double)progress)/MAX_DELAY_TIME_PROGRESS);
        setDelayTime(mDelayTime);
        mTextDelayTime.setText("DelayLine: " + (int)(mDelayTime * 1000) + " (msec)");
    }

    private native void setDelayTime(double delayTimeSeconds);

    int getActivityType() {
        return ACTIVITY_ECHO;
    }

    @Override
    protected void resetConfiguration() {
        super.resetConfiguration();
        mAudioOutTester.reset();
    }

    public void onStartEcho(View view) {
        try {
            openAudio();
            startAudio();
            setDelayTime(mDelayTime);
            mStartButton.setEnabled(false);
            mStopButton.setEnabled(true);
            keepScreenOn(true);
            mNativeSniffer.startSniffer();
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopEcho(View view) {
        mNativeSniffer.stopSniffer();
        stopAudio();
        closeAudio();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        keepScreenOn(false);
    }

    @Override
    boolean isOutput() {
        return false;
    }
}
