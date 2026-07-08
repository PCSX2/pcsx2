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

import static com.mobileer.oboetester.StreamConfiguration.convertChannelMaskToText;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Environment;

import androidx.annotation.Nullable;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Locale;

public class BaseAutoGlitchActivity extends GlitchActivity {

    private static final int SETUP_TIME_SECONDS = 4; // Time for the stream to settle.
    protected static final int DEFAULT_DURATION_SECONDS = 8; // Run time for each test.
    private static final int DEFAULT_GAP_MILLIS = 400; // Idle time between each test.
    private static final String TEXT_SKIP = "SKIP";
    public static final String TEXT_PASS = "PASS";
    public static final String TEXT_FAIL = "FAIL !!!!";

    protected int mDurationSeconds = DEFAULT_DURATION_SECONDS;
    protected int mGapMillis = DEFAULT_GAP_MILLIS;
    private String mTestName = "";

    protected AudioManager   mAudioManager;

    protected ArrayList<TestResult> mTestResults = new ArrayList<TestResult>();

    public static boolean arrayContains(int[] haystack, int needle) {
        for (int n: haystack) {
            if (n == needle) return true;
        }
        return false;
    }

    void logDeviceInfo() {
        log("\n############################");
        log("\nDevice Info:");
        AudioManager audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        log(AudioQueryTools.getAudioManagerReport(audioManager));
        log(AudioQueryTools.getAudioFeatureReport(getPackageManager()));
        log(AudioQueryTools.getAudioPropertyReport());
        log("\n############################");
    }

    void setTestName(String name) {
        mTestName = name;
    }

    @Override
    public int getDeviceId() {
        return super.getDeviceId();
    }

    private static class TestStreamOptions {
        public final int channelUsed;
        public final int channelCount;
        public final int channelMask;
        public final int deviceId;
        public final int mmapUsed;
        public final int performanceMode;
        public final int sharingMode;
        public final int sessionId;

        public TestStreamOptions(StreamConfiguration configuration, int channelUsed) {
            this.channelUsed = channelUsed;
            channelCount = configuration.getChannelCount();
            channelMask = configuration.getChannelMask();
            deviceId = configuration.getDeviceId();
            mmapUsed = configuration.isMMap() ? 1 : 0;
            performanceMode = configuration.getPerformanceMode();
            sharingMode = configuration.getSharingMode();
            sessionId = configuration.getSessionId();
        }

        int countDifferences(TestStreamOptions other) {
            int count = 0;
            count += (channelUsed != other.channelUsed) ? 1 : 0;
            count += (channelCount != other.channelCount) ? 1 : 0;
            count += (channelMask != other.channelMask) ? 1 : 0;
            count += (deviceId != other.deviceId) ? 1 : 0;
            count += (mmapUsed != other.mmapUsed) ? 1 : 0;
            count += (performanceMode != other.performanceMode) ? 1 : 0;
            count += (sharingMode != other.sharingMode) ? 1 : 0;
            count += (sessionId != other.sessionId) ? 1 : 0;
            return count;
        }

        public String comparePassedDirection(String prefix, TestStreamOptions passed) {
            StringBuffer text = new StringBuffer();
            text.append(TestDataPathsActivity.comparePassedField(prefix, this, passed, "channelUsed"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "channelCount"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "channelMask"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "deviceId"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "mmapUsed"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "performanceMode"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "sharingMode"));
            text.append(TestDataPathsActivity.comparePassedField(prefix,this, passed, "sessionId"));
            return text.toString();
        }
        @Override
        public String toString() {
            return "D=" + deviceId
                    + ", " + ((mmapUsed > 0) ? "MMAP" : "Lgcy")
                    + ", ch=" + channelText(channelUsed, channelCount)
                    + ", cm=" + convertChannelMaskToText(channelMask)
                    + "," + StreamConfiguration.convertPerformanceModeToText(performanceMode)
                    + "," + StreamConfiguration.convertSharingModeToText(sharingMode);
        }
    }

    protected static class TestResult {
        final int testIndex;
        final TestStreamOptions input;
        final TestStreamOptions output;
        public final int inputPreset;
        public final int sampleRate;
        final String testName; // name or purpose of test

        int result = TEST_RESULT_SKIPPED; // TEST_RESULT_FAILED, etc
        private String mComments = ""; // additional info, ideas for why it failed

