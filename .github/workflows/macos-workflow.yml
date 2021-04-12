name: MacOS Build

# Controls when the action will run. Triggers the workflow on push or pull request
# events but only for the master branch
on:
  push:
    branches:
      - master
    paths-ignore:
      - .gitignore
      - "**/*.md"
      - .clang-format
      - debian-packager/
      - bin/PCSX2_keys.ini.default
      - "pcsx2/PAD/Windows/**"
  pull_request:
    branches:
      - master
    paths-ignore:
      - .gitignore
      - "**/*.md"
      - .clang-format
      - debian-packager/
      - bin/PCSX2_keys.ini.default
      - "pcsx2/PAD/Windows/**"

jobs:
  build:
    strategy:
      # Prevent one build from failing everything (although maybe those should be included as experimental builds instead)
      fail-fast: false
      matrix:
        os: [macos-10.15]
        platform: [x64]
        experimental: [false]

    name: ${{ matrix.os }}-${{ matrix.platform }}
    runs-on: ${{ matrix.os }}
    continue-on-error: ${{ matrix.experimental }}
    # Set some sort of timeout in the event of run-away builds.  We are limited on concurrent jobs so, get rid of them.
    timeout-minutes: 30

    steps:
      # NOTE - useful for debugging
      # - name: Dump GitHub context
      #   env:
      #     GITHUB_CONTEXT: ${{ toJson(github) }}
      #   run: |
      #     echo "$GITHUB_CONTEXT"
      #     echo ${{ github.event.pull_request.title }}

      - name: Checkout Repository
        uses: actions/checkout@v2

      - name: Checkout Submodules
        run: git submodule update --init --recursive --jobs 2

      - name: Install Packages
        env:
          PLATFORM: ${{ matrix.platform }}
        run: |
          export HOMEBREW_NO_INSTALL_CLEANUP=1
          brew update
          brew install sound-touch portaudio wxmac gtk+3 sdl2 libsamplerate
          brew install --cask xquartz # We use its OpenGL headers

      - name: Generate CMake Files
        run: cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_BUILD_PO=FALSE -B build .

      - name: Build PCSX2
        working-directory: ./build
        run: make -j2 # macOS doesn't use make install
