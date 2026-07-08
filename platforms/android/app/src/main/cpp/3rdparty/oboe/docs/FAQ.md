# Frequently Asked Questions (FAQ)

## Can I write audio data from Java/Kotlin to Oboe?

Oboe is a native library written in C++ which uses the Android NDK. To move data from Java to C++ you can use [JNI](https://developer.android.com/training/articles/perf-jni). 

If you're generating audio data in Java or Kotlin you should consider whether the reduced latency which Oboe gives you (particularly on high-end devices) is worth the extra complexity of passing data via JNI. An alternative is to use [Java AudioTrack](https://developer.android.com/reference/android/media/AudioTrack). This can be created with low latency using the AudioTrack.Builder method [`setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY).

You can dynamically tune the latency of the stream just like in Oboe using [`setBufferSizeInFrames(int)`](https://developer.android.com/reference/android/media/AudioTrack.html#setBufferSizeInFrames(int))
Also you can use blocking writes with the Java AudioTrack and still get a low latency stream.
Oboe requires a data callback to get a low latency stream and that does not work well with Java.

Note that [`AudioTrack.PERFORMANCE_MODE_LOW_LATENCY`](https://developer.android.com/reference/android/media/AudioTrack#PERFORMANCE_MODE_LOW_LATENCY) was added in API 26, For API 24 or 25 use [`AudioAttributes.FLAG_LOW_LATENCY`](https://developer.android.com/reference/kotlin/android/media/AudioAttributes#flag_low_latency). That was deprecated but will still work with later APIs.

## Can I use Oboe to play compressed audio files, such as MP3 or AAC?
Oboe only works with PCM data. It does not include any extraction or decoding classes. However, the [RhythmGame sample](https://github.com/google/oboe/tree/main/samples/RhythmGame) includes extractors for both NDK and FFmpeg. 

For more information on using FFmpeg in your app [check out this article](https://medium.com/@donturner/using-ffmpeg-for-faster-audio-decoding-967894e94e71).

## Android Studio doesn't find the Oboe symbols, how can I fix this?
Start by ensuring that your project builds successfully. The main thing to do is ensure that the Oboe include paths are set correctly in your project's `CMakeLists.txt`. [Full instructions here](https://github.com/google/oboe/blob/main/docs/GettingStarted.md#2-update-cmakeliststxt).

If that doesn't fix it try the following: 

1) Invalidate the Android Studio cache by going to File->Invalidate Caches / Restart
2) Delete the contents of `$HOME/Library/Caches/AndroidStudio<version>`

We have had several reports of this happening and are keen to understand the root cause. If this happens to you please file an issue with your Android Studio version and we'll investigate further. 

## I requested a stream with `PerformanceMode::LowLatency`, but didn't get it. Why not?
Usually if you call `builder.setPerformanceMode(PerformanceMode::LowLatency)` and don't specify other stream properties you will get a `LowLatency` stream. The most common reasons for not receiving one are: 

- You are opening an output stream and did not specify a **data callback**.
- You requested a **sample** rate which does not match the audio device's native sample rate. For playback streams, this means the audio data you write into the stream must be resampled before it's sent to the audio device. For recording streams, the  audio data must be resampled before you can read it. In both cases the resampling process (performed by the Android audio framework) adds latency and therefore providing a `LowLatency` stream is not possible. To avoid the resampler on API 26 and below you can specify a default value for the sample rate [as detailed here](https://github.com/google/oboe/blob/main/docs/GettingStarted.md#obtaining-optimal-latency).  Or you can enable sample rate conversion by calling [AudioStreamBuilder::setSampleRateConversionQuality()](https://google.github.io/oboe/classoboe_1_1_audio_stream_builder.html#a0c98d21da654da6d197b004d29d8499c) in Oboe, which allows the lower level code to run at the optimal rate and provide lower latency.
- If you request **AudioFormat::Float on an Input** stream before Android 9.0 then you will **not** get a FAST track. You need to either request AudioFormat::Int16 or enable format conversion by calling [AudioStreamBuilder::setFormatConversionAllowed()](https://google.github.io/oboe/classoboe_1_1_audio_stream_builder.html#aa30150d2d0b3c925b545646962dffca0) in Oboe.
- The audio **device** does not support `LowLatency` streams, for example Bluetooth. 
- You requested a **channel count** which is not supported natively by the audio device. On most devices and Android API levels it is possible to obtain a `LowLatency` stream for both mono and stereo, however, there are a few exceptions, some of which are listed [here](https://github.com/google/oboe/blob/main/docs/AndroidAudioHistory.md). 
- The **maximum number** of `LowLatency` streams has been reached. This could be by your app, or by other apps. This is often caused by opening multiple playback streams for different "tracks". To avoid this open a single audio stream and perform 
your own mixing in the app. 
- You are on Android 7.0 or below and are receiving `PerformanceMode::None`. The ability to query the performance mode of a stream was added in Android 7.1 (Nougat MR1). Low latency streams (aka FAST tracks) _are available_ on Android 7.0 and below but there is no programmatic way of knowing whether yours is one. [Question on StackOverflow](https://stackoverflow.com/questions/56828501/does-opensl-es-support-performancemodelowlatency/5683499)

## My question isn't listed, where can I ask it?
Please ask questions on [Stack Overflow](https://stackoverflow.com/questions/ask) with the [Oboe tag](https://stackoverflow.com/tags/oboe) or in the GitHub Issues tab.
