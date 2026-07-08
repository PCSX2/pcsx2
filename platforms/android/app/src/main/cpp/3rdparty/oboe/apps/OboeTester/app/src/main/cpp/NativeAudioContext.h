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

#ifndef NATIVEOBOE_NATIVEAUDIOCONTEXT_H
#define NATIVEOBOE_NATIVEAUDIOCONTEXT_H

#include <atomic>
#include <condition_variable>
#include <jni.h>
#include <mutex>
#include <sys/system_properties.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/OboeDebug.h"
#include "common/Trace.h"
#include "oboe/Oboe.h"

#include "aaudio/AAudioExtensions.h"
#include "AudioStreamGateway.h"
#include "SinkMemoryDirect.h"

#include "flowunits/ImpulseOscillator.h"
#include "flowgraph/ManyToMultiConverter.h"
#include "flowgraph/MonoToMultiConverter.h"
#include "flowgraph/RampLinear.h"
#include "flowgraph/SinkFloat.h"
#include "flowgraph/SinkI16.h"
#include "flowgraph/SinkI24.h"
#include "flowgraph/SinkI32.h"
#include "flowunits/ExponentialShape.h"
#include "flowunits/LinearShape.h"
#include "flowunits/SineOscillator.h"
#include "flowunits/SawtoothOscillator.h"
#include "flowunits/TriangleOscillator.h"
#include "flowunits/WhiteNoise.h"

#include "FullDuplexAnalyzer.h"
#include "FullDuplexEcho.h"
#include "analyzer/GlitchAnalyzer.h"
#include "analyzer/DataPathAnalyzer.h"
#include "InputStreamCallbackAnalyzer.h"
#include "MultiChannelRecording.h"
#include "NoisePulseGenerator.h"
#include "OboeStreamCallbackProxy.h"
#include "OboeTools.h"
#include "PlayRecordingCallback.h"
#include "SawPingGenerator.h"

// These must match order in strings.xml and in StreamConfiguration.java
#define NATIVE_MODE_UNSPECIFIED  0
#define NATIVE_MODE_OPENSLES     1
#define NATIVE_MODE_AAUDIO       2

#define MAX_SINE_OSCILLATORS     16
#define AMPLITUDE_SINE           1.0
#define AMPLITUDE_SAWTOOTH       0.5
#define FREQUENCY_SAW_PING       800.0
#define AMPLITUDE_SAW_PING       0.8
#define AMPLITUDE_NOISE_PULSE    0.8
#define AMPLITUDE_IMPULSE        0.7


#define SECONDS_TO_RECORD        10

/**
 * Abstract base class that corresponds to a test at the Java level.
 */
class ActivityContext {
public:

    ActivityContext();

    virtual ~ActivityContext() = default;

    std::shared_ptr<oboe::AudioStream> getStream(int32_t streamIndex) {
        auto it = mOboeStreams.find(streamIndex);
        if (it != mOboeStreams.end()) {
            return it->second;
        } else {
            return nullptr;
        }
    }

    void setPartialCallbackPercentage(int percentage) {
        if (percentage < 0 || percentage > 100) {
            // Ignoring the error, this only comes from the UI and must be valid value.
            return;
        }
        oboeCallbackProxy->setPartialDataCallbackPercentage(percentage);
    }

