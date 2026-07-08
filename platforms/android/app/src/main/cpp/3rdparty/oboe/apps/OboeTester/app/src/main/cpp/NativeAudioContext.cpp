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

// Set to 1 for debugging race condition #1180 with mAAudioStream.
// See also AudioStreamAAudio.cpp in Oboe.
// This was left in the code so that we could test the fix again easily in the future.
// We could not trigger the race condition without adding these get calls and the sleeps.
#define DEBUG_CLOSE_RACE 0

#include <chrono>
#include <fstream>
#include <iostream>
#if DEBUG_CLOSE_RACE
#include <thread>
#endif // DEBUG_CLOSE_RACE
#include <vector>

#include "oboe/AudioClock.h"
#include "util/WaveFileWriter.h"

#include "NativeAudioContext.h"

using namespace oboe;

static oboe::AudioApi convertNativeApiToAudioApi(int nativeApi) {
    switch (nativeApi) {
        default:
        case NATIVE_MODE_UNSPECIFIED:
            return oboe::AudioApi::Unspecified;
        case NATIVE_MODE_AAUDIO:
            return oboe::AudioApi::AAudio;
        case NATIVE_MODE_OPENSLES:
            return oboe::AudioApi::OpenSLES;
    }
}

class MyOboeOutputStream : public WaveFileOutputStream {
public:
    void write(uint8_t b) override {
        mData.push_back(b);
    }

    int32_t length() {
        return (int32_t) mData.size();
    }

    uint8_t *getData() {
        return mData.data();
    }

private:
    std::vector<uint8_t> mData;
};

bool ActivityContext::mUseCallback = true;
bool ActivityContext::mUsePartialDataCallback = false;
int  ActivityContext::callbackSize = 0;

ActivityContext::ActivityContext() {
    oboeCallbackProxy = std::make_shared<OboeStreamCallbackProxy>();
}

std::shared_ptr<oboe::AudioStream> ActivityContext::getOutputStream() {
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        if (oboeStream->getDirection() == oboe::Direction::Output) {
            return oboeStream;
        }
    }
    return nullptr;
}

std::shared_ptr<oboe::AudioStream> ActivityContext::getInputStream() {
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        if (oboeStream != nullptr) {
            if (oboeStream->getDirection() == oboe::Direction::Input) {
                return oboeStream;
            }
        }
    }
    return nullptr;
}

void ActivityContext::freeStreamIndex(int32_t streamIndex) {
    mOboeStreams[streamIndex].reset();
    mOboeStreams.erase(streamIndex);
}

int32_t ActivityContext::allocateStreamIndex() {
    return mNextStreamHandle++;
}

oboe::Result ActivityContext::release() {
    oboe::Result result = oboe::Result::OK;
    stopBlockingIOThread();
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        result = oboeStream->release();
    }
    return result;
}

void ActivityContext::close(int32_t streamIndex) {
    stopBlockingIOThread();
    std::shared_ptr<oboe::AudioStream> oboeStream = getStream(streamIndex);
    if (oboeStream != nullptr) {
        oboeStream->close();
        LOGD("ActivityContext::%s() delete stream %d ", __func__, streamIndex);
        freeStreamIndex(streamIndex);
    }
}

bool ActivityContext::isMMapUsed(int32_t streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = getStream(streamIndex);
    if (oboeStream == nullptr) return false;
    if (oboeStream->getAudioApi() != AudioApi::AAudio) return false;
    return AAudioExtensions::getInstance().isMMapUsed(oboeStream.get());
}

oboe::Result ActivityContext::pause() {
    oboe::Result result = oboe::Result::OK;
    stopBlockingIOThread();
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        result = oboeStream->requestPause();
    }
    return result;
}

oboe::Result ActivityContext::stopAllStreams() {
    oboe::Result result = oboe::Result::OK;
    stopBlockingIOThread();
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        result = oboeStream->requestStop();
    }
    return result;
}

void ActivityContext::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    // We needed the proxy because we did not know the channelCount when we setup the Builder.
    if (mUseCallback) {
        if (mUsePartialDataCallback) {
            builder.setPartialDataCallback(
                    std::dynamic_pointer_cast<oboe::AudioStreamPartialDataCallback>(
                            oboeCallbackProxy));
        } else {
            builder.setDataCallback(oboeCallbackProxy);
        }
    }
}

