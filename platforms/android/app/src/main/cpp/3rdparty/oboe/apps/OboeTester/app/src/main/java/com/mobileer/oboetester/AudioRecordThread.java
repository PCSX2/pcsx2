/*
 * Copyright (C) 2013 The Android Open Source Project
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


import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.util.Log;

/**
 * Abstract class for recording.
 * Call processBuffer(buffer) when data is read.
 */
class AudioRecordThread implements Runnable {
    private static final String TAG = "AudioRecordThread";

    private final int mSampleRate;
    private final int mChannelCount;
    private Thread mThread;
    protected boolean mGo;
    private AudioRecord mRecorder;
    private CircularCaptureBuffer mCaptureBuffer;
    protected float[] mBuffer = new float[256];
    private static int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_FLOAT;
    private Runnable mTask;
    private int mTaskCountdown;
    private boolean mCaptureEnabled = true;

    private AudioDeviceInfo mDeviceInfo;
    private String mAudioSource;

    public AudioRecordThread(int frameRate, int channelCount, int maxFrames) {
        mSampleRate = frameRate;
        mChannelCount = channelCount;
        mCaptureBuffer = new CircularCaptureBuffer(maxFrames);
    }

    private void createRecorder() {
        int channelConfig = (mChannelCount == 1)
                ? AudioFormat.CHANNEL_IN_MONO : AudioFormat.CHANNEL_IN_STEREO;
        int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;
        int minRecordBuffSizeInBytes = AudioRecord.getMinBufferSize(mSampleRate,
                channelConfig,
                audioFormat);
        int audioSourceInt = audioSourceToInt(mAudioSource);
        mRecorder = new AudioRecord(
                audioSourceInt,
                mSampleRate,
                channelConfig,
                audioFormat,
                2 * minRecordBuffSizeInBytes);
        mRecorder.setPreferredDevice(mDeviceInfo);
        if (mRecorder.getState() == AudioRecord.STATE_UNINITIALIZED) {
            throw new RuntimeException("Could not make the AudioRecord - UNINITIALIZED");
        }
    }

    @Override
    public void run() {
        startAudioRecording();

        while (mGo) {
            int result = handleAudioPeriod();
            if (result < 0) {
                mGo = false;
            }
        }

        stopAudioRecording();
    }

    public void setAudioSource(String audioSource) {
        mAudioSource = audioSource;
    }

    public void startAudio() {
        if (mThread == null) {
            mGo = true;
            mThread = new Thread(this);
            mThread.start();
        }
    }

    public void stopAudio() {
        mGo = false;
        if (mThread != null) {
            try {
                mThread.join(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            mThread = null;
        }
    }

    public int getSampleRate() {
        return mSampleRate;
    }

    /**
     * @return number of samples read or negative error
     */
    private int handleAudioPeriod() {
        int numSamplesRead = mRecorder.read(mBuffer, 0, mBuffer.length,
                AudioRecord.READ_BLOCKING);
        if (numSamplesRead <= 0) {
            return numSamplesRead;
        } else {
            if (mTaskCountdown > 0) {
                mTaskCountdown -= numSamplesRead;
                if (mTaskCountdown <= 0) {
                    mTaskCountdown = 0;
                    new Thread(mTask).start(); // run asynchronously with audio thread
                }
            }
            if (mCaptureEnabled) {
                return mCaptureBuffer.write(mBuffer, 0, numSamplesRead);
            } else {
                return numSamplesRead;
            }
        }
    }

    private void startAudioRecording() {
        stopAudioRecording();
        createRecorder();
        mRecorder.startRecording();
    }

    private void stopAudioRecording() {
        if (mRecorder != null) {
            mRecorder.stop();
            mRecorder.release();
            mRecorder = null;
        }
    }

    /**
     * Schedule task to be run on its own thread when numSamples more samples have been recorded.
     *
     * @param numSamples
     * @param task
     */
    public void scheduleTask(int numSamples, Runnable task) {
        mTask = task;
        mTaskCountdown = numSamples;
    }

    public void setCaptureEnabled(boolean captureEnabled) {
        mCaptureEnabled = captureEnabled;
    }

    public int readMostRecent(float[] buffer) {
        return mCaptureBuffer.readMostRecent(buffer);
    }

    public void setInputDevice(AudioDeviceInfo deviceInfo) {
        mDeviceInfo = deviceInfo;
    }

    private int audioSourceToInt(String audioSource) {
        if (audioSource.equals("DEFAULT")) {
            return MediaRecorder.AudioSource.DEFAULT;
        } if (audioSource.equals("MIC")) {
            return MediaRecorder.AudioSource.MIC;
        } else if (audioSource.equals("VOICE_RECOGNITION")) {
            return MediaRecorder.AudioSource.VOICE_RECOGNITION;
        } else if (audioSource.equals("VOICE_COMMUNICATION")) {
            return MediaRecorder.AudioSource.VOICE_COMMUNICATION;
        } else if (audioSource.equals("CAMCORDER")) {
            return MediaRecorder.AudioSource.CAMCORDER;
        } else if (audioSource.equals("UNPROCESSED")) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                return MediaRecorder.AudioSource.UNPROCESSED;
            } else {
                Log.d(TAG, "MediaRecorder.AudioSource.UNPROCESSED not supported");
                return MediaRecorder.AudioSource.DEFAULT;
            }
        } else if (audioSource.equals("VOICE_PERFORMANCE")) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                return MediaRecorder.AudioSource.VOICE_PERFORMANCE;
            } else {
                Log.d(TAG, "MediaRecorder.AudioSource.VOICE_PERFORMANCE not supported");
                return MediaRecorder.AudioSource.DEFAULT;
            }
        } else {
            Log.d(TAG, "Unknown audio source: " + audioSource);
            return MediaRecorder.AudioSource.DEFAULT;
        }
    }
}
