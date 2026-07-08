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

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import java.io.IOException;

/**
 * Guide the user through a series of tests plugging in and unplugging a headset.
 * Print a summary at the end of any failures.
 */
public class TestDisconnectActivity extends TestAudioActivity {

    private static final String TEXT_SKIP = "SKIP";
    private static final String TEXT_PASS = "PASS";
    private static final String TEXT_FAIL = "FAIL !!!!";
    public static final int POLL_DURATION_MILLIS = 50;
    public static final int SETTLING_TIME_MILLIS = 600;
    public static final int TIME_TO_FAILURE_MILLIS = 3000;

    private TextView     mInstructionsTextView;
    private TextView     mStatusTextView;
    private TextView     mPlugTextView;

    private volatile boolean mTestFailed;
    private volatile boolean mSkipTest;
    private volatile int mPlugCount;
    private volatile int mUsbDeviceAttachedCount;
    private volatile int mPlugState;
    private volatile int mPlugMicrophone;
    private BroadcastReceiver mPluginReceiver = new PluginBroadcastReceiver();
    private Button       mFailButton;
    private Button       mSkipButton;
    private CheckBox     mCheckBoxInputs;
    private CheckBox     mCheckBoxOutputs;

    protected AutomatedTestRunner mAutomatedTestRunner;

    private native int getRoutingChangedCount();