int ActivityContext::open(jint nativeApi,
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
                          const char *attributionTag) {
    oboe::AudioApi audioApi = oboe::AudioApi::Unspecified;
    switch (nativeApi) {
        case NATIVE_MODE_UNSPECIFIED:
        case NATIVE_MODE_AAUDIO:
        case NATIVE_MODE_OPENSLES:
            audioApi = convertNativeApiToAudioApi(nativeApi);
            break;
        default:
            return (jint) oboe::Result::ErrorOutOfRange;
    }

    int32_t streamIndex = allocateStreamIndex();
    if (streamIndex < 0) {
        LOGE("ActivityContext::open() stream array full");
        return (jint) oboe::Result::ErrorNoFreeHandles;
    }

    if (channelCount < 0 || channelCount > 256) {
        LOGE("ActivityContext::open() channels out of range");
        return (jint) oboe::Result::ErrorOutOfRange;
    }

    // Create an audio stream.
    oboe::AudioStreamBuilder builder;
    builder.setChannelCount(channelCount)
            ->setDirection(isInput ? oboe::Direction::Input : oboe::Direction::Output)
            ->setSharingMode((oboe::SharingMode) sharingMode)
            ->setPerformanceMode((oboe::PerformanceMode) performanceMode)
            ->setInputPreset((oboe::InputPreset)inputPreset)
            ->setUsage((oboe::Usage)usage)
            ->setContentType((oboe::ContentType)contentType)
            ->setBufferCapacityInFrames(bufferCapacityInFrames)
            ->setDeviceId(deviceId)
            ->setSessionId((oboe::SessionId) sessionId)
            ->setSampleRate(sampleRate)
            ->setFormat((oboe::AudioFormat) format)
            ->setChannelConversionAllowed(channelConversionAllowed)
            ->setFormatConversionAllowed(formatConversionAllowed)
            ->setSampleRateConversionQuality((oboe::SampleRateConversionQuality) rateConversionQuality)
            ->setSpatializationBehavior((oboe::SpatializationBehavior) spatializationBehavior)
            ->setPackageName(packageName)
            ->setAttributionTag(attributionTag)
            ;
    if (channelMask != (jint) oboe::ChannelMask::Unspecified) {
        // Set channel mask when it is specified.
        builder.setChannelMask((oboe::ChannelMask) channelMask);
    }
    if (mUseCallback) {
        builder.setFramesPerCallback(callbackSize);
    }
    oboeCallbackProxy->setIsPartialDataCallback(mUsePartialDataCallback);
    configureBuilder(isInput, builder);

    builder.setAudioApi(audioApi);

    // Temporarily set the AAudio MMAP policy to disable MMAP or not.
    bool oldMMapEnabled = AAudioExtensions::getInstance().isMMapEnabled();
    AAudioExtensions::getInstance().setMMapEnabled(isMMap);

    // Record time for opening.
    if (isInput) {
        mInputOpenedAt = oboe::AudioClock::getNanoseconds();
    } else {
        mOutputOpenedAt = oboe::AudioClock::getNanoseconds();
    }
    // Open a stream based on the builder settings.
    std::shared_ptr<oboe::AudioStream> oboeStream;
    Result result = builder.openStream(oboeStream);
    AAudioExtensions::getInstance().setMMapEnabled(oldMMapEnabled);
    if (result != Result::OK) {
        freeStreamIndex(streamIndex);
        streamIndex = -1;
    } else {
        mOboeStreams[streamIndex] = oboeStream; // save shared_ptr

        mChannelCount = oboeStream->getChannelCount(); // FIXME store per stream
        mFramesPerBurst = oboeStream->getFramesPerBurst();
        mSampleRate = oboeStream->getSampleRate();
        mBufferSizeInFrames = oboeStream->getBufferSizeInFrames();

        createRecording();

        finishOpen(isInput, oboeStream);
    }

    if (!mUseCallback) {
        int numSamples = getFramesPerBlock() * mChannelCount;
        dataBuffer = std::make_unique<float[]>(numSamples);
    }

    if (result != Result::OK) {
        return (int) result;
    } else {
        configureAfterOpen();
        return streamIndex;
    }
}