    virtual void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder);

    /**
     * Open a stream with the given parameters.
     * @param nativeApi
     * @param sampleRate
     * @param channelCount
     * @param channelMask
     * @param format
     * @param sharingMode
     * @param performanceMode
     * @param inputPreset
     * @param deviceId
     * @param sessionId
     * @param framesPerBurst
     * @param channelConversionAllowed
     * @param formatConversionAllowed
     * @param rateConversionQuality
     * @param isMMap
     * @param isInput
     * @param spatializationBehavior
     * @param packageName
     * @param attributionTag
     * @return stream ID
     */
    int open(jint nativeApi,
             jint sampleRate,
             jint channelCount,
             jint channelMask,
             jint format,
             jint sharingMode,
             jint performanceMode,
             jint inputPreset,
             jint usage,
             jint contentType,
             jint bufferCapacityInFrames,
             jint deviceId,
             jint sessionId,
             jboolean channelConversionAllowed,
             jboolean formatConversionAllowed,
             jint rateConversionQuality,
             jboolean isMMap,
             jboolean isInput,
             jint spatializationBehavior,
             const char *packageName,
             const char *attributionTag);

    oboe::Result release();

    virtual void close(int32_t streamIndex);

    virtual void configureAfterOpen() {}

    oboe::Result start();

    oboe::Result pause();

    oboe::Result flush();

    virtual int64_t flushFromFrame(int32_t accuracy, int64_t frame) {
        return static_cast<int64_t>(oboe::Result::ErrorUnimplemented);
    }

    oboe::Result stopAllStreams();

    virtual oboe::Result stop() {
        return stopAllStreams();
    }

    float getCpuLoad() {
        return oboeCallbackProxy->getCpuLoad();
    }

    float getAndResetMaxCpuLoad() {
        return oboeCallbackProxy->getAndResetMaxCpuLoad();
    }

    uint32_t getAndResetCpuMask() {
        return oboeCallbackProxy->getAndResetCpuMask();
    }

    std::string getCallbackTimeString() {
        return oboeCallbackProxy->getCallbackTimeString();
    }

    void setWorkload(int32_t workload) {
        oboeCallbackProxy->setWorkload(workload);
        bool traceEnabled = oboe::Trace::getInstance().isEnabled();
        if (traceEnabled) {
            oboe::Trace::getInstance().setCounter("Workload", workload);
        }
    }

    void setHearWorkload(bool enabled) {
        oboeCallbackProxy->setHearWorkload(enabled);
    }

    virtual oboe::Result startPlayback() {
        return oboe::Result::OK;
    }

    virtual oboe::Result stopPlayback() {
        return oboe::Result::OK;
    }

    virtual void runBlockingIO() {};

    static void threadCallback(ActivityContext *context) {
        context->runBlockingIO();
    }

    void stopBlockingIOThread();

    virtual double getPeakLevel(int index) {
        return 0.0;
    }

    static int64_t getNanoseconds(clockid_t clockId = CLOCK_MONOTONIC) {
        struct timespec time;
        int result = clock_gettime(clockId, &time);
        if (result < 0) {
            return result;
        }
        return (time.tv_sec * NANOS_PER_SECOND) + time.tv_nsec;
    }

    // Calculate time between beginning and when frame[0] occurred.
    int32_t calculateColdStartLatencyMillis(int32_t sampleRate,
                                            int64_t beginTimeNanos,
                                            int64_t timeStampPosition,
                                            int64_t timestampNanos) const {
        int64_t elapsedNanos = NANOS_PER_SECOND * (timeStampPosition / (double) sampleRate);
        int64_t timeOfFrameZero = timestampNanos - elapsedNanos;
        int64_t coldStartLatencyNanos = timeOfFrameZero - beginTimeNanos;
        return coldStartLatencyNanos / NANOS_PER_MILLISECOND;
    }

    int32_t getColdStartInputMillis() {
        std::shared_ptr<oboe::AudioStream> oboeStream = getInputStream();
        if (oboeStream != nullptr) {
            int64_t framesRead = oboeStream->getFramesRead();
            if (framesRead > 0) {
                // Base latency on the time that frame[0] would have been received by the app.
                int64_t nowNanos = getNanoseconds();
                return calculateColdStartLatencyMillis(oboeStream->getSampleRate(),
                                                       mInputOpenedAt,
                                                       framesRead,
                                                       nowNanos);
            }
        }
        return -1;
    }

    int32_t getColdStartOutputMillis() {
        std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
        if (oboeStream != nullptr) {
            auto result = oboeStream->getTimestamp(CLOCK_MONOTONIC);
            if (result) {
                auto frameTimestamp = result.value();
                // Calculate the time that frame[0] would have been played by the speaker.
                int64_t position = frameTimestamp.position;
                int64_t timestampNanos = frameTimestamp.timestamp;
                return calculateColdStartLatencyMillis(oboeStream->getSampleRate(),
                                                       mOutputOpenedAt,
                                                       position,
                                                       timestampNanos);
            }
        }
        return -1;
    }

    /**
     * Trigger a sound or impulse.
     * @param enabled
     */
    virtual void trigger() {}

    bool isMMapUsed(int32_t streamIndex);

    int32_t getFramesPerBlock() {
        return (callbackSize == 0) ? mFramesPerBurst : callbackSize;
    }

    int64_t getCallbackCount() {
        return oboeCallbackProxy->getCallbackCount();
    }

    oboe::Result getLastErrorCallbackResult() {
        std::shared_ptr<oboe::AudioStream> stream = getOutputStream();
        if (stream == nullptr) {
            stream = getInputStream();
        }
        return stream ? oboe::Result::ErrorNull : stream->getLastErrorCallbackResult();
    }

    int32_t getFramesPerCallback() {
        return oboeCallbackProxy->getFramesPerCallback();
    }

    virtual void setChannelEnabled(int channelIndex, bool enabled) {}

    virtual void setSignalType(int signalType) {}

    virtual void setAmplitude(float amplitude) {}

    virtual void setDuck(bool isDucked) {}

    virtual oboe::Result setPlaybackParameters(const oboe::PlaybackParameters& parameters) {
        return oboe::Result::ErrorUnimplemented;
    }

    virtual oboe::ResultWithValue<oboe::PlaybackParameters> getPlaybackParameters() {
        return oboe::ResultWithValue<oboe::PlaybackParameters>(oboe::Result::ErrorUnimplemented);
    }

    virtual int32_t saveWaveFile(const char *filename);

    virtual void setMinimumFramesBeforeRead(int32_t numFrames) {}

    static bool   mUseCallback;
    static bool   mUsePartialDataCallback;
    static int    callbackSize;

    double getTimestampLatency(int32_t streamIndex);

    void setCpuAffinityMask(uint32_t mask) {
        oboeCallbackProxy->setCpuAffinityMask(mask);
    }

    void setWorkloadReportingEnabled(bool enabled) {
        oboeCallbackProxy->setWorkloadReportingEnabled(enabled);
    }

    void setNotifyWorkloadIncreaseEnabled(bool enabled) {
        oboeCallbackProxy->setNotifyWorkloadIncreaseEnabled(enabled);
    }

    int32_t setBufferSizeInFrames(int streamIndex, int threshold);

    virtual void setupMemoryBuffer([[maybe_unused]] std::unique_ptr<uint8_t[]>& buffer,
                                   [[maybe_unused]] int length) {}

