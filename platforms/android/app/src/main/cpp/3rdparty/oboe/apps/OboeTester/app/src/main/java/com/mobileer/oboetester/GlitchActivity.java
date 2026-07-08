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
import android.widget.TextView;

import java.io.IOException;
import java.util.Locale;

/**
 * Activity to measure the number of glitches.
 */
public class GlitchActivity extends AnalyzerActivity {
    private TextView mAnalyzerTextView;
    private Button mStartButton;
    private Button mStopButton;
    private Button mShareButton;

    // These must match the values in LatencyAnalyzer.h
    final static int STATE_IDLE = 0;
    final static int STATE_IMMUNE = 1;
    final static int STATE_WAITING_FOR_SIGNAL = 2;
    final static int STATE_WAITING_FOR_LOCK = 3;
    final static int STATE_LOCKED = 4;
    final static int STATE_GLITCHING = 5;
    String mLastGlitchReport;
    private int mInputChannel;
    private int mOutputChannel;

    native int getStateFrameCount(int state);
    native int getGlitchCount();

    // Number of frames in last glitch.
    native int getGlitchLength();
    native double getPhase();
    native double getSignalToNoiseDB();
    native double getPeakAmplitude();
    native double getSineAmplitude();
    native int getSinePeriod();

    protected NativeSniffer mNativeSniffer = createNativeSniffer();

    synchronized NativeSniffer createNativeSniffer() {
        return new GlitchSniffer();
    }

    // Note that these strings must match the enum result_code in LatencyAnalyzer.h
    static String stateToString(int resultCode) {
        switch (resultCode) {
            case STATE_IDLE:
                return "IDLE";
            case STATE_IMMUNE:
                return "IMMUNE";
            case STATE_WAITING_FOR_SIGNAL:
                return "WAITING_FOR_SIGNAL";
            case STATE_WAITING_FOR_LOCK:
                return "WAITING_FOR_LOCK";
            case STATE_LOCKED:
                return "LOCKED";
            case STATE_GLITCHING:
                return "GLITCHING";
            default:
                return "UNKNOWN";
        }
    }

    static String magnitudeToString(double magnitude) {
        return String.format(Locale.US, "%6.4f", magnitude);
    }

    // Periodically query for glitches from the native detector.
    protected class GlitchSniffer extends NativeSniffer {

        private long mTimeAtStart;
        private long mTimeOfLastGlitch;
        private double mSecondsWithoutGlitches;
        private double mMaxSecondsWithoutGlitches;
        private int mLastGlitchCount;
        private int mLastUnlockedFrames;
        private int mLastLockedFrames;
        private int mLastGlitchFrames;

        private int mStartResetCount;
        private int mLastResetCount;
        private int mPreviousState;

        private double mSignalToNoiseDB;
        private double mPeakAmplitude;
        private double mSineAmplitude;

        @Override
        public void startSniffer() {
            long now = System.currentTimeMillis();
            mTimeAtStart = now;
            mTimeOfLastGlitch = now;
            mLastUnlockedFrames = 0;
            mLastLockedFrames = 0;
            mLastGlitchFrames = 0;
            mSecondsWithoutGlitches = 0.0;
            mMaxSecondsWithoutGlitches = 0.0;
            mLastGlitchCount = 0;
            mStartResetCount = mLastResetCount;
            super.startSniffer();
        }

        private void gatherData() {
            int state = getAnalyzerState();
            mSignalToNoiseDB = getSignalToNoiseDB();
            mPeakAmplitude = getPeakAmplitude();
            mSineAmplitude = getSineAmplitude();
            int glitchCount = getGlitchCount();
            if (state != mPreviousState) {
                if ((state == STATE_WAITING_FOR_SIGNAL || state == STATE_WAITING_FOR_LOCK)
                        && glitchCount == 0) { // did not previously lock
                    GlitchActivity.this.giveAdvice("Try raising volume!");
                } else {
                    GlitchActivity.this.giveAdvice(null);
                }
            }
            mPreviousState = state;

            long now = System.currentTimeMillis();
            int resetCount = getResetCount();
            mLastUnlockedFrames = getStateFrameCount(STATE_WAITING_FOR_LOCK);
            int lockedFrames = getStateFrameCount(STATE_LOCKED);
            int glitchFrames = getStateFrameCount(STATE_GLITCHING);

            if (glitchFrames > mLastGlitchFrames || glitchCount > mLastGlitchCount) {
                mTimeOfLastGlitch = now;
                mSecondsWithoutGlitches = 0.0;
                if (glitchCount > mLastGlitchCount) {
                    onGlitchDetected();
                }
            } else if (lockedFrames > mLastLockedFrames) {
                mSecondsWithoutGlitches = (now - mTimeOfLastGlitch) / 1000.0;
            }

            if (resetCount > mLastResetCount) {
                mLastResetCount = resetCount;
            }

            if (mSecondsWithoutGlitches > mMaxSecondsWithoutGlitches) {
                mMaxSecondsWithoutGlitches = mSecondsWithoutGlitches;
            }

            mLastGlitchCount = glitchCount;
            mLastGlitchFrames = glitchFrames;
            mLastLockedFrames = lockedFrames;
            mLastResetCount = resetCount;
        }

