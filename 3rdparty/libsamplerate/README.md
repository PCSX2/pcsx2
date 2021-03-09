![Logo](docs/SRC.png)

This is libsamplerate, `0.2.1`.

libsamplerate (also known as Secret Rabbit Code) is a library for performing sample rate conversion of audio data.

* The [`src/`](https://github.com/libsndfile/libsamplerate/tree/master/src) directory contains the source code for library itself.
* The [`docs/`](https://github.com/libsndfile/libsamplerate/tree/master/docs) directory contains the libsamplerate documentation.
* The [`examples/`](https://github.com/libsndfile/libsamplerate/tree/master/examples) directory contains examples of how to write code using libsamplerate.
* The [`tests/`](https://github.com/libsndfile/libsamplerate/tree/master/tests) directory contains programs which link against libsamplerate and test its functionality.
* The [`Win32/`](https://github.com/libsndfile/libsamplerate/tree/master/Win32) directory contains files and documentation to allow libsamplerate to compile under Win32 with the Microsoft Visual C++ compiler.

Additional references:

* [Official website](http://libsndfile.github.io/libsamplerate//)
* [GitHub](https://github.com/libsndfile/libsamplerate)

---

## Build Status

| Branch         | Status                                                                                                            |
|----------------|-------------------------------------------------------------------------------------------------------------------|
| `master`       | ![Build](https://github.com/libsndfile/libsamplerate/workflows/Build/badge.svg)       |

Branches [actively built](https://github.com/libsndfile/libsamplerate/actions) by GitHub Actions.

---

## Win32

There are detailed instructions for building libsamplerate on Win32 in the file [`docs/win32.md`](https://github.com/libsndfile/libsamplerate/tree/master/docs/win32.md).

## macOS

Building on macOS should be the same as building it on any other Unix platform.

## Other Platforms

To compile libsamplerate on platforms which have a Bourne compatible shell, an ANSI C compiler and a make utility should require no more that the following three commands:
```bash
./configure
make
make install
```

## CMake

There is a new [CMake](https://cmake.org/download/)-based build system available:
```bash
mkdir build
cd build
cmake ..
make
```

* Use `cmake -DCMAKE_BUILD_TYPE=Release ..` to make a release build.
* Use `cmake -DBUILD_SHARED_LIBS=ON ..` to build a shared library.

## Contacts

libsamplerate was written by [Erik de Castro Lopo](mailto:erikd@mega-nerd.com).
