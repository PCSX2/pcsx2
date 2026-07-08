/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef OBOE_STREAM_CALLBACK_H
#define OBOE_STREAM_CALLBACK_H

#include "oboe/Definitions.h"

namespace oboe {

class AudioStream;

/**
 * AudioStreamDataCallback defines a callback interface for
 * moving data to/from an audio stream using `onAudioReady`
 * 2) being alerted when a stream has an error using `onError*` methods
 *
 * It is used with AudioStreamBuilder::setDataCallback().
 */

class AudioStreamDataCallback {
public:
    virtual ~AudioStreamDataCallback() = default;

    /**
     * A buffer is ready for processing.
     *
     * For an output stream, this function should render and write numFrames of data
     * in the stream's current data format to the audioData buffer.
     *
     * For an input stream, this function should read and process numFrames of data
     * from the audioData buffer.
     *
     * The audio data is passed through the buffer. So do NOT call read() or
     * write() on the stream that is making the callback.
     *
     * Note that numFrames can vary unless AudioStreamBuilder::setFramesPerCallback()
     * is called. If AudioStreamBuilder::setFramesPerCallback() is NOT called then
     * numFrames should always be <= AudioStream::getFramesPerBurst().
     *
     * Also note that this callback function should be considered a "real-time" function.
     * It must not do anything that could cause an unbounded delay because that can cause the
     * audio to glitch or pop.
     *
     * These are things the function should NOT do:
     * <ul>
     * <li>allocate memory using, for example, malloc() or new</li>
     * <li>any file operations such as opening, closing, reading or writing</li>
     * <li>any network operations such as streaming</li>
     * <li>use any mutexes or other synchronization primitives</li>
     * <li>sleep</li>
     * <li>oboeStream->stop(), pause(), flush() or close()</li>
     * <li>oboeStream->read()</li>
     * <li>oboeStream->write()</li>
     * </ul>
     *
     * The following are OK to call from the data callback:
     * <ul>
     * <li>oboeStream->get*()</li>
     * <li>oboe::convertToText()</li>
     * <li>oboeStream->setBufferSizeInFrames()</li>
     * </ul>
     *
     * If you need to move data, eg. MIDI commands, in or out of the callback function then
     * we recommend the use of non-blocking techniques such as an atomic FIFO.
     *
     * @param audioStream pointer to the associated stream
     * @param audioData buffer containing input data or a place to put output data
     * @param numFrames number of frames to be processed
     * @return DataCallbackResult::Continue or DataCallbackResult::Stop
     */
    virtual DataCallbackResult onAudioReady(
            AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) = 0;
};

/**
 * AudioStreamErrorCallback defines a callback interface for
 * being alerted when a stream has an error or is disconnected
 * using `onError*` methods.
 *
 * Note: This callback is only fired when an AudioStreamCallback is set.
 * If you use AudioStream::write() you have to evaluate the return codes of
 * AudioStream::write() to notice errors in the stream.
 *
 * It is used with AudioStreamBuilder::setErrorCallback().
 */
class AudioStreamErrorCallback {
public:
    virtual ~AudioStreamErrorCallback() = default;

    /**
     * This will be called before other `onError` methods when an error occurs on a stream,
     * such as when the stream is disconnected.
     *
     * It can be used to override and customize the normal error processing.
     * Use of this method is considered an advanced technique.
     * It might, for example, be used if an app want to use a high level lock when
     * closing and reopening a stream.
     * Or it might be used when an app want to signal a management thread that handles
     * all of the stream state.
     *
     * If this method returns false it indicates that the stream has *not been stopped and closed
     * by the application. In this case it will be stopped by Oboe in the following way:
     * onErrorBeforeClose() will be called, then the stream will be closed and onErrorAfterClose()
     * will be closed.
     *
     * If this method returns true it indicates that the stream *has* been stopped and closed
     * by the application and Oboe will not do this.
     * In that case, the app MUST stop() and close() the stream.
     *
     * This method will be called on a thread created by Oboe.
     *
     * @param audioStream pointer to the associated stream
     * @param error
     * @return true if the stream has been stopped and closed, false if not
     */
    virtual bool onError(AudioStream* /* audioStream */, Result /* error */) {
        return false;
    }

    /**
     * This will be called when an error occurs on a stream,
     * such as when the stream is disconnected,
     * and if onError() returns false (indicating that the error has not already been handled).
     *
     * Note that this will be called on a thread created by Oboe.
     *
     * The underlying stream will already be stopped by Oboe but not yet closed.
     * So the stream can be queried.
     *
     * Do not close or delete the stream in this method because it will be
     * closed after this method returns.
     *
     * @param audioStream pointer to the associated stream
     * @param error
     */
    virtual void onErrorBeforeClose(AudioStream* /* audioStream */, Result /* error */) {}

    /**
     * This will be called when an error occurs on a stream,
     * such as when the stream is disconnected,
     * and if onError() returns false (indicating that the error has not already been handled).
     *
     * The underlying AAudio or OpenSL ES stream will already be stopped AND closed by Oboe.
     * So the underlying stream cannot be referenced.
     * But you can still query most parameters.
     *
     * This callback could be used to reopen a new stream on another device.
     *
     * @param audioStream pointer to the associated stream
     * @param error
     */
    virtual void onErrorAfterClose(AudioStream* /* audioStream */, Result /* error */) {}

};

/**
 * AudioStreamPresentationCallback defines a callback interface for
 * being notified when a data presentation event is filed.
 *
 * It is used with AudioStreamBuilder::setPresentationCallback().
 */
class AudioStreamPresentationCallback {
public:
    virtual ~AudioStreamPresentationCallback() = default;

