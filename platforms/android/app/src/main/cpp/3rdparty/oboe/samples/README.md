Oboe Samples
==============
These samples demonstrate how to use the Oboe library:

1. [MinimalOboe](minimaloboe): Just create an Oboe stream and play white noise. Restart stream when disconnected. (Kotlin/Compose)
1. [hello-oboe](hello-oboe): Creates an output (playback) stream and plays a
sine wave when you tap the screen. (Java)
1. [RhythmGame](RhythmGame): A simple rhythm game where you copy the clap patterns you hear by tapping on the screen.
There is an associated codelab to follow along with. (Java)
1. [MegaDrone](MegaDrone): A one hundred oscillator synthesizer, demonstrates low latency and CPU performance. (Java)
1. [DrumThumper](drumthumper): A drum pad that plays sounds from loaded WAV files. (Kotlin)
1. [LiveEffect](LiveEffect): Loops audio from input stream to output stream to demonstrate duplex capability. (Java)
1. [SoundBoard](SoundBoard): A 25 to 40 note dynamic synthesizer, demonstating combining signals. The stream restarts
when the display rotates. (Kotlin)

Pre-requisites
-------------
* Android device or emulator running API 16 (Jelly Bean) or above
* [Android SDK 26](https://developer.android.com/about/versions/oreo/android-8.0-migration.html#ptb)
* [NDK r17](https://developer.android.com/ndk/downloads/index.html) or above
* [Android Studio 2.3.0+](https://developer.android.com/studio/index.html)

Getting Started
---------------
1. [Install Android Studio](https://developer.android.com/studio/index.html)
1. Import the sample project into Android Studio
    - File -> New -> Import Project
    - Browse to oboe/samples/build.gradle
    - Click "OK"
1. Click Run, click on the sample you wish to run

Support
-------
If you've found an error in these samples, please [file an issue](https://github.com/google/oboe/issues/new).

Patches are encouraged, and may be submitted by [forking this project](https://github.com/google/oboe/fork) and
submitting a pull request through GitHub. Please see [CONTRIBUTING.md](../CONTRIBUTING.md) for more details.

- [Stack Overflow](http://stackoverflow.com/questions/tagged/android-ndk)
- [Google+ Community](https://plus.google.com/communities/105153134372062985968)
- [Android Tools Feedback](http://tools.android.com/feedback)


License
-------
Copyright 2017 Google, Inc.

Licensed to the Apache Software Foundation (ASF) under one or more contributor
license agreements.  See the NOTICE file distributed with this work for
additional information regarding copyright ownership.  The ASF licenses this
file to you under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License.  You may obtain a copy of
the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
License for the specific language governing permissions and limitations under
the License.
