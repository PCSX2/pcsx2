/*
 * Copyright 2025 The Android Open Source Project
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

#ifndef AUDIO_WORKLOAD_TEST_H
#define AUDIO_WORKLOAD_TEST_H

#include <oboe/Oboe.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>
#include <unistd.h> // For CPU affinity
#include <mutex>
#include "SynthWorkload.h"

/**
 * @class AudioWorkloadTest
 * @brief A class designed to test audio workload performance using the Oboe library.
 *
 * This class sets up an audio stream, generates a synthetic audio load (sine wave and/or
 * a more complex synth workload), and collects statistics about the audio callback performance,
 * such as callback duration, XRun counts, and CPU usage.
 *
 * Example usage:
 * open();
 * start();
 * while (isRunning()) { getXRunCount(); getCallbackCount(); getLastDurationNs(); sleep(); }
 * getCallbackStatistics();
 * close();
 *
 */
class AudioWorkloadTest : oboe::AudioStreamDataCallback {
public:
    /**
     * @struct CallbackStatus
     * @brief Structure to store statistics for each audio callback invocation.
     */
    struct CallbackStatus {
        int32_t numVoices;      // Number of synthesizer voices active during this callback
        int64_t beginTimeNs;    // Timestamp (nanoseconds) when the callback started
        int64_t finishTimeNs;   // Timestamp (nanoseconds) when the callback finished
        int32_t xRunCount;      // Cumulative XRun (underrun/overrun) count at this point
        int32_t cpuIndex;       // CPU core index on which the callback executed
    };

    /**
     * @brief Constructor for AudioWorkloadTest.
     * Initializes the audio stream pointer to nullptr.
     */
    AudioWorkloadTest() = default;

    /**
     * @brief Destructor. Close the stream when this class is destroyed.
     */
    ~AudioWorkloadTest() {
        close();
    }

    /**
     * @brief Opens a float stereo audio stream.
     * Configures the stream for low latency output.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t open() {
        std::lock_guard<std::mutex> lock(mStreamLock);
        if (mStream) {
            LOGE("Error: Stream already open.");
            return static_cast<int32_t>(oboe::Result::ErrorUnavailable);
        }

        oboe::AudioStreamBuilder builder;
        builder.setDirection(oboe::Direction::Output);
        builder.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        builder.setSharingMode(oboe::SharingMode::Exclusive);
        builder.setFormat(oboe::AudioFormat::Float);
        builder.setChannelCount(2);
        builder.setDataCallback(this);

        oboe::Result result = builder.openStream(mStream);
        if (result != oboe::Result::OK) {
            LOGE("Error opening stream: %s", oboe::convertToText(result));
            return static_cast<int32_t>(result);
        }

        mFramesPerBurst = mStream->getFramesPerBurst();
        mSampleRate = mStream->getSampleRate();
        mPreviousXRunCount = 0;
        mXRunCount = 0;
        mPhaseIncrement = 2.0f * (float) M_PI * 440.0f / mSampleRate; // 440 Hz sine wave
        mSynthWorkload = std::make_unique<SynthWorkload>((int) 0.2 * mSampleRate, (int) 0.3 * mSampleRate);

        return 0;
    }

    /**
     * @brief Gets the number of frames processed in a single audio callback burst.
     * @return The number of frames per burst.
     */
    int32_t getFramesPerBurst() const {
        return mFramesPerBurst;
    }

    /**
     * @brief Gets the sample rate of the audio stream.
     * @return The sample rate in Hz.
     */
    int32_t getSampleRate() const {
        return mSampleRate;
    }

    /**
     * @brief Gets the current buffer size of the audio stream in frames.
     * @return The buffer size in frames.
     */
    int32_t getBufferSizeInFrames() const {
        return mBufferSizeInFrames;
    }

