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

import java.io.IOException;
import java.util.Locale;

/**
 * Base class for any audio input or output.
 */
public abstract class AudioStreamBase {

    private StreamConfiguration mRequestedStreamConfiguration;
    private StreamConfiguration mActualStreamConfiguration;
    private AudioStreamBase.DoubleStatistics mLatencyStatistics;
    private SampleRateMonitor mSampleRateMonitor = new SampleRateMonitor();
    private int mBufferSizeInFrames;

    private class SampleRateMonitor {
        private static final int SIZE = 16; // power of 2
        private static final long MASK = SIZE - 1L;
        private long[] times = new long[SIZE];
        private long[] frames = new long[SIZE];
        private long cursor;

        void add(long numFrames) {
            int index = (int) (cursor & MASK);
            frames[index] = numFrames;
            times[index] = System.currentTimeMillis();
            cursor++;
        }

        int getRate() {
            if (cursor < 2) return 0;
            long numValid = Math.min((long)SIZE, cursor);
            int oldestIndex = (int)((cursor - numValid) & MASK);
            int newestIndex = (int)((cursor - 1) & MASK);
            long deltaTime = times[newestIndex] - times[oldestIndex];
            long deltaFrames = frames[newestIndex] - frames[oldestIndex];
            if (deltaTime <= 0) {
                return -1;
            }
            long sampleRate = (deltaFrames * 1000) / deltaTime;
            return (int) sampleRate;
        }

        void reset() {
            cursor = 0;
        }
    }

    public StreamStatus getStreamStatus() {
        StreamStatus status = new StreamStatus();
        status.bufferSize = getBufferSizeInFrames();
        status.xRunCount = getXRunCount();
        status.framesRead = getFramesRead();
        status.framesWritten = getFramesWritten();
        status.callbackCount = getCallbackCount();
        status.latency = getLatency();
        mLatencyStatistics.add(status.latency);
        status.callbackTimeStr = getCallbackTimeStr();
        status.cpuLoad = getCpuLoad();
        status.state = getState();
        mSampleRateMonitor.add(status.framesRead);
        status.measuredRate = mSampleRateMonitor.getRate();
        return status;
    }

    public DoubleStatistics getLatencyStatistics() {
        return mLatencyStatistics;
    }

    public void setPerformanceHintEnabled(boolean checked) {
    }
    public void setHearWorkload(boolean checked) {
    }
    public int notifyWorkloadIncrease(boolean cpu, boolean gpu) {
        return -1;
    }
    public int notifyWorkloadReset(boolean cpu, boolean gpu) {
        return -1;
    }

    public static class DoubleStatistics {
        private double sum;
        private int count;
        private double minimum = Double.MAX_VALUE;
        private double maximum = Double.MIN_VALUE;

        void add(double statistic) {
            if (statistic <= 0.0) return;
            sum += statistic;
            count++;
            minimum = Math.min(statistic, minimum);
            maximum = Math.max(statistic, maximum);
        }

        double getAverage() {
            return sum / count;
        }

        public String dump() {
            if (count == 0) return "?";
            return String.format(Locale.getDefault(), "%3.1f/%3.1f/%3.1f ms", minimum, getAverage(), maximum);
        }
    }

    /**
     * Changes dynamic at run-time.
     */
    public static class StreamStatus {
        public int bufferSize;
        public int xRunCount;
        public long framesWritten;
        public long framesRead;
        public double latency; // msec
        public int state;
        public long callbackCount;
        public int framesPerCallback;
        public float cpuLoad;
        public String callbackTimeStr;
        public int measuredRate;

