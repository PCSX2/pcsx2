/*
 * Copyright 2015 The Android Open Source Project
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

import static com.mobileer.oboetester.StreamConfiguration.convertErrorToText;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Locale;

/**
 * Demonstrate the behavior of a changing CPU load on underruns.
 * Display the workload and the callback duration in a chart.
 * Enable or disable PerformanceHints (ADPF) using a checkbox.
 * This might boost the CPU frequency when Oboe is taking too long to compute the next buffer.
 * ADPF docs at: https://developer.android.com/reference/android/os/PerformanceHintManager
 */
public class DynamicWorkloadActivity extends TestOutputActivityBase {
    private static final int WORKLOAD_HIGH_MIN = 30;
    private static final int WORKLOAD_HIGH_MAX = 150;
    // When the CPU is completely saturated then the load will be above 1.0.
    public static final double LOAD_RECOVERY_HIGH = 1.0;
    // Use a slightly lower value for going low so that the comparator has hysteresis.
    public static final double LOAD_RECOVERY_LOW = 0.95;

    private static final float MARGIN_ABOVE_WORKLOAD_FOR_CPU = 1.2f;

    // By default, set high workload to 70 voices, which is reasonable for most devices.
    public static final double WORKLOAD_PROGRESS_FOR_70_VOICES = 0.53;

    public static final String KEY_USE_ADPF = "use_adpf";
    public static final boolean VALUE_DEFAULT_USE_ADPF = false;
    public static final String KEY_USE_WORKLOAD = "use_workload";
    public static final boolean VALUE_DEFAULT_USE_WORKLOAD = false;
    public static final String KEY_SCROLL_GRAPHICS = "scroll_graphics";
    public static final boolean VALUE_DEFAULT_SCROLL_GRAPHICS = false;
    public static final String KEY_USE_WORKLOAD_INCREASE_API = "use_workload_increase_api";
    public static final boolean VALUE_DEFAULT_USE_WORKLOAD_INCREASE_API = false;

    private Button mStopButton;
    private Button mStartButton;
    private TextView mResultView;
    private LinearLayout mAffinityLayout;
    private ArrayList<CheckBox> mAffinityBoxes = new ArrayList<CheckBox>();
    private WorkloadUpdateThread mUpdateThread;

    private MultiLineChart mMultiLineChart;
    private MultiLineChart.Trace mMaxCpuLoadTrace;
    private MultiLineChart.Trace mWorkloadTrace;
    private CheckBox mUseAltAdpfBox;
    private CheckBox mPerfHintBox;
    private CheckBox mWorkloadReportBox;
    private boolean mDrawChartAlways = true;
    private CheckBox mDrawAlwaysBox;
    private CheckBox mSustainedPerformanceModeBox;
    private CheckBox mWorkloadIncreaseApiBox;
    private int mCpuCount;
    private boolean mShouldUseADPF;
    private boolean mShouldUseWorkloadReporting;
    private boolean mEnableWorkloadIncreaseApi;
    private int mLastNotifyWorkloadResult;

    private static final int WORKLOAD_LOW = 1;
    private int mWorkloadHigh; // this will get set later
    private WorkloadView mDynamicWorkloadView;

    // Periodically query the status of the streams.
    protected class WorkloadUpdateThread {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 40;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;
        public static final int SNIFFER_TOGGLE_PERIOD_MSEC = 3000;
        private static final int STATE_IDLE = 0;
        private static final int STATE_RUN_LOW = 1;
        private static final int STATE_RUN_HIGH = 2;

        private Handler mHandler;

        private int mWorkloadCurrent = 1;

        private int mState = STATE_IDLE;
        private long mLastToggleTime = 0;
        private long mLastXRunCount = 0;
        private long mRecoveryTimeBegin;
        private long mRecoveryTimeEnd;
        private long mStartTimeNanos;

        String stateToString(int state) {
            switch(state) {
                case STATE_IDLE:
                    return "Idle";
                case STATE_RUN_LOW:
                    return "low";
                case STATE_RUN_HIGH:
                    return "HIGH";
                default:
                    return "Unrecognized";
            }
        }

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                int nextWorkload = mWorkloadCurrent;
                AudioStreamBase stream = mAudioOutTester.getCurrentAudioStream();
                float cpuLoad = stream.getCpuLoad();
                float maxCpuLoad = stream.getAndResetMaxCpuLoad();
                int cpuMask = stream.getAndResetCpuMask();
                long now = System.currentTimeMillis();
                boolean drawChartOnce = false;