oboe::Result ActivityContext::start() {
    oboe::Result result = oboe::Result::OK;
    std::shared_ptr<oboe::AudioStream> inputStream = getInputStream();
    std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
    if (inputStream == nullptr && outputStream == nullptr) {
        LOGD("%s() - no streams defined", __func__);
        return oboe::Result::ErrorInvalidState; // not open
    }

    audioStreamGateway.reset();
    result = startStreams();

    if (!mUseCallback && result == oboe::Result::OK) {
        // Instead of using the callback, start a thread that writes the stream.
        threadEnabled.store(true);
        dataThread = new std::thread(threadCallback, this);
    }

#if DEBUG_CLOSE_RACE
    // Also put a sleep for 400 msec in AudioStreamAAudio::updateFramesRead().
    if (outputStream != nullptr) {
        std::thread raceDebugger([outputStream]() {
            while (outputStream->getState() != StreamState::Closed) {
                int64_t framesRead = outputStream->getFramesRead();
                LOGD("raceDebugger, framesRead = %d, state = %d",
                     (int) framesRead, (int) outputStream->getState());
            }
        });
        raceDebugger.detach();
    }
#endif // DEBUG_CLOSE_RACE

    return result;
}

oboe::Result ActivityContext::flush() {
    oboe::Result result = oboe::Result::OK;
    for (auto entry : mOboeStreams) {
        std::shared_ptr<oboe::AudioStream> oboeStream = entry.second;
        result = oboeStream->requestFlush();
    }
    return result;
}

int32_t  ActivityContext::saveWaveFile(const char *filename) {
    if (mRecording == nullptr) {
        LOGW("ActivityContext::saveWaveFile(%s) but no recording!", filename);
        return -1;
    }
    if (mRecording->getSizeInFrames() == 0) {
        LOGW("ActivityContext::saveWaveFile(%s) but no frames!", filename);
        return -2;
    }
    MyOboeOutputStream outStream;
    WaveFileWriter writer(&outStream);
    // You must setup the format before the first write().
    writer.setFrameRate(mSampleRate);
    writer.setSamplesPerFrame(mRecording->getChannelCount());
    writer.setBitsPerSample(24);
    writer.setFrameCount(mRecording->getSizeInFrames());
    std::vector<float> buffer(mRecording->getChannelCount());
    // Read samples from start to finish.
    mRecording->rewind();
    for (int32_t frameIndex = 0; frameIndex < mRecording->getSizeInFrames(); frameIndex++) {
        mRecording->read(buffer.data(), 1 /* numFrames */);
        for (int32_t i = 0; i < mRecording->getChannelCount(); i++) {
            writer.write(buffer[i]);
        }
    }
    writer.close();

    if (outStream.length() > 0) {
        auto myfile = std::ofstream(filename, std::ios::out | std::ios::binary);
        myfile.write((char *) outStream.getData(), outStream.length());
        myfile.close();
    }

    return outStream.length();
}

double ActivityContext::getTimestampLatency(int32_t streamIndex) {
    std::shared_ptr<oboe::AudioStream> oboeStream = getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto result = oboeStream->calculateLatencyMillis();
        return (!result) ? -1.0 : result.value();
    }
    return -1.0;
}

void ActivityContext::stopBlockingIOThread() {
    if (dataThread != nullptr) {
        // stop a thread that runs in place of the callback
        threadEnabled.store(false); // ask thread to exit its loop
        std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
        if (oboeStream != nullptr &&
            oboeStream->getPerformanceMode() == PerformanceMode::PowerSavingOffloaded) {
            std::lock_guard _l(threadLock);
            threadWorkCV.notify_one();
        }
        dataThread->join();
        dataThread = nullptr;
    }
}

int32_t ActivityContext::setBufferSizeInFrames(int streamIndex, int threshold) {
    std::shared_ptr<oboe::AudioStream> oboeStream = getStream(streamIndex);
    if (oboeStream != nullptr) {
        auto result = oboeStream->setBufferSizeInFrames(threshold);
        if (result) {
            mBufferSizeInFrames = result.value();
        }
        return (!result) ? (int32_t) result.error() : result.value();
    }
    return (int32_t) oboe::Result::ErrorNull;
}

// =================================================================== ActivityTestOutput
void ActivityTestOutput::close(int32_t streamIndex) {
    ActivityContext::close(streamIndex);
    manyToMulti.reset(nullptr);
    monoToMulti.reset(nullptr);
    mVolumeRamp.reset();
    mSinkFloat.reset();
    mSinkI16.reset();
    mSinkI24.reset();
    mSinkI32.reset();
    mSinkMemoryDirect.reset();
}

