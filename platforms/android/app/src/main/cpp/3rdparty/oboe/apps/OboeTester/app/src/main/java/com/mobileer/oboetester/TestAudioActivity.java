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

import static com.mobileer.oboetester.AudioForegroundService.ACTION_START;
import static com.mobileer.oboetester.AudioForegroundService.ACTION_STOP;
import static com.mobileer.oboetester.IntentBasedTestSupport.KEY_RESTART_STREAM_IF_CLOSED;
import static com.mobileer.oboetester.StreamConfiguration.convertContentTypeAudioAttributesContentType;
import static com.mobileer.oboetester.StreamConfiguration.convertErrorToText;
import static com.mobileer.oboetester.StreamConfiguration.convertUsageToAudioAttributeUsage;
import static com.mobileer.oboetester.StreamConfiguration.convertUsageToStreamType;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;
import android.widget.Toast;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Locale;

/**
 * Base class for other Activities.
 */
abstract class TestAudioActivity extends AppCompatActivity implements AudioManager.OnAudioFocusChangeListener {
    public static final String TAG = "OboeTester";

    protected static final int FADER_PROGRESS_MAX = 1000;
    private static final int INTENT_TEST_DELAY_MILLIS = 1100;

    public static final int AUDIO_STATE_OPEN = 0;
    public static final int AUDIO_STATE_STARTED = 1;
    public static final int AUDIO_STATE_PAUSED = 2;
    public static final int AUDIO_STATE_FLUSHED = 3;
    public static final int AUDIO_STATE_STOPPED = 4;
    public static final int AUDIO_STATE_RELEASED = 5;
    public static final int AUDIO_STATE_CLOSING = 6;
    public static final int AUDIO_STATE_CLOSED = 7;

    public static final int COLOR_ACTIVE = 0xFFD0D0A0;
    public static final int COLOR_IDLE = 0xFFD0D0D0;

    // Pass the activity index to native so it can know how to respond to the start and stop calls.
    // WARNING - must match definitions in NativeAudioContext.h ActivityType
    public static final int ACTIVITY_TEST_OUTPUT = 0;
    public static final int ACTIVITY_TEST_INPUT = 1;
    public static final int ACTIVITY_TAP_TO_TONE = 2;
    public static final int ACTIVITY_RECORD_PLAY = 3;
    public static final int ACTIVITY_ECHO = 4;
    public static final int ACTIVITY_RT_LATENCY = 5;
    public static final int ACTIVITY_GLITCHES = 6;
    public static final int ACTIVITY_TEST_DISCONNECT = 7;
    public static final int ACTIVITY_DATA_PATHS = 8;
    public static final int ACTIVITY_DYNAMIC_WORKLOAD = 9;

    private static final int MP3_RES_ID = R.raw.sine441stereo;
    private static final AudioConfig MP3_FILE_CONFIG =
            new AudioConfig(44100 /*sampleRate*/, StreamConfiguration.AUDIO_FORMAT_MP3,
                    StreamConfiguration.CHANNEL_STEREO, 2 /*channelCount*/);

    private final Object mAudioStateLock = new Object();
    @GuardedBy("mAudioStateLock")
    private int mAudioState = AUDIO_STATE_CLOSED;

    protected ArrayList<StreamContext> mStreamContexts;
    private Button mOpenButton;
    private Button mStartButton;
    private Button mPauseButton;
    private Button mFlushButton;
    private Button mStopButton;
    private Button mReleaseButton;
    private Button mCloseButton;
    private MyStreamSniffer mStreamSniffer;
    private CheckBox mCallbackReturnStopBox;
    private Spinner mHangTimeSpinner;
    private AudioManager mAudioManager;

    // Only set in some activities
    protected CommunicationDeviceView mCommunicationDeviceView;
    private int mSampleRate;
    private int mSingleTestIndex = -1;
    private static boolean mBackgroundEnabled;
    private static boolean mForegroundServiceEnabled;
    private static boolean mRestartStreamIfClosed = false;
    private static boolean mAudioFocusEnabled;

    protected Bundle mBundleFromIntent;
    protected boolean mTestRunningByIntent;
    protected String mResultFileName;
    private String mTestResults;
    private ExternalFileWriter mExternalFileWriter = new ExternalFileWriter(this);