protected:
    std::shared_ptr<oboe::AudioStream> getInputStream();
    std::shared_ptr<oboe::AudioStream> getOutputStream();
    int32_t allocateStreamIndex();
    void freeStreamIndex(int32_t streamIndex);

    virtual void createRecording() {
        mRecording = std::make_unique<MultiChannelRecording>(mChannelCount,
                                                             SECONDS_TO_RECORD * mSampleRate);
    }

    virtual void finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) {}

    virtual oboe::Result startStreams() = 0;

    std::unique_ptr<float []>    dataBuffer{};

    AudioStreamGateway           audioStreamGateway;
    std::shared_ptr<OboeStreamCallbackProxy> oboeCallbackProxy;

    std::unique_ptr<MultiChannelRecording>  mRecording{};

    int32_t                      mNextStreamHandle = 0;
    std::unordered_map<int32_t, std::shared_ptr<oboe::AudioStream>>  mOboeStreams;
    int32_t                      mFramesPerBurst = 0; // TODO per stream
    int32_t                      mChannelCount = 0; // TODO per stream
    int32_t                      mSampleRate = 0; // TODO per stream
    std::atomic<int32_t>         mBufferSizeInFrames = 0; // TODO per stream

    std::atomic<bool>            threadEnabled{false};
    std::thread                 *dataThread = nullptr; // FIXME never gets deleted
    std::mutex                   threadLock;
    std::condition_variable      threadWorkCV;