    // Receive a broadcast Intent when a headset is plugged in or unplugged.
    // Display a count on screen.
    public class PluginBroadcastReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            switch (intent.getAction()) {
                case Intent.ACTION_HEADSET_PLUG: {
                    mPlugMicrophone = intent.getIntExtra("microphone", -1);
                    mPlugState = intent.getIntExtra("state", -1);
                    mPlugCount++;
                } break;
                case UsbManager.ACTION_USB_DEVICE_ATTACHED:
                case UsbManager.ACTION_USB_DEVICE_DETACHED: {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    final boolean hasAudioPlayback =
                            containsAudioStreamingInterface(device, UsbConstants.USB_DIR_OUT);
                    final boolean hasAudioCapture =
                            containsAudioStreamingInterface(device, UsbConstants.USB_DIR_IN);
                    if (hasAudioPlayback || hasAudioCapture) {
                        mPlugState =
                                intent.getAction() == UsbManager.ACTION_USB_DEVICE_ATTACHED ? 1 : 0;
                        mUsbDeviceAttachedCount++;
                        mPlugMicrophone = hasAudioCapture ? 1 : 0;
                    }
                } break;
                default:
                    break;
            }
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    String message = "HEADSET_PLUG #" + mPlugCount
                            + ", USB_DEVICE_DE/ATTACHED #" + mUsbDeviceAttachedCount
                            + ", mic = " + mPlugMicrophone
                            + ", state = " + mPlugState;
                    mPlugTextView.setText(message);
                    log(message);
                }
            });
        }

        private static final int AUDIO_STREAMING_SUB_CLASS = 2;

        /**
         * Figure out if an UsbDevice contains audio input/output streaming interface or not.
         *
         * @param device the given UsbDevice
         * @param direction the direction of the audio streaming interface
         * @return true if the UsbDevice contains the audio input/output streaming interface.
         */
        private boolean containsAudioStreamingInterface(UsbDevice device, int direction) {
            final int interfaceCount = device.getInterfaceCount();
            for (int i = 0; i < interfaceCount; ++i) {
                UsbInterface usbInterface = device.getInterface(i);
                if (usbInterface.getInterfaceClass() != UsbConstants.USB_CLASS_AUDIO
                        && usbInterface.getInterfaceSubclass() != AUDIO_STREAMING_SUB_CLASS) {
                    continue;
                }
                final int endpointCount = usbInterface.getEndpointCount();
                for (int j = 0; j < endpointCount; ++j) {
                    if (usbInterface.getEndpoint(j).getDirection() == direction) {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    @Override
    protected void inflateActivity() {
        setContentView(R.layout.activity_test_disconnect);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mAutomatedTestRunner = findViewById(R.id.auto_test_runner);
        mAutomatedTestRunner.setActivity(this);

        mInstructionsTextView = (TextView) findViewById(R.id.text_instructions);
        mStatusTextView = (TextView) findViewById(R.id.text_status);
        mPlugTextView = (TextView) findViewById(R.id.text_plug_events);

        mCheckBoxInputs = (CheckBox)findViewById(R.id.checkbox_disco_inputs);
        mCheckBoxOutputs = (CheckBox)findViewById(R.id.checkbox_disco_outputs);

        mFailButton = (Button) findViewById(R.id.button_fail);
        mSkipButton = (Button) findViewById(R.id.button_skip);
        updateFailSkipButton(false);
    }

    @Override
    public String getTestName() {
        return "Disconnect";
    }

    int getActivityType() {
        return ACTIVITY_TEST_DISCONNECT;
    }

    @Override
    boolean isOutput() {
        return true;
    }

    private void updateFailSkipButton(final boolean running) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mFailButton.setEnabled(running);
                mSkipButton.setEnabled(running);
            }
        });
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

    // Write to status and command view
    private void setStatusText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mStatusTextView.setText(text);
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        this.registerReceiver(mPluginReceiver, filter);
    }

    @Override
    public void onPause() {
        this.unregisterReceiver(mPluginReceiver);
        super.onPause();
    }

    // This should only be called from UI events such as onStop or a button press.
    @Override
    public void onStopTest() {
        mAutomatedTestRunner.stopTest();
    }

    public void startAudioTest() throws IOException {
        startAudio();
    }

    public void stopAudioTest() {
        stopAudioQuiet();
        closeAudio();
    }

    public void onCancel(View view) {
        stopAudioTest();
        mAutomatedTestRunner.onTestFinished();
    }

    // Called on UI thread
    public void onStopAudioTest(View view) {
        stopAudioTest();
        mAutomatedTestRunner.onTestFinished();
        keepScreenOn(false);
    }

    public void onFailTest(View view) {
        mTestFailed = true;
    }

    public void onSkipTest(View view) {
        mSkipTest = true;
    }

    private String getConfigText(StreamConfiguration config) {
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "IN")
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                        config.getPerformanceMode())
                + ", " + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", " + config.getSampleRate()
                + ", SRC = " + StreamConfiguration.convertRateConversionQualityToText(config.getRateConversionQuality());
    }

    private void log(Exception e) {
        Log.e(TestAudioActivity.TAG, "Caught ", e);
        mAutomatedTestRunner.log("Caught " + e);
    }

    private void log(String text) {
        mAutomatedTestRunner.log(text);
    }

    private void flushLog() {
        mAutomatedTestRunner.flushLog();
    }

    private void appendFailedSummary(String text) {
        mAutomatedTestRunner.appendFailedSummary(text);
    }

    private void testConfiguration(boolean isInput,
                                   int perfMode,
                                   int sharingMode,
                                   int sampleRate,
                                   int sampleRateConversionQuality,
                                   boolean requestPlugin) throws InterruptedException {
        if ((getSingleTestIndex() >= 0) && (mAutomatedTestRunner.getTestCount() != getSingleTestIndex())) {
            mAutomatedTestRunner.incrementTestCount();
            return;
        }

        if (!isInput && !mCheckBoxOutputs.isChecked()) {
            return;
        }
        if (isInput && !mCheckBoxInputs.isChecked()) {
            return;
        }

        updateFailSkipButton(true);

        String actualConfigText = "none";
        mSkipTest = false;
        mTestFailed = false;

        // Try to synchronize with the current headset state, IN or OUT.
        while (mAutomatedTestRunner.isThreadEnabled() && !mSkipTest && !mTestFailed) {
            if (requestPlugin != (mPlugState == 0)) {
                String message = "SYNC: " + (requestPlugin ? "UNplug" : "Plug IN") + " headset now!";
                setInstructionsText(message);
                Thread.sleep(POLL_DURATION_MILLIS);
            } else {
                break;
            }
        }

        AudioInputTester    mAudioInTester = null;
        AudioOutputTester   mAudioOutTester = null;

        clearStreamContexts();

        if (isInput) {
            mAudioInTester = addAudioInputTester();
        } else {
            mAudioOutTester = addAudioOutputTester();
        }

        // Configure settings
        StreamConfiguration requestedConfig = (isInput)
                ? mAudioInTester.requestedConfiguration
                : mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualConfig = (isInput)
                ? mAudioInTester.actualConfiguration
                : mAudioOutTester.actualConfiguration;

        requestedConfig.reset();
        requestedConfig.setPerformanceMode(perfMode);
        requestedConfig.setSharingMode(sharingMode);
        requestedConfig.setSampleRate(sampleRate);

        if (sampleRate != 0) {
            requestedConfig.setRateConversionQuality(sampleRateConversionQuality);
        }

        log("========================== #" + mAutomatedTestRunner.getTestCount());
        log("Requested:");
        log(getConfigText(requestedConfig));

        // Give previous stream time to close and release resources. Avoid race conditions.
        Thread.sleep(SETTLING_TIME_MILLIS);
        if (!mAutomatedTestRunner.isThreadEnabled()) return;
        boolean openFailed = false;
        boolean hasMicFailed = false;
        AudioStreamBase stream = null;
        try {
            openAudio();
            log("Actual:");
            actualConfigText = getConfigText(actualConfig)
                    + ", " + ((actualConfig.isMMap() ? "MMAP" : "Legacy")
                    + ", Dev = " + actualConfig.getDeviceId()
            );
            log(actualConfigText);
            flushLog();

            stream = (isInput)
                    ? mAudioInTester.getCurrentAudioStream()
                    : mAudioOutTester.getCurrentAudioStream();
        } catch (IOException e) {
            openFailed = true;
            log(e);
        }

        // The test is only worth running if we got the configuration we requested.
        boolean valid = true;
        if (!openFailed) {
            if(actualConfig.getSharingMode() != sharingMode) {
                log("did not get requested sharing mode");
                valid = false;
            }
            if (actualConfig.getPerformanceMode() != perfMode) {
                log("did not get requested performance mode");
                valid = false;
            }
            if (actualConfig.getNativeApi() == StreamConfiguration.NATIVE_API_OPENSLES) {
                log("OpenSL ES does not support automatic disconnect");
                valid = false;
            }
        }

        if (!openFailed && valid) {
            try {
                startAudioTest();
            } catch (IOException e) {
                e.printStackTrace();
                valid = false;
                log(e);
            }
        }

        int oldPlugCount = mPlugCount;
        int oldRoutingChangedCount = getRoutingChangedCount();
        if (!openFailed && valid) {
            mTestFailed = false;
            // poll until stream started
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTING) {
                Thread.sleep(POLL_DURATION_MILLIS);
            }
            String message = (requestPlugin ? "Plug IN" : "UNplug") + " headset now!";
            setStatusText("Testing:\n" + actualConfigText);
            setInstructionsText(message);
            int timeoutCount = 0;
            // Wait for Java plug count to change or stream to disconnect.
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED) {
                flushLog();
                Thread.sleep(POLL_DURATION_MILLIS);
                if (mPlugCount > oldPlugCount) {
                    timeoutCount = TIME_TO_FAILURE_MILLIS / POLL_DURATION_MILLIS;
                    break;
                }
            }

            int currentRoutingCount = getRoutingChangedCount();
            // Wait for timeout or stream to disconnect.
            while (!mTestFailed && mAutomatedTestRunner.isThreadEnabled() && !mSkipTest && (timeoutCount > 0) &&
                    stream.getState() == StreamConfiguration.STREAM_STATE_STARTED &&
                    oldRoutingChangedCount == currentRoutingCount) {
                flushLog();
                Thread.sleep(POLL_DURATION_MILLIS);
                timeoutCount--;
                currentRoutingCount = getRoutingChangedCount();
                if (timeoutCount == 0) {
                    mTestFailed = true;
                } else {
                    setStatusText("Plug detected by Java.\nCounting down to Oboe failure: " + timeoutCount);
                }
            }

            if (mSkipTest) {
                setStatusText("Skipped");
            } else {
                if (mTestFailed) {
                    // Check whether the peripheral has a microphone.
                    // Sometimes the microphones does not appear on the first HEADSET_PLUG event.
                    if (isInput && (mPlugMicrophone == 0)) {
                        hasMicFailed = true;
                    }
                } else {
                    int error = stream.getLastErrorCallbackResult();
                    if (error != StreamConfiguration.ERROR_DISCONNECTED &&
                        oldRoutingChangedCount == currentRoutingCount) {
                        log("onErrorCallback error = " + error
                                + ", expected " + StreamConfiguration.ERROR_DISCONNECTED +
                                ", old routing count = " + oldRoutingChangedCount +
                                ", current routing count = " + currentRoutingCount);
                        mTestFailed = true;
                    }
                }
                setStatusText(mTestFailed ? "Failed" : "Passed - detected");
            }
        }
        updateFailSkipButton(false);
        setInstructionsText("Wait...");

        if (!openFailed) {
            stopAudioTest();
        }

        if (mSkipTest) valid = false;

        if (valid) {
            if (openFailed) {
                appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                appendFailedSummary(getConfigText(requestedConfig) + "\n");
                appendFailedSummary("Open failed!\n");
                mAutomatedTestRunner.incrementFailCount();
            } else {
                log("Result:");
                boolean passed = !mTestFailed;
                String resultText = requestPlugin ? "plugIN" : "UNplug";
                resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
                if (hasMicFailed) {
                    resultText += ", Headset has no mic!";
                }
                log(resultText);
                if (!passed) {
                    appendFailedSummary("------ #" + mAutomatedTestRunner.getTestCount() + "\n");
                    appendFailedSummary("  " + actualConfigText + "\n");
                    appendFailedSummary("    " + resultText + "\n");
                    mAutomatedTestRunner.incrementFailCount();
                } else {
                    mAutomatedTestRunner.incrementPassCount();
                }
            }
        } else {
            log(TEXT_SKIP);
        }
        flushLog();
        // Give hardware time to settle between tests.
        Thread.sleep(1000);
        mAutomatedTestRunner.incrementTestCount();
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode, int sampleRate,
                                   int sampleRateConversionQuality) throws InterruptedException {
        boolean requestPlugin = true; // plug IN
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate,
                sampleRateConversionQuality, requestPlugin);
        requestPlugin = false; // UNplug
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate,
                sampleRateConversionQuality, requestPlugin);
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode, int sampleRate) throws InterruptedException {
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate,
                StreamConfiguration.RATE_CONVERSION_QUALITY_NONE);
    }

    private void testConfiguration(boolean isInput, int performanceMode,
                                   int sharingMode) throws InterruptedException {
        final int sampleRate = 0;
        testConfiguration(isInput, performanceMode, sharingMode, sampleRate);
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode) throws InterruptedException {
        testConfiguration(false, performanceMode, sharingMode);
        testConfiguration(true, performanceMode, sharingMode);
    }

    private void testConfiguration(int performanceMode,
                                   int sharingMode, int sampleRate,
                                   int sampleRateConversionQuality) throws InterruptedException {
        testConfiguration(false, performanceMode, sharingMode, sampleRate, sampleRateConversionQuality);
        testConfiguration(true, performanceMode, sharingMode, sampleRate, sampleRateConversionQuality);
    }

    @Override
    public void runTest() {

        runOnUiThread(() -> keepScreenOn(true));

        mPlugCount = 0;

        // Try several different configurations.
        try {
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_NONE,
                    StreamConfiguration.SHARING_MODE_SHARED);
            if (NativeEngine.isMMapExclusiveSupported()){
                testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                        StreamConfiguration.SHARING_MODE_EXCLUSIVE);
            }
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_SHARED);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_SHARED, 44100,
                    StreamConfiguration.RATE_CONVERSION_QUALITY_NONE);
            testConfiguration(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY,
                    StreamConfiguration.SHARING_MODE_SHARED, 44100,
                    StreamConfiguration.RATE_CONVERSION_QUALITY_MEDIUM);
        } catch (InterruptedException e) {
            log("Test CANCELLED - INVALID!");
        } catch (Exception e) {
            log(e);
            showErrorToast("Caught " + e);
        } finally {
            setInstructionsText("Test finished.");
            updateFailSkipButton(false);
            runOnUiThread(() -> keepScreenOn(false));
        }
    }
}
