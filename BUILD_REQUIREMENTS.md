# PCSX2 Build Requirements

## Quick Answer

To build PCSX2 with the subsystem tracer enhancements on Linux, you need:

### Minimum Requirements
- **OS**: Ubuntu 22.04 or similar (Debian-based)
- **Compiler**: Clang 17 or GCC 13+
- **CMake**: 3.16+ (3.28+ recommended)
- **Build Tool**: Ninja or Make

---

## Complete Dependency List

### Core Build Tools
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ccache \
    patchelf
```

### Compilers (Choose ONE)

**Option 1: Clang 17 (Recommended - used by PCSX2 CI)**
```bash
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc
sudo apt-add-repository 'deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-17 main'
sudo apt-get update
sudo apt-get install -y clang-17 lld-17 llvm-17
```

**Option 2: GCC (System Default)**
```bash
# Usually already installed with build-essential
gcc --version  # Should be 13.x or newer
```

### Graphics Libraries
```bash
sudo apt-get install -y \
    libegl-dev \
    libopengl-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxcb1-dev \
    libxcb-composite0-dev \
    libxcb-cursor-dev \
    libxcb-damage0-dev \
    libxcb-glx0-dev \
    libxcb-icccm4-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-present-dev \
    libxcb-randr0-dev \
    libxcb-render0-dev \
    libxcb-render-util0-dev \
    libxcb-shape0-dev \
    libxcb-shm0-dev \
    libxcb-sync-dev \
    libxcb-util-dev \
    libxcb-xfixes0-dev \
    libxcb-xinput-dev \
    libxcb-xkb-dev \
    libxext-dev \
    libxkbcommon-x11-dev \
    libxrandr-dev
```

### Multimedia Libraries
```bash
sudo apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libswscale-dev \
    libasound2-dev \
    libpulse-dev \
    libpipewire-0.3-dev
```

### System Libraries
```bash
sudo apt-get install -y \
    libaio-dev \
    libcurl4-openssl-dev \
    libdbus-1-dev \
    libdecor-0-dev \
    libevdev-dev \
    libfontconfig-dev \
    libfreetype-dev \
    libfuse2 \
    libgtk-3-dev \
    libgudev-1.0-dev \
    libharfbuzz-dev \
    libinput-dev \
    libpcap-dev \
    libssl-dev \
    libudev-dev \
    libwayland-dev \
    zlib1g-dev
```

### **CRITICAL: Image Libraries** (The one we were missing!)
```bash
sudo apt-get install -y \
    libjpeg-dev \
    libpng-dev \
    libwebp-dev
```

### Qt6 (Will be built from source by PCSX2 script)
PCSX2 builds its own Qt6 from source via `.github/workflows/scripts/linux/build-dependencies-qt.sh`

---

## Complete One-Line Installation (Ubuntu 22.04)

```bash
sudo apt-get update && sudo apt-get install -y \
  build-essential ccache cmake curl extra-cmake-modules git \
  libasound2-dev libaio-dev libavcodec-dev libavformat-dev libavutil-dev \
  libcurl4-openssl-dev libdbus-1-dev libdecor-0-dev libegl-dev libevdev-dev \
  libfontconfig-dev libfreetype-dev libfuse2 libgtk-3-dev libgudev-1.0-dev \
  libharfbuzz-dev libinput-dev libjpeg-dev libopengl-dev libpcap-dev \
  libpipewire-0.3-dev libpng-dev libpulse-dev libssl-dev libswresample-dev \
  libswscale-dev libudev-dev libwayland-dev libwebp-dev libx11-dev \
  libx11-xcb-dev libxcb1-dev libxcb-composite0-dev libxcb-cursor-dev \
  libxcb-damage0-dev libxcb-glx0-dev libxcb-icccm4-dev libxcb-image0-dev \
  libxcb-keysyms1-dev libxcb-present-dev libxcb-randr0-dev libxcb-render0-dev \
  libxcb-render-util0-dev libxcb-shape0-dev libxcb-shm0-dev libxcb-sync-dev \
  libxcb-util-dev libxcb-xfixes0-dev libxcb-xinput-dev libxcb-xkb-dev \
  libxext-dev libxkbcommon-x11-dev libxrandr-dev ninja-build patchelf \
  pkg-config zlib1g-dev
```

---

## Build Steps

### 1. Clone Repository (if not already done)
```bash
git clone https://github.com/MagnificentS/pcsx2.git
cd pcsx2
git checkout claude/codebase-review-011CUowrwYh5jiTw19ffAoiN
```

### 2. Build Qt6 Dependencies (First Time Only)
```bash
# This builds Qt6 and other dependencies from source
# Takes ~30-60 minutes depending on your system
.github/workflows/scripts/linux/build-dependencies-qt.sh "$HOME/pcsx2-deps"
```

### 3. Configure CMake

**With Clang 17:**
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps" \
  -DCMAKE_C_COMPILER=clang-17 \
  -DCMAKE_CXX_COMPILER=clang++-17 \
  -DCMAKE_EXE_LINKER_FLAGS_INIT="-fuse-ld=lld" \
  -DCMAKE_MODULE_LINKER_FLAGS_INIT="-fuse-ld=lld" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DENABLE_SETCAP=OFF
```

**With GCC:**
```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"
```

