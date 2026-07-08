package com.mobileer.oboetester;

import android.app.Activity;
import android.media.AudioDeviceInfo;
import android.os.Build;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.util.Locale;

/**
 * Measure tap-to-tone latency by and update the waveform display.
 */
public class TapToToneTester {

    private static final float MAX_TOUCH_LATENCY = 0.200f;
    private static final float MAX_OUTPUT_LATENCY = 1.200f;
    private static final float ANALYSIS_TIME_MARGIN = 0.500f;

    private static final float ANALYSIS_TIME_DELAY = MAX_OUTPUT_LATENCY;
    private static final float ANALYSIS_TIME_TOTAL = MAX_TOUCH_LATENCY + MAX_OUTPUT_LATENCY;
    private static final int ANALYSIS_SAMPLE_RATE = 48000; // need not match output rate

    private final AudioRecordThread mRecorder;
    private final TapLatencyAnalyser mTapLatencyAnalyser;

    private final Activity mActivity;
    private final WaveformView mWaveformView;
    WaveformView mFastWaveformView;
    WaveformView mSlowWaveformView;
    WaveformView mLowThresholdWaveformView;
    WaveformView mArmedWaveformView;
    private final TextView mResultView;
    private final Spinner mAudioSourceSpinner;

    private final String mTapInstructions;
    private float mAnalysisTimeMargin = ANALYSIS_TIME_MARGIN;

    private boolean mArmed = true;

    // Stats for latency
    private int mMeasurementCount;
    private int mLatencySumSamples;
    private int mLatencyMin;
    private int mLatencyMax;

    public static class TestResult {
        public float[] samples;
        public float[] filtered;
        public int frameRate;
        public TapLatencyAnalyser.TapLatencyEvent[] events;
    }