        public TestResult(int testIndex,
                          String testName,
                          StreamConfiguration inputConfiguration,
                          int inputChannel,
                          StreamConfiguration outputConfiguration,
                          int outputChannel) {
            this.testIndex = testIndex;
            this.testName = testName;
            input = new TestStreamOptions(inputConfiguration, inputChannel);
            output = new TestStreamOptions(outputConfiguration, outputChannel);
            sampleRate = outputConfiguration.getSampleRate();
            this.inputPreset = inputConfiguration.getInputPreset();
        }

        int countDifferences(TestResult other) {
            int count = 0;
            count += input.countDifferences((other.input));
            count += output.countDifferences((other.output));
            count += (sampleRate != other.sampleRate) ? 1 : 0;
            count += (inputPreset != other.inputPreset) ? 1 : 0;
            return count;
        }

        public boolean failed() {
            return result == TEST_RESULT_FAILED;
        }

        public boolean passed() {
            return result == TEST_RESULT_PASSED;
        }

        public String comparePassed(TestResult passed) {
            StringBuffer text = new StringBuffer();
            text.append("Compare with passed test #" + passed.testIndex + "\n");
            text.append(input.comparePassedDirection("IN", passed.input));
            text.append(TestDataPathsActivity.comparePassedInputPreset("IN", this, passed));
            text.append(output.comparePassedDirection("OUT", passed.output));
            text.append(TestDataPathsActivity.comparePassedField("I/O",this, passed, "sampleRate"));

            return text.toString();
        }

        @Override
        public String toString() {
            return "IN:  " + input + ", ip=" + inputPreset + "\n"
                    + "OUT: " + output + ", sr=" + sampleRate
                    + mComments;
        }

        public void addComment(String comment) {
            mComments += "\n";
            mComments += comment;
        }

        public void setResult(int result) {
            this.result = result;
        }
        public int getResult(int result) {
            return result;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);

        mAutomatedTestRunner = findViewById(R.id.auto_test_runner);
        mAutomatedTestRunner.setActivity(this);
    }

    protected void log(String text) {
        mAutomatedTestRunner.log(text);
    }

    protected void appendFailedSummary(String text) {
        mAutomatedTestRunner.appendFailedSummary(text);
    }
    protected void appendSummary(String text) {
        mAutomatedTestRunner.appendSummary(text);
    }

    @Override
    public void onStopTest() {
        mAutomatedTestRunner.stopTest();
    }

    static String channelText(int index, int count) {
        return index + "/" + count;
    }

    protected String getConfigText(StreamConfiguration config) {
        int channel = (config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT)
                ? getOutputChannel() : getInputChannel();
        return ((config.getDirection() == StreamConfiguration.DIRECTION_OUTPUT) ? "OUT" : "INP")
                + (config.isMMap() ? "-M" : "-L")
                + "-" + StreamConfiguration.convertSharingModeToText(config.getSharingMode())
                + ", ID = " + String.format(Locale.getDefault(), "%2d", config.getDeviceId())
                + ", Perf = " + StreamConfiguration.convertPerformanceModeToText(
                        config.getPerformanceMode())
                + ((config.getSessionId() > 0) ? (", sessionId = " + config.getSessionId()) : "")
                + ",\n     ch = " + channelText(channel, config.getChannelCount())
                + ", cm = " + convertChannelMaskToText(config.getChannelMask());
    }

    protected String getStreamText(AudioStreamBase stream) {
        return ("burst=" + stream.getFramesPerBurst()
                + ", size=" + stream.getBufferSizeInFrames()
                + ", cap=" + stream.getBufferCapacityInFrames()
                + "\n     HW: sr=" + stream.getHardwareSampleRate()
                + ", ch=" + stream.getHardwareChannelCount()
                + ", fmt=" + (stream.getHardwareFormat() == StreamConfiguration.UNSPECIFIED ?
                "?" : StreamConfiguration.convertFormatToText(stream.getHardwareFormat()))
        );
    }

    public final static int TEST_RESULT_FAILED = -2;
    public final static int TEST_RESULT_WARNING = -1;
    public final static int TEST_RESULT_SKIPPED = 0;
    public final static int TEST_RESULT_PASSED = 1;