### 4. Build
```bash
cmake --build build -j$(nproc)
```

Expected build time:
- **First build**: 15-30 minutes (depending on CPU)
- **Incremental builds**: 1-5 minutes (with ccache)

### 5. Run
```bash
./build/pcsx2-qt/pcsx2-qt
```

---

## Troubleshooting

### Error: "Could NOT find JPEG"
**Solution**: Install libjpeg-dev
```bash
sudo apt-get install libjpeg-dev
```

### Error: "Could NOT find Qt6"
**Solution**: Run the dependency build script first
```bash
.github/workflows/scripts/linux/build-dependencies-qt.sh "$HOME/pcsx2-deps"
```
Then add to CMake: `-DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"`

### Error: Clang not found
**Solution**: Install Clang 17 or use GCC
```bash
# Either install Clang 17 (see above)
# OR use GCC (remove -DCMAKE_C_COMPILER and -DCMAKE_CXX_COMPILER flags)
```

### Compilation errors in subsystem code
**Solution**: Make sure you're on the correct branch
```bash
git checkout claude/codebase-review-011CUowrwYh5jiTw19ffAoiN
git pull origin claude/codebase-review-011CUowrwYh5jiTw19ffAoiN
```

### Out of memory during build
**Solution**: Reduce parallel jobs
```bash
cmake --build build -j4  # Instead of -j$(nproc)
```

---

## Platform-Specific Notes

### Ubuntu/Debian
- Use the commands above as-is
- Ubuntu 22.04+ recommended
- Debian 12+ works

### Arch Linux
```bash
sudo pacman -S base-devel cmake ninja git ccache \
  qt6-base qt6-tools qt6-svg \
  ffmpeg libpulse alsa-lib \
  libaio libpcap libxrandr libxinerama \
  wayland libdecor \
  libjpeg-turbo libpng libwebp
```

### Fedora
```bash
sudo dnf install gcc-c++ cmake ninja-build git ccache \
  qt6-qtbase-devel qt6-qttools-devel qt6-qtsvg-devel \
  ffmpeg-devel pulseaudio-libs-devel alsa-lib-devel \
  libaio-devel libpcap-devel libXrandr-devel \
  wayland-devel libdecor-devel \
  libjpeg-turbo-devel libpng-devel libwebp-devel
```

### macOS
See PCSX2 official documentation - requires Xcode and Homebrew

### Windows
See PCSX2 official documentation - requires Visual Studio 2022

---

## Testing Your Build

### Verify Subsystem Tracer Integration

1. **Check binary exists:**
   ```bash
   ls -lh ./build/pcsx2-qt/pcsx2-qt
   ```

2. **Run PCSX2:**
   ```bash
   ./build/pcsx2-qt/pcsx2-qt
   ```

3. **Open Debugger:**
   - Menu: Debug → Open Debugger

4. **Open Instruction Trace View:**
   - In debugger, look for "Instruction Trace" tab

5. **Verify subsystem detection:**
   - Start tracing
   - Set a breakpoint
   - Run a game (or BIOS)
   - Check trace table has **7 columns** (including "Subsystem")
   - Verify color coding (green for graphics, etc.)

---

## Build Variants

### Debug Build (for development)
```bash
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"
cmake --build build-debug
```

### Release Build (optimized, no debug symbols)
```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"
cmake --build build-release
```

### RelWithDebInfo (recommended for testing)
```bash
# Optimized but keeps debug symbols for debugging
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"
cmake --build build
```

---

## Disk Space Requirements

- **Source code**: ~500 MB
- **Dependencies build**: ~3 GB (`$HOME/pcsx2-deps`)
- **PCSX2 build**: ~2 GB (`build/` directory)
- **ccache**: ~500 MB (`.ccache/` directory)
- **Total**: ~6 GB

---

## Memory Requirements

- **Minimum**: 8 GB RAM
- **Recommended**: 16 GB RAM
- **For parallel builds**: 2 GB RAM per job (e.g., `-j8` needs 16 GB)

---

## CPU Requirements

- **x86-64** with SSE4.1 support (modern Intel/AMD from 2008+)
- **ARM64** - experimental support (some features may not work)

---

## Next Steps After Building

1. **Test subsystem tracer:**
   - See `SUBSYSTEM_TRACER_COMPLETE.md` for usage examples

2. **Extract assets:**
   - Filter traces to GIF subsystem
   - Identify texture uploads
   - Use MCP `mem_read` to extract data

3. **Create PR** (optional):
   - See `PR_GUIDE.md` for instructions

---

## Summary

**TL;DR for Ubuntu 22.04:**

```bash
# 1. Install dependencies
sudo apt-get update && sudo apt-get install -y \
  build-essential cmake ninja-build git ccache pkg-config \
  libjpeg-dev libpng-dev libwebp-dev \
  [... see complete list above ...]

# 2. Build Qt6 dependencies (first time only, ~30-60 min)
.github/workflows/scripts/linux/build-dependencies-qt.sh "$HOME/pcsx2-deps"

# 3. Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps"

# 4. Build (~15-30 min first time)
cmake --build build -j$(nproc)

# 5. Run
./build/pcsx2-qt/pcsx2-qt
```

**That's it!** You now have PCSX2 with full subsystem tracer support.
