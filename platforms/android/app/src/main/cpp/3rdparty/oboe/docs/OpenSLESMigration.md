OpenSLES Migration Guide
===

# Introduction

This guide will show you how to migrate your code from [OpenSL ES for Android](https://developer.android.com/ndk/guides/audio/opensl/opensl-for-android) (just OpenSL from now on) to Oboe. 

To familiarise yourself with Oboe, please read the [Getting Started guide](https://github.com/google/oboe/blob/main/docs/GettingStarted.md) and ensure that Oboe has been added as a dependency in your project.


# Concepts

At a high level, OpenSL and Oboe have some similarities. They both create objects which communicate with an audio device capable of playing or recording audio samples. They also use a callback mechanism to read data from or write data to that audio device.

This is where the similarities end.

Oboe has been designed to be a simpler, easier to use API than OpenSL. It aims to reduce the amount of boilerplate code and guesswork associated with recording and playing audio.


# Key differences


## Object mappings

OpenSL uses an audio engine object, created using `slCreateEngine`, to create other objects. Oboe's equivalent object is `AudioStreamBuilder`, although it will only create an `AudioStream`.

OpenSL uses audio player and audio recorder objects to communicate with audio devices. In Oboe an `AudioStream` is used.

In OpenSL the audio callback mechanism is a user-defined function which is called each time a buffer is enqueued. In Oboe you construct an `AudioStreamDataCallback` object, and its `onAudioReady` method is called each time audio data is ready to be read or written.  

Here's a table which summarizes the object mappings:


<table>
  <tr>
   <td><strong>OpenSL</strong>
   </td>
   <td><strong>Oboe </strong>(all classes are in the <code>oboe</code> namespace)
   </td>
  </tr>
  <tr>
   <td>Audio engine (an <code>SLObjectItf</code>)
   </td>
   <td><code>AudioStreamBuilder</code>
   </td>
  </tr>
  <tr>
   <td>Audio player
   </td>
   <td><code>AudioStream</code> configured for output
   </td>
  </tr>
  <tr>
   <td>Audio recorder
   </td>
   <td><code>AudioStream</code> configured for input
   </td>
  </tr>
  <tr>
   <td>Callback function
   </td>
   <td><code>AudioStreamDataCallback::onAudioReady</code>
   </td>
  </tr>
</table>



## Buffers and callbacks

In OpenSL your app must create and manage a queue of buffers. Each time a buffer is dequeued, the callback function is called and your app must enqueue a new buffer.

In Oboe, rather than owning and enqueuing buffers, you are given direct access to the `AudioStream`'s buffer through the `audioData` parameter of `onAudioReady`.

This is a container array which you can read audio data from when recording, or write data into when playing. The `numFrames` parameter tells you how many frames to read/write. Here's the method signature of `onAudioReady`:


```
DataCallbackResult onAudioReady(
    AudioStream *oboeStream,
    void *audioData,
    int32_t numFrames
)
```


You supply your implementation of `onAudioReady` when building the audio stream by constructing an `AudioStreamDataCallback` object. [Here's an example.](https://github.com/google/oboe/blob/main/docs/GettingStarted.md#creating-an-audio-stream)


### Buffer sizes

In OpenSL you cannot specify the size of the internal buffers of the audio player/recorder because your app is supplying them so they can have arbitrary size. You can only specify the _number of buffers_ through the `SLDataLocator_AndroidSimpleBufferQueue.numBuffers` field.

By contrast, Oboe will use the information it has about the current audio device to configure its buffer size. It will determine the optimal number of audio frames which should be read/written in a single callback. This is known as a _burst_, and usually represents the minimum possible buffer size. Typical values are 96, 128, 192 and 240 frames.  

An audio stream's burst size, given by `AudioStream::getFramesPerBurst()`, is important because it is used when configuring the buffer size. Here's an example which uses two bursts for the buffer size, which usually represents a good tradeoff between latency and glitch protection:


```
audioStream.setBufferSizeInFrames(audioStream.getFramesPerBurst() * 2);
```


**Note:** because Oboe uses OpenSL under-the-hood on older devices which does not provide the same information about audio devices, it still needs to know [sensible default values for the burst to be used with OpenSL](https://github.com/google/oboe/blob/main/docs/GettingStarted.md#obtaining-optimal-latency).


## Audio stream properties

In OpenSL you must explicitly specify various properties, including the sample rate and audio format, when opening an audio player or audio recorder.

In Oboe, you do not need to specify any properties to open a stream. For example, this will open a valid output `AudioStream` with sensible default values.


```
AudioStreamBuilder builder;
builder.openStream(myStream);
```


However, you may want to specify some properties. These are set using the `AudioStreamBuilder` ([example](https://github.com/google/oboe/blob/main/docs/FullGuide.md#set-the-audio-stream-configuration-using-an-audiostreambuilder)).


## Stream disconnection

OpenSL has no mechanism, other than stopping callbacks, to indicate that an audio device has been disconnected - for example, when headphones are unplugged.

In Oboe, you can be notified of stream disconnection by overriding one of the `onError` methods in `AudioStreamErrorCallback`. This allows you to clean up any resources associated with the audio stream and create a new stream with optimal properties for the current audio device ([more info](https://github.com/google/oboe/blob/main/docs/FullGuide.md#disconnected-audio-stream)).


# Unsupported features


## Formats

Oboe audio streams only accept [PCM](https://en.wikipedia.org/wiki/Pulse-code_modulation) data in float or signed 16-bit ints. Additional formats including 8-bit unsigned, 24-bit packed, 8.24 and 32-bit are not supported.

Compressed audio, such as MP3, is not supported for a number of reasons but chiefly:



*   The OpenSL ES implementation has performance and reliability issues.
*   It keeps the Oboe API and the underlying implementation simple.

Extraction and decoding can be done either through the NDK [Media APIs](https://developer.android.com/ndk/reference/group/media) or by using a third party library like [FFmpeg](https://ffmpeg.org/). An example of both these approaches can be seen in the [RhythmGame sample](https://github.com/google/oboe/tree/main/samples/RhythmGame).


## Miscellaneous features

Oboe does **not** support the following features:



*   Channel masks - only [indexed channel masks](https://developer.android.com/reference/kotlin/android/media/AudioFormat#channel-index-masks) are supported.
*   Playing audio content from a file pathname or [URI](https://en.wikipedia.org/wiki/Uniform_Resource_Identifier).
*   Notification callbacks for position updates.
*   Platform output effects on API 27 and below. [They are supported from API 28 and above.](https://github.com/google/oboe/wiki/TechNote_Effects)


# Summary



*   Replace your audio player or recorder with an `AudioStream` created using an `AudioStreamBuilder`.
*   Use your value for `numBuffers` to set the audio stream's buffer size as a multiple of the burst size. For example: `audioStream.setBufferSizeInFrames(audioStream.getFramesPerBurst * numBuffers)`.
*   Create an `AudioStreamDataCallback` object and move your OpenSL callback code inside the `onAudioReady` method.
*   Handle stream disconnect events by creating an `AudioStreamErrorCallback` object and overriding one of its `onError` methods.
*   Pass sensible default sample rate and buffer size values to Oboe from `AudioManager` [using this method](https://github.com/google/oboe/blob/main/docs/GettingStarted.md#obtaining-optimal-latency) so that your app is still performant on older devices.

For more information please read the [Full Guide to Oboe](https://github.com/google/oboe/blob/main/docs/FullGuide.md).
