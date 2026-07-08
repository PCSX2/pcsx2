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

import static com.mobileer.oboetester.IntentBasedTestSupport.configureStreamsFromBundle;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;

import java.io.File;
import java.io.IOException;
import java.util.Locale;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class RoundTripLatencyActivity extends AnalyzerActivity {

    // STATEs defined in LatencyAnalyzer.h
    private static final int STATE_MEASURE_BACKGROUND = 0;
    private static final int STATE_IN_PULSE = 1;
    private static final int STATE_GOT_DATA = 2;
    private final static String LATENCY_FORMAT = "%4.2f";
    // When I use 5.3g I only get one digit after the decimal point!
    private final static String CONFIDENCE_FORMAT = "%5.3f";

    private TextView mAnalyzerView;
    private Button   mMeasureButton;
    private Button   mAverageButton;
    private Button   mScanButton;
    private Button   mCancelButton;
    private Button   mShareButton;
    private boolean  mHasRecording = false;

    private int     mBufferBursts = -1;
    private int     mInputBufferCapacityInBursts = -1;
    private int     mInputFramesPerBurst;
    private int     mOutputBufferCapacityInBursts = -1;
    private int     mOutputFramesPerBurst;
    private int     mActualBufferBursts;
    private boolean mOutputIsMMapExclusive;
    private boolean mInputIsMMapExclusive;

    private Handler mHandler = new Handler(Looper.getMainLooper()); // UI thread

    DoubleStatistics mTimestampLatencyStats = new DoubleStatistics(); // for single measurement

    protected abstract class MultipleLatencyTestRunner {
        final static int AVERAGE_TEST_DELAY_MSEC = 1000; // arbitrary
        boolean mActive;
        String  mLastReport = "";

        abstract String onAnalyserDone();

        public abstract void start();

        public void clear() {
            mActive = false;
            mLastReport = "";
        }

        public void cancel() {
            mActive = false;
        }

        public boolean isActive() {
            return mActive;
        }

        public String getLastReport() {
            return mLastReport;
        }
    }

    // Run the test several times and report the average latency.
    protected class AverageLatencyTestRunner extends MultipleLatencyTestRunner {
        private final static int AVERAGE_TEST_DELAY_MSEC = 1000; // arbitrary
        private static final int GOOD_RUNS_REQUIRED = 5; // arbitrary
        private static final int MAX_BAD_RUNS_ALLOWED = 5; // arbitrary
        private int mBadCount = 0; // number of bad measurements

        DoubleStatistics mLatencies = new DoubleStatistics();
        DoubleStatistics mConfidences = new DoubleStatistics();
        DoubleStatistics mTimestampLatencies = new DoubleStatistics(); // for multiple measurements

        private int getGoodCount() {
            return mLatencies.count();
        }

        // Called on UI thread.
        @Override
        String onAnalyserDone() {
            String message;
            boolean reschedule = false;
            if (!mActive) {
                message = "";
            } else if (getMeasuredResult() != 0) {
                mBadCount++;
                if (mBadCount > MAX_BAD_RUNS_ALLOWED) {
                    cancel();
                    updateButtons(false);
                    message = "averaging cancelled due to error\n";
                } else {
                    message = "skipping this bad run, "
                            + mBadCount + " of " + MAX_BAD_RUNS_ALLOWED + " max\n";
                    reschedule = true;
                }
            } else {
                double latency = getMeasuredLatencyMillis();
                mLatencies.add(latency);
                double confidence = getMeasuredConfidence();
                mConfidences.add(confidence);

                double timestampLatency = getTimestampLatencyMillis();
                if (timestampLatency > 0.0) {
                    mTimestampLatencies.add(timestampLatency);
                }
                if (getGoodCount() < GOOD_RUNS_REQUIRED) {
                    reschedule = true;
                } else {
                    mActive = false;
                    updateButtons(false);
                }
                message = reportAverage();
            }
            if (reschedule) {
                mHandler.postDelayed(new Runnable() {
                    @Override
                    public void run() {
                        measureSingleLatency();
                    }
                }, AVERAGE_TEST_DELAY_MSEC);
            }
            return message;
        }

        private String reportAverage() {
            String message;
            if (getGoodCount() == 0 || mConfidences.getSum() == 0.0) {
                message = "num.iterations = " + getGoodCount() + "\n";
            } else {
                final double mAverageConfidence = mConfidences.calculateMean();
                double meanLatency = mLatencies.calculateMean();
                double meanAbsoluteDeviation = mLatencies.calculateMeanAbsoluteDeviation(meanLatency);
                double timestampLatencyMean = -1;
                double timestampLatencyMAD = 0.0;
                if (mTimestampLatencies.count() > 0) {
                    timestampLatencyMean = mTimestampLatencies.calculateMean();
                    timestampLatencyMAD =
                            mTimestampLatencies.calculateMeanAbsoluteDeviation(timestampLatencyMean);
                }
                message = "average.latency.msec = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, meanLatency) + "\n"
                        + "mean.absolute.deviation = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, meanAbsoluteDeviation) + "\n"
                        + "average.confidence = "
                        + String.format(Locale.getDefault(), CONFIDENCE_FORMAT, mAverageConfidence) + "\n"
                        + "min.latency.msec = " + String.format(Locale.getDefault(), LATENCY_FORMAT, mLatencies.getMin()) + "\n"
                        + "max.latency.msec = " + String.format(Locale.getDefault(), LATENCY_FORMAT, mLatencies.getMax()) + "\n"
                        + "num.iterations = " + mLatencies.count() + "\n"
                        + "timestamp.latency.msec = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, timestampLatencyMean) + "\n"
                        + "timestamp.latency.mad = "
                        + String.format(Locale.getDefault(), LATENCY_FORMAT, timestampLatencyMAD) + "\n";
            }
            message += "num.failed = " + mBadCount + "\n";
            message += "\n"; // mark end of average report
            mLastReport = message;
            return message;
        }

        // Called on UI thread.
        @Override
        public void start() {
            mLatencies = new DoubleStatistics();
            mConfidences = new DoubleStatistics();
            mTimestampLatencies = new DoubleStatistics();
            mBadCount = 0;
            mActive = true;
            mLastReport = "";
            measureSingleLatency();
        }

    }
    AverageLatencyTestRunner mAverageLatencyTestRunner = new AverageLatencyTestRunner();
    MultipleLatencyTestRunner mCurrentLatencyTestRunner = mAverageLatencyTestRunner;

    /**
     * Search for a discontinuity in latency based on a number of bursts.
     * Use binary subdivision search algorithm.
     */
    protected static class BinaryDiscontinuityFinder {
        public static final double MAX_ALLOWED_DEVIATION = 0.2;
        // Run the test with various buffer sizes to detect DSP MMAP position errors.
        private int mLowBufferBursts = 2;
        private int mLowBufferLatency = -1;
        private int mMiddleBufferBursts = -1;
        private int mMiddleBufferLatency = -1;
        private int mHighBufferBursts = -1;
        private int mHighBufferLatency = -1;
        private String mMessage = "---";

        private static final int STATE_MEASURE_LOW = 1;
        private static final int STATE_MEASURE_HIGH = 2;
        private static final int STATE_MEASURE_MIDDLE = 3;
        private static final int STATE_DONE = 4;
        private int mState = STATE_MEASURE_LOW;
        private int mFramesPerBurst;

        public static final int RESULT_CONTINUE= 1;
        public static final int RESULT_OK = 0;
        public static final int RESULT_DISCONTINUITY = -1; // DSP is reading from the wrong place.
        public static final int RESULT_ERROR = -2; // Could not measure latency
        public static final int RESULT_UNDEFINED = -3; // Could not measure latency

        public int getFramesPerBurst() {
            return mFramesPerBurst;
        }

        public void setFramesPerBurst(int framesPerBurst) {
            mFramesPerBurst = framesPerBurst;
        }

        public String getMessage() {
            return mMessage;
        }

        public static class Result {
            public int code = RESULT_UNDEFINED;
            public int numBursts = -1;
        }

        /**
         * @return Result object with number of bursts and a RESULT code
         */
        Result onAnalyserDone(int latencyFrames,
                           double confidence,
                           int actualBufferBursts,
                           int capacityInBursts,
                           boolean isMMapExclusive) {
            Result result = new Result();
            mMessage = "analyzing";
            if (!isMMapExclusive) {
                mMessage = "skipped, not MMAP Exclusive";
                result.code = RESULT_OK;
                return result;
            }
            result.code = RESULT_CONTINUE;
            switch (mState) {
                case STATE_MEASURE_LOW:
                    mLowBufferLatency = latencyFrames;
                    mLowBufferBursts = actualBufferBursts;
                    // Now we measure the high side.
                    mHighBufferBursts = capacityInBursts;
                    result.code = RESULT_CONTINUE;
                    result.numBursts = mHighBufferBursts;
                    mMessage = "checked low bufferSize";
                    mState = STATE_MEASURE_HIGH;
                    break;
                case STATE_MEASURE_HIGH:
                    mMessage = "checked high bufferSize";
                    mHighBufferLatency = latencyFrames;
                    mHighBufferBursts = actualBufferBursts;
                    if (measureLatencyLinearity(mLowBufferBursts,
                            mLowBufferLatency,
                            mHighBufferBursts,
                            mHighBufferLatency) > MAX_ALLOWED_DEVIATION) {
                        mState = STATE_MEASURE_MIDDLE;
                    } else {
                        result.code = RESULT_OK;
                        mMessage = "DSP position looks good";
                        mState = STATE_DONE;
                    }
                    break;
                case STATE_MEASURE_MIDDLE:
                    mMiddleBufferLatency = latencyFrames;
                    mMiddleBufferBursts = actualBufferBursts;
                    // Check to see which side is bad.
                    if (confidence < 0.5) {
                        // We may have landed on the DSP so we got a scrambled result.
                        result.code = RESULT_DISCONTINUITY;
                        mMessage = "on top of DSP!";
                        mState = STATE_DONE;
                    } else  {
                        double deviationLow = measureLatencyLinearity(
                                mLowBufferBursts,
                                mLowBufferLatency,
                                mMiddleBufferBursts,
                                mMiddleBufferLatency);
                        double deviationHigh = measureLatencyLinearity(
                                mMiddleBufferBursts,
                                mMiddleBufferLatency,
                                mHighBufferBursts,
                                mHighBufferLatency);
                        if (deviationLow >= deviationHigh) {
                            // bottom half was bad so subdivide it
                            mHighBufferBursts = mMiddleBufferBursts;
                            mHighBufferLatency = mMiddleBufferLatency;
                            mMessage = "low half not linear";
                        } else {
                            // top half was bad so subdivide it
                            mLowBufferBursts = mMiddleBufferBursts;
                            mLowBufferLatency = mMiddleBufferLatency;
                            mMessage = "high half not linear";
                        }
                    }
                    break;
                default:
                    break;
            }

            if (result.code == RESULT_CONTINUE) {
                if (mState == STATE_MEASURE_MIDDLE) {
                    if ((mHighBufferBursts - mLowBufferBursts) <= 1) {
                        result.code = RESULT_DISCONTINUITY;
                        mMessage = "ERROR - DSP position error between "
                                + mLowBufferBursts + " and "
                                + mHighBufferBursts + " bursts!";
                    } else {
                        // Subdivide the remaining search space.
                        mMiddleBufferBursts = (mHighBufferBursts + mLowBufferBursts) / 2;
                        result.numBursts = mMiddleBufferBursts;
                    }
                }
            } else if (result.code == RESULT_OK) {
                mMessage = "PASS - no discontinuity";
            }
            return result;
        }

        private double measureLatencyLinearity(int bufferBursts1, int bufferLatency1,
                                                int bufferBursts2, int bufferLatency2) {
            int bufferFrames1 = bufferBursts1 * mFramesPerBurst;
            int bufferFrames2 = bufferBursts2 * mFramesPerBurst;
            int expectedLatencyDifference = bufferFrames2 - bufferFrames1;
            int actualLatencyDifference = bufferLatency2 - bufferLatency1;
            return Math.abs(expectedLatencyDifference - actualLatencyDifference)
                    / (double) expectedLatencyDifference;
        }

        private String reportResults(String prefix) {
            String message;
            message = prefix + "buffer.bursts.low = " + mLowBufferBursts + "\n";
            message += prefix + "latency.frames.low = " + mLowBufferLatency + "\n";
            message += prefix + "buffer.bursts.high = " + mHighBufferBursts + "\n";
            message += prefix + "latency.frames.high = " + mHighBufferLatency + "\n";
            message += prefix + "result = " + mMessage + "\n";
            return message;
        }
    }

    protected class ScanLatencyTestRunner extends MultipleLatencyTestRunner {
        BinaryDiscontinuityFinder inputFinder;
        BinaryDiscontinuityFinder outputFinder;
        private static final int MAX_BAD_RUNS_ALLOWED = 5; // arbitrary
        private int mBadCount = 0; // number of bad measurements

        private static final int STATE_SCANNING_OUTPUT = 0;
        private static final int STATE_SCANNING_INPUT = 1;
        private static final int STATE_DONE = 2;
        private int mState = STATE_SCANNING_OUTPUT;

        // Called on UI thread after each single latency measurement is complete.
        // It decides whether the series is complete or more measurements are needed.
        // If more measurements are needed then it sets mBufferBursts for Output or mInputMarginBursts for Input
        // It keeps moving the low and high sizes until it bounds the discontinuity within a single burst.
        @Override
        String onAnalyserDone() {
            BinaryDiscontinuityFinder.Result result = new BinaryDiscontinuityFinder.Result();
            result.code = BinaryDiscontinuityFinder.RESULT_OK;
            String message = "";

            if (!mActive) {
                message = "done";
            } else if (getMeasuredResult() != 0) {
                mBadCount++;
                if (mBadCount > MAX_BAD_RUNS_ALLOWED) {
                    cancel();
                    result.code = BinaryDiscontinuityFinder.RESULT_ERROR;
                    updateButtons(false);
                    message = "scanning cancelled due to error, " + mBadCount + " bad runs\n";
                } else {
                    message = "skipping this bad run, "
                            + mBadCount + " of " + MAX_BAD_RUNS_ALLOWED + " max\n";
                    result.numBursts = mActualBufferBursts;
                }
            } else {
                switch (mState) {
                    case STATE_SCANNING_OUTPUT:
                        outputFinder.setFramesPerBurst(mOutputFramesPerBurst);
                        result = outputFinder.onAnalyserDone(getMeasuredLatency(),
                                getMeasuredConfidence(),
                                mActualBufferBursts,
                                mOutputBufferCapacityInBursts,
                                mOutputIsMMapExclusive);
                        mBufferBursts = result.numBursts;
                        mInputMarginBursts = 0;
                        break;
                    case STATE_SCANNING_INPUT:
                        inputFinder.setFramesPerBurst(mInputFramesPerBurst);
                        result = inputFinder.onAnalyserDone(getMeasuredLatency(),
                                getMeasuredConfidence(),
                                mInputMarginBursts,
                                mInputBufferCapacityInBursts,
                                mInputIsMMapExclusive);
                        mBufferBursts = -1;
                        mInputMarginBursts = Math.min(result.numBursts,
                                mInputBufferCapacityInBursts - 1);
                        break;
                }
            }

            if (result.code == BinaryDiscontinuityFinder.RESULT_CONTINUE) {
                runAnotherTest();
            } else {
                // We finished one series.
                mBufferBursts = -1;
                mInputMarginBursts = 0;
                switch (mState) {
                    case STATE_SCANNING_OUTPUT:
                        // Finished an output series to start an input series.
                        mState = STATE_SCANNING_INPUT;
                        runAnotherTest();
                        break;
                    case STATE_SCANNING_INPUT:
                        mActive = false;
                        updateButtons(false);
                        mState = STATE_DONE;
                        break;
                }
            }
            message += reportResults();
            return message;
        }

        private void runAnotherTest() {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    measureSingleLatency();
                }
            }, AVERAGE_TEST_DELAY_MSEC);
        }

        private String reportResults() {
            String message = "test = check MMAP DSP position\n";
            message += outputFinder.reportResults("output.");
            message += "\n"; // separator between in/out
            message += inputFinder.reportResults("input.");
            mLastReport = message;
            return message;
        }

        // Called on UI thread.
        @Override
        public void start() {
            mBadCount = 0;
            inputFinder = new BinaryDiscontinuityFinder();
            outputFinder = new BinaryDiscontinuityFinder();
            mState = STATE_SCANNING_OUTPUT;
            mBufferBursts = 2;
            mActive = true;
            mLastReport = "";
            measureSingleLatency();
        }
    }
    ScanLatencyTestRunner mScanLatencyTestRunner = new ScanLatencyTestRunner();

    // Periodically query the status of the stream.
    protected class LatencySniffer {
        private int mCounter = 0;
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;
        public static final int SNIFFER_MAX_COUNTER = 30 * 1000 / SNIFFER_UPDATE_PERIOD_MSEC;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                double timestampLatency = -1.0;
                int state = getAnalyzerState();
                if (state == STATE_MEASURE_BACKGROUND || state == STATE_IN_PULSE) {
                    timestampLatency = measureTimestampLatency();
                    // Some configurations do not support input timestamps.
                    if (timestampLatency > 0) {
                        mTimestampLatencyStats.add(timestampLatency);
                    }
                }

                String message;
                if (isAnalyzerDone()) {
                    if (mCurrentLatencyTestRunner.isActive()) {
                        message = mCurrentLatencyTestRunner.onAnalyserDone();
                    } else {
                        message = getResultString();
                    }
                    File resultFile = onAnalyzerDone();
                    if (resultFile != null) {
                        message = "result.file = " + resultFile.getAbsolutePath() + "\n" + message;
                    }
                } else if (mCounter > SNIFFER_MAX_COUNTER) {
                    message = getProgressText();
                    message += convertStateToString(getAnalyzerState()) + "\n";
                    message += "TIMEOUT after " + mCounter + " loops!\n";
                } else {
                    message = getProgressText();
                    message += "please wait... " + mCounter + "\n";
                    message += convertStateToString(getAnalyzerState()) + "\n";
                    // Repeat this runnable code block again.
                    mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                }
                setAnalyzerText(message);
                mCounter++;
            }
        };

        private void startSniffer() {
            mCounter = 0;
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }
    }

    static String convertStateToString(int state) {
        switch (state) {
            case STATE_MEASURE_BACKGROUND: return "BACKGROUND";
            case STATE_IN_PULSE: return "RECORDING";
            case STATE_GOT_DATA: return "ANALYZING";
            default: return "DONE";
        }
    }

    private String getProgressText() {
        int progress = getAnalyzerProgress();
        int state = getAnalyzerState();
        int resetCount = getResetCount();
        String message = String.format(Locale.getDefault(), "progress = %d\nstate = %d\n#resets = %d\n",
                progress, state, resetCount);
        message += mCurrentLatencyTestRunner.getLastReport();
        return message;
    }

    private File onAnalyzerDone() {
        File resultFile = null;
        if (mTestRunningByIntent) {
            String report = getCommonTestReport();
            report += getResultString();
            resultFile = maybeWriteTestResult(report);
        }
        mTestRunningByIntent = false;
        mHasRecording = true;
        stopAudioTest();
        return resultFile;
    }

    @NonNull
    private String getResultString() {
        int result = getMeasuredResult();
        int resetCount = getResetCount();
        double confidence = getMeasuredConfidence();
        String message = "";

        message += String.format(Locale.getDefault(), "confidence = " + CONFIDENCE_FORMAT + "\n", confidence);
        message += String.format(Locale.getDefault(), "result.text = %s\n", resultCodeToString(result));

        // Only report valid latencies.
        if (result == 0) {
            int latencyFrames = getMeasuredLatency();
            double latencyMillis = getMeasuredLatencyMillis();
            int bufferSize = mAudioOutTester.getCurrentAudioStream().getBufferSizeInFrames();
            int latencyEmptyFrames = latencyFrames - bufferSize;
            double latencyEmptyMillis = latencyEmptyFrames * 1000.0 / getSampleRate();
            message += String.format(Locale.getDefault(), "latency.msec = " + LATENCY_FORMAT + "\n", latencyMillis);
            message += String.format(Locale.getDefault(), "latency.frames = %d\n", latencyFrames);
            message += String.format(Locale.getDefault(), "latency.empty.msec = " + LATENCY_FORMAT + "\n", latencyEmptyMillis);
            message += String.format(Locale.getDefault(), "latency.empty.frames = %d\n", latencyEmptyFrames);
        }

        message += String.format(Locale.getDefault(), "rms.signal = %7.5f\n", getSignalRMS());
        message += String.format(Locale.getDefault(), "rms.noise = %7.5f\n", getBackgroundRMS());
        message += String.format(Locale.getDefault(), "correlation = " + CONFIDENCE_FORMAT + "\n",
                getMeasuredCorrelation());
        double timestampLatency = getTimestampLatencyMillis();
        message += String.format(Locale.getDefault(), "timestamp.latency.msec = " + LATENCY_FORMAT + "\n",
                timestampLatency);
        if (mTimestampLatencyStats.count() > 0) {
            message += String.format(Locale.getDefault(), "timestamp.latency.mad = " + LATENCY_FORMAT + "\n",
                    mTimestampLatencyStats.calculateMeanAbsoluteDeviation(timestampLatency));
        }
        message +=  "timestamp.latency.count = " + mTimestampLatencyStats.count() + "\n";
        message += String.format(Locale.getDefault(), "reset.count = %d\n", resetCount);
        message += String.format(Locale.getDefault(), "result = %d\n", result);

        return message;
    }

    private LatencySniffer mLatencySniffer = new LatencySniffer();

    double getMeasuredLatencyMillis() {
        return getMeasuredLatency() * 1000.0 / getSampleRate();
    }

    double getTimestampLatencyMillis() {
        if (mTimestampLatencyStats.count() == 0) return -1.0;
        else return mTimestampLatencyStats.calculateMean();
    }

    native int getAnalyzerProgress();
    native int getMeasuredLatency();
    native double measureTimestampLatency();
    native double getMeasuredConfidence();
    native double getMeasuredCorrelation();
    native double getBackgroundRMS();
    native double getSignalRMS();

    private void setAnalyzerText(String s) {
        mAnalyzerView.setText(s);
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_rt_latency);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mMeasureButton = (Button) findViewById(R.id.button_measure);
        mAverageButton = (Button) findViewById(R.id.button_average);
        mScanButton = (Button) findViewById(R.id.button_scan);
        mCancelButton = (Button) findViewById(R.id.button_cancel);
        mShareButton = (Button) findViewById(R.id.button_share);
        mShareButton.setEnabled(false);
        mAnalyzerView = (TextView) findViewById(R.id.text_status);
        updateEnabledWidgets();

        hideSettingsViews();

        mBufferSizeView.setFaderNormalizedProgress(0.0); // for lowest latency

        mCommunicationDeviceView = (CommunicationDeviceView) findViewById(R.id.comm_device_view);
    }

    @Override
    int getActivityType() {
        return ACTIVITY_RT_LATENCY;
    }

    @Override
    protected void onStart() {
        super.onStart();
        mHasRecording = false;
        updateButtons(false);
    }

    @Override
    public void startTestUsingBundle() {
        try {
            StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
            StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
            configureStreamsFromBundle(mBundleFromIntent, requestedInConfig, requestedOutConfig);

            mBufferBursts = mBundleFromIntent.getInt(IntentBasedTestSupport.KEY_BUFFER_BURSTS, mBufferBursts);

            onMeasure(null);
        } finally {
            mBundleFromIntent = null;
        }
    }

    @Override
    protected void onStop() {
        mLatencySniffer.stopSniffer();
        super.onStop();
    }

    public void onMeasure(View view) {
        mCurrentLatencyTestRunner.clear();
        measureSingleLatency();
    }

    void updateButtons(boolean running) {
        boolean busy = running || mCurrentLatencyTestRunner.isActive();
        mMeasureButton.setEnabled(!busy);
        mAverageButton.setEnabled(!busy);
        mScanButton.setEnabled(!busy && NativeEngine.isMMapExclusiveSupported());
        mCancelButton.setEnabled(running);
        mShareButton.setEnabled(!busy && mHasRecording);
    }

    private void measureSingleLatency() {
        try {
            openAudio();
            AudioStreamBase outputStream = mAudioOutTester.getCurrentAudioStream();
            mOutputFramesPerBurst = outputStream.getFramesPerBurst();
            mOutputBufferCapacityInBursts = outputStream.getBufferCapacityInFrames() / mOutputFramesPerBurst ;
            mOutputIsMMapExclusive = mAudioOutTester.actualConfiguration.getSharingMode()
                    == StreamConfiguration.SHARING_MODE_EXCLUSIVE;
            AudioStreamBase inputStream = mAudioInputTester.getCurrentAudioStream();
            mInputFramesPerBurst = inputStream.getFramesPerBurst();
            mInputBufferCapacityInBursts = inputStream.getBufferCapacityInFrames() / mInputFramesPerBurst ;
            mInputIsMMapExclusive = mAudioInputTester.actualConfiguration.getSharingMode()
                    == StreamConfiguration.SHARING_MODE_EXCLUSIVE;

            if (mBufferBursts >= 0) {
                int actualBufferSizeInFrames = outputStream.setBufferSizeInFrames(mOutputFramesPerBurst * mBufferBursts);
                mActualBufferBursts = actualBufferSizeInFrames / mOutputFramesPerBurst;
                // override buffer size fader
                mBufferSizeView.setEnabled(false);
                mBufferBursts = -1;
            }

            startAudio();
            mTimestampLatencyStats  = new DoubleStatistics();
            mLatencySniffer.startSniffer();
            updateButtons(true);
        } catch (IOException e) {
            showErrorToast(e.getMessage());
        }
    }

    public void onAverage(View view) {
        mCurrentLatencyTestRunner = mAverageLatencyTestRunner;
        mCurrentLatencyTestRunner.start();
    }

    public void onScan(View view) {
        mCurrentLatencyTestRunner = mScanLatencyTestRunner;
        mCurrentLatencyTestRunner.start();
    }

    public void onCancel(View view) {
        mCurrentLatencyTestRunner.cancel();
        stopAudioTest();
    }

    // Call on UI thread
    public void stopAudioTest() {
        mLatencySniffer.stopSniffer();
        stopAudio();
        closeAudio();
        updateButtons(false);
    }

    @Override
    String getWaveTag() {
        return "rtlatency";
    }

    @Override
    boolean isOutput() {
        return false;
    }
}