        // These are constantly changing.
        String dump(int framesPerBurst) {
            if (bufferSize < 0 || framesWritten < 0) {
                return "idle";
            }
            StringBuffer buffer = new StringBuffer();

            buffer.append("time between callbacks = " + callbackTimeStr + "\n");

            buffer.append("wr "
                    + String.format(Locale.getDefault(), "%Xh", framesWritten)
                    + " - rd " + String.format(Locale.getDefault(), "%Xh", framesRead)
                    + " = " + (framesWritten - framesRead) + " fr"
                    + ", SR = " + ((measuredRate <= 0) ? "?" : measuredRate) + "\n");

            String cpuLoadText = String.format(Locale.getDefault(), "%2d%c", (int)(cpuLoad * 100), '%');
            buffer.append(
                    convertStateToString(state)
                    + ", #cb=" + callbackCount
                    + ", f/cb=" + String.format(Locale.getDefault(), "%3d", framesPerCallback)
                    + ", " + cpuLoadText + " CPU"
                    + "\n");

            buffer.append("buffer size = ");
            if (bufferSize <= 0 || framesPerBurst <= 0) {
                buffer.append("?");
            } else {
                int numBuffers = bufferSize / framesPerBurst;
                int remainder = bufferSize - (numBuffers * framesPerBurst);
                buffer.append(bufferSize + " = (" + numBuffers + " * " + framesPerBurst + ") + " + remainder);
            }
            buffer.append(",   xRun# = " + ((xRunCount < 0) ? "?" : xRunCount));

            return buffer.toString();
        }
        /**
         * Converts ints from Oboe index to human-readable stream state
         */
        private String convertStateToString(int stateId) {
            final String[] STATE_ARRAY = {"Uninit.", "Unknown", "Open", "Starting", "Started",
                    "Pausing", "Paused", "Flushing", "Flushed",
                    "Stopping", "Stopped", "Closing", "Closed", "Disconn."};
            if (stateId < 0 || stateId >= STATE_ARRAY.length) {
                return "Invalid - " + stateId;
            }
            return STATE_ARRAY[stateId];
        }
    }

    /**
     *
     * @param requestedConfiguration
     * @param actualConfiguration
     * @param bufferSizeInFrames
     * @throws IOException
     */
    public void open(StreamConfiguration requestedConfiguration,
                     StreamConfiguration actualConfiguration,
                     int bufferSizeInFrames) throws IOException {
        mRequestedStreamConfiguration = requestedConfiguration;
        mActualStreamConfiguration = actualConfiguration;
        mBufferSizeInFrames = bufferSizeInFrames;
        mLatencyStatistics = new AudioStreamBase.DoubleStatistics();
    }

    public void onStart() {
        mSampleRateMonitor.reset();
    }
    public void onStop() {
        mSampleRateMonitor.reset();
    }

    public abstract boolean isInput();

    public void startPlayback() throws IOException {}

    public void stopPlayback() throws IOException {}

    public abstract void close();

    public int getFormat() {
        return mActualStreamConfiguration.getFormat();
    }

    public int getChannelCount() {
        return mActualStreamConfiguration.getChannelCount();
    }

    public int getSampleRate() {
        return mActualStreamConfiguration.getSampleRate();
    }

    public int getFramesPerBurst() {
        return mActualStreamConfiguration.getFramesPerBurst();
    }

    public int getHardwareChannelCount() {
        return mActualStreamConfiguration.getHardwareChannelCount();
    }

    public int getHardwareSampleRate() {
        return mActualStreamConfiguration.getHardwareSampleRate();
    }

    public int getHardwareFormat() {
        return mActualStreamConfiguration.getHardwareFormat();
    }

    public int getBufferCapacityInFrames() {
        return mBufferSizeInFrames;
    }

    public int getBufferSizeInFrames() {
        return mBufferSizeInFrames;
    }

    public int setBufferSizeInFrames(int bufferSize) {
        throw new UnsupportedOperationException("bufferSize cannot be changed");
    }

    public long getCallbackCount() { return -1; }

    public int getLastErrorCallbackResult() { return 0; }

    public long getFramesWritten() { return -1; }

    public long getFramesRead() { return -1; }

    public double getLatency() { return -1.0; }

    public float getCpuLoad() { return 0.0f; }
    public float getAndResetMaxCpuLoad() { return 0.0f; }
    public int getAndResetCpuMask() { return 0; }

    public String getCallbackTimeStr() { return "?"; };

    public int getState() { return -1; }

    public void setWorkload(int workload) {}

    public abstract int getXRunCount();

}