void ActivityTestOutput::setChannelEnabled(int channelIndex, bool enabled) {
    if (manyToMulti == nullptr) {
        return;
    }
    if (enabled) {
        switch (mSignalType) {
            case SignalType::Sine:
                sineOscillators[channelIndex].frequency.disconnect();
                sineOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            case SignalType::Sawtooth:
                sawtoothOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            case SignalType::FreqSweep:
                mLinearShape.output.connect(&sineOscillators[channelIndex].frequency);
                sineOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            case SignalType::PitchSweep:
                mExponentialShape.output.connect(&sineOscillators[channelIndex].frequency);
                sineOscillators[channelIndex].output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            case SignalType::WhiteNoise:
                mWhiteNoise.output.connect(manyToMulti->inputs[channelIndex].get());
                break;
            default:
                break;
        }
    } else {
        manyToMulti->inputs[channelIndex]->disconnect();
    }
}

void ActivityTestOutput::configureAfterOpen() {
    manyToMulti = std::make_unique<ManyToMultiConverter>(mChannelCount);

    std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();

    mVolumeRamp = std::make_shared<RampLinear>(mChannelCount);
    mVolumeRamp->setLengthInFrames(kRampMSec * outputStream->getSampleRate() /
            MILLISECONDS_PER_SECOND);
    mVolumeRamp->setTarget(mAmplitude);

    mSinkFloat = std::make_shared<SinkFloat>(mChannelCount);
    mSinkI16 = std::make_shared<SinkI16>(mChannelCount);
    mSinkI24 = std::make_shared<SinkI24>(mChannelCount);
    mSinkI32 = std::make_shared<SinkI32>(mChannelCount);
    static constexpr int COMPRESSED_FORMAT_BYTES_PER_FRAME = 1;
    mSinkMemoryDirect = std::make_shared<SinkMemoryDirect>(
            mChannelCount, COMPRESSED_FORMAT_BYTES_PER_FRAME);

    mTriangleOscillator.setSampleRate(outputStream->getSampleRate());
    mTriangleOscillator.frequency.setValue(1.0/kSweepPeriod);
    mTriangleOscillator.amplitude.setValue(1.0);
    mTriangleOscillator.setPhase(-1.0);

    mLinearShape.setMinimum(0.0);
    mLinearShape.setMaximum(outputStream->getSampleRate() * 0.5); // Nyquist

    mExponentialShape.setMinimum(110.0);
    mExponentialShape.setMaximum(outputStream->getSampleRate() * 0.5); // Nyquist

    mTriangleOscillator.output.connect(&(mLinearShape.input));
    mTriangleOscillator.output.connect(&(mExponentialShape.input));
    {
        double frequency = 330.0;
        // Go up by a minor third or a perfect fourth just intoned interval.
        const float interval = (mChannelCount > 8) ? (6.0f / 5.0f) : (4.0f / 3.0f);
        for (int i = 0; i < mChannelCount; i++) {
            sineOscillators[i].setSampleRate(outputStream->getSampleRate());
            sineOscillators[i].frequency.setValue(frequency);
            sineOscillators[i].amplitude.setValue(AMPLITUDE_SINE / mChannelCount);
            sawtoothOscillators[i].setSampleRate(outputStream->getSampleRate());
            sawtoothOscillators[i].frequency.setValue(frequency);
            sawtoothOscillators[i].amplitude.setValue(AMPLITUDE_SAWTOOTH / mChannelCount);

            frequency *= interval; // each wave is at a higher frequency
            setChannelEnabled(i, true);
        }
    }

    mWhiteNoise.amplitude.setValue(0.5);

    manyToMulti->output.connect(&(mVolumeRamp.get()->input));

    mVolumeRamp->output.connect(&(mSinkFloat.get()->input));
    mVolumeRamp->output.connect(&(mSinkI16.get()->input));
    mVolumeRamp->output.connect(&(mSinkI24.get()->input));
    mVolumeRamp->output.connect(&(mSinkI32.get()->input));

    mSinkFloat->pullReset();
    mSinkI16->pullReset();
    mSinkI24->pullReset();
    mSinkI32->pullReset();
    mSinkMemoryDirect->pullReset();

    configureStreamGateway();
}

