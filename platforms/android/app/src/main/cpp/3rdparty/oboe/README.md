# Oboe [![Build CI](https://github.com/google/oboe/workflows/Build%20CI/badge.svg)](https://github.com/google/oboe/actions)

[![Introduction to Oboe video](docs/images/getting-started-video.jpg)](https://www.youtube.com/watch?v=csfHAbr5ilI&list=PLWz5rJ2EKKc_duWv9IPNvx9YBudNMmLSa)

Oboe is a C++ library which makes it easy to build high-performance audio apps on Android. It was created primarily to allow developers to target a simplified API that works across multiple API levels back to API level 16 (Jelly Bean).

## Features
- Compatible with API 16 onwards - runs on 99% of Android devices
- Chooses the audio API (OpenSL ES on API 16+ or AAudio on API 27+) which will give the best audio performance on the target Android device
- Automatic latency tuning
- Modern C++ allowing you to write clean, elegant code
- Workarounds for some known issues
- [Used by popular apps and frameworks](https://github.com/google/oboe/wiki/AppsUsingOboe)

## Documentation
- [Getting Started Guide](docs/GettingStarted.md)
- [Full Guide to Oboe](docs/FullGuide.md)
- [API reference](https://google.github.io/oboe)
- [History of Audio features/bugs by Android version](docs/AndroidAudioHistory.md)
- [Migration guide for apps using OpenSL ES](docs/OpenSLESMigration.md)
- [Frequently Asked Questions](docs/FAQ.md) (FAQ)
- [Wiki](https://github.com/google/oboe/wiki)
- [Our roadmap](https://github.com/google/oboe/milestones) - Vote on a feature/issue by adding a thumbs up to the first comment.

### Community
- Reddit: [r/androidaudiodev](https://www.reddit.com/r/androidaudiodev/)
- StackOverflow: [#oboe](https://stackoverflow.com/questions/tagged/oboe)

## Testing
- [**OboeTester** app for measuring latency, glitches, etc.](apps/OboeTester/docs)
- [Oboe unit tests](tests)

## Videos
- [Getting started with Oboe](https://www.youtube.com/playlist?list=PLWz5rJ2EKKc_duWv9IPNvx9YBudNMmLSa)
- [Low Latency Audio - Because Your Ears Are Worth It](https://www.youtube.com/watch?v=8vOf_fDtur4) (Android Dev Summit '18)
- [Winning on Android](https://www.youtube.com/watch?v=tWBojmBpS74) - How to optimize an Android audio app. (ADC '18)

## Sample code and apps
- Sample apps can be found in the [samples directory](samples). 
- A complete "effects processor" app called FXLab can  be found in the [apps/fxlab folder](apps/fxlab). 
- Also check out the [Rhythm Game codelab](https://developer.android.com/codelabs/musicalgame-using-oboe?hl=en#0).

### Third party sample code
- [Ableton Link integration demo](https://github.com/jbloit/AndroidLinkAudio) (author: jbloit)

## Contributing
We would love to receive your pull requests. Before we can though, please read the [contributing](CONTRIBUTING.md) guidelines.

## Version history
View the [releases page](../../releases).

## License
[LICENSE](LICENSE)