private:
    int64_t mInputOpenedAt = 0;
    int64_t mOutputOpenedAt = 0;
};

/**
 * Test a single input stream.
 */
class ActivityTestInput : public ActivityContext {
public:

    ActivityTestInput() {}
    virtual ~ActivityTestInput() = default;

    void configureAfterOpen() override;

    double getPeakLevel(int index) override {
        return mInputAnalyzer.getPeakLevel(index);
    }

    void runBlockingIO() override;

    void setMinimumFramesBeforeRead(int32_t numFrames) override {
        mInputAnalyzer.setMinimumFramesBeforeRead(numFrames);
        mMinimumFramesBeforeRead = numFrames;
    }

    int32_t getMinimumFramesBeforeRead() const {
        return mMinimumFramesBeforeRead;
    }

protected:

    oboe::Result startStreams() override {
        mInputAnalyzer.reset();
        mInputAnalyzer.setup(std::max(getInputStream()->getFramesPerBurst(), callbackSize),
                             getInputStream()->getChannelCount(),
                             getInputStream()->getFormat());
        return getInputStream()->requestStart();
    }

    InputStreamCallbackAnalyzer  mInputAnalyzer;
    int32_t mMinimumFramesBeforeRead = 0;
};

/**
 * Record a configured input stream and play it back some simple way.
 */
class ActivityRecording : public ActivityTestInput {
public:

    ActivityRecording() {}
    virtual ~ActivityRecording() = default;

    oboe::Result stop() override {

        oboe::Result resultStopPlayback = stopPlayback();
        oboe::Result resultStopAudio = ActivityContext::stop();

        return (resultStopPlayback != oboe::Result::OK) ? resultStopPlayback : resultStopAudio;
    }

    oboe::Result startPlayback() override;

    oboe::Result stopPlayback() override;

    PlayRecordingCallback        mPlayRecordingCallback;
    oboe::AudioStream           *playbackStream = nullptr;

    struct RecordingStats {
        double peakAbs = 0.0;
        double sumSq   = 0.0;
        int64_t n      = 0;      // total samples
    };

    RecordingStats computeRecordingStats();

    ActivityRecording::RecordingStats getRecordingStats();
};

/**
 * Test a single output stream.
 */
class ActivityTestOutput : public ActivityContext {
public:
    ActivityTestOutput()
            : sineOscillators(MAX_SINE_OSCILLATORS)
            , sawtoothOscillators(MAX_SINE_OSCILLATORS) {}

    virtual ~ActivityTestOutput() = default;

    void close(int32_t streamIndex) override;

    oboe::Result startStreams() override;

    void configureAfterOpen() override;

    virtual void configureStreamGateway();

    void runBlockingIO() override;

    void setChannelEnabled(int channelIndex, bool enabled) override;

    // WARNING - must match order in strings.xml and OboeAudioOutputStream.java
    enum SignalType {
        Sine = 0,
        Sawtooth = 1,
        FreqSweep = 2,
        PitchSweep = 3,
        WhiteNoise = 4
    };

    void setSignalType(int signalType) override {
        mSignalType = (SignalType) signalType;
    }

    void setAmplitude(float amplitude) override {
        mAmplitude = amplitude;
        if (mVolumeRamp) {
            mVolumeRamp->setTarget(mAmplitude * mDuckingMultiplier);
        }
    }

    void setDuck(bool isDucked) override {
        mDuckingMultiplier = isDucked ? kDuckingVolumeMultiplier : kNormalVolumeMultiplier;
        setAmplitude(mAmplitude);
    }

    void setupMemoryBuffer(std::unique_ptr<uint8_t[]>& buffer, int length) final;

    int64_t flushFromFrame(int32_t accuracy, int64_t frame) final;

    oboe::Result setPlaybackParameters(const oboe::PlaybackParameters& parameters) final;