    // Run one test based on the requested input/output configurations.
    @Nullable
    protected TestResult testCurrentConfigurations() throws InterruptedException {
        mAutomatedTestRunner.incrementTestCount();
        if ((getSingleTestIndex() >= 0) && (getTestCount() != getSingleTestIndex())) {
            mAutomatedTestRunner.incrementSkipCount();
            return null;
        }

        log("========================== #" + getTestCount());
        if (getTestCount() == 1) {
            deleteOldWaveFiles();
        }

        int result = 0;
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;

        log("Requested:");
        log("  SR = " + requestedOutConfig.getSampleRate());
        log("  " + getConfigText(requestedInConfig));
        log("  " + getConfigText(requestedOutConfig));

        String reason = "";
        boolean openFailed = false;
        try {
            openAudio(); // this will fill in actualConfig

            log("Actual:");
            log("  SR = " + actualOutConfig.getSampleRate());
            // Set output size to a level that will avoid glitches.
            AudioStreamBase outStream = mAudioOutTester.getCurrentAudioStream();
            int sizeFrames = outStream.getBufferCapacityInFrames() / 2;
            sizeFrames = Math.max(sizeFrames, 2 * outStream.getFramesPerBurst());
            outStream.setBufferSizeInFrames(sizeFrames);
            AudioStreamBase inStream = mAudioInputTester.getCurrentAudioStream();
            log("  " + getConfigText(actualInConfig));
            log("     " + getStreamText(inStream));
            log("  " + getConfigText(actualOutConfig));
            log("     " + getStreamText(outStream));
        } catch (Exception e) {
            openFailed = true;
            log(e.getMessage());
            reason = e.getMessage();
        }

        TestResult testResult = new TestResult(
                getTestCount(),
                mTestName,
                mAudioInputTester.actualConfiguration,
                getInputChannel(),
                mAudioOutTester.actualConfiguration,
                getOutputChannel()
        );

        // The test will only be worth running if we got the configuration we requested on input or output.
        String skipReason = whyShouldTestBeSkipped();
        boolean skipped = skipReason.length() > 0;
        boolean valid = !openFailed && !skipped;
        boolean startFailed = false;
        if (valid) {
            try {
                startAudioTest();   // Start running the test in the background.
            } catch (IOException e) {
                e.printStackTrace();
                valid = false;
                startFailed = true;
                log(e.getMessage());
                reason = e.getMessage();
            }
        }
        mAutomatedTestRunner.flushLog();

        if (valid) {
            // Check for early return until we reach full duration.
            long now = System.currentTimeMillis();
            long startedAt = now;
            long endTime = System.currentTimeMillis() + (mDurationSeconds * 1000);
            while (now < endTime) {
                Thread.sleep(100); // Let test run.
                now = System.currentTimeMillis();
                double runningTimeSeconds = (now - startedAt) / 1000.0;
                if (isFinishedEarly(runningTimeSeconds)) {
                    log("Finished early after " + (now - startedAt) + " msec.");
                    break;
                }
            }
        }
        int inXRuns = 0;
        int outXRuns = 0;

        if (!openFailed) {
            // get xRuns before closing the streams.
            inXRuns = mAudioInputTester.getCurrentAudioStream().getXRunCount();
            outXRuns = mAudioOutTester.getCurrentAudioStream().getXRunCount();

            super.stopAudioTest();
        }

        if (openFailed || startFailed) {
            appendFailedSummary("------ #" + getTestCount() + "\n");
            appendFailedSummary(getConfigText(requestedInConfig) + "\n");
            appendFailedSummary(getConfigText(requestedOutConfig) + "\n");
            appendFailedSummary(reason + "\n");
            mAutomatedTestRunner.incrementFailCount();
        } else if (skipped) {
            log(TEXT_SKIP + " - " + skipReason);
            mAutomatedTestRunner.incrementSkipCount();
        } else {
            log("Result:");
            reason += didTestFail();
            boolean passed = reason.length() == 0;

            String resultText = getShortReport();
            resultText += ", xruns = " + inXRuns + "/" + outXRuns;
            resultText += ", " + (passed ? TEXT_PASS : TEXT_FAIL);
            resultText += reason;
            log("  " + resultText);
            if (!passed) {
                appendFailedSummary("------ #" + getTestCount() + "\n");
                appendFailedSummary("  " + getConfigText(actualInConfig) + "\n");
                appendFailedSummary("  " + getConfigText(actualOutConfig) + "\n");
                appendFailedSummary("    " + resultText + "\n");
                mAutomatedTestRunner.incrementFailCount();
                result = TEST_RESULT_FAILED;
            } else {
                mAutomatedTestRunner.incrementPassCount();
                result = TEST_RESULT_PASSED;
            }
            saveRecordingAsWave(); // Save recording for both pass and fail results.
        }
        mAutomatedTestRunner.flushLog();

        // Give hardware time to settle between tests.
        Thread.sleep(mGapMillis);

        if (valid) {
            testResult.setResult(result);
            mTestResults.add(testResult);
        }
        return testResult;
    }