    public TapToToneTester(Activity activity, String tapInstructions) {
        mActivity = activity;
        mTapInstructions = tapInstructions;
        mResultView = (TextView) activity.findViewById(R.id.resultView);
        mWaveformView = (WaveformView) activity.findViewById(R.id.waveview_audio_original);
        mWaveformView.setEnabled(false);
        mFastWaveformView = (WaveformView) activity.findViewById(R.id.waveview_audio_fast_avg);
        mSlowWaveformView = (WaveformView) activity.findViewById(R.id.waveview_audio_slow_avg);
        mLowThresholdWaveformView = (WaveformView) activity.findViewById(R.id.waveview_audio_lowThreshold);
        mArmedWaveformView = (WaveformView) activity.findViewById(R.id.waveview_audio_armed_waveform);

        float analysisTimeMax = ANALYSIS_TIME_TOTAL + mAnalysisTimeMargin;
        mRecorder = new AudioRecordThread(ANALYSIS_SAMPLE_RATE,
                1,
                (int) (analysisTimeMax * ANALYSIS_SAMPLE_RATE));

        mAudioSourceSpinner = (Spinner) activity.findViewById(R.id.audio_source_spinner);
        mAudioSourceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int position, long id) {
                String audioSourceText = (String) mAudioSourceSpinner.getAdapter().getItem(position);
                if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.Q && "VOICE_PERFORMANCE".equals(audioSourceText)) {
                    Toast.makeText(mActivity,
                            "Error: VOICE_PERFORMANCE not supported on API < 29",
                            Toast.LENGTH_SHORT).show();
                } if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.N && "UNPROCESSED".equals(audioSourceText)) {
                    Toast.makeText(mActivity,
                            "Error: UNPROCESSED not supported on API < 24",
                            Toast.LENGTH_SHORT).show();
                } else {
                    mRecorder.setAudioSource(audioSourceText);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // no-op
            }
        });
        mRecorder.setAudioSource((String) mAudioSourceSpinner.getAdapter().getItem(0));

        mTapLatencyAnalyser = new TapLatencyAnalyser();
    }

    public void start() throws IOException {
        mRecorder.startAudio();
        mWaveformView.setEnabled(true);
    }

    public void stop() {
        mRecorder.stopAudio();
        mWaveformView.setEnabled(false);
    }

    /**
     * @return true if ready to process a tap, false if already triggered
     */
    public boolean isArmed() {
        return mArmed;
    }

    public void setArmed(boolean armed) {
        this.mArmed = armed;
    }

    public void analyzeLater(String message) {
        showPendingStatus(message);
        Runnable task = this::analyseAndShowResults;
        scheduleTaskWhenDone(task);
        mArmed = false;
    }

    private void showPendingStatus(final String message) {
        mWaveformView.post(() -> {
            mWaveformView.setMessage(message);
            mWaveformView.clearSampleData();
            mWaveformView.invalidate();
        });
    }

    private void scheduleTaskWhenDone(Runnable task) {
        // schedule an analysis to start in the near future
        int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_DELAY);
        mRecorder.scheduleTask(numSamples, task);
    }

    private void analyseAndShowResults() {
        TestResult result = analyzeCapturedAudio();
        if (result != null) {
            showTestResults(result);
        }
    }

    public TestResult analyzeCapturedAudio() {
        int numSamples = (int) (mRecorder.getSampleRate() * ANALYSIS_TIME_TOTAL);
        float[] buffer = new float[numSamples];
        mRecorder.setCaptureEnabled(false); // TODO wait for it to settle
        int numRead = mRecorder.readMostRecent(buffer);

        TestResult result = new TestResult();
        result.samples = buffer;
        result.frameRate = mRecorder.getSampleRate();
        result.events = mTapLatencyAnalyser.analyze(buffer, 0, numRead);
        result.filtered = mTapLatencyAnalyser.getFilteredBuffer();
        mRecorder.setCaptureEnabled(true);
        return result;
    }

    public void resetLatency() {
        mMeasurementCount = 0;
        mLatencySumSamples = 0;
        mLatencyMin = Integer.MAX_VALUE;
        mLatencyMax = 0;
        showTestResults(null);
    }

    // Runs on UI thread.
    public void showTestResults(TestResult result) {
        String text;
        mWaveformView.setMessage(null);
        if (result == null) {
            text = mTapInstructions;
            mWaveformView.clearSampleData();
        } else {
            // Show edges detected.
            if (result.events.length == 0) {
                mWaveformView.setCursorData(null);
            } else {
                int numEdges = Math.min(8, result.events.length);
                int[] cursors = new int[numEdges];
                for (int i = 0; i < numEdges; i++) {
                    cursors[i] = result.events[i].sampleIndex;
                }
                mArmedWaveformView.setCursorData(cursors);
            }
            // Did we get a good measurement?
            if (result.events.length < 2) {
                text = "Not enough edges. Use fingernail.\n";
            } else if (result.events.length > 2) {
                text = "Too many edges.\n";
            } else {
                int latencySamples = result.events[1].sampleIndex - result.events[0].sampleIndex;
                mLatencySumSamples += latencySamples;
                mMeasurementCount++;

                int latencyMillis = 1000 * latencySamples / result.frameRate;
                if (mLatencyMin > latencyMillis) {
                    mLatencyMin = latencyMillis;
                }
                if (mLatencyMax < latencyMillis) {
                    mLatencyMax = latencyMillis;
                }

                text = String.format(Locale.getDefault(), "tap-to-tone latency = %3d msec\n", latencyMillis);
            }
            mWaveformView.setSampleData(result.filtered);
            mFastWaveformView.setSampleData(mTapLatencyAnalyser.getFastBuffer());
            mSlowWaveformView.setSampleData(mTapLatencyAnalyser.getSlowBuffer());
            mLowThresholdWaveformView.setSampleData(mTapLatencyAnalyser.getLowThresholdBuffer());
            mArmedWaveformView.setSampleData(mTapLatencyAnalyser.getArmedIndexes());
        }

        if (mMeasurementCount > 0) {
            int averageLatencySamples = mLatencySumSamples / mMeasurementCount;
            int averageLatencyMillis = 1000 * averageLatencySamples / result.frameRate;
            final String plural = (mMeasurementCount == 1) ? "test" : "tests";
            text = text + String.format(Locale.getDefault(), "min = %3d, avg = %3d, max = %3d, %d %s",
                    mLatencyMin, averageLatencyMillis, mLatencyMax, mMeasurementCount, plural);
        }
        final String postText = text;
        mWaveformView.post(new Runnable() {
            public void run() {
                mResultView.setText(postText);
                mWaveformView.postInvalidate();
                mFastWaveformView.postInvalidate();
                mSlowWaveformView.postInvalidate();
                mLowThresholdWaveformView.postInvalidate();
                mArmedWaveformView.postInvalidate();
            }
        });

        mArmed = true;
    }

    void setInputDevice(AudioDeviceInfo deviceInfo) {
        mRecorder.setInputDevice(deviceInfo);
    }
}