        private String getCurrentStatusReport() {
            long now = System.currentTimeMillis();
            double totalSeconds = (now - mTimeAtStart) / 1000.0;

            StringBuffer message = new StringBuffer();
            message.append("state = " + stateToString(mPreviousState) + "\n");
            message.append(String.format(Locale.getDefault(), "unlocked.frames = %d\n", mLastUnlockedFrames));
            message.append(String.format(Locale.getDefault(), "locked.frames = %d\n", mLastLockedFrames));
            message.append(String.format(Locale.getDefault(), "glitch.frames = %d\n", mLastGlitchFrames));
            message.append(String.format(Locale.getDefault(), "reset.count = %d\n", mLastResetCount - mStartResetCount));
            message.append(String.format(Locale.getDefault(), "peak.amplitude = %8.6f\n", mPeakAmplitude));
            message.append(String.format(Locale.getDefault(), "sine.amplitude = %8.6f\n", mSineAmplitude));
            if (mLastLockedFrames > 0) {
                message.append(String.format(Locale.getDefault(), "signal.noise.ratio.db = %5.1f\n", mSignalToNoiseDB));
            }
            message.append(String.format(Locale.getDefault(), "time.total = %4.2f seconds\n", totalSeconds));
            if (mLastLockedFrames > 0) {
                message.append(String.format(Locale.getDefault(), "time.no.glitches = %4.2f\n", mSecondsWithoutGlitches));
                message.append(String.format(Locale.getDefault(), "max.time.no.glitches = %4.2f\n",
                        mMaxSecondsWithoutGlitches));
                message.append(String.format(Locale.getDefault(), "glitch.length = %d\n", getGlitchLength()));
                message.append(String.format(Locale.getDefault(), "glitch.count = %d\n", mLastGlitchCount));
            }
            return message.toString();
        }

        public String getShortReport() {
            String resultText = "amplitude: peak = " + magnitudeToString(mPeakAmplitude)
                    + ", sine = " + magnitudeToString(mSineAmplitude) + "\n";
            if (mPeakAmplitude < 0.01) {
                resultText += "WARNING: volume is very low!\n";
            }
            resultText += "#glitches = " + getLastGlitchCount()
                    + ", #resets = " + getLastResetCount()
                    + ", max no glitch = " + getMaxSecondsWithNoGlitch() + " secs\n";
            resultText += String.format(Locale.getDefault(), "SNR = %5.1f db", mSignalToNoiseDB);
            resultText += ", #locked = " + mLastLockedFrames;
            return resultText;
        }

        @Override
        public void updateStatusText() {
            gatherData();
            mLastGlitchReport = getCurrentStatusReport();
            setAnalyzerText(mLastGlitchReport);
            maybeDisplayWaveform();
        }

        public double getMaxSecondsWithNoGlitch() {
            return mMaxSecondsWithoutGlitches;
        }

        public int getLastGlitchCount() {
            return mLastGlitchCount;
        }
        public int getLastResetCount() {
            return mLastResetCount;
        }
    }

    public void giveAdvice(String s) {
    }

    // Called on UI thread
    protected void onGlitchDetected() {
    }

    protected void maybeDisplayWaveform() {}

    protected void setAnalyzerText(String s) {
        mAnalyzerTextView.setText(s);
    }

    /**
     * Set tolerance to deviations from expected value.
     * The normalized value will be scaled by the measured magnitude
     * of the sine wave..
     * @param tolerance normalized between 0.0 and 1.0
     */
    public native void setTolerance(float tolerance);

    public void setInputChannel(int channel) {
        mInputChannel = channel;
        setInputChannelNative(channel);
    }

    public void setOutputChannel(int channel) {
        mOutputChannel = channel;
        setOutputChannelNative(channel);
    }

    public int getInputChannel() {
        return mInputChannel;
    }

    public int getOutputChannel() {
        return mOutputChannel;
    }

    /**
     * Set the duration of a periodic forced glitch.
     * @param frames or zero for no glitch
     */
    public native void setForcedGlitchDuration(int frames);

    public native void setInputChannelNative(int channel);

    public native void setOutputChannelNative(int channel);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);
        mStopButton.setEnabled(false);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mAnalyzerTextView = (TextView) findViewById(R.id.text_status);
        updateEnabledWidgets();
        hideSettingsViews();
        // TODO hide sample rate menu
        StreamContext streamContext = getFirstInputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.hideSampleRateMenu();
            }
        }
    }

    @Override
    int getActivityType() {
        return ACTIVITY_GLITCHES;
    }

    @Override
    protected void onStart() {
        super.onStart();
        setInputChannel(0);
        setOutputChannel(0);
    }

    @Override
    protected void onStop() {
        if (!isBackgroundEnabled()) {
            stopAudioTest();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (isBackgroundEnabled()) {
            stopAudioTest();
        }
        super.onDestroy();
    }

    // Called on UI thread
    public void onStartAudioTest(View view) {
        try {
            openStartAudioTestUI();
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    protected void openStartAudioTestUI() throws IOException {
        openAudio();
        startAudioTest();
        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mShareButton.setEnabled(false);
        keepScreenOn(true);
    }

    public void startAudioTest() throws IOException {
        startAudio();
        mNativeSniffer.startSniffer();
        onTestBegan();
    }

    // Called on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
        onTestFinished();
        keepScreenOn(false);
    }

    // Must be called on UI thread.
    public void onTestBegan() {
    }

    // Must be called on UI thread.
    public void onTestFinished() {
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        mShareButton.setEnabled(true);
    }

    public void stopAudioTest() {
        mNativeSniffer.stopSniffer();
        stopAudio();
        closeAudio();
    }

    public void stopTest() {
        mNativeSniffer.stopSniffer();
        stopAudio();
    }

    @Override
    boolean isOutput() {
        return false;
    }

    public double getMaxSecondsWithNoGlitch() {
        return ((GlitchSniffer)mNativeSniffer).getMaxSecondsWithNoGlitch();
    }

    public String getShortReport() {
        return ((GlitchSniffer)mNativeSniffer).getShortReport();
    }

    @Override
    String getWaveTag() {
        return "glitches";
    }
}
