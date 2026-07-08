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

/**
 * Activity to record and play back audio.
 */
public class RecorderActivity extends TestInputActivity {

    private static final int STATE_RECORDING = 5;
    private static final int STATE_PLAYING = 6;
    private int mRecorderState = AUDIO_STATE_STOPPED;
    private Button mRecordButton;
    private Button mStopButton;
    private Button mPlayButton;
    private Button mShareButton;
    private TextView mStatusView;
    private boolean mGotRecording;

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_recorder);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mRecordButton = (Button) findViewById(R.id.button_start_recording);
        mStopButton = (Button) findViewById(R.id.button_stop_record_play);
        mPlayButton = (Button) findViewById(R.id.button_start_playback);
        mShareButton = (Button) findViewById(R.id.button_share);
        mStatusView = (TextView) findViewById(R.id.status_view);
        mRecorderState = AUDIO_STATE_STOPPED;
        mGotRecording = false;
        updateButtons();
    }

    int getActivityType() {
        return ACTIVITY_RECORD_PLAY;
    }

    public void onStartRecording(View view) {
        try {
            openAudio();
            startAudio();
            mRecorderState = STATE_RECORDING;
            mGotRecording = true;
            updateButtons();
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onStopRecordPlay(View view) {
        stopAudio();
        closeAudio();
        mRecorderState = AUDIO_STATE_STOPPED;
        updateButtons();

        StringBuilder text = new StringBuilder();
        for (StreamContext streamContext: mStreamContexts) {
            RecordingStats recordingStats = getRecordingStatsJni();
            text
                .append("Peak amplitude: ")
                .append(recordingStats.peak)
                .append("\n")
                .append("Rms: ")
                .append(recordingStats.rms)
                .append("\n");
        }
        mStatusView.setText(text);
    }

    public void onStartPlayback(View view) {
        startPlayback();
        mRecorderState = STATE_PLAYING;
        updateButtons();
    }

    private void updateButtons() {
        mRecordButton.setEnabled(mRecorderState == AUDIO_STATE_STOPPED);
        mStopButton.setEnabled(mRecorderState != AUDIO_STATE_STOPPED);
        mPlayButton.setEnabled(mRecorderState == AUDIO_STATE_STOPPED && mGotRecording);
        mShareButton.setEnabled(mRecorderState == AUDIO_STATE_STOPPED && mGotRecording);
    }

    public void startPlayback() {
        try {
            mAudioInputTester.startPlayback();
            updateStreamConfigurationViews();
            updateEnabledWidgets();
        } catch (Exception e) {
            e.printStackTrace();
            showErrorToast(e.getMessage());
        }
    }

    @Override
    String getWaveTag() {
        return "recording";
    }

    private native RecordingStats getRecordingStatsJni();
}
