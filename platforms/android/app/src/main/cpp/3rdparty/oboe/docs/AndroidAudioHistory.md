Android audio history
===

A list of important audio features, bugs, fixes and workarounds for various Android versions. [(List of all Android Versions)](https://developer.android.com/guide/topics/manifest/uses-sdk-element#ApiLevels)

### 11.0 R - API 30
- Bug in **AAudio** on RQ1A (Oboe works around this issue from version 1.5 onwards). A stream is normally disconnected when a **headset is plugged in** or because of other device changes. An `AAUDIO_ERROR_DISCONNECTED` error code should be passed to the error callback. But a bug in Shared MMAP streams causes `AAUDIO_ERROR_TIMEOUT` to be returned. So if your error callback is checking for `AAUDIO_ERROR_DISCONNECTED` then it may not respond properly. We recommend **always stopping and closing the stream** regardless of the error code. Oboe does this. So if you are using Oboe callbacks you are OK. This issue was not in the original R release. It was introduced in RQ1A, which is being delivered by OTA starting in November 2020. It will be fixed in a future update. Follow it on [this public Android issue](https://issuetracker.google.com/173928197).

- Fixed. A race condition in AudioFlinger could cause an assert in releaseBuffer() when a headset was plugged in or out. More details [here](https://github.com/google/oboe/wiki/TechNote_ReleaseBuffer)

### 10.0 Q - API 29
- Fixed: Setting capacity of Legacy input streams < 4096 can prevent use of FAST path. https://github.com/google/oboe/issues/183. Also fixed in AAudio with ag/7116429
- Add InputPreset:VoicePerformance for low latency recording.
- Regression bug: [AAudio] Headphone disconnect event not fired for MMAP streams. See P item below. Still in first Q release but fixed in some Q updates. 

### 9.0 Pie - API 28 (August 6, 2018)
- AAudio adds support for setUsage(), setSessionId(), setContentType(), setInputPreset() for builders.
- Regression bug: [AAudio] Headphone disconnect event not fired for MMAP streams. Issue [#252](https://github.com/google/oboe/issues/252) Also see tech note [Disconnected Streams](https://github.com/google/oboe/wiki/TechNote_Disconnect).
- AAudio input streams with LOW_LATENCY will open a FAST path using INT16 and convert the data to FLOAT if needed. See: https://github.com/google/oboe/issues/276

### 8.1 Oreo MR1 - API 27
- Oboe uses AAudio by default.
- AAudio MMAP data path enabled on Pixel devices. PerformanceMode::Exclusive supported.
- Fixed: [AAudio] RefBase issue
- Fixed: Requesting a stereo recording stream can result in sub-optimal latency. 

### 8.0 Oreo - API 26 (August 21, 2017)
- [AAudio API introduced](https://developer.android.com/ndk/guides/audio/aaudio/aaudio)
- Bug: RefBase issue causes crash after stream closed. This why AAudio is not recommended for 8.0. Oboe will use OpenSL ES for 8.0 and earlier.
  https://github.com/google/oboe/issues/40
- Bug: Requesting a stereo recording stream can result in sub-optimal latency. [Details](https://issuetracker.google.com/issues/68666622)

### 7.1 Nougat MR1 - API 25
- OpenSL adds supports for setting and querying of PerformanceMode.

### 7.0 Nougat - API 24 (August 22, 2016)
- OpenSL method `acquireJavaProxy` added, which allows the Java AudioTrack object associated with playback to be obtained (which allows underrun count).

### 6.0 Marshmallow - API 23 (October 5, 2015)
- Floating point recording supported. But it does not allow a FAST "low latency" path.
- [MIDI API introduced](https://developer.android.com/reference/android/media/midi/package-summary)
- Sound output is broken on the API 23 emulator

### 5.0 Lollipop - API 21 (November 12, 2014)
- Floating point playback supported.