void ActivityTestOutput::configureStreamGateway() {
    std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
    if (outputStream->getFormat() == oboe::AudioFormat::I16) {
        audioStreamGateway.setAudioSink(mSinkI16);
    } else if (outputStream->getFormat() == oboe::AudioFormat::I24) {
        audioStreamGateway.setAudioSink(mSinkI24);
    } else if (outputStream->getFormat() == oboe::AudioFormat::I32) {
        audioStreamGateway.setAudioSink(mSinkI32);
    } else if (outputStream->getFormat() == oboe::AudioFormat::Float) {
        audioStreamGateway.setAudioSink(mSinkFloat);
    } else if (outputStream->getFormat() == oboe::AudioFormat::MP3) {
        audioStreamGateway.setAudioSink(mSinkMemoryDirect);
    }

    if (mUseCallback) {
        oboeCallbackProxy->setDataCallback(&audioStreamGateway);
    }
}

void ActivityTestOutput::runBlockingIO() {
    int32_t framesPerBlock = getFramesPerBlock();
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;

    std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
    if (oboeStream == nullptr) {
        LOGE("%s() : no stream found\n", __func__);
        return;
    }

    while (threadEnabled.load()
           && callbackResult == oboe::DataCallbackResult::Continue) {
        // generate output by calling the callback
        callbackResult = audioStreamGateway.onAudioReady(oboeStream.get(),
                                                         dataBuffer.get(),
                                                         framesPerBlock);

        auto result = oboeStream->write(dataBuffer.get(),
                                        framesPerBlock,
                                        NANOS_PER_SECOND);

        if (!result) {
            LOGE("%s() returned %s\n", __func__, convertToText(result.error()));
            break;
        }
        int32_t framesWritten = result.value();
        if (framesWritten < framesPerBlock) {
            LOGE("%s() : write() wrote %d of %d\n", __func__, framesWritten, framesPerBlock);
            break;
        }

        const int64_t bufferSizeInFrames = mBufferSizeInFrames.load();
        if (oboeStream->getPerformanceMode() == PerformanceMode::PowerSavingOffloaded &&
            bufferSizeInFrames > mSampleRate) {
            // If it is offload stream, the buffer size is more than 1 second and it is almost full,
            // sleep to drain most of the data to save battery and make sure the next write can
            // succeed on time. The one second here is a naive assumption that the OS won't suspend
            // if there are CPUs working within one second. It is usually longer than 1 second.
            // Use one second here as a minimum requirement.
            int64_t dataAvailable = oboeStream->getFramesWritten() - oboeStream->getFramesRead();
            if (dataAvailable > bufferSizeInFrames - mFramesPerBurst) {
                static const double kDataBufferFullRatio = 0.9f;
                int64_t drainNanos = dataAvailable * kDataBufferFullRatio * 1e9 / mSampleRate;
                std::unique_lock _l(threadLock);
                threadWorkCV.wait_for(_l, std::chrono::nanoseconds(drainNanos));
            }
        }
    }
}

oboe::Result ActivityTestOutput::startStreams() {
    mSinkFloat->pullReset();
    mSinkI16->pullReset();
    mSinkI24->pullReset();
    mSinkI32->pullReset();
    if (mSinkMemoryDirect != nullptr) {
        mSinkMemoryDirect->pullReset();
    }
    if (mVolumeRamp != nullptr) {
        mVolumeRamp->setTarget(mAmplitude);
    }
    return getOutputStream()->start();
}

void ActivityTestOutput::setupMemoryBuffer(std::unique_ptr<uint8_t[]> &buffer, int length) {
    if (mSinkMemoryDirect != nullptr) {
        mSinkMemoryDirect->setupMemoryBuffer(buffer, length);
    }
}

int64_t ActivityTestOutput::flushFromFrame(int32_t accuracy, int64_t frame) {
    std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
    if (oboeStream == nullptr) {
        return static_cast<int64_t>(oboe::Result::ErrorInvalidState);
    }
    const int64_t requestedFrames = frame;
    if (auto result = oboeStream->flushFromFrame(
            static_cast<oboe::FlushFromAccuracy>(accuracy), frame);
        result.error() != oboe::Result::OK) {
        LOGE("Failed to flushFromFrame(%d, %jd), error=%d, suggestedFrame=%jd",
             accuracy, requestedFrames, result.error(), frame);
        return static_cast<int64_t>(result.error());
    } else {
        LOGD("Successfully flushFromFrame(%d, %jd), actual flushed frame: %jd",
             accuracy, requestedFrames, result.value());
        return result.value();
    }
}

