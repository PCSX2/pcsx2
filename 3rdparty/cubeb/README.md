# libcubeb - Cross-platform Audio I/O Library

[![Build Status](https://github.com/mozilla/cubeb/actions/workflows/build.yml/badge.svg)](https://github.com/mozilla/cubeb/actions/workflows/build.yml)

`libcubeb` is a cross-platform C library for high and low-latency audio input/output. It provides a simple, consistent API for audio playback and recording across multiple platforms and audio backends. It is written in C, C++ and Rust, with a C ABI and [Rust](https://github.com/mozilla/cubeb-rs) bindings. While originally written for use in the Firefox Web browser, a number of other software projects have adopted it.

## Features

- **Cross-platform support**: Windows, macOS, Linux, Android, and other platforms
- **Versatile**: Optimized for low-latency real-time audio applications, or power efficient higher latency playback
- **A/V sync**: Latency compensated audio clock reporting for easy audio/video synchronization
- **Full-duplex support**: Simultaneous audio input and output, reclocked
- **Device enumeration**: Query available audio devices
- **Audio processing for speech**: Can use VoiceProcessing IO on recent macOS

## Supported Backends & status

| *Backend*         | *Support Level* | *Platform version* | *Notes*                                          |
|-------------------|-----------------|--------------------|--------------------------------------------------|
| PulseAudio (Rust) | Tier-1          |                    | Main Linux desktop backend                       |
| AudioUnit (Rust)  | Tier-1          |                    | Main macOS backend                               |
| WASAPI            | Tier-1          | Windows >= 7       | Main Windows backend                             |
| AAudio            | Tier-1          | Android >= 8       | Main Android backend for most devices            |
| OpenSL            | Tier-1          | Android >= 2.3     | Android backend for older devices                |
| OSS               | Tier-2          |                    |                                                  |
| sndio             | Tier-2          |                    |                                                  |
| Sun               | Tier-2          |                    |                                                  |
| WinMM             | Tier-3          | Windows XP         | Was Tier-1, Firefox minimum Windows version 7.   |
| AudioTrack        | Tier-3          | Android < 2.3      | Was Tier-1, Firefox minimum Android version 4.1. |
| ALSA              | Tier-3          |                    |                                                  |
| JACK              | Tier-3          |                    |                                                  |
| KAI               | Tier-3          |                    |                                                  |
| PulseAudio (C)    | Tier-4          |                    | Was Tier-1, superseded by Rust                   |
| AudioUnit (C++)   | Tier-4          |                    | Was Tier-1, superseded by Rust                   |

Tier-1: Actively maintained.  Should have CI coverage. Critical for Firefox.

Tier-2: Actively maintained by contributors.  CI coverage appreciated.

Tier-3: Maintainers/patches accepted.  Status unclear.

Tier-4: Deprecated, obsolete.  Scheduled to be removed.

Note that the support level is not a judgement of the relative merits
of a backend, only the current state of support, which is informed
by Firefox's needs, the responsiveness of a backend's
maintainer, and the level of contributions to that backend.

## Building

### Prerequisites

- CMake 3.15 or later
- Non-ancient MSVC, clang or gcc, for compiling both C and C++
- Platform-specific audio libraries (automatically detected)
- Optional but recommended: Rust compiler to compile and link more recent backends for macOS and PulseAudio

### Quick build

```bash
git clone https://github.com/mozilla/cubeb.git
cd cubeb
cmake -B build
cmake --build build
```

### Better build with Rust backends

```bash
git clone --recursive https://github.com/mozilla/cubeb.git
cd cubeb
cmake -B build -DBUILD_RUST_LIBS=ON
cmake --build build
```

### Platform-Specific Notes

**Windows**: Supports Visual Studio 2015+ and MinGW-w64. Use `-G "Visual Studio 16 2019"` or `-G "MinGW Makefiles"`.

**macOS**: Requires Xcode command line tools. Audio frameworks are automatically linked.

**Linux**: Development packages for desired backends:
```bash
# Ubuntu/Debian
sudo apt-get install libpulse-dev libasound2-dev libjack-dev

# Fedora/RHEL
sudo dnf install pulseaudio-libs-devel alsa-lib-devel jack-audio-connection-kit-devel
```

**Android**: Use with Android NDK. AAudio requires API level 26+.

## Testing

Run the test suite:
```bash
cd build
ctest
```

Use the interactive test tool:
```bash
./cubeb-test
```

## License

Licensed under an ISC-style license. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please see the [contribution guidelines](CONTRIBUTING.md) and check the [issue tracker](https://github.com/mozilla/cubeb/issues).

## Links

- [GitHub Repository](https://github.com/mozilla/cubeb)
- [API Documentation](https://mozilla.github.io/cubeb/)
