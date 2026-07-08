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
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

/**
 * Audio Workload Test Runner Activity
 */
public class AudioWorkloadTestRunnerActivity extends BaseOboeTesterActivity {

    private ExponentialSliderView mTargetDurationMsSlider;
    private ExponentialSliderView mNumBurstsSlider;
    private ExponentialSliderView mNumVoicesSlider;
    private ExponentialSliderView mAlternateNumVoicesSlider;
    private ExponentialSliderView mAlternatingPeriodMsSlider;

    private CheckBox mEnableAdpfBox;
    private CheckBox mEnableAdpfWorkloadIncreaseBox;
    private CheckBox mHearWorkloadBox;

    private Button mStartButton;
    private Button mStopButton;
    private TextView mStatusTextView;
    private TextView mResultTextView;

    private Handler mHandler;
    private static final int STATUS_UPDATE_PERIOD_MS = 100;

    private static final int OPERATION_SUCCESS = 0;

    private Runnable mUpdateStatusRunnable = new Runnable() {
        @Override
        public void run() {
            if (!stopIfDone()) {
                mStatusTextView.setText(getStatus());
                mHandler.postDelayed(this, STATUS_UPDATE_PERIOD_MS);
            } else {
                mStatusTextView.setText(getStatus());
                updateResultTextView();
                mStartButton.setEnabled(true);
                mStopButton.setEnabled(false);
                enableParamsUI(true);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_audio_workload_test_runner);

        mTargetDurationMsSlider = findViewById(R.id.target_duration_ms);
        mNumBurstsSlider = findViewById(R.id.num_bursts);
        mNumVoicesSlider = findViewById(R.id.num_voices);
        mAlternateNumVoicesSlider = findViewById(R.id.alternate_num_voices);
        mAlternatingPeriodMsSlider = findViewById(R.id.alternating_period_ms);

        mEnableAdpfBox = findViewById(R.id.enable_adpf);
        mEnableAdpfWorkloadIncreaseBox = findViewById(R.id.enable_adpf_workload_increase);
        mHearWorkloadBox = findViewById(R.id.hear_workload);

        mStartButton = findViewById(R.id.button_start_test);
        mStopButton = findViewById(R.id.button_stop_test);
        mStatusTextView = findViewById(R.id.status_text_view);
        mResultTextView = findViewById(R.id.result_text_view);

        mHandler = new Handler(Looper.getMainLooper());

        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        enableParamsUI(true);
    }

    @Override
    protected void onStop() {
        stop();
        super.onStop();
    }

    public void startTest(View view) {
        int targetDurationMs = mTargetDurationMsSlider.getValue();
        int numBursts = mNumBurstsSlider.getValue();
        int numVoices = mNumVoicesSlider.getValue();
        int alternateNumVoices = mAlternateNumVoicesSlider.getValue();
        int alternatingPeriodMs = mAlternatingPeriodMsSlider.getValue();
        boolean adpfEnabled = mEnableAdpfBox.isChecked();
        boolean adpfWorkloadIncreaseEnabled = mEnableAdpfWorkloadIncreaseBox.isChecked();
        boolean hearWorkload = mHearWorkloadBox.isChecked();

        int result = start(targetDurationMs, numBursts, numVoices, alternateNumVoices,
                alternatingPeriodMs, adpfEnabled, adpfWorkloadIncreaseEnabled, hearWorkload);
        if (result != OPERATION_SUCCESS) {
            showErrorToast("start failed! Error:" + result);
            return;
        }

        mStartButton.setEnabled(false);
        mStopButton.setEnabled(true);
        mResultTextView.setText("");
        enableParamsUI(false);
        mHandler.postDelayed(mUpdateStatusRunnable, STATUS_UPDATE_PERIOD_MS);
    }

    public void stopTest(View view) {
        int result = stop();
        if (result != OPERATION_SUCCESS) {
            showErrorToast("stop failed! Error:" + result);
            return;
        }
        mHandler.removeCallbacks(mUpdateStatusRunnable);
        updateResultTextView();
        mStartButton.setEnabled(true);
        mStopButton.setEnabled(false);
        enableParamsUI(true);
    }

    public void enableParamsUI(boolean enabled) {
        mTargetDurationMsSlider.setEnabled(enabled);
        mNumBurstsSlider.setEnabled(enabled);
        mNumVoicesSlider.setEnabled(enabled);
        mAlternateNumVoicesSlider.setEnabled(enabled);
        mAlternatingPeriodMsSlider.setEnabled(enabled);
        mEnableAdpfBox.setEnabled(enabled);
        mEnableAdpfWorkloadIncreaseBox.setEnabled(enabled);
        mHearWorkloadBox.setEnabled(enabled);
    }

    private void updateResultTextView() {
        int result = getResult();
        int xRunCount = getXRunCount();
        if (result == 1) {
            mResultTextView.setText("Result: PASS");
        } else if (result == -1) {
            mResultTextView.setText("Result: FAIL. XRunCount: " + xRunCount);
        } else {
            mResultTextView.setText("Result: UNKNOWN. XRunCount: " + xRunCount);
        }
    }

    public native int start(int targetDurationMs, int numBursts, int numVoices,
                            int alternateNumVoices, int alternatingPeriodMs, boolean adpfEnabled,
                            boolean adpfWorkloadIncreaseEnabled, boolean hearWorkload);
    public native boolean stopIfDone();
    public native String getStatus();
    public native int stop();
    public native int getResult();
    public native int getXRunCount();
}