    /**
     * @brief Starts the audio stream and the workload test.
     * @param targetDurationMillis The desired duration of the test in milliseconds.
     * @param numBursts The desired buffer size in terms of multiples of framesPerBurst.
     * @param numVoices The primary number of synthesizer voices to simulate.
     * @param alternateNumVoices An alternative number of voices for alternating workload. Set this
     * the same as numVoices if you don't want the workload to change.
     * @param alternatingPeriodMs The period in milliseconds to alternate between numVoices and
     * alternateNumVoices.
     * @param adpfEnabled Whether to enable Adaptive Performance (ADPF) hints.
     * @param adpfWorkloadIncreaseEnabled Whether to use ADPF setWorkloadIncrease() API.
     * @param hearWorkload If true, the synthesized audio will be audible; otherwise, it's processed
     * silently and a sine wave will be audible instead.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t start(int32_t targetDurationMillis, int32_t numBursts, int32_t numVoices,
                  int32_t alternateNumVoices, int32_t alternatingPeriodMs, bool adpfEnabled,
                  bool adpfWorkloadIncreaseEnabled, bool hearWorkload) {
        std::lock_guard<std::mutex> lock(mStreamLock);
        if (!mStream) {
            LOGE("Error: Stream not open.");
            return static_cast<int32_t>(oboe::Result::ErrorInvalidState);
        }
        if (mRunning) {
            LOGE("Error: Stream already started.");
            return static_cast<int32_t>(oboe::Result::ErrorUnavailable);
        }
        mTargetDurationMs = targetDurationMillis;
        mNumBursts = numBursts;
        mNumVoices = numVoices;
        mAlternateNumVoices = alternateNumVoices;
        mAlternatingPeriodMs = alternatingPeriodMs;
        mStartTimeMs = 0;
        {
            std::lock_guard<std::mutex> statisticsLock(mStatisticsLock);
            mCallbackStatistics.clear();
        }
        mCallbackCount = 0;
        mPreviousXRunCount = mXRunCount.load();
        mXRunCount = 0;
        mRunning = true;
        mHearWorkload = hearWorkload;
        mAdpfWorkloadIncreaseEnabled = adpfWorkloadIncreaseEnabled;
        mStream->setPerformanceHintEnabled(adpfEnabled);
        mStream->setBufferSizeInFrames(mNumBursts * mFramesPerBurst);
        mBufferSizeInFrames = mStream->getBufferSizeInFrames();
        oboe::Result result = mStream->start();
        if (result != oboe::Result::OK) {
            mRunning = false;
            LOGE("Error starting stream: %s", oboe::convertToText(result));
            return static_cast<int32_t>(result);
        }

        return 0;
    }

    /**
     * @brief Gets the number of available CPU cores on the system.
     * @return The number of CPU cores.
     */
    static int32_t getCpuCount() {
        return sysconf(_SC_NPROCESSORS_CONF);
    }

    /**
     * @brief Sets the CPU affinity for the current thread (intended for the audio callback
     * thread).
     * @param mask A bitmask specifying the allowed CPU cores.
     * @return 0 on success, -1 on failure.
     */
    static int32_t setCpuAffinityForCallback(uint32_t mask) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (uint32_t i = 0; i < 32; ++i) {
            if ((mask >> i) & 1) {
                CPU_SET(i, &cpuset);
            }
        }

        if (sched_setaffinity(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
            LOGE("Error setting CPU affinity.");
            return -1;
        }
        return 0;
    }

    /**
     * @brief Gets the number of XRuns (underruns/overruns) that occurred during the last test run.
     * @return The XRun count.
     */
    int32_t getXRunCount() const {
        return mXRunCount - mPreviousXRunCount;
    }

    /**
     * @brief Gets the total number of audio callbacks invoked during the last test run.
     * @return The callback count.
     */
    int32_t getCallbackCount() const {
        return mCallbackCount;
    }

    /**
     * @brief Gets the duration of the last audio callback in nanoseconds.
     * @return The duration in nanoseconds.
     */
    int64_t getLastDurationNs() {
        return mLastDurationNs;
    }

    /**
     * @brief Checks if the audio workload test is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() {
        return mRunning;
    }

    /**
     * @brief Stops the audio stream.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t stop() {
        std::lock_guard<std::mutex> lock(mStreamLock);
        if (mStream) {
            oboe::Result result = mStream->requestStop();
            if (result != oboe::Result::OK) {
                LOGE("Error stopping stream: %s", oboe::convertToText(result));
                return static_cast<int32_t>(result);
            }
            oboe::StreamState next;
            result = mStream->waitForStateChange(oboe::StreamState::Stopping, &next, kTimeoutInNanos);
            if (result != oboe::Result::OK) {
                LOGE("Error while waiting for stream to stop: %s", oboe::convertToText(result));
                return static_cast<int32_t>(result);
            }
            if (next != oboe::StreamState::Stopped) {
                LOGE("Error: Stream did not stop. State: %s", oboe::convertToText(next));
                return static_cast<int32_t>(oboe::Result::ErrorInvalidState);
            }
        }
        mRunning = false;
        return 0;
    }

    /**
     * @brief Closes the audio stream.
     * @return 0 on success, or a negative Oboe error code on failure.
     */
    int32_t close() {
        std::lock_guard<std::mutex> lock(mStreamLock);
        if (mStream) {
            oboe::Result result = mStream->close();
            mStream = nullptr;
            if (result != oboe::Result::OK) {
                LOGE("Error closing stream: %s", oboe::convertToText(result));
                return static_cast<int32_t>(result);
            }
        }
        mRunning = false;
        return 0;
    }

    /**
     * @brief Retrieves the collected statistics for each audio callback.
     * Call this only after the stream is stopped as this is not atomic.
     * @return A vector of CallbackStatus structures.
     */
    std::vector<CallbackStatus> getCallbackStatistics() {
        std::lock_guard<std::mutex> statisticsLock(mStatisticsLock);
        return mCallbackStatistics;
    }

