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

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.widget.Toast;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.util.Locale;

/**
 * Activity to measure latency on a full duplex stream.
 */
public class AnalyzerActivity extends TestInputActivity {

    AudioOutputTester mAudioOutTester;
    protected BufferSizeView mBufferSizeView;
    protected AutomatedTestRunner mAutomatedTestRunner;

    // Note that these string must match the enum result_code in LatencyAnalyzer.h
    String resultCodeToString(int resultCode) {
        switch (resultCode) {
            case 0:
                return "OK";
            case -99:
                return "ERROR_NOISY";
            case -98:
                return "ERROR_VOLUME_TOO_LOW";
            case -97:
                return "ERROR_VOLUME_TOO_HIGH";
            case -96:
                return "ERROR_CONFIDENCE";
            case -95:
                return "ERROR_INVALID_STATE";
            case -94:
                return "ERROR_GLITCHES";
            case -93:
                return "ERROR_NO_LOCK";
            default:
                return "UNKNOWN";
        }
    }

    public native int getAnalyzerState();
    public native boolean isAnalyzerDone();
    public native int getMeasuredResult();
    public native int getResetCount();

    @Override
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

        // INPUT
        report.append(mAudioInputTester.actualConfiguration.dump());
        AudioStreamBase inStream = mAudioInputTester.getCurrentAudioStream();
        report.append(String.format(Locale.getDefault(), "in.burst.frames = %d\n", inStream.getFramesPerBurst()));
        report.append(String.format(Locale.getDefault(), "in.xruns = %d\n", inStream.getXRunCount()));
        report.append(String.format(Locale.getDefault(), "in.frames.read = %d\n", inStream.getFramesRead()));

        // OUTPUT
        report.append(mAudioOutTester.actualConfiguration.dump());
        AudioStreamBase outStream = mAudioOutTester.getCurrentAudioStream();
        report.append(String.format(Locale.getDefault(), "out.burst.frames = %d\n", outStream.getFramesPerBurst()));
        int bufferSize = outStream.getBufferSizeInFrames();
        report.append(String.format(Locale.getDefault(), "out.buffer.size.frames = %d\n", bufferSize));
        int bufferCapacity = outStream.getBufferCapacityInFrames();
        report.append(String.format(Locale.getDefault(), "out.buffer.capacity.frames = %d\n", bufferCapacity));
        report.append(String.format(Locale.getDefault(), "out.xruns = %d\n", outStream.getXRunCount()));
        report.append(String.format(Locale.getDefault(), "out.frames.written = %d\n", outStream.getFramesWritten()));

        return report.toString();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mAudioOutTester = addAudioOutputTester();
        mBufferSizeView = (BufferSizeView) findViewById(R.id.buffer_size_view);
    }

    @Override
    protected void resetConfiguration() {
        super.resetConfiguration();
        mAudioOutTester.reset();

        StreamContext streamContext = getFirstInputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setFormat(StreamConfiguration.AUDIO_FORMAT_PCM_FLOAT);
                streamContext.configurationView.setFormatConversionAllowed(true);
            }
        }
        streamContext = getFirstOutputStreamContext();
        if (streamContext != null) {
            if (streamContext.configurationView != null) {
                streamContext.configurationView.setFormat(StreamConfiguration.AUDIO_FORMAT_PCM_FLOAT);
                streamContext.configurationView.setFormatConversionAllowed(true);
            }
        }
    }

    @Override
    public void openAudio() throws IOException {
        super.openAudio();
        if (mBufferSizeView != null) {
            mBufferSizeView.onStreamOpened((OboeAudioStream) mAudioOutTester.getCurrentAudioStream());
        }
    }

    public void onStreamClosed() {
        Toast.makeText(getApplicationContext(),
                "Stream was closed or disconnected!",
                Toast.LENGTH_SHORT)
                .show();
        stopAudioTest();
    }

    public void stopAudioTest() {
    }

    @Override
    public void saveIntentLog() {
        if (mTestRunningByIntent) {
            String report = mAutomatedTestRunner.getFullLogs();
            maybeWriteTestResult(report);
            mTestRunningByIntent = false;
            mBundleFromIntent = null;
        }
    }

}
