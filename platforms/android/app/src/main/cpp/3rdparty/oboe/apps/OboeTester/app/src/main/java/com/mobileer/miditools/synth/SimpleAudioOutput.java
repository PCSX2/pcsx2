/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.mobileer.miditools.synth;

import android.annotation.TargetApi;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Build;
import android.util.Log;

/**
 * Simple base class for implementing audio output for examples.
 * This can be sub-classed for experimentation or to redirect audio output.
 */
public class SimpleAudioOutput {

    private static final String TAG = "SimpleAudioOutput";
    public static final int SAMPLES_PER_FRAME = 2;
    public static final int BYTES_PER_SAMPLE = 4; // float
    public static final int BYTES_PER_FRAME = SAMPLES_PER_FRAME * BYTES_PER_SAMPLE;
    // Arbitrary weighting factor for CPU load filter. Higher number for slower response.
    private static final int LOAD_FILTER_SHIFT = 6;
    private static final int LOAD_FILTER_SCALER = (1<<LOAD_FILTER_SHIFT) - 1;
    // LOW_LATENCY_BUFFER_CAPACITY_IN_FRAMES is only used when we do low latency tuning.
    // The *3 is because some devices have a 1 msec period. And at
    // 48000 Hz that is 48, which is 16*3.
    // The 512 is arbitrary. 512*3 gives us a 32 msec buffer at 48000 Hz.
    // That is more than we need but not hugely wasteful.
    private static final int LOW_LATENCY_BUFFER_CAPACITY_IN_FRAMES = 512 * 3;

    private AudioTrack mAudioTrack;
    private int mFrameRate;
    private AudioLatencyTuner mLatencyTuner;
    private MyLatencyController mLatencyController = new MyLatencyController();
    private long previousBeginTime;
    private volatile long filteredCpuInterval;
    private volatile long filteredTotalInterval;

    class MyLatencyController extends LatencyController
    {
        @Override
        public boolean isLowLatencySupported() {
            return AudioLatencyTuner.isLowLatencySupported();
        }

        @Override
        public boolean isRunning() {
            return mAudioTrack != null;
        }

        public void setAutoSizeEnabled(boolean enabled) {
            super.setAutoSizeEnabled(enabled);
            if (!enabled) {
                AudioLatencyTuner tuner = mLatencyTuner;
                if (tuner != null) {
                    tuner.reset();
                }
            }
        }

        @Override
        public int getBufferSizeInFrames() {
            AudioTrack track = mAudioTrack;
            if (track != null) {
                return track.getBufferSizeInFrames();
            } else {
                return 0;
            }
        }

        @Override
        public int getBufferCapacityInFrames() {
            AudioLatencyTuner tuner = mLatencyTuner;
            if (tuner != null) {
                return tuner.getBufferCapacityInFrames();
            } else {
                return 0;
            }
        }

        @Override
        public int getUnderrunCount() {
            AudioLatencyTuner tuner = mLatencyTuner;
            if (tuner != null) {
                return tuner.getUnderrunCount();
            } else {
                return 0;
            }
        }


        @Override
        public int getCpuLoad() {
            int load = 0;
            if (filteredTotalInterval > 0) {
                load = (int) ((filteredCpuInterval * 100) / filteredTotalInterval);
            }
            return load;
        }
    }

    /**
     * Create an audio track then call play().
     */
    public void start(int framesPerBlock) {
        stop();
        mAudioTrack = createAudioTrack();

        mLatencyTuner = new AudioLatencyTuner(mAudioTrack, framesPerBlock);
        // Use frame rate chosen by the AudioTrack so that we can get a
        // low latency fast mixer track.
        mFrameRate = mAudioTrack.getSampleRate();
        // AudioTrack will wait until it has enough data before starting.
        mAudioTrack.play();
        previousBeginTime = 0;
        filteredCpuInterval = 0;
        filteredTotalInterval = 0;
    }

    @TargetApi(Build.VERSION_CODES.M)
    protected AudioTrack createAudioTrack() {
        AudioAttributes.Builder attributesBuilder = new AudioAttributes.Builder()
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC);
        boolean doLowLatency = (AudioLatencyTuner.isLowLatencySupported()
                && mLatencyController.isLowLatencyEnabled());
        if (doLowLatency) {
            Log.i(TAG, "createAudioTrack() using FLAG_LOW_LATENCY");
            attributesBuilder.setFlags(AudioLatencyTuner.getLowLatencyFlag());
        }
        AudioAttributes attributes = attributesBuilder.build();

        AudioFormat format = new AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .build();
        AudioTrack.Builder builder = new AudioTrack.Builder()
                .setAudioAttributes(attributes)
                .setAudioFormat(format);
        if (doLowLatency) {
            // Start with a bigger buffer because we can lower it later.
            int bufferSizeInFrames = LOW_LATENCY_BUFFER_CAPACITY_IN_FRAMES;
            builder.setBufferSizeInBytes(bufferSizeInFrames * BYTES_PER_FRAME);
        }
        AudioTrack track = builder.build();
        if (track == null) {
            throw new RuntimeException("Could not make the Audio Track! attributes = "
                    + attributes + ", format = " + format);
        }
        return track;
    }

    public int write(float[] buffer, int offset, int length) {
        endCpuLoadInterval();
        int result = mAudioTrack.write(buffer, offset, length,
                AudioTrack.WRITE_BLOCKING);
        beginCpuLoadInterval();
        if (result > 0 && mLatencyController.isAutoSizeEnabled()) {
            mLatencyTuner.update();
        }
        return result;
    }

    private void endCpuLoadInterval() {
        long now = System.nanoTime();
        if (previousBeginTime > 0) {
            long elapsed = now - previousBeginTime;
            // recursive low pass filter
            filteredCpuInterval = ((filteredCpuInterval * LOAD_FILTER_SCALER) + elapsed)
                    >> LOAD_FILTER_SHIFT;
        }

    }
    private void beginCpuLoadInterval() {
        long now = System.nanoTime();
        if (previousBeginTime > 0) {
            long elapsed = now - previousBeginTime;
            // recursive low pass filter
            filteredTotalInterval = ((filteredTotalInterval * LOAD_FILTER_SCALER) + elapsed)
                    >> LOAD_FILTER_SHIFT;
        }
        previousBeginTime = now;
    }

    public void stop() {
        if (mAudioTrack != null) {
            mAudioTrack.stop();
            mAudioTrack.release();
            mAudioTrack = null;
        }
    }

    public int getFrameRate() {
        return mFrameRate;
    }

    public LatencyController getLatencyController() {
        return mLatencyController;
    }
}