    /**
     * @brief The Oboe audio callback function.
     * This function is called by the Oboe library when it needs more audio data.
     * It generates audio, performs workload simulation, and collects statistics.
     * @param audioStream Pointer to the Oboe audio stream.
     * @param audioData Pointer to the buffer where audio data should be written.
     * @param numFrames The number of audio frames to be filled.
     * @return oboe::DataCallbackResult::Continue to continue streaming, or
     * oboe::DataCallbackResult::Stop to stop.
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* audioStream, void* audioData,
                                          int32_t numFrames) override {
        int64_t beginTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        int lastVoices = mNumVoices;
        int currentVoices = mNumVoices;
        if (mAlternatingPeriodMs > 0) {
            int64_t timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
            if (mStartTimeMs == 0) {
                mStartTimeMs = timeMs;
            }
            if (((timeMs - mStartTimeMs) % (2 * mAlternatingPeriodMs)) >= mAlternatingPeriodMs) {
                currentVoices = mAlternateNumVoices;
            }
        }

        if (mAdpfWorkloadIncreaseEnabled) {
            if (currentVoices > lastVoices) {
                audioStream->notifyWorkloadIncrease(true /* cpu */, false /* gpu */,
                                                    kTestName.c_str());
            } else if (currentVoices < lastVoices) {
                audioStream->notifyWorkloadReset(true /* cpu */, false /* gpu */,
                                                    kTestName.c_str());
            }
        }

        auto floatData = static_cast<float *>(audioData);
        int channelCount = audioStream->getChannelCount();

        // Fill buffer with a sine wave.
        for (int i = 0; i < numFrames; i++) {
            float value = sinf(mPhase) * 0.2f;
            for (int j = 0; j < channelCount; j++) {
                *floatData++ = value;
            }
            mPhase = mPhase + mPhaseIncrement;
            // Wrap the phase around in a circle.
            if (mPhase >= M_PI) mPhase = mPhase - 2.0f * M_PI;
        }

        if (mSynthWorkload) {
            mSynthWorkload->onCallback(currentVoices);
            if (currentVoices > 0) {
                // Render synth workload into the buffer or discard the synth voices.
                float *buffer = (audioStream->getChannelCount() == 2 && mHearWorkload)
                                ? static_cast<float *>(audioData) : nullptr;
                mSynthWorkload->renderStereo(buffer, numFrames);
            }
        }

        int64_t finishTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        mXRunCount = audioStream->getXRunCount().value();

        CallbackStatus status{};
        status.numVoices = currentVoices;
        status.beginTimeNs = beginTimeNs;
        status.finishTimeNs = finishTimeNs;
        status.xRunCount = mXRunCount - mPreviousXRunCount;
        status.cpuIndex = sched_getcpu();

        {
            std::lock_guard<std::mutex> statisticsLock(mStatisticsLock);
            mCallbackStatistics.push_back(status);
        }
        mCallbackCount++;
        mLastDurationNs = finishTimeNs - beginTimeNs;

        int64_t currentTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        if (currentTimeMs - mStartTimeMs > mTargetDurationMs) {
            mRunning = false;
            return oboe::DataCallbackResult::Stop;
        }

        return oboe::DataCallbackResult::Continue;
    }

private:
    const std::string kTestName = "AudioWorkloadTest";

    static constexpr int kTimeoutInNanos = 500 * oboe::kNanosPerMillisecond;

    // Lock for protecting mStream
    std::mutex                         mStreamLock;
    std::shared_ptr<oboe::AudioStream> mStream; // Pointer to the Oboe audio stream instance

    // Atomic variables for thread-safe access from audio callback and other threads
    std::atomic<int32_t> mFramesPerBurst{0};
    std::atomic<int32_t> mSampleRate{0};
    std::atomic<int32_t> mCallbackCount{0};
    std::atomic<int32_t> mPreviousXRunCount{0};
    std::atomic<int32_t> mXRunCount{0};
    std::atomic<int32_t> mTargetDurationMs{0};
    std::atomic<int32_t> mNumBursts{0};
    std::atomic<int32_t> mBufferSizeInFrames{0};
    std::atomic<int32_t> mNumVoices{0};
    std::atomic<int32_t> mAlternateNumVoices{0};
    std::atomic<int32_t> mAlternatingPeriodMs{0};
    std::atomic<int64_t> mLastDurationNs{0};
    std::atomic<int64_t> mStartTimeMs{0};
    std::atomic<bool> mHearWorkload{false};
    std::atomic<bool> mAdpfWorkloadIncreaseEnabled{false};

    // Lock to protect mCallbackStatistics
    std::mutex mStatisticsLock;
    std::vector<CallbackStatus> mCallbackStatistics;

    std::atomic<bool> mRunning{false};

    // Sine wave generation parameters
    std::atomic<float> mPhase{0.0f};           // Current phase of the sine wave oscillator
    std::atomic<float> mPhaseIncrement{0.0f};  // Phase increment for sine wave

    std::unique_ptr<SynthWorkload> mSynthWorkload; // Pointer to the synthetic workload generator
};

#endif // AUDIO_WORKLOAD_TEST_H