    oboe::ResultWithValue<oboe::PlaybackParameters> getPlaybackParameters() final;

protected:
    SignalType                       mSignalType = SignalType::Sine;

    std::vector<SineOscillator>      sineOscillators;
    std::vector<SawtoothOscillator>  sawtoothOscillators;
    static constexpr float           kSweepPeriod = 10.0; // for triangle up and down
    static constexpr float           kDuckingVolumeMultiplier = 0.2f; // volume multiplier when ducking
    static constexpr float           kNormalVolumeMultiplier = 1.0f; // when not ducking

    // A triangle LFO is shaped into either a linear or an exponential range for sweep.
    TriangleOscillator               mTriangleOscillator;
    LinearShape                      mLinearShape;
    ExponentialShape                 mExponentialShape;
    class WhiteNoise                 mWhiteNoise;

    static constexpr int             kRampMSec = 10; // for volume control
    float                            mAmplitude = 1.0f;
    float                            mDuckingMultiplier = kNormalVolumeMultiplier;
    std::shared_ptr<RampLinear> mVolumeRamp;

    std::unique_ptr<ManyToMultiConverter>   manyToMulti;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::shared_ptr<oboe::flowgraph::SinkFloat>   mSinkFloat;
    std::shared_ptr<oboe::flowgraph::SinkI16>     mSinkI16;
    std::shared_ptr<oboe::flowgraph::SinkI24>     mSinkI24;
    std::shared_ptr<oboe::flowgraph::SinkI32>     mSinkI32;
    std::shared_ptr<SinkMemoryDirect>             mSinkMemoryDirect;
};

/**
 * Generate a short beep with a very short attack.
 * This is used by Java to measure output latency.
 */
class ActivityTapToTone : public ActivityTestOutput {
public:
    ActivityTapToTone() {}
    virtual ~ActivityTapToTone() = default;

    void configureAfterOpen() override;

    void trigger() override {
        if (mUseNoisePulse) {
            mNoisePulseGenerator.trigger();
        } else {
            mSawPingGenerator.trigger();
        }
    }

    void useNoisePulse(bool enabled) {
        mUseNoisePulse = enabled;
    }

    bool                         mUseNoisePulse;
    SawPingGenerator             mSawPingGenerator;
    NoisePulseGenerator          mNoisePulseGenerator;
};

/**
 * Activity that uses synchronized input/output streams.
 */
class ActivityFullDuplex : public ActivityContext {
public:

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    virtual int32_t getState() { return -1; }
    virtual int32_t getResult() { return -1; }
    virtual bool isAnalyzerDone() { return false; }

    void setMinimumFramesBeforeRead(int32_t numFrames) override {
        getFullDuplexAnalyzer()->setMinimumFramesBeforeRead(numFrames);
    }

    virtual FullDuplexAnalyzer *getFullDuplexAnalyzer() = 0;

    int32_t getResetCount() {
        auto analyzer = getFullDuplexAnalyzer();
        if (analyzer == nullptr) {
            return -1;
        }
        auto processor = analyzer->getLoopbackProcessor();
        if (processor == nullptr) {
            return -1;
        }
        return processor->getResetCount();
    }

protected:
    void createRecording() override {
        mRecording = std::make_unique<MultiChannelRecording>(2, // output and input
                                                             SECONDS_TO_RECORD * mSampleRate);
    }
};

/**
 * Echo input to output through a delay line.
 */
class ActivityEcho : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexEcho->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    void setDelayTime(double delayTimeSeconds) {
        if (mFullDuplexEcho) {
            mFullDuplexEcho->setDelayTime(delayTimeSeconds);
        }
    }

    double getPeakLevel(int index) override {
        return mFullDuplexEcho->getPeakLevel(index);
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexEcho.get();
    }

protected:
    void finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) override;

private:
    std::unique_ptr<FullDuplexEcho>   mFullDuplexEcho{};
};

/**
 * Measure Round Trip Latency
 */