oboe::Result ActivityTestOutput::setPlaybackParameters(const oboe::PlaybackParameters& parameters) {
    std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
    if (oboeStream == nullptr) {
        return oboe::Result::ErrorInvalidState;
    }

    return oboeStream->setPlaybackParameters(parameters);
}

oboe::ResultWithValue<oboe::PlaybackParameters>  ActivityTestOutput::getPlaybackParameters() {
    std::shared_ptr<oboe::AudioStream> oboeStream = getOutputStream();
    if (oboeStream == nullptr) {
        return {oboe::Result::ErrorInvalidState};
    }
    return oboeStream->getPlaybackParameters();
}

// ======================================================================= ActivityTestInput
void ActivityTestInput::configureAfterOpen() {
    mInputAnalyzer.reset();
    if (mUseCallback) {
        oboeCallbackProxy->setDataCallback(&mInputAnalyzer);
    }
    mInputAnalyzer.setRecording(mRecording.get());
}

void ActivityTestInput::runBlockingIO() {
    int32_t framesPerBlock = getFramesPerBlock();
    oboe::DataCallbackResult callbackResult = oboe::DataCallbackResult::Continue;

    std::shared_ptr<oboe::AudioStream> oboeStream = getInputStream();
    if (oboeStream == nullptr) {
        LOGE("%s() : no stream found\n", __func__);
        return;
    }

    while (threadEnabled.load()
           && callbackResult == oboe::DataCallbackResult::Continue) {

        // Avoid glitches by waiting until there is extra data in the FIFO.
        auto err = oboeStream->waitForAvailableFrames(mMinimumFramesBeforeRead, kNanosPerSecond);
        if (!err) break;

        // read from input
        auto result = oboeStream->read(dataBuffer.get(),
                                       framesPerBlock,
                                       NANOS_PER_SECOND);
        if (!result) {
            LOGE("%s() : read() returned %s\n", __func__, convertToText(result.error()));
            break;
        }
        int32_t framesRead = result.value();
        if (framesRead < framesPerBlock) { // timeout?
            LOGE("%s() : read() read %d of %d\n", __func__, framesRead, framesPerBlock);
            break;
        }

        // analyze input
        callbackResult = mInputAnalyzer.onAudioReady(oboeStream.get(),
                                                     dataBuffer.get(),
                                                     framesRead);
    }
}

oboe::Result ActivityRecording::stopPlayback() {
    oboe::Result result = oboe::Result::OK;
    if (playbackStream != nullptr) {
        result = playbackStream->requestStop();
        playbackStream->close();
        mPlayRecordingCallback.setRecording(nullptr);
        delete playbackStream;
        playbackStream = nullptr;
    }
    return result;
}

oboe::Result ActivityRecording::startPlayback() {
    stop();
    oboe::AudioStreamBuilder builder;
    builder.setChannelCount(mChannelCount)
            ->setSampleRate(mSampleRate)
            ->setFormat(oboe::AudioFormat::Float)
            ->setCallback(&mPlayRecordingCallback);
    oboe::Result result = builder.openStream(&playbackStream);
    if (result != oboe::Result::OK) {
        delete playbackStream;
        playbackStream = nullptr;
    } else if (playbackStream != nullptr) {
        if (mRecording != nullptr) {
            mRecording->rewind();
            mPlayRecordingCallback.setRecording(mRecording.get());
            result = playbackStream->requestStart();
        }
    }
    return result;
}

ActivityRecording::RecordingStats ActivityRecording::computeRecordingStats() {
    constexpr int chunkFrames = 512;

    mRecording->rewind();

    const int ch = mRecording->getChannelCount();
    auto buffer = std::make_unique<float[]>(chunkFrames * ch);

    const int32_t totalFrames = mRecording->getSizeInFrames();
    int32_t frameIndex = 0;

    RecordingStats recordingStats;

    while (frameIndex < totalFrames) {
        const int framesToRead = std::min<int>(chunkFrames, totalFrames - frameIndex);
        const int framesRead = mRecording->read(buffer.get(), framesToRead);
        if (framesRead <= 0) break;

        const int samplesRead = framesRead * ch;

        for (int i = 0; i < samplesRead; ++i) {
            const double x = buffer[i];
            recordingStats.peakAbs = std::max(recordingStats.peakAbs, std::abs(x));
            recordingStats.sumSq += x * x;
        }
        recordingStats.n += samplesRead;

        frameIndex += framesRead;

        bool lastRemainingFramesRead = framesRead < framesToRead;
        if (lastRemainingFramesRead) break;
    }

    return recordingStats;
}