    private TestTimeoutScheduler mTestTimeoutScheduler = new TestTimeoutScheduler();
    private BroadcastReceiver mStopTestReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (mTestRunningByIntent) {
                stopAutomaticTest();
            }
        }
    };

    private BroadcastReceiver mScreenStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                Log.d(TAG, "action screen off");
                if (mStreamSniffer != null) {
                    mStreamSniffer.stopStreamSniffer();
                }
            } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                Log.d(TAG, "action screen on");
                if (mStreamSniffer != null && !isClosingOrClosed()) {
                    mStreamSniffer.startStreamSniffer();
                }
            }
        }
    };

    private final long mActivityId = generateActivityId();
    private static long CURRENT_ACTIVITY_ID = 0;

    private static synchronized long generateActivityId() {
        return ++CURRENT_ACTIVITY_ID;
    }

    public String getTestName() {
        return "TestAudio";
    }

    public static class StreamContext {
        StreamConfigurationView configurationView;
        AudioStreamTester tester;

        boolean isInput() {
            return tester.getCurrentAudioStream().isInput();
        }
    }

    // Periodically query the status of the streams.
    protected class MyStreamSniffer {
        public static final int SNIFFER_UPDATE_PERIOD_MSEC = 150;
        public static final int SNIFFER_UPDATE_DELAY_MSEC = 300;

        private Handler mHandler;

        // Display status info for the stream.
        private Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                boolean streamClosed = false;
                boolean gotViews = false;
                for (StreamContext streamContext : mStreamContexts) {
                    AudioStreamBase.StreamStatus status = streamContext.tester.getCurrentAudioStream().getStreamStatus();
                    AudioStreamBase.DoubleStatistics latencyStatistics =
                            streamContext.tester.getCurrentAudioStream().getLatencyStatistics();
                    int errorCode = streamContext.tester.getCurrentAudioStream().getLastErrorCallbackResult();
                    if (streamContext.configurationView != null) {
                        // Handler runs this on the main UI thread.
                        int framesPerBurst = streamContext.tester.getCurrentAudioStream().getFramesPerBurst();
                        status.framesPerCallback = getFramesPerCallback();
                        String msg = "";
                        msg += "timestamp.latency = " + latencyStatistics.dump() + "\n";
                        msg += "lastErrorCallbackResult = " + convertErrorToText(errorCode) + "\n";
                        msg += status.dump(framesPerBurst);
                        streamContext.configurationView.setStatusText(msg);
                        updateStreamDisplay();
                        gotViews = true;
                    }

                    streamClosed = streamClosed || (status.state >= 12) || (errorCode != StreamConfiguration.ERROR_OK);
                }

                if (streamClosed) {
                    onStreamClosed();
                    if (mRestartStreamIfClosed) {
                        try {
                            openAudio();
                            startAudio();
                        } catch (IOException e) {
                            showErrorToast("restarting stream caught " + e.getMessage());
                            throw new RuntimeException(e);
                        }
                    }
                } else {
                    // Repeat this runnable code block again.
                    if (gotViews) {
                        mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_PERIOD_MSEC);
                    }
                }
            }
        };

        private void startStreamSniffer() {
            stopStreamSniffer();
            mHandler = new Handler(Looper.getMainLooper());
            // Start the initial runnable task by posting through the handler
            mHandler.postDelayed(runnableCode, SNIFFER_UPDATE_DELAY_MSEC);
        }

        private void stopStreamSniffer() {
            if (mHandler != null) {
                mHandler.removeCallbacks(runnableCode);
            }
        }

    }

    public static void setBackgroundEnabled(boolean enabled) {
        mBackgroundEnabled = enabled;
    }

    public static boolean isBackgroundEnabled() {
        return mBackgroundEnabled;
    }

    public static void setForegroundServiceEnabled(boolean enabled) {
        mForegroundServiceEnabled = enabled;
    }

    public static boolean isForegroundServiceEnabled() {
        return mForegroundServiceEnabled;
    }

    public static void setAudioFocusEnabled(boolean enabled) {
        mAudioFocusEnabled = enabled;
    }

    public static boolean isAudioFocusEnabled() {
        return mAudioFocusEnabled;
    }

    public int getServiceType() {
        switch(getActivityType()) {
            case ACTIVITY_TEST_OUTPUT:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
            case ACTIVITY_TEST_INPUT:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_TAP_TO_TONE:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_RECORD_PLAY:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_ECHO:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_RT_LATENCY:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_GLITCHES:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_TEST_DISCONNECT:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_DATA_PATHS:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
            case ACTIVITY_DYNAMIC_WORKLOAD:
                return ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK;
            default:
                Log.i(TAG, "getServiceType() called on unknown activity type " + getActivityType());
                return 0;
        }
    }

    public void onStreamClosed() {
    }

    protected abstract void inflateActivity();

    void updateStreamDisplay() {
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        inflateActivity();
        findAudioCommon();

        mBundleFromIntent = getIntent().getExtras();

        IntentFilter screenStateFilter = new IntentFilter();
        screenStateFilter.addAction(Intent.ACTION_SCREEN_ON);
        screenStateFilter.addAction(Intent.ACTION_SCREEN_OFF);
        registerReceiver(mScreenStateReceiver, screenStateFilter);
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        mBundleFromIntent = intent.getExtras();
    }

    public boolean isTestConfiguredUsingBundle() {
        return mBundleFromIntent != null;
    }

    public void hideSettingsViews() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.hideSettingsView();
            }
        }
    }

    abstract int getActivityType();

    public void setSingleTestIndex(int testIndex) {
        mSingleTestIndex = testIndex;
    }

    public int getSingleTestIndex() {
        return mSingleTestIndex;
    }

    @Override
    protected void onStart() {
        super.onStart();
        resetConfiguration();
        setActivityType(getActivityType());
        // TODO Use LifeCycleObserver instead of this.
        if (mCommunicationDeviceView != null) {
            mCommunicationDeviceView.onStart();
        }
        if (isForegroundServiceEnabled()) {
            enableForegroundService(true);
        }
    }

    protected void resetConfiguration() {
    }

    @Override
    public void onResume() {
        super.onResume();
        LocalBroadcastManager.getInstance(this).registerReceiver(mStopTestReceiver,
                new IntentFilter(TestTimeoutReceiver.ACTION_STOP_TEST));
        if (mBundleFromIntent != null) {
            processBundleFromIntent();
        }
    }

    private void setVolumeFromIntent() {
        float normalizedVolume = IntentBasedTestSupport.getNormalizedVolumeFromBundle(mBundleFromIntent);
        if (normalizedVolume >= 0.0) {
            int streamType = IntentBasedTestSupport.getVolumeStreamTypeFromBundle(mBundleFromIntent);
            AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
            int maxVolume = audioManager.getStreamMaxVolume(streamType);
            int requestedVolume = (int) (maxVolume * normalizedVolume);
            audioManager.setStreamVolume(streamType, requestedVolume, 0);
        }
    }

    private void processBundleFromIntent() {
        if (mTestRunningByIntent) {
            return;
        }

        // Delay the test start to avoid race conditions. See Oboe Issue #1533
        mTestRunningByIntent = true;
        Handler handler = new Handler(Looper.getMainLooper()); // UI thread
        handler.postDelayed(new DelayedTestByIntentRunnable(),
                INTENT_TEST_DELAY_MILLIS); // Delay long enough to get past the onStop() call!

    }

    private class DelayedTestByIntentRunnable implements Runnable {
        @Override
        public void run() {
            try {
                mResultFileName = mBundleFromIntent.getString(IntentBasedTestSupport.KEY_FILE_NAME);
                mRestartStreamIfClosed = mBundleFromIntent.getBoolean(KEY_RESTART_STREAM_IF_CLOSED,
                        false);
                setVolumeFromIntent();

                int durationSeconds = IntentBasedTestSupport.getDurationSeconds(mBundleFromIntent);
                if (durationSeconds > 0) {
                    mTestTimeoutScheduler.scheduleTestTimeout(TestAudioActivity.this, durationSeconds);
                }

                startTestUsingBundle();
            } catch( Exception e) {
                showErrorToast(e.getMessage());
            }
        }
    }

    public void startTestUsingBundle() {
    }

    public void stopAutomaticTest() {

    }

    @Override
    protected void onPause() {
        super.onPause();
        LocalBroadcastManager.getInstance(this).unregisterReceiver(mStopTestReceiver);
    }

    @Override
    protected void onStop() {
        if (!isBackgroundEnabled()) {
            Log.i(TAG, "onStop() called so stop the test =========================");
            onStopTest();
            if (isForegroundServiceEnabled()) {
                enableForegroundService(false);
            }
        }
        if (mCommunicationDeviceView != null) {
            mCommunicationDeviceView.onStop();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (isBackgroundEnabled()) {
            Log.i(TAG, "onDestroy() called so stop the test =========================");
            onStopTest();
            if (isForegroundServiceEnabled()) {
                enableForegroundService(false);
            }
        }
        synchronized (mAudioStateLock) {
            mAudioState = AUDIO_STATE_CLOSED;
        }
        unregisterReceiver(mScreenStateReceiver);
        super.onDestroy();
    }

    public void enableForegroundService(boolean enabled) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            String action = enabled ? ACTION_START : ACTION_STOP;
            Intent serviceIntent = new Intent(action, null, this,
                    AudioForegroundService.class);
            serviceIntent.putExtra("service_types", getServiceType());
            serviceIntent.putExtra(AudioForegroundService.KEY_ACTIVITY_ID, mActivityId);
            startForegroundService(serviceIntent);
        }
    }

    protected boolean isStreamClosed() {
        synchronized (mAudioStateLock) {
            return mAudioState == AUDIO_STATE_CLOSED;
        }
    }

    protected void updateEnabledWidgets() {
        int currentState;
        synchronized (mAudioStateLock) {
            currentState = mAudioState;
        }
        if (mOpenButton != null) {
            mOpenButton.setBackgroundColor(currentState == AUDIO_STATE_OPEN ? COLOR_ACTIVE : COLOR_IDLE);
            mStartButton.setBackgroundColor(currentState == AUDIO_STATE_STARTED ? COLOR_ACTIVE : COLOR_IDLE);
            mPauseButton.setBackgroundColor(currentState == AUDIO_STATE_PAUSED ? COLOR_ACTIVE : COLOR_IDLE);
            mFlushButton.setBackgroundColor(currentState == AUDIO_STATE_FLUSHED ? COLOR_ACTIVE : COLOR_IDLE);
            mStopButton.setBackgroundColor(currentState == AUDIO_STATE_STOPPED ? COLOR_ACTIVE : COLOR_IDLE);
            mReleaseButton.setBackgroundColor(currentState == AUDIO_STATE_RELEASED ? COLOR_ACTIVE : COLOR_IDLE);
            mCloseButton.setBackgroundColor(currentState == AUDIO_STATE_CLOSED ? COLOR_ACTIVE : COLOR_IDLE);
        }
        setConfigViewsEnabled(currentState == AUDIO_STATE_CLOSED);
    }

    private void setConfigViewsEnabled(boolean b) {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setChildrenEnabled(b);
            }
        }
    }

    private void applyConfigurationViewsToModels() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.applyToModel(streamContext.tester.requestedConfiguration);
            }
        }
    }

    abstract boolean isOutput();

    public void clearStreamContexts() {
        mStreamContexts.clear();
    }

    public StreamContext addOutputStreamContext() {
        StreamContext streamContext = new StreamContext();
        streamContext.tester = AudioOutputTester.getInstance();
        streamContext.configurationView = (StreamConfigurationView)
                findViewById(R.id.outputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView = (StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        if (streamContext.configurationView != null) {
            streamContext.configurationView.setOutput(true);
        }
        mStreamContexts.add(streamContext);
        return streamContext;
    }

    public AudioOutputTester addAudioOutputTester() {
        StreamContext streamContext = addOutputStreamContext();
        return (AudioOutputTester) streamContext.tester;
    }

    public StreamContext addInputStreamContext() {
        StreamContext streamContext = new StreamContext();
        streamContext.tester = AudioInputTester.getInstance();
        streamContext.configurationView = (StreamConfigurationView)
                findViewById(R.id.inputStreamConfiguration);
        if (streamContext.configurationView == null) {
            streamContext.configurationView = (StreamConfigurationView)
                    findViewById(R.id.streamConfiguration);
        }
        if (streamContext.configurationView != null) {
            streamContext.configurationView.setOutput(false);
        }
        mStreamContexts.add(streamContext);
        return streamContext;
    }

    public AudioInputTester addAudioInputTester() {
        StreamContext streamContext = addInputStreamContext();
        return (AudioInputTester) streamContext.tester;
    }

    void updateStreamConfigurationViews() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.updateDisplay(streamContext.tester.actualConfiguration);
            }
        }
    }

    StreamContext getFirstInputStreamContext() {
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.isInput())
                return streamContext;
        }
        return null;
    }

    StreamContext getFirstOutputStreamContext() {
        for (StreamContext streamContext : mStreamContexts) {
            if (!streamContext.isInput())
                return streamContext;
        }
        return null;
    }

    protected void findAudioCommon() {
        mOpenButton = (Button) findViewById(R.id.button_open);
        if (mOpenButton != null) {
            mStartButton = (Button) findViewById(R.id.button_start);
            mPauseButton = (Button) findViewById(R.id.button_pause);
            mFlushButton = (Button) findViewById(R.id.button_flush);
            mStopButton = (Button) findViewById(R.id.button_stop);
            mReleaseButton = (Button) findViewById(R.id.button_release);
            mCloseButton = (Button) findViewById(R.id.button_close);
        }
        mStreamContexts = new ArrayList<StreamContext>();

        mCallbackReturnStopBox = (CheckBox) findViewById(R.id.callbackReturnStop);
        if (mCallbackReturnStopBox != null) {
            mCallbackReturnStopBox.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    OboeAudioStream.setCallbackReturnStop(mCallbackReturnStopBox.isChecked());
                }
            });
        }
        OboeAudioStream.setCallbackReturnStop(false);

        mHangTimeSpinner = (Spinner) findViewById(R.id.spinner_hang_time);
        if (mHangTimeSpinner != null) {
            mHangTimeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
                public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                    String hangTimeText = (String) mHangTimeSpinner.getAdapter().getItem(position);
                    int hangTimeMillis = Integer.parseInt(hangTimeText);
                    Log.d(TAG, "Hang Time = " + hangTimeMillis + " msec");

                    OboeAudioStream.setHangTimeMillis(hangTimeMillis);
                }

                public void onNothingSelected(AdapterView<?> parent) {
                    OboeAudioStream.setHangTimeMillis(0);
                }
            });
        }
        OboeAudioStream.setHangTimeMillis(0);

        mStreamSniffer = new MyStreamSniffer();

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
    }

    private void updateNativeAudioParameters() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            String text = mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
            int audioManagerSampleRate = Integer.parseInt(text);
            text = mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
            int audioManagerFramesPerBurst = Integer.parseInt(text);
            setDefaultAudioValues(audioManagerSampleRate, audioManagerFramesPerBurst);
        }
    }

    protected void showErrorToast(String message) {
        Log.e(TAG, "showErrorToast(\"" + message + "\")");
        String text = "Error: " + message;
        showToast(text);
    }

    protected void showToast(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(TestAudioActivity.this,
                        message,
                        Toast.LENGTH_SHORT).show();
            }
        });
    }

    private void onStartAllContexts() {
        for (StreamContext streamContext : mStreamContexts) {
            streamContext.tester.getCurrentAudioStream().onStart();
        }
    }
    private void onStopAllContexts() {
        for (StreamContext streamContext : mStreamContexts) {
            streamContext.tester.getCurrentAudioStream().onStop();
        }
    }

    public void openAudio(View view) {
        try {
            openAudio();
        } catch (Exception e) {
            showErrorToast("openAudio() caught " + e.getMessage());
        }
    }

    void clearHangTime() {
        OboeAudioStream.setHangTimeMillis(0);
        if (mHangTimeSpinner != null) {
            mHangTimeSpinner.setSelection(0);
        }
    }

    public void startAudio(View view) {
        Log.i(TAG, "startAudio() called =======================================");
        clearHangTime(); // start running
        try {
            startAudio();
        } catch (Exception e) {
            showErrorToast("startAudio() caught " + e.getMessage());
        }
        keepScreenOn(true);
    }

    protected void keepScreenOn(boolean on) {
        if (on) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        }
    }

    public void stopAudio(View view) {
        stopAudio();
        keepScreenOn(false);
    }

    public void pauseAudio(View view) {
        pauseAudio();
        keepScreenOn(false);
    }

    public void flushAudio(View view) {
        flushAudio();
    }

    public void closeAudio(View view) {
        closeAudio();
    }

    public void releaseAudio(View view) {
        releaseAudio();
    }

    public int getSampleRate() {
        return mSampleRate;
    }

    public void openAudio() throws IOException {
        synchronized (mAudioStateLock) {
            // Always close existing streams before opening new ones.
            closeAudioLocked();

            updateNativeAudioParameters();

            if (!isTestConfiguredUsingBundle()) {
                applyConfigurationViewsToModels();
            }

            if (isAudioFocusEnabled() && (getFirstOutputStreamContext() != null)) {
                requestAudioFocus();
            }

            int sampleRate = 0; // Use the OUTPUT sample rate for INPUT

            // Open output streams then open input streams.
            // This is so that the capacity of input stream can be expanded to
            // match the burst size of the output for full duplex.
            for (StreamContext streamContext : mStreamContexts) {
                if (!streamContext.isInput()) { // OUTPUT?
                    openStreamContextLocked(streamContext);
                    int streamSampleRate = streamContext.tester.actualConfiguration.getSampleRate();
                    if (sampleRate == 0) {
                        sampleRate = streamSampleRate;
                    }

                    if (shouldSetStreamControlByAttributes()) {
                        int actualUsage = streamContext.tester.actualConfiguration.getUsage();
                        int actualContentType = streamContext.tester.actualConfiguration.getContentType();
                        setStreamControlByAttributes(actualUsage, actualContentType);
                    }

                    if (streamContext.tester.actualConfiguration.getFormat() ==
                            StreamConfiguration.AUDIO_FORMAT_MP3) {
                        setupMp3BufferFromFile();
                    }
                }
            }
            for (StreamContext streamContext : mStreamContexts) {
                if (streamContext.isInput()) {
                    if (sampleRate != 0) {
                        streamContext.tester.requestedConfiguration.setSampleRate(sampleRate);
                    }
                    openStreamContextLocked(streamContext);
                }
            }
        }
        updateEnabledWidgets();
        onStartAllContexts();
        mStreamSniffer.startStreamSniffer();
    }

    private void requestAudioFocus() {
        StreamContext streamContext = getFirstOutputStreamContext();
        StreamConfiguration config = streamContext.tester.requestedConfiguration;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AudioAttributes.Builder attributesBuilder = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC);
            attributesBuilder.setUsage(convertUsageToAudioAttributeUsage(config.getUsage()));
            attributesBuilder.setContentType(convertContentTypeAudioAttributesContentType(config.getContentType()));
            AudioAttributes audioAttributes = attributesBuilder.build();
            AudioFocusRequest focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(audioAttributes)
                    .setAcceptsDelayedFocusGain(true)
                    .setOnAudioFocusChangeListener(this)
                    .build();
            int result = mAudioManager.requestAudioFocus(focusRequest);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                showErrorToast("Could not get audio focus");
            }
        } else {
            int streamType = convertUsageToStreamType(config.getUsage());
            int result = mAudioManager.requestAudioFocus(this, streamType, AudioManager.AUDIOFOCUS_GAIN);
            if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                showErrorToast("Could not get audio focus");
            }
        }
    }

    @Override
    public void onAudioFocusChange(int focusChange) {
        Log.i(TAG, "onAudioFocusChange(" + focusChange + ")");
        int currentState;
        synchronized (mAudioStateLock) {
            currentState = mAudioState;
            switch (focusChange) {
                case AudioManager.AUDIOFOCUS_GAIN:
                    setDuck(false);
                    if (currentState == AUDIO_STATE_STOPPED || currentState == AUDIO_STATE_PAUSED) {
                        try {
                            startAudioLocked();
                        } catch (IOException e) {
                            showErrorToast("startAudio() caught " + e.getMessage());
                        }
                    }
                    break;
                case AudioManager.AUDIOFOCUS_LOSS:
                    if (currentState == AUDIO_STATE_STARTED) {
                        // Stop audio permanently when audio focus is lost
                        stopAudioLocked();
                    }
                    break;
                case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                    if (currentState == AUDIO_STATE_STARTED) {
                        // Pause audio when audio focus is lost only temporarily
                        pauseAudioLocked();
                    }
                    break;
                case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                    // Lower the volume when
                    setDuck(true);
                    break;
            }
        }
    }

    protected boolean shouldSetStreamControlByAttributes() {
        return true;
    }

    /**
     * Associate the volume keys with the stream we are playing.
     * @param usage usage for the stream
     * @param contentType tupe of the stream
     */
    private void setStreamControlByAttributes(int usage, int contentType) {
        AudioAttributes attributes = new AudioAttributes.Builder().setUsage(usage)
                .setContentType(contentType).build();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            int volumeControlStream = attributes.getVolumeControlStream();
            Log.i(TAG, "setVolumeControlStream(" + volumeControlStream + ")");
            setVolumeControlStream(volumeControlStream);
        }
    }

    /**
     * @param deviceId
     * @return true if the device is TYPE_BLUETOOTH_SCO
     */
    boolean isScoDevice(int deviceId) {
        if (deviceId == 0) return false;
        final AudioDeviceInfo[] devices = mAudioManager.getDevices(
                AudioManager.GET_DEVICES_INPUTS | AudioManager.GET_DEVICES_OUTPUTS);
        for (AudioDeviceInfo device : devices) {
            if (device.getId() == deviceId) {
                return device.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
            }
        }
        return false;
    }

    @GuardedBy("mAudioStateLock")
    private void openStreamContextLocked(StreamContext streamContext) throws IOException {
        StreamConfiguration requestedConfig = streamContext.tester.requestedConfiguration;
        StreamConfiguration actualConfig = streamContext.tester.actualConfiguration;

        if (requestedConfig.getFormat() == StreamConfiguration.AUDIO_FORMAT_MP3 &&
                (requestedConfig.getDirection() != StreamConfiguration.DIRECTION_OUTPUT ||
                        requestedConfig.getChannelMask() != MP3_FILE_CONFIG.mChannelMask ||
                        requestedConfig.getSampleRate() != MP3_FILE_CONFIG.mSampleRate)) {
            showErrorToast("MP3 format uses builtin 44.1kHz stereo mp3 file for playback, " +
                           "the requested configuration must be 44.1kHz stereo when format is MP3");
            return;
        }

        streamContext.tester.open(); // OPEN the stream

        mSampleRate = actualConfig.getSampleRate();
        mAudioState = AUDIO_STATE_OPEN;
        int sessionId = actualConfig.getSessionId();
        if (streamContext.configurationView != null) {
            if (sessionId > 0) {
                try {
                    streamContext.configurationView.setupEffects(sessionId);
                } catch (Exception e) {
                    showErrorToast("openStreamContextLocked() caught " + e.getMessage());
                }
            }
            streamContext.configurationView.updateDisplay(streamContext.tester.actualConfiguration);
        }
    }

    // Native methods
    private native int startNative();

    private native int pauseNative();

    private native int flushNative();

    private native int stopNative();

    private native int releaseNative();

    protected native long flushFromFrameNative(int accuracy, long frames);

    protected native void setActivityType(int activityType);

    private native int getFramesPerCallback();

    private native void setupMemoryBuffer(byte[] buffer, int offset, int length);

    public native void setUseAlternativeAdpf(boolean enabled);

    private native void setDuck(boolean isDucked);

    private static native void setDefaultAudioValues(int audioManagerSampleRate, int audioManagerFramesPerBurst);

    protected native int setPlaybackParametersNative(PlaybackParameters parameters);
    protected native PlaybackParameters getPlaybackParametersNative();

    public void startAudio() throws IOException {
        synchronized (mAudioStateLock) {
            startAudioLocked();
        }
    }

    @GuardedBy("mAudioStateLock")
    private void startAudioLocked() throws IOException {
        int result;

        if (mAudioState != AUDIO_STATE_OPEN
                && mAudioState != AUDIO_STATE_STOPPED
                && mAudioState != AUDIO_STATE_PAUSED
                && mAudioState != AUDIO_STATE_FLUSHED) {
            Log.w(TAG, "startAudio() called in invalid state " + mAudioState + ", ignoring.");
            return;
        }
        result = startNative();
        if (result != 0) {
            String message = "Start failed with " + result + ", " + convertErrorToText(result);
            showErrorToast(message);
            throw new IOException("startNative returned " + result + ", " + convertErrorToText(result));
        } else {
            mAudioState = AUDIO_STATE_STARTED;
            Log.i(TAG, "startAudio() transitioning to STARTED");
            onStartAllContexts();
            for (StreamContext streamContext : mStreamContexts) {
                StreamConfigurationView configView = streamContext.configurationView;
                if (configView != null) {
                    configView.updateDisplay(streamContext.tester.actualConfiguration);
                }
            }
            updateEnabledWidgets();
        }
    }

    protected void toastPauseError(int result) {
        showErrorToast("Pause failed with " + result + ", " + convertErrorToText(result));
    }

    public void pauseAudio() {
        synchronized (mAudioStateLock) {
            pauseAudioLocked();
        }
    }

    @GuardedBy("mAudioStateLock")
    private void pauseAudioLocked() {
        int result;
        result = pauseNative();

        if (result != 0) {
            toastPauseError(result);
        } else {
            mAudioState = AUDIO_STATE_PAUSED;
            Log.i(TAG, "pauseAudio() transitioning to PAUSED");
            updateEnabledWidgets();
            onStopAllContexts();
        }
    }

    public void flushAudio() {
        int result;
        synchronized (mAudioStateLock) {
            result = flushNative();
            if (result != 0) {
                showErrorToast("Flush failed with " + result + ", " + convertErrorToText(result));
            } else {
                mAudioState = AUDIO_STATE_FLUSHED;
                Log.i(TAG, "flushAudio() transitioning to FLUSHED");
                updateEnabledWidgets();
            }
        }
    }

    public void stopAudio() {
        synchronized (mAudioStateLock) {
            stopAudioLocked();
        }
    }

    @GuardedBy("mAudioStateLock")
    private void stopAudioLocked() {
        int result;
        result = stopNative();

        if (result != 0) {
            showErrorToast("Stop failed with " + result + ", " + convertErrorToText(result));
        } else {
            mAudioState = AUDIO_STATE_STOPPED;
            Log.i(TAG, "stopAudio() transitioning to STOPPED");
            updateEnabledWidgets();
            onStopAllContexts();
        }
    }

    public void releaseAudio() {
        int result;
        synchronized (mAudioStateLock) {
            result = releaseNative();
            if (result != 0) {
                showErrorToast("Release failed with " + result + ", " + convertErrorToText(result));
            } else {
                mAudioState = AUDIO_STATE_RELEASED;
                Log.i(TAG, "releaseAudio() transitioning to RELEASED");
                updateEnabledWidgets();
                onStopAllContexts();
            }
        }
    }

    public void runTest() {
    }

    public void saveIntentLog() {
    }

    public void onStopTest() {
        if (mTestRunningByIntent) {
            mTestTimeoutScheduler.cancelTestTimeout(this);
        }
        stopTest();
    }

    public void stopTest() {
        synchronized (mAudioStateLock) {
            if (mAudioState == AUDIO_STATE_STARTED || mAudioState == AUDIO_STATE_PAUSED) {
                stopNative();
                mAudioState = AUDIO_STATE_STOPPED;
            }
            closeAudioLocked();
        }
        updateEnabledWidgets();
    }

    public void stopAudioQuiet() {
        synchronized (mAudioStateLock) {
            if (mAudioState == AUDIO_STATE_STARTED || mAudioState == AUDIO_STATE_PAUSED) {
                stopNative();
                mAudioState = AUDIO_STATE_STOPPED;
            }
        }
        updateEnabledWidgets();
    }

    public void closeAudio() {
        synchronized (mAudioStateLock) {
            closeAudioLocked();
        }
        updateEnabledWidgets();
    }

    @GuardedBy("mAudioStateLock")
    private void closeAudioLocked() {
        if (isClosingOrClosed()) {
            Log.d(TAG, "closeAudio() already closing");
            return;
        }
        mAudioState = AUDIO_STATE_CLOSING;

        mStreamSniffer.stopStreamSniffer();
        // Close output streams first because legacy callbacks may still be active
        // and an output stream may be calling the input stream.
        for (StreamContext streamContext : mStreamContexts) {
            if (!streamContext.isInput()) {
                streamContext.tester.close();
            }
        }
        for (StreamContext streamContext : mStreamContexts) {
            if (streamContext.isInput()) {
                streamContext.tester.close();
            }
        }

        if (isAudioFocusEnabled()) {
            mAudioManager.abandonAudioFocus(this);
        }

        mAudioState = AUDIO_STATE_CLOSED;
    }

    public boolean isClosingOrClosed() {
        return (mAudioState == AUDIO_STATE_CLOSING) || (mAudioState == AUDIO_STATE_CLOSED);
    }

    void startBluetoothSco() {
        mAudioManager.startBluetoothSco();
    }

    void stopBluetoothSco() {
        mAudioManager.stopBluetoothSco();
    }

    @NonNull
    protected String getCommonTestReport() {
        StringBuffer report = new StringBuffer();
        // Add some extra information for the remote tester.
        report.append("build.fingerprint = " + Build.FINGERPRINT + "\n");
        try {
            PackageInfo pinfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            report.append(String.format(Locale.getDefault(), "test.version = %s\n", pinfo.versionName));
            report.append(String.format(Locale.getDefault(), "test.version.code = %d\n", pinfo.versionCode));
        } catch (PackageManager.NameNotFoundException e) {
        }
        report.append("time.millis = " + System.currentTimeMillis() + "\n");

        if (mStreamContexts.size() == 0) {
            report.append("ERROR: no active streams" + "\n");
        } else {
            StreamContext streamContext = mStreamContexts.get(0);
            AudioStreamTester streamTester = streamContext.tester;
            report.append(streamTester.actualConfiguration.dump());
            AudioStreamBase.StreamStatus status = streamTester.getCurrentAudioStream().getStreamStatus();
            AudioStreamBase.DoubleStatistics latencyStatistics =
                    streamTester.getCurrentAudioStream().getLatencyStatistics();
            int framesPerBurst = streamTester.getCurrentAudioStream().getFramesPerBurst();
            status.framesPerCallback = getFramesPerCallback();
            report.append("timestamp.latency = " + latencyStatistics.dump() + "\n");
            // TODO The following report is not in a name=value format!
            // report.append(status.dump(framesPerBurst));
        }

        return report.toString();
    }

    File maybeWriteTestResult(String resultString) {
        File fileWritten = null;
        if (mResultFileName != null) {
            try {
                fileWritten = mExternalFileWriter.writeStringToExternalFile(resultString, mResultFileName);
            } catch (IOException e) {
                e.printStackTrace();
                showErrorToast(" writing result file. " + e.getMessage());
            }
            mResultFileName = null;
        }
        return fileWritten;
    }

    void setupMp3BufferFromFile() {
        try {
            InputStream inputStream = this.getResources().openRawResource(MP3_RES_ID);
            final int length = inputStream.available();
            byte[] buffer = new byte[length];
            int readLength = inputStream.read(buffer, 0 /*off*/, length);
            inputStream.close();
            Log.i(TAG, "Total file length=" + length + ", read length=" + readLength);
            ByteBuffer byteBuffer = ByteBuffer.wrap(buffer);
            byteBuffer.order(ByteOrder.nativeOrder());
            setupMemoryBuffer(byteBuffer.array(), 0, readLength);
        } catch (Exception e) {
            showErrorToast("Failed to load mp3 file " + e.getMessage());
        }
    }

    private static class AudioConfig {
        public int mSampleRate;
        public int mFormat;
        public int mChannelMask;
        public int mChannelCount;

        public AudioConfig(int sampleRate, int format, int channelMask, int channelCount) {
            mSampleRate = sampleRate;
            mFormat = format;
            mChannelMask = channelMask;
            mChannelCount = channelCount;
        }
    }
}