class ActivityRoundTripLatency : public ActivityFullDuplex {
public:
    ActivityRoundTripLatency() {
#define USE_WHITE_NOISE_ANALYZER 1
#if USE_WHITE_NOISE_ANALYZER
        // New analyzer that uses a short pattern of white noise bursts.
        mLatencyAnalyzer = std::make_unique<WhiteNoiseLatencyAnalyzer>();
#else
        // Old analyzer based on encoded random bits.
        mLatencyAnalyzer = std::make_unique<EncodedRandomLatencyAnalyzer>();
#endif
        mLatencyAnalyzer->setup();
    }
    virtual ~ActivityRoundTripLatency() = default;

    oboe::Result startStreams() override {
        mAnalyzerLaunched = false;
        return mFullDuplexLatency->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    LatencyAnalyzer *getLatencyAnalyzer() {
        return mLatencyAnalyzer.get();
    }

    int32_t getState() override {
        return getLatencyAnalyzer()->getState();
    }

    int32_t getResult() override {
        return getLatencyAnalyzer()->getState(); // TODO This does not look right.
    }

    bool isAnalyzerDone() override {
        if (!mAnalyzerLaunched) {
            mAnalyzerLaunched = launchAnalysisIfReady();
        }
        return mLatencyAnalyzer->isDone();
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexLatency.get();
    }

    static void analyzeData(LatencyAnalyzer *analyzer) {
        analyzer->analyze();
    }

    bool launchAnalysisIfReady() {
        // Are we ready to do the analysis?
        if (mLatencyAnalyzer->hasEnoughData()) {
            // Crunch the numbers on a separate thread.
            std::thread t(analyzeData, mLatencyAnalyzer.get());
            t.detach();
            return true;
        }
        return false;
    }

    jdouble measureTimestampLatency();

protected:
    void finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexLatency{};

    std::unique_ptr<LatencyAnalyzer>  mLatencyAnalyzer;
    bool                              mAnalyzerLaunched = false;
};

/**
 * Measure Glitches
 */
class ActivityGlitches : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexGlitches->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    GlitchAnalyzer *getGlitchAnalyzer() {
        return &mGlitchAnalyzer;
    }

    int32_t getState() override {
        return getGlitchAnalyzer()->getState();
    }

    int32_t getResult() override {
        return getGlitchAnalyzer()->getResult();
    }

    bool isAnalyzerDone() override {
        return mGlitchAnalyzer.isDone();
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexGlitches.get();
    }

protected:
    void finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexGlitches{};
    GlitchAnalyzer  mGlitchAnalyzer;
};

/**
 * Measure Data Path
 */
class ActivityDataPath : public ActivityFullDuplex {
public:

    oboe::Result startStreams() override {
        return mFullDuplexDataPath->start();
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    void configureAfterOpen() override {
        // set buffer size
        std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
        int32_t capacityInFrames = outputStream->getBufferCapacityInFrames();
        int32_t burstInFrames = outputStream->getFramesPerBurst();
        int32_t capacityInBursts = capacityInFrames / burstInFrames;
        int32_t sizeInBursts = std::max(2, capacityInBursts / 2);
        // Set size of buffer to minimize underruns.
        auto result = outputStream->setBufferSizeInFrames(sizeInBursts * burstInFrames);
        static_cast<void>(result);  // Avoid unused variable.
        LOGD("ActivityDataPath: %s() capacity = %d, burst = %d, size = %d",
             __func__, capacityInFrames, burstInFrames, result.value());
    }

    DataPathAnalyzer *getDataPathAnalyzer() {
        return &mDataPathAnalyzer;
    }

    FullDuplexAnalyzer *getFullDuplexAnalyzer() override {
        return (FullDuplexAnalyzer *) mFullDuplexDataPath.get();
    }

protected:
    void finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) override;

private:
    std::unique_ptr<FullDuplexAnalyzer>   mFullDuplexDataPath{};

    DataPathAnalyzer  mDataPathAnalyzer;
};

/**
 * Test a single output stream.
 */
class ActivityTestDisconnect : public ActivityContext {
public:
    ActivityTestDisconnect();