ActivityRecording::RecordingStats ActivityRecording::getRecordingStats() {
    return computeRecordingStats();
}


// ======================================================================= ActivityTapToTone
void ActivityTapToTone::configureAfterOpen() {
    monoToMulti = std::make_unique<MonoToMultiConverter>(mChannelCount);

    mSinkFloat = std::make_shared<SinkFloat>(mChannelCount);
    mSinkI16 = std::make_shared<SinkI16>(mChannelCount);
    mSinkI24 = std::make_shared<SinkI24>(mChannelCount);
    mSinkI32 = std::make_shared<SinkI32>(mChannelCount);

    std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();

    mNoisePulseGenerator.amplitude.setValue(AMPLITUDE_NOISE_PULSE);
    mSawPingGenerator.setSampleRate(outputStream->getSampleRate());
    mSawPingGenerator.frequency.setValue(FREQUENCY_SAW_PING);
    mSawPingGenerator.amplitude.setValue(AMPLITUDE_SAW_PING);

    if (mUseNoisePulse) {
        mNoisePulseGenerator.output.connect(&(monoToMulti->input));
    } else {
        mSawPingGenerator.output.connect(&(monoToMulti->input));
    }

    monoToMulti->output.connect(&(mSinkFloat.get()->input));
    monoToMulti->output.connect(&(mSinkI16.get()->input));
    monoToMulti->output.connect(&(mSinkI24.get()->input));
    monoToMulti->output.connect(&(mSinkI32.get()->input));

    mSinkFloat->pullReset();
    mSinkI16->pullReset();
    mSinkI24->pullReset();
    mSinkI32->pullReset();

    configureStreamGateway();
}

// ======================================================================= ActivityFullDuplex
void ActivityFullDuplex::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    if (isInput) {
        // Ideally the output streams should be opened first.
        std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
        if (outputStream != nullptr) {
            // The input and output buffers will run in sync with input empty
            // and output full. So set the input capacity to match the output.
            builder.setBufferCapacityInFrames(outputStream->getBufferCapacityInFrames());
        }
    }
}

// ======================================================================= ActivityEcho
void ActivityEcho::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexEcho.get() == nullptr) {
        mFullDuplexEcho = std::make_unique<FullDuplexEcho>();
    }
    // only output uses a callback, input is polled
    if (!isInput) {
        builder.setCallback((oboe::AudioStreamCallback *) oboeCallbackProxy.get());
        oboeCallbackProxy->setDataCallback(mFullDuplexEcho.get());
    }
}

void ActivityEcho::finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) {
    if (isInput) {
        mFullDuplexEcho->setSharedInputStream(oboeStream);
    } else {
        mFullDuplexEcho->setSharedOutputStream(oboeStream);
    }
}

// ======================================================================= ActivityRoundTripLatency
void ActivityRoundTripLatency::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexLatency.get() == nullptr) {
        mFullDuplexLatency = std::make_unique<FullDuplexAnalyzer>(mLatencyAnalyzer.get());
    }
    if (!isInput) {
        // only output uses a callback, input is polled
        builder.setCallback((oboe::AudioStreamCallback *) oboeCallbackProxy.get());
        oboeCallbackProxy->setDataCallback(mFullDuplexLatency.get());
    }
}

void ActivityRoundTripLatency::finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream>
        &oboeStream) {
    if (isInput) {
        mFullDuplexLatency->setSharedInputStream(oboeStream);
        mFullDuplexLatency->setRecording(mRecording.get());
    } else {
        mFullDuplexLatency->setSharedOutputStream(oboeStream);
    }
}