    void testPerformancePaths() throws InterruptedException {
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;

        final boolean requestedInConfigWasMMap = requestedInConfig.isMMap();
        final boolean requestedOutConfigWasMMap = requestedOutConfig.isMMap();

        requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);
        requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_SHARED);

        // Legacy NONE
        requestedInConfig.setMMap(false);
        requestedOutConfig.setMMap(false);
        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_NONE);
        testCurrentConfigurations();

        // Legacy LOW_LATENCY
        requestedInConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        requestedOutConfig.setPerformanceMode(StreamConfiguration.PERFORMANCE_MODE_LOW_LATENCY);
        testCurrentConfigurations();

        if (NativeEngine.isMMapSupported()) {
            requestedInConfig.setMMap(true);
            requestedOutConfig.setMMap(true);

            // MMAP_LOW_LATENCY_SHARED
            testCurrentConfigurations();

            // MMAP LOW_LATENCY EXCLUSIVE
            requestedInConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
            requestedOutConfig.setSharingMode(StreamConfiguration.SHARING_MODE_EXCLUSIVE);
            testCurrentConfigurations();
        }

        requestedInConfig.setMMap(requestedInConfigWasMMap);
        requestedOutConfig.setMMap(requestedOutConfigWasMMap);
    }

    File getRecordingDir() {
        return getExternalFilesDir(Environment.DIRECTORY_MUSIC);
    }

    void reportSavedWaveFiles() {
        logAnalysis("\n --------\n"
                + "Audio for each test saved in:\n  "
                + getRecordingDir().getAbsolutePath()
                + "/glitch_{###}.wav\n");
    }

    private void saveRecordingAsWave() {
        File waveFile = new File(getRecordingDir(),
                String.format("glitch_%03d.wav", getTestCount()));
        int saveResult = saveWaveFile(waveFile.getAbsolutePath());
        if (saveResult <= 0) {
            appendSummary("ERROR: saving "
                    + waveFile.getAbsolutePath()
                    + " returned " + saveResult + "\n");
        }
    }

    /**
     * Delete all the previously saved WAV files so the user does not
     * debug obsolete data.
     */
    private void deleteOldWaveFiles() {
        File recordingDir = getRecordingDir();
        if (recordingDir.exists()) {
            File[] files = recordingDir.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (file.isFile()) {
                        if (!file.delete()) {
                            appendFailedSummary("Failed to delete file: " + file.getAbsolutePath());
                        }
                    }
                }
            }
        }
    }

    protected int getTestCount() {
        return mAutomatedTestRunner.getTestCount();
    }

    protected AudioDeviceInfo getDeviceInfoById(int deviceId) {
        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_ALL);
        for (AudioDeviceInfo deviceInfo : devices) {
            if (deviceInfo.getId() == deviceId) {
                return deviceInfo;
            }
        }
        return null;
    }

    protected AudioDeviceInfo getDeviceInfoByType(int deviceType, int flags) {
        AudioDeviceInfo[] devices = mAudioManager.getDevices(flags);
        for (AudioDeviceInfo deviceInfo : devices) {
            if (deviceInfo.getType() == deviceType) {
                return deviceInfo;
            }
        }
        return null;
    }

    /**
     * Are outputs mixed in the air or by a loopback plug?
     * @param type device type, eg AudioDeviceInfo.TYPE_BUILTIN_SPEAKER
     * @return true if stereo output channels get mixed to mono input
     */
    protected boolean isDeviceTypeMixedForLoopback(int type) {
        switch(type) {
            // Mixed in the air.
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE:
            // Mixed in the loopback fun-plug.
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
            case AudioDeviceInfo.TYPE_USB_HEADSET:
                return true;

            case AudioDeviceInfo.TYPE_USB_DEVICE:
            default:
                return false; // channels are discrete
        }
    }

    /**
     * @param type
     * @return list of compatible device types in preferred order
     */
    protected ArrayList<Integer> getCompatibleDeviceTypes(int type) {
        ArrayList<Integer> compatibleTypes = new ArrayList<Integer>();
        switch(type) {
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE:
                compatibleTypes.add(AudioDeviceInfo.TYPE_BUILTIN_MIC);
                break;
            case AudioDeviceInfo.TYPE_BUILTIN_MIC:
                compatibleTypes.add(AudioDeviceInfo.TYPE_BUILTIN_SPEAKER);
                break;
            case AudioDeviceInfo.TYPE_USB_DEVICE:
                // Give priority to an exact match of DEVICE.
                compatibleTypes.add(AudioDeviceInfo.TYPE_USB_DEVICE);
                compatibleTypes.add(AudioDeviceInfo.TYPE_USB_HEADSET);
                break;
            case AudioDeviceInfo.TYPE_USB_HEADSET:
                // Give priority to an exact match of HEADSET.
                compatibleTypes.add(AudioDeviceInfo.TYPE_USB_HEADSET);
                compatibleTypes.add(AudioDeviceInfo.TYPE_USB_DEVICE);
                break;
            default:
                compatibleTypes.add(type);
                break;
        }
        return compatibleTypes;
    }

    /**
     * Scan available device for one with a compatible device type for loopback testing.
     * @return deviceId
     */

    protected AudioDeviceInfo findCompatibleInputDevice(int outputDeviceType) {
        ArrayList<Integer> compatibleDeviceTypes = getCompatibleDeviceTypes(outputDeviceType);
        AudioDeviceInfo[] devices = mAudioManager.getDevices(AudioManager.GET_DEVICES_INPUTS);
        // Scan the compatible types in order of preference.
        for (int compatibleType : compatibleDeviceTypes) {
            for (AudioDeviceInfo candidate : devices) {
                if (candidate.getType() == compatibleType) {
                    return candidate;
                }
            }
        }
        return null;
    }

    protected boolean isFinishedEarly(double runningTimeSeconds) {
        return false;
    }

    /**
     * Figure out if a test should be skipped and return the reason.
     *
     * @return reason for skipping or an empty string
     */
    protected String whyShouldTestBeSkipped() {
        String why = "";
        StreamConfiguration requestedInConfig = mAudioInputTester.requestedConfiguration;
        StreamConfiguration requestedOutConfig = mAudioOutTester.requestedConfiguration;
        StreamConfiguration actualInConfig = mAudioInputTester.actualConfiguration;
        StreamConfiguration actualOutConfig = mAudioOutTester.actualConfiguration;
        // No point running the test if we don't get any of the sharing modes we requested.
        if (actualInConfig.getSharingMode() != requestedInConfig.getSharingMode()
                && actualOutConfig.getSharingMode() != requestedOutConfig.getSharingMode()) {
            log("Did not get requested sharing mode.");
            why += "share,";
        }
        if (actualInConfig.getPerformanceMode() != requestedInConfig.getPerformanceMode()
                && actualOutConfig.getPerformanceMode() != requestedOutConfig.getPerformanceMode()) {
            log("Did not get requested performance mode.");
            why += "perf,";
        }
        if (actualInConfig.isMMap() != requestedInConfig.isMMap()
                && actualOutConfig.isMMap() != requestedOutConfig.isMMap()) {
            log("Did not get requested MMAP data path.");
            why += "mmap,";
        }
        return why;
    }

    public String didTestFail() {
        String why = "";
        if (getMaxSecondsWithNoGlitch() <= (mDurationSeconds - SETUP_TIME_SECONDS)) {
            why += ", glitch";
        }
        return why;
    }

    void logAnalysis(String text) {
        appendFailedSummary(text + "\n");
    }

    private int countPassingTests() {
        int numPassed = 0;
        for (TestResult other : mTestResults) {
            if (other.passed()) {
                numPassed++;
            }
        }
        return numPassed;
    }

    protected void compareFailedTestsWithNearestPassingTest() {
        logAnalysis("\n==== COMPARISON ANALYSIS ===========");
        if (countPassingTests() == 0) {
            logAnalysis("Comparison skipped because NO tests passed.");
            return;
        }
        logAnalysis("Compare failed tests with others that passed.");
        // Analyze each failed test.
        for (TestResult testResult : mTestResults) {
            if (testResult.failed()) {
                logAnalysis("-------------------- #" + testResult.testIndex + " FAILED");
                String name = testResult.testName;
                if (name.length() > 0) {
                    logAnalysis(name);
                }
                TestResult[] closest = findClosestPassingTestResults(testResult);
                for (TestResult other : closest) {
                    logAnalysis(testResult.comparePassed(other));
                }
                logAnalysis(testResult.toString());
            }
        }
    }


    @Nullable
    private TestResult[] findClosestPassingTestResults(TestResult testResult) {
        int minDifferences = Integer.MAX_VALUE;
        for (TestResult other : mTestResults) {
            if (other.passed()) {
                int numDifferences = testResult.countDifferences(other);
                if (numDifferences < minDifferences) {
                    minDifferences = numDifferences;
                }
            }
        }
        // Now find all the tests that are just as close as the closest.
        ArrayList<TestResult> list = new ArrayList<TestResult>();
        for (TestResult other : mTestResults) {
            if (other.passed()) {
                int numDifferences = testResult.countDifferences(other);
                if (numDifferences == minDifferences) {
                    list.add(other);
                }
            }
        }
        return list.toArray(new TestResult[0]);
    }

}