    virtual ~ActivityTestDisconnect() = default;

    void close(int32_t streamIndex) override;

    oboe::Result startStreams() override {
        std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
        if (outputStream) {
            return outputStream->start();
        }

        std::shared_ptr<oboe::AudioStream> inputStream = getInputStream();
        if (inputStream) {
            return inputStream->start();
        }
        return oboe::Result::ErrorNull;
    }

    void configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) override;

    void configureAfterOpen() override;

    int32_t getRoutingChangedCount() {
        return mRoutingChangedCount;
    }

    void onRoutingChanged() {
        mRoutingChangedCount++;
    }

private:
    class TestRoutingCallback : public oboe::AudioStreamRoutingCallback {
    public:
        TestRoutingCallback(ActivityTestDisconnect *activity)
                : mActivity(activity) {}
        virtual ~TestRoutingCallback() = default;
        void onRoutingChanged(oboe::AudioStream* /* audioStream */,
                              const int32_t* /* deviceIds */,
                              int32_t /* numDevices */) override {
            mActivity->onRoutingChanged();
        }
    private:
        ActivityTestDisconnect *mActivity;
    };

    std::shared_ptr<TestRoutingCallback>    mRoutingCallback;
    std::atomic<int32_t>                    mRoutingChangedCount{0};
    std::unique_ptr<SineOscillator>         sineOscillator;
    std::unique_ptr<MonoToMultiConverter>   monoToMulti;
    std::shared_ptr<oboe::flowgraph::SinkFloat>   mSinkFloat;
};

/**
 * Global context for native tests.
 * Switch between various ActivityContexts.
 */
class NativeAudioContext {
public:

    ActivityContext *getCurrentActivity() {
        return currentActivity;
    };

    void setActivityType(int activityType) {
        mActivityType = (ActivityType) activityType;
        switch(mActivityType) {
            default:
            case ActivityType::Undefined:
            case ActivityType::TestOutput:
                currentActivity = &mActivityTestOutput;
                break;
            case ActivityType::TestInput:
                currentActivity = &mActivityTestInput;
                break;
            case ActivityType::TapToTone:
                currentActivity = &mActivityTapToTone;
                break;
            case ActivityType::RecordPlay:
                currentActivity = &mActivityRecording;
                break;
            case ActivityType::Echo:
                currentActivity = &mActivityEcho;
                break;
            case ActivityType::RoundTripLatency:
                currentActivity = &mActivityRoundTripLatency;
                break;
            case ActivityType::Glitches:
                currentActivity = &mActivityGlitches;
                break;
            case ActivityType::TestDisconnect:
                currentActivity = &mActivityTestDisconnect;
                break;
            case ActivityType::DataPath:
                currentActivity = &mActivityDataPath;
                break;
        }
    }

    void setDelayTime(double delayTimeMillis) {
        mActivityEcho.setDelayTime(delayTimeMillis);
    }

    ActivityTestOutput           mActivityTestOutput;
    ActivityTestInput            mActivityTestInput;
    ActivityTapToTone            mActivityTapToTone;
    ActivityRecording            mActivityRecording;
    ActivityEcho                 mActivityEcho;
    ActivityRoundTripLatency     mActivityRoundTripLatency;
    ActivityGlitches             mActivityGlitches;
    ActivityDataPath             mActivityDataPath;
    ActivityTestDisconnect       mActivityTestDisconnect;

private:

    // WARNING - must match definitions in TestAudioActivity.java
    enum ActivityType {
        Undefined = -1,
        TestOutput = 0,
        TestInput = 1,
        TapToTone = 2,
        RecordPlay = 3,
        Echo = 4,
        RoundTripLatency = 5,
        Glitches = 6,
        TestDisconnect = 7,
        DataPath = 8,
    };

    ActivityType                 mActivityType = ActivityType::Undefined;
    ActivityContext             *currentActivity = &mActivityTestOutput;
};

#endif //NATIVEOBOE_NATIVEAUDIOCONTEXT_H