    /**
     * This will be called when all the buffers of an offloaded
     * stream that were queued in the audio system (e.g. the
     * combination of the Android audio framework and the device's
     * audio hardware) have been played.
     *
     * @param audioStream pointer to the associated stream
     */
    virtual void onPresentationEnded(AudioStream* /* audioStream */) {}
};

/**
 * AudioStreamRoutingCallback defines a callback interface for
 * being notified when the routed devices of a stream are changed.
 *
 * It is used with AudioStreamBuilder::setRoutingCallback().
 */
class AudioStreamRoutingCallback {
public:
    /**
     * This will be called when the routed devices of the stream are changed.
     *
     * @param audioStream pointer to the associated stream
     * @param deviceIds a pointer to a list of current routed device ids.
     * @param numDevices the number of current routed devices.
     */
    virtual void onRoutingChanged(AudioStream* /* audioStream */,
                                  const int32_t* /* deviceIds */,
                                  int32_t /* numDevices */) {}
};

/**
 * AudioStreamPartialDataCallback defines a callback interface for
 * moving data to/from an audio stream using `onAudioReady`
 *
 * It is used with AudioStreamBuilder::setPartialDataCallback(). The only difference
 * between AudioStreamPartialDataCallback and AudioStreamDataCallback is that the
 * AudioStreamPartialDataCallback allows apps to only process part of the requested
 * data.
 *
 * Note that Android only support partial data callback from aaudio API at
 * API level 37. In that case, when partial data callback is set on the Android
 * device that is not supporting partial data callback API, apps must process all
 * the requested data to avoid data lost.
 *
 * Call OboeExtensions::isPartialDataCallbackSupported() to check if partial data callback
 * is supported by the device or not.
 */
class AudioStreamPartialDataCallback {
public:
    virtual ~AudioStreamPartialDataCallback() = default;

    /**
     * A buffer is ready for processing. If the client cannot process all the provided/requested
     * data, returns a number to indicate the actual processed frames.
     *
     * For an output stream, this function should render and write at most numFrames of data
     * in the stream's current data format to the audioData buffer.
     *
     * For an input stream, this function should read and process at most numFrames of data
     * from the audioData buffer.
     *
     * The audio data is passed through the buffer. So do NOT call read() or
     * write() on the stream that is making the callback.
     *
     * Note that numFrames can vary unless AudioStreamBuilder::setFramesPerCallback()
     * is called. If AudioStreamBuilder::setFramesPerCallback() is NOT called then
     * numFrames should always be <= AudioStream::getFramesPerBurst().
     *
     * Also note that this callback function should be considered a "real-time" function.
     * It must not do anything that could cause an unbounded delay because that can cause the
     * audio to glitch or pop.
     *
     * These are things the function should NOT do:
     * <ul>
     * <li>allocate memory using, for example, malloc() or new</li>
     * <li>any file operations such as opening, closing, reading or writing</li>
     * <li>any network operations such as streaming</li>
     * <li>use any mutexes or other synchronization primitives</li>
     * <li>sleep</li>
     * <li>oboeStream->stop(), pause(), flush() or close()</li>
     * <li>oboeStream->read()</li>
     * <li>oboeStream->write()</li>
     * </ul>
     *
     * The following are OK to call from the data callback:
     * <ul>
     * <li>oboeStream->get*()</li>
     * <li>oboe::convertToText()</li>
     * <li>oboeStream->setBufferSizeInFrames()</li>
     * </ul>
     *
     * If you need to move data, eg. MIDI commands, in or out of the callback function then
     * we recommend the use of non-blocking techniques such as an atomic FIFO.
     *
     * @param audioStream pointer to the associated stream
     * @param audioData buffer containing input data or a place to put output data
     * @param numFrames number of frames to be processed
     * @return the actual processed data frames, must be within range of [0, numFrames].
     *         Returning a negative number will stop the stream.
     */
    virtual int32_t onPartialAudioReady(
            AudioStream *audioStream,
            void *audioData,
            int32_t numFrames) = 0;
};

/**
 * AudioStreamCallback defines a callback interface for:
 *
 * 1) moving data to/from an audio stream using `onAudioReady`
 * 2) being alerted when a stream has an error using `onError*` methods
 *
 * It is used with AudioStreamBuilder::setCallback().
 *
 * It combines the interfaces defined by AudioStreamDataCallback and AudioStreamErrorCallback.
 * This was the original callback object. We now recommend using the individual interfaces
 * and using setDataCallback() and setErrorCallback().
 *
 * @deprecated Use `AudioStreamDataCallback` and `AudioStreamErrorCallback` instead
 */
class AudioStreamCallback : public AudioStreamDataCallback,
                            public AudioStreamErrorCallback {
public:
    virtual ~AudioStreamCallback() = default;
};

} // namespace oboe

#endif //OBOE_STREAM_CALLBACK_H
