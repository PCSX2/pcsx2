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

package com.mobileer.oboetester;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.util.Locale;

/**
 * Test basic output.
 */
public final class TestOutputActivity extends TestOutputActivityBase {

    public static final int MAX_CHANNEL_BOXES = 16;
    private CheckBox[] mChannelBoxes;
    private Spinner mOutputSignalSpinner;
    private TextView mVolumeTextView;
    private SeekBar mVolumeSeekBar;
    private CheckBox mShouldSetStreamControlByAttributes;
    private boolean mShouldDisableForCompressedFormat = false;

    private LinearLayout mFlushFromFrameLayout;
    private EditText mFlushFromFrameEditText;
    private Spinner mFlushFromAccuracySpinner;
    private Button mFlushFromFrameButton;

    private LinearLayout mPlaybackParametersLayout;
    private Spinner mFallbackModeSpinner;
    private TextView mFallbackModeTextView;
    private Spinner mStretchModeSpinner;
    private TextView mStretchModeTextView;
    private EditText mPitchEditText;
    private TextView mPitchTextView;
    private EditText mSpeedEditText;
    private TextView mSpeedTextView;
    private Button mSetPlaybackParametersButton;

    private class OutputSignalSpinnerListener implements android.widget.AdapterView.OnItemSelectedListener {
        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int pos, long id) {
            mAudioOutTester.setSignalType(pos);
        }
        @Override
        public void onNothingSelected(AdapterView<?> parent) {
            mAudioOutTester.setSignalType(0);
        }
    }
    private SeekBar.OnSeekBarChangeListener mVolumeChangeListener = new SeekBar.OnSeekBarChangeListener() {
        @Override
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
            setVolume(progress);
        }
        @Override
        public void onStartTrackingTouch(SeekBar seekBar) {
        }
        @Override
        public void onStopTrackingTouch(SeekBar seekBar) {
        }
    };
    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_output);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        updateEnabledWidgets();

        mAudioOutTester = addAudioOutputTester();

        mChannelBoxes = new CheckBox[MAX_CHANNEL_BOXES];
        int ic = 0;
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox0);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox1);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox2);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox3);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox4);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox5);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox6);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox7);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox8);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox9);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox10);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox11);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox12);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox13);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox14);
        mChannelBoxes[ic++] = (CheckBox) findViewById(R.id.channelBox15);
        configureChannelBoxes(0 /*channelCount*/, false /*shouldDisable*/);

        mOutputSignalSpinner = (Spinner) findViewById(R.id.spinnerOutputSignal);
        mOutputSignalSpinner.setOnItemSelectedListener(new OutputSignalSpinnerListener());
        mOutputSignalSpinner.setSelection(StreamConfiguration.NATIVE_API_UNSPECIFIED);

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);

        mVolumeTextView = (TextView) findViewById(R.id.textVolumeSlider);
        mVolumeSeekBar = (SeekBar) findViewById(R.id.faderVolumeSlider);
        mVolumeSeekBar.setOnSeekBarChangeListener(mVolumeChangeListener);

        mShouldSetStreamControlByAttributes = (CheckBox) findViewById(R.id.enableSetStreamControlByAttributes);

        mFlushFromFrameLayout = (LinearLayout) findViewById(R.id.flushFromFrameLayout);
        mFlushFromFrameEditText = (EditText) findViewById(R.id.flushFromFrameEditText);
        mFlushFromAccuracySpinner = (Spinner) findViewById(R.id.flushFromAccuracySpinner);
        mFlushFromFrameButton = (Button) findViewById(R.id.flushFromFrameButton);
        mFlushFromFrameButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                flushFromFrame();
            }
        });

        mPlaybackParametersLayout = (LinearLayout) findViewById(R.id.playbackParametersLayout);
        mPlaybackParametersLayout.setVisibility(View.GONE);
        mFallbackModeSpinner = (Spinner) findViewById(R.id.fallbackModeSpinner);
        mFallbackModeTextView = (TextView) findViewById(R.id.fallbackModeText);
        mStretchModeSpinner = (Spinner) findViewById(R.id.stretchModeSpinner);
        mStretchModeTextView = (TextView) findViewById(R.id.stretchModeText);
        mPitchEditText = (EditText) findViewById(R.id.pitchEditText);
        mPitchTextView = (TextView) findViewById(R.id.pitchText);
        mSpeedEditText = (EditText) findViewById(R.id.speedEditText);
        mSpeedTextView = (TextView) findViewById(R.id.speedText);
        mSetPlaybackParametersButton = (Button) findViewById(R.id.setPlaybackParametersButton);
        mSetPlaybackParametersButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                setPlaybackParameters();
            }
        });
    }

    private void setPlaybackParameters() {
        try {
            int fallbackMode = mFallbackModeSpinner.getSelectedItemPosition();
            int stretchMode = mStretchModeSpinner.getSelectedItemPosition();
            float pitch = Float.parseFloat(mPitchEditText.getText().toString());
            float speed = Float.parseFloat(mSpeedEditText.getText().toString());
            PlaybackParameters params = new PlaybackParameters(fallbackMode, stretchMode, pitch, speed);
            int result = setPlaybackParametersNative(params);
            if (result == 0) {
                Toast.makeText(this, "Successfully set playback parameters",
                        Toast.LENGTH_SHORT).show();
                updatePlaybackParametersText();
            } else {
                Toast.makeText(this, "Failed to set playback parameters, result: " + result,
                        Toast.LENGTH_SHORT).show();
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "Failed to setPlaybackParameters, invalid pitch or speed");
            showErrorToast("Invalid pitch or speed");
        }
    }

    private void updatePlaybackParametersText() {
        PlaybackParameters playbackParameters = getPlaybackParametersNative();
        if (playbackParameters == null) {
            Toast.makeText(this, "Failed to get playback parameters", Toast.LENGTH_SHORT).show();
            return;
        }
        mFallbackModeTextView.setText(playbackParameters.getFallbackModeAsStr());
        mStretchModeTextView.setText(playbackParameters.getStretchModeAsStr());
        mPitchTextView.setText(Float.toString(playbackParameters.mPitch));
        mSpeedTextView.setText(Float.toString(playbackParameters.mSpeed));
    }

    @Override
    int getActivityType() {
        return ACTIVITY_TEST_OUTPUT;
    }

    public void openAudio() throws IOException {
        super.openAudio();
        mShouldSetStreamControlByAttributes.setEnabled(false);
        mShouldDisableForCompressedFormat = StreamConfiguration.isCompressedFormat(
                mAudioOutTester.getCurrentAudioStream().getFormat());
        if (!isStreamClosed() && isOffloadStream()) {
            mPlaybackParametersLayout.setVisibility(View.VISIBLE);
            updatePlaybackParametersText();
        } else {
            mPlaybackParametersLayout.setVisibility(View.GONE);
        }
    }

    private void configureChannelBoxes(int channelCount, boolean shouldDisable) {
        for (int i = 0; i < mChannelBoxes.length; i++) {
            mChannelBoxes[i].setChecked(i < channelCount);
            mChannelBoxes[i].setEnabled(!shouldDisable && (i < channelCount));
        }
    }

    private void setVolume(int progress) {
        // Convert from (0, 500) range to (-50, 0).
        double decibels = (progress - 500) / 10.0f;
        double amplitude = Math.pow(10.0, decibels / 20.0);
        // When the slider is all way to the left, set a zero amplitude.
        if (progress == 0) {
            amplitude = 0;
        }
        mVolumeTextView.setText("Volume(dB): " + String.format(Locale.getDefault(), "%.1f",
                decibels));
        mAudioOutTester.setAmplitude((float) amplitude);
    }


    public void stopAudio() {
        configureChannelBoxes(0 /*channelCount*/, mShouldDisableForCompressedFormat);
        mOutputSignalSpinner.setEnabled(!mShouldDisableForCompressedFormat);
        super.stopAudio();
    }

    public void pauseAudio() {
        configureChannelBoxes(0 /*channelCount*/, mShouldDisableForCompressedFormat);
        mOutputSignalSpinner.setEnabled(!mShouldDisableForCompressedFormat);
        super.pauseAudio();
    }

    public void releaseAudio() {
        configureChannelBoxes(0 /*channelCount*/, false /*shouldDisable*/);
        mOutputSignalSpinner.setEnabled(true);
        super.releaseAudio();
    }

    public void closeAudio() {
        configureChannelBoxes(0 /*channelCount*/, false /*shouldDisable*/);
        mOutputSignalSpinner.setEnabled(true);
        mShouldSetStreamControlByAttributes.setEnabled(true);
        super.closeAudio();
        if (isStreamClosed()) {
            mPlaybackParametersLayout.setVisibility(View.GONE);
        }
    }

    public void startAudio() throws IOException {
        super.startAudio();
        int channelCount = mAudioOutTester.getCurrentAudioStream().getChannelCount();
        configureChannelBoxes(channelCount, mShouldDisableForCompressedFormat);
        mOutputSignalSpinner.setEnabled(false);
    }

    public void flushFromFrame() {
        try {
            int accuracy = mFlushFromAccuracySpinner.getSelectedItemPosition();
            long positionInFrames = Long.parseLong(mFlushFromFrameEditText.getText().toString());
            long result = flushFromFrameNative(accuracy, positionInFrames);
            if (result >= 0) {
                Toast.makeText(this,
                        String.format("Successfully flushed from: %d, actual flushed position: %d",
                                positionInFrames, result),
                        Toast.LENGTH_LONG).show();
            } else {
                Toast.makeText(
                        this, "Failed to flush from Frame", Toast.LENGTH_LONG).show();
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, "Failed to flushFromFrame, the requested frame(" +
                    mFlushFromFrameEditText.getText().toString() + ") is invalid");
            showErrorToast("Invalid number of frames");
        }
    }

    public void onChannelBoxClicked(View view) {
        CheckBox checkBox = (CheckBox) view;
        String text = (String) checkBox.getText();
        int channelIndex = Integer.parseInt(text);
        mAudioOutTester.setChannelEnabled(channelIndex, checkBox.isChecked());
    }

    @Override
    protected boolean shouldSetStreamControlByAttributes() {
        return mShouldSetStreamControlByAttributes.isChecked();
    }

    @Override
    public void startTestUsingBundle() {
        try {
            StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
            IntentBasedTestSupport.configureOutputStreamFromBundle(mBundleFromIntent, requestedOutConfig);

            int signalType = IntentBasedTestSupport.getSignalTypeFromBundle(mBundleFromIntent);
            mAudioOutTester.setSignalType(signalType);

            openAudio();
            int frameCount = IntentBasedTestSupport.getBufferFrameCount(mBundleFromIntent);
            if (frameCount != 0) {
                setBufferSizeByNumFrames(frameCount);
            } else {
                int burstCount = IntentBasedTestSupport.getBurstCount(mBundleFromIntent);
                if (burstCount != 0) {
                    setBufferSizeByNumBursts(burstCount);
                }
            }
            startAudio();
        } catch (Exception e) {
            showErrorToast(e.getMessage());
        } finally {
            mBundleFromIntent = null;
        }
    }

    @Override
    public void stopAutomaticTest() {
        String report = getCommonTestReport();
        stopAudio();
        maybeWriteTestResult(report);
        mTestRunningByIntent = false;
    }
}
