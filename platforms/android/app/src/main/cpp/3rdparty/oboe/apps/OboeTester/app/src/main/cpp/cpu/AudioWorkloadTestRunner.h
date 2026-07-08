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

#ifndef AUDIO_WORKLOAD_TEST_RUNNER_H
#define AUDIO_WORKLOAD_TEST_RUNNER_H

#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include "AudioWorkloadTest.h"

/**
 * @class AudioWorkloadTestRunner
 * @brief Manages the execution and results of an AudioWorkloadTest.
 *
 * This class encapsulates an AudioWorkloadTest instance, controls its lifecycle (start, stop),
 * monitors its progress, and determines a pass/fail result based on criteria like XRuns.
 *
 * Example usage:
 * start();
 * while(!stopIfDone()) { getStatus(); sleep(); }
 * getResult(); getXRunCount();
 *
 */
class AudioWorkloadTestRunner {
public:
    /**
     * @brief Default constructor.
     */
    AudioWorkloadTestRunner() = default;

    /**
     * @brief Destructor. Ensures the test is stopped when the runner is destroyed.
     */
    ~AudioWorkloadTestRunner() {
        stop();
    }

    /**
     * @brief Starts the audio workload test with the specified parameters.
     * @param targetDurationMs The desired duration of the test in milliseconds.
     * @param numBursts The desired buffer size in terms of multiples of framesPerBurst.
     * @param numVoices The primary number of synthesizer voices to simulate.
     * @param alternateNumVoices An alternative number of voices for alternating workload. Set this
     * the same as numVoices if you don't want the workload to change.
     * @param alternatingPeriodMs The period in milliseconds to alternate between numVoices and
     * alternateNumVoices.
     * @param adpfEnabled Whether to enable Adaptive Performance (ADPF) hints.
     * @param adpfWorkloadIncreaseEnabled Whether to use ADPF setWorkloadIncrease() API.
     * @param hearWorkload If true, the synthesized audio will be audible; otherwise, it's
     * processed silently and a sine wave will be audible instead.
     * @return 0 on success, -1 on failure (e.g., test already running, error opening/starting
     * stream).
     */
    int32_t start(
            int32_t targetDurationMs,
            int32_t numBursts,
            int32_t numVoices,
            int32_t alternateNumVoices,
            int32_t alternatingPeriodMs,
            bool adpfEnabled,
            bool adpfWorkloadIncreaseEnabled,
            bool hearWorkload) {
        if (mIsRunning) {
            LOGE("Error: Test already running.");
            return -1;
        }

        if (mAudioWorkloadTest.open() != static_cast<int32_t>(oboe::Result::OK)) {
            mResult = -1;
            mIsDone = true;
            return -1;
        }

        mIsRunning = true;
        mIsDone = false;
        mResult = 0;

        int32_t result = mAudioWorkloadTest.start(
                targetDurationMs,
                numBursts,
                numVoices,
                alternateNumVoices,
                alternatingPeriodMs,
                adpfEnabled,
                adpfWorkloadIncreaseEnabled,
                hearWorkload);

        if (result != static_cast<int32_t>(oboe::Result::OK)) {
            mResult = -1;
            mIsDone = true;
            mIsRunning = false;
            mAudioWorkloadTest.close();
            return -1;
        }

        return 0;
    }

    /**
     * @brief Stops the test if it has completed its run.
     * This is useful for polling and cleaning up once the underlying test finishes due to duration.
     * @return True if the test is done (either stopped here or previously), false otherwise.
     */
    bool stopIfDone() {
        if (!mIsDone && !mAudioWorkloadTest.isRunning()) {
            stop();
        }
        return mIsDone;
    }

    /**
     * @brief Gets the current status of the test.
     * This can indicate if it's running, how many callbacks have occurred, or the final result text.
     * @return A string describing the current status.
     */
    std::string getStatus() const {
        if (!mIsRunning) {
            if (mResult == -1) {
                return "FAIL: Encountered " + std::to_string(getXRunCount()) + " xruns.";
            } else if (mResult == 1) {
                return "PASS: No xruns encountered.";
            } else {
                return "Unknown result.";
            }
        }
        int32_t callbacksCompleted = mAudioWorkloadTest.getCallbackCount();
        return "Running: " + std::to_string(callbacksCompleted)
               + " callbacks completed. XRuns: "
               + std::to_string(mAudioWorkloadTest.getXRunCount());
    }

    /**
     * @brief Stops the audio workload test explicitly.
     * This will also finalize the test result.
     * @return 0 if the test was running and is now stopped, -1 if it was not running.
     */
    int32_t stop() {
        if (mIsRunning) {
            mAudioWorkloadTest.stop();
            mAudioWorkloadTest.close();
            mIsRunning = false;

            int32_t xRunCount = mAudioWorkloadTest.getXRunCount();
            if (xRunCount > 0) {
                mResult = -1;
            } else {
                mResult = 1;
            }
            mIsDone = true;
            return 0;
        }
        return -1; // Not running
    }

    /**
     * @brief Gets the numerical result of the test.
     * @return 0 if the test is not finished, 1 if it passed, -1 if it failed.
     */
    int32_t getResult() const {
        return mResult;
    }

    /**
     * @brief Gets a descriptive string for the test result (e.g., "PASS", "FAIL: X xruns").
     * @return A string containing the result text.
     */
    int32_t getXRunCount() const {
        return mAudioWorkloadTest.getXRunCount();
    }

private:
    AudioWorkloadTest mAudioWorkloadTest;    // The actual audio workload test instance
    std::atomic<bool> mIsRunning{false};
    std::atomic<bool> mIsDone{true};
    std::atomic<int32_t> mResult{0};
};

#endif // AUDIO_WORKLOAD_TEST_RUNNER_H