// The timestamp latency is the difference between the input
// and output times for a specific frame.
// Start with the position and time from an input timestamp.
// Map the input position to the corresponding position in output
// and calculate its time.
// Use the difference between framesWritten and framesRead to
// convert input positions to output positions.
jdouble ActivityRoundTripLatency::measureTimestampLatency() {
    if (!mFullDuplexLatency->isWriteReadDeltaValid()) return -1.0;

    int64_t writeReadDelta = mFullDuplexLatency->getWriteReadDelta();
    auto inputTimestampResult = mFullDuplexLatency->getInputStream()->getTimestamp(CLOCK_MONOTONIC);
    if (!inputTimestampResult) return -1.0;
    auto outputTimestampResult = mFullDuplexLatency->getOutputStream()->getTimestamp(CLOCK_MONOTONIC);
    if (!outputTimestampResult) return -1.0;

    int64_t inputPosition = inputTimestampResult.value().position;
    int64_t inputTimeNanos = inputTimestampResult.value().timestamp;
    int64_t ouputPosition = outputTimestampResult.value().position;
    int64_t outputTimeNanos = outputTimestampResult.value().timestamp;

    // Map input frame position to the corresponding output frame.
    int64_t mappedPosition = inputPosition + writeReadDelta;
    // Calculate when that frame will play.
    int32_t sampleRate = mFullDuplexLatency->getOutputStream()->getSampleRate();
    int64_t mappedTimeNanos = outputTimeNanos + ((mappedPosition - ouputPosition) * 1e9) / sampleRate;

    // Latency is the difference in time between when a frame was recorded and
    // when its corresponding echo was played.
    return (mappedTimeNanos - inputTimeNanos) * 1.0e-6; // convert nanos to millis
}

// ======================================================================= ActivityGlitches
void ActivityGlitches::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexGlitches.get() == nullptr) {
        mFullDuplexGlitches = std::make_unique<FullDuplexAnalyzer>(&mGlitchAnalyzer);
    }
    if (!isInput) {
        // only output uses a callback, input is polled
        builder.setCallback((oboe::AudioStreamCallback *) oboeCallbackProxy.get());
        oboeCallbackProxy->setDataCallback(mFullDuplexGlitches.get());
    }
}

void ActivityGlitches::finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) {
    if (isInput) {
        mFullDuplexGlitches->setSharedInputStream(oboeStream);
        mFullDuplexGlitches->setRecording(mRecording.get());
    } else {
        mFullDuplexGlitches->setSharedOutputStream(oboeStream);
    }
}

// ======================================================================= ActivityDataPath
void ActivityDataPath::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityFullDuplex::configureBuilder(isInput, builder);

    if (mFullDuplexDataPath.get() == nullptr) {
        mFullDuplexDataPath = std::make_unique<FullDuplexAnalyzer>(&mDataPathAnalyzer);
    }
    if (!isInput) {
        // only output uses a callback, input is polled
        builder.setCallback((oboe::AudioStreamCallback *) oboeCallbackProxy.get());
        oboeCallbackProxy->setDataCallback(mFullDuplexDataPath.get());
    }
}

void ActivityDataPath::finishOpen(bool isInput, std::shared_ptr<oboe::AudioStream> &oboeStream) {
    if (isInput) {
        mFullDuplexDataPath->setSharedInputStream(oboeStream);
        mFullDuplexDataPath->setRecording(mRecording.get());
    } else {
        mFullDuplexDataPath->setSharedOutputStream(oboeStream);
    }
}

// =================================================================== ActivityTestDisconnect
ActivityTestDisconnect::ActivityTestDisconnect() {
    mRoutingCallback = std::make_shared<TestRoutingCallback>(this);
}

void ActivityTestDisconnect::close(int32_t streamIndex) {
    ActivityContext::close(streamIndex);
    mSinkFloat.reset();
}

void ActivityTestDisconnect::configureBuilder(bool isInput, oboe::AudioStreamBuilder &builder) {
    ActivityContext::configureBuilder(isInput, builder);
    builder.setRoutingCallback(mRoutingCallback);
}

void ActivityTestDisconnect::configureAfterOpen() {
    mRoutingChangedCount = 0;
    std::shared_ptr<oboe::AudioStream> outputStream = getOutputStream();
    std::shared_ptr<oboe::AudioStream> inputStream = getInputStream();
    if (outputStream) {
        mSinkFloat = std::make_unique<SinkFloat>(mChannelCount);
        sineOscillator = std::make_unique<SineOscillator>();
        monoToMulti = std::make_unique<MonoToMultiConverter>(mChannelCount);

        sineOscillator->setSampleRate(outputStream->getSampleRate());
        sineOscillator->frequency.setValue(440.0);
        sineOscillator->amplitude.setValue(AMPLITUDE_SINE);
        sineOscillator->output.connect(&(monoToMulti->input));

        monoToMulti->output.connect(&(mSinkFloat->input));
        mSinkFloat->pullReset();
        audioStreamGateway.setAudioSink(mSinkFloat);
    } else if (inputStream) {
        audioStreamGateway.setAudioSink(nullptr);
    }
    oboeCallbackProxy->setDataCallback(&audioStreamGateway);
}