                switch (mState) {
                    case STATE_IDLE:
                        drawChartOnce = true; // clear old chart
                        mState = STATE_RUN_LOW;
                        mLastToggleTime = now;
                        break;
                    case STATE_RUN_LOW:
                        nextWorkload = WORKLOAD_LOW;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_HIGH;
                            mRecoveryTimeBegin = 0;
                            mRecoveryTimeEnd = 0;
                        }
                        break;
                    case STATE_RUN_HIGH:
                        nextWorkload = mWorkloadHigh;
                        if ((now - mLastToggleTime) > SNIFFER_TOGGLE_PERIOD_MSEC) {
                            mLastToggleTime = now;
                            mState = STATE_RUN_LOW;
                            // Draw now when a CPU spike will not affect the result.
                            drawChartOnce = true;
                        }

                        if (mRecoveryTimeBegin == 0) {
                            if (maxCpuLoad > LOAD_RECOVERY_HIGH) {
                                mRecoveryTimeBegin = now;
                            }
                        } else if (mRecoveryTimeEnd == 0) {
                            if (maxCpuLoad < LOAD_RECOVERY_LOW) {
                                mRecoveryTimeEnd = now;
                            }
                        } else if (maxCpuLoad > LOAD_RECOVERY_LOW) {
                            mRecoveryTimeEnd = now;
                        }
                        break;
                }
                stream.setWorkload((int) nextWorkload);
                mWorkloadCurrent = nextWorkload;
                final int xRunCount = stream.getXRunCount();
                final boolean useSecondaryColor = (xRunCount != mLastXRunCount);
                mLastXRunCount = xRunCount;
                // Update chart
                float nowMicros = (System.nanoTime() - mStartTimeNanos) *  0.001f;
                mMultiLineChart.addX(nowMicros);
                mMaxCpuLoadTrace.add((float) maxCpuLoad, useSecondaryColor);
                mWorkloadTrace.add((float) mWorkloadCurrent, false /* useSecondaryColor */);
                if (drawChartOnce || mDrawChartAlways){
                    mMultiLineChart.update();
                }

                // Display numbers
                String recoveryTimeString = (mRecoveryTimeEnd <= mRecoveryTimeBegin) ?
                        "---" : ((mRecoveryTimeEnd - mRecoveryTimeBegin) + " msec");
                String message =
                        "#Voices = " + (int) nextWorkload
                        + "\nWorkState = " + stateToString(mState)
                        + "\nCPU = " + String.format(Locale.getDefault(), "%6.3f%c", cpuLoad * 100, '%')
                        + "\ncores = " + cpuMaskToString(cpuMask, mCpuCount)
                        + "\nRecovery = " + recoveryTimeString
                        + "\nNotify = " + convertErrorToText(mLastNotifyWorkloadResult);
                postResult(message);

                mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
            }
        };

        private void start() {
            stop();
            mStartTimeNanos = System.nanoTime();
            mMultiLineChart.reset();
            mState = STATE_IDLE;
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stop() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }

    }

    private void setWorkloadHigh(int workloadHigh) {
        mWorkloadHigh = workloadHigh;
    }


    /**
     * This text will look best in a monospace font.
     * @param cpuMask CPU core bit mask
     * @return a text display of the selected cores like "--2-45-7"
     */
    // TODO move this to some utility class
    private String cpuMaskToString(int cpuMask, int cpuCount) {
        String text = "";
        long longMask = ((long) cpuMask) & 0x0FFFFFFFFL;
        int index = 0;
        while (longMask != 0 || index < cpuCount) {
            text += ((longMask & 1) != 0) ? hexDigit(index) : "-";
            longMask = longMask >> 1;
            index++;
        }
        return text;
    }

    private char hexDigit(int n) {
        byte x = (byte)(n & 0x0F);
        if (x < 10) return (char)('0' + x);
        else return (char)('A' + x);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_dynamic_workload);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAudioOutTester = addAudioOutputTester();

        mResultView = (TextView) findViewById(R.id.resultView);
        mResultView.setTypeface(Typeface.MONOSPACE);
        mStartButton = (Button) findViewById(R.id.button_start);
        mStopButton = (Button) findViewById(R.id.button_stop);

        mDynamicWorkloadView = (WorkloadView) findViewById(R.id.dynamic_workload_view);
        mWorkloadView.setVisibility(View.GONE);

        // Add a row of checkboxes for setting CPU affinity.
        mCpuCount = NativeEngine.getCpuCount();
        final int defaultCpuAffinityMask = 0;
        View.OnClickListener checkBoxListener = new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                // Create a mack from all the checkboxes.
                int mask = 0;
                for (int cpuIndex = 0; cpuIndex < mCpuCount; cpuIndex++) {
                    CheckBox checkBox = mAffinityBoxes.get(cpuIndex);
                    if (checkBox.isChecked()) {
                        mask |= (1 << cpuIndex);
                    }
                }
                NativeEngine.setCpuAffinityMask(mask);
            }
        };
        mAffinityLayout = (LinearLayout)  findViewById(R.id.affinityLayout);
        for (int cpuIndex = 0; cpuIndex < mCpuCount; cpuIndex++) {
            CheckBox checkBox = new CheckBox(DynamicWorkloadActivity.this);
            mAffinityLayout.addView(checkBox);
            mAffinityBoxes.add(checkBox);
            checkBox.setText(cpuIndex + "");
            checkBox.setOnClickListener(checkBoxListener);
            if (((1 << cpuIndex) & defaultCpuAffinityMask) != 0) {
                checkBox.setChecked(true);
            }
        }
        NativeEngine.setCpuAffinityMask(defaultCpuAffinityMask);

        mMultiLineChart = (MultiLineChart) findViewById(R.id.multiline_chart);
        mMaxCpuLoadTrace = mMultiLineChart.createTrace("CPU", Color.GREEN, Color.RED,
                0.0f, 2.0f);
        mWorkloadTrace = mMultiLineChart.createTrace("Work", Color.DKGRAY,
                0.0f, (MARGIN_ABOVE_WORKLOAD_FOR_CPU * WORKLOAD_HIGH_MAX));

        mPerfHintBox = (CheckBox) findViewById(R.id.enable_perf_hint);
        mWorkloadReportBox = (CheckBox) findViewById(R.id.enable_workload_report);

        // TODO remove when finished with ADPF experiments.
        mUseAltAdpfBox = (CheckBox) findViewById(R.id.use_alternative_adpf);
        mUseAltAdpfBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setUseAlternativeAdpf(checkBox.isChecked());
            mPerfHintBox.setEnabled(!checkBox.isChecked());
        });
        mUseAltAdpfBox.setVisibility(View.GONE);

        mPerfHintBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            mShouldUseADPF = checkBox.isChecked();
            setPerformanceHintEnabled(mShouldUseADPF);
            mUseAltAdpfBox.setEnabled(!mShouldUseADPF);
            mWorkloadReportBox.setEnabled(mShouldUseADPF);
            mWorkloadIncreaseApiBox.setEnabled(mShouldUseADPF);
        });

        mWorkloadReportBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            mShouldUseWorkloadReporting = checkBox.isChecked();
            setWorkloadReportingEnabled(mShouldUseWorkloadReporting);
        });
        mWorkloadReportBox.setEnabled(mShouldUseADPF);

        mWorkloadIncreaseApiBox = (CheckBox) findViewById(R.id.enable_adpf_workload_increase);
        mWorkloadIncreaseApiBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            mEnableWorkloadIncreaseApi = checkBox.isChecked();
            setNotifyWorkloadIncreaseEnabled(mEnableWorkloadIncreaseApi);
        });
        mWorkloadIncreaseApiBox.setEnabled(mEnableWorkloadIncreaseApi);

        CheckBox hearWorkloadBox = (CheckBox) findViewById(R.id.hear_workload);
        hearWorkloadBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            setHearWorkload(checkBox.isChecked());
        });

        mDrawAlwaysBox = (CheckBox) findViewById(R.id.draw_always);
        mDrawAlwaysBox.setOnClickListener(buttonView -> {
            CheckBox checkBox = (CheckBox) buttonView;
            mDrawChartAlways = checkBox.isChecked();
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            PowerManager powerManager = (PowerManager) getApplicationContext().getSystemService(Context.POWER_SERVICE);
            mSustainedPerformanceModeBox = (CheckBox) findViewById(R.id.sustained_perf_mode);
            if (powerManager.isSustainedPerformanceModeSupported()) {
                mSustainedPerformanceModeBox.setOnClickListener(buttonView -> {
                    CheckBox checkBox = (CheckBox) buttonView;
                    getWindow().setSustainedPerformanceMode(checkBox.isChecked());
                });
            } else {
                mSustainedPerformanceModeBox.setEnabled(false);
            }
        }

        if (mDynamicWorkloadView != null) {
            mDynamicWorkloadView.setWorkloadReceiver((w) -> {
                setWorkloadHigh(w);
            });

            mDynamicWorkloadView.setLabel("High Workload");
            mDynamicWorkloadView.setRange(WORKLOAD_HIGH_MIN, WORKLOAD_HIGH_MAX);
            mDynamicWorkloadView.setFaderNormalizedProgress(WORKLOAD_PROGRESS_FOR_70_VOICES);
        }

        updateButtons(false);
        updateEnabledWidgets();
        hideSettingsViews(); // make more room
    }

    private void setHearWorkload(boolean checked) {
        mAudioOutTester.getCurrentAudioStream().setHearWorkload(checked);
    }

    private void setPerformanceHintEnabled(boolean checked) {
        mAudioOutTester.getCurrentAudioStream().setPerformanceHintEnabled(checked);
    }

    private void setWorkloadReportingEnabled(boolean enabled) {
        NativeEngine.setWorkloadReportingEnabled(enabled);
    }

    private void setNotifyWorkloadIncreaseEnabled(boolean enabled) {
        NativeEngine.setNotifyWorkloadIncreaseEnabled(enabled);
    }

    private void updateButtons(boolean running) {
        mStartButton.setEnabled(!running);
        mStopButton.setEnabled(running);
        mPerfHintBox.setEnabled(running);
        mWorkloadReportBox.setEnabled(running && mShouldUseADPF);
        mWorkloadIncreaseApiBox.setEnabled(running && mShouldUseADPF);
    }

    private void postResult(final String text) {
        runOnUiThread(new Runnable() {
            public void run() {
                mResultView.setText(text);
            }
        });
    }

    @Override
    int getActivityType() {
        return ACTIVITY_DYNAMIC_WORKLOAD;
    }

    public void startTest(View view) {
        startTest();
    }

    private void startTest() {
        try {
            openAudio();
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Open audio failed!");
            return;
        }
        try {
            super.startAudio();
            setPerformanceHintEnabled(mShouldUseADPF);
            updateButtons(true);
            postResult("Running test");
            mUpdateThread = new WorkloadUpdateThread();
            mUpdateThread.start();
        } catch (IOException e) {
            e.printStackTrace();
            showErrorToast("Start audio failed! " + e.getMessage());
            return;
        }
    }

    public void stopTest(View view) {
        onStopTest();
    }

    @Override
    public void onStopTest() {
        WorkloadUpdateThread updateThread = mUpdateThread;
        if (updateThread != null) {
            updateThread.stop();
        }
        updateButtons(false);
        super.onStopTest();
    }


    @Override
    public void startTestUsingBundle() {
        try {
            StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
            IntentBasedTestSupport.configureOutputStreamFromBundle(mBundleFromIntent, requestedOutConfig);

            // Specific options.
            mShouldUseADPF = mBundleFromIntent.getBoolean(KEY_USE_ADPF,
                    VALUE_DEFAULT_USE_ADPF);
            mShouldUseWorkloadReporting = mBundleFromIntent.getBoolean(KEY_USE_WORKLOAD,
                    VALUE_DEFAULT_USE_WORKLOAD);
            mDrawChartAlways =
                    mBundleFromIntent.getBoolean(KEY_SCROLL_GRAPHICS,
                            VALUE_DEFAULT_SCROLL_GRAPHICS);
            mEnableWorkloadIncreaseApi = mBundleFromIntent.getBoolean(KEY_USE_WORKLOAD_INCREASE_API,
                    VALUE_DEFAULT_USE_WORKLOAD_INCREASE_API);

            startTest();

            runOnUiThread(() -> {
                mPerfHintBox.setChecked(mShouldUseADPF);
                setPerformanceHintEnabled(mShouldUseADPF);
                mWorkloadReportBox.setChecked(mShouldUseWorkloadReporting);
                setWorkloadReportingEnabled(mShouldUseWorkloadReporting);
                mWorkloadIncreaseApiBox.setChecked(mEnableWorkloadIncreaseApi);
                setNotifyWorkloadIncreaseEnabled(mEnableWorkloadIncreaseApi);
                mDrawAlwaysBox.setChecked(mDrawChartAlways);
            });

            
        } catch (Exception e) {
            showErrorToast(e.getMessage());
        } finally {
            mBundleFromIntent = null;
        }
    }

    @Override
    public void stopAutomaticTest() {
        String report = getCommonTestReport();
        AudioStreamBase outputStream =mAudioOutTester.getCurrentAudioStream();
        report += "out.xruns = " + outputStream.getXRunCount() + "\n";
        report += "use.adpf = " + (mShouldUseADPF ? "yes" : "no") + "\n";
        report += "use.workload = " + (mShouldUseWorkloadReporting ? "yes" : "no") + "\n";
        report += "scroll.graphics = " + (mDrawChartAlways ? "yes" : "no") + "\n";
        onStopTest();
        maybeWriteTestResult(report);
        mTestRunningByIntent = false;
    }
}
