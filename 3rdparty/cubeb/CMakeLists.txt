# TODO
# - backend selection via command line, rather than simply detecting headers.

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(cubeb
  VERSION 0.0.0)

option(BUILD_RUST_LIBS "Build rust backends" OFF)
option(LAZY_LOAD_LIBS "Lazily load shared libraries" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (BUILD_RUST_LIBS)
  if(EXISTS "${PROJECT_SOURCE_DIR}/src/cubeb-pulse-rs")
    set(USE_PULSE_RUST 1)
  endif()
  if(EXISTS "${PROJECT_SOURCE_DIR}/src/cubeb-coreaudio-rs")
    set(USE_AUDIOUNIT_RUST 1)
  endif()
endif()

set(CMAKE_CXX_WARNING_LEVEL 4)
if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti")
else()
  string(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable RTTI
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}) # Disable Exceptions
endif()

add_library(cubeb
  src/cubeb.c
  src/cubeb_mixer.cpp
  src/cubeb_resampler.cpp
  src/cubeb_log.cpp
  src/cubeb_strings.c
  src/cubeb_utils.cpp
)
target_include_directories(cubeb
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
)
set_target_properties(cubeb PROPERTIES
  VERSION ${cubeb_VERSION}
  SOVERSION ${cubeb_VERSION_MAJOR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "Config.cmake.in"
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(
  FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake" "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(TARGETS cubeb EXPORT "${PROJECT_NAME}Targets")
install(
  EXPORT "${PROJECT_NAME}Targets"
  NAMESPACE "${PROJECT_NAME}::"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

add_library(speex OBJECT subprojects/speex/resample.c)
set_target_properties(speex PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_include_directories(speex INTERFACE subprojects)
target_compile_definitions(speex PUBLIC
OUTSIDE_SPEEX
FLOATING_POINT
EXPORT=
RANDOM_PREFIX=speex
)

# $<BUILD_INTERFACE:> required because of https://gitlab.kitware.com/cmake/cmake/-/issues/15415
target_link_libraries(cubeb PRIVATE $<BUILD_INTERFACE:speex>)

include(CheckIncludeFiles)

# Threads needed by cubeb_log, _pulse, _alsa, _jack, _sndio, _oss and _sun
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)
target_link_libraries(cubeb PRIVATE Threads::Threads)

if(LAZY_LOAD_LIBS)
  check_include_files(pulse/pulseaudio.h USE_PULSE)
  check_include_files(alsa/asoundlib.h   USE_ALSA)
  check_include_files(jack/jack.h        USE_JACK)
  check_include_files(sndio.h            USE_SNDIO)
  check_include_files(aaudio/AAudio.h    USE_AAUDIO)

  if(USE_PULSE OR USE_ALSA OR USE_JACK OR USE_SNDIO OR USE_AAUDIO)
    target_link_libraries(cubeb PRIVATE ${CMAKE_DL_LIBS})
  endif()

else()

  find_package(PkgConfig REQUIRED)

  pkg_check_modules(libpulse IMPORTED_TARGET libpulse)
  if(libpulse_FOUND)
    set(USE_PULSE ON)
    target_compile_definitions(cubeb PRIVATE DISABLE_LIBPULSE_DLOPEN)
    target_link_libraries(cubeb PRIVATE PkgConfig::libpulse)
  endif()

  pkg_check_modules(alsa IMPORTED_TARGET alsa)
  if(alsa_FOUND)
    set(USE_ALSA ON)
    target_compile_definitions(cubeb PRIVATE DISABLE_LIBASOUND_DLOPEN)
    target_link_libraries(cubeb PRIVATE PkgConfig::alsa)
  endif()

  pkg_check_modules(jack IMPORTED_TARGET jack)
  if(jack_FOUND)
    set(USE_JACK ON)
    target_compile_definitions(cubeb PRIVATE DISABLE_LIBJACK_DLOPEN)
    target_link_libraries(cubeb PRIVATE PkgConfig::jack)
  endif()

  check_include_files(sndio.h USE_SNDIO)
  if(USE_SNDIO)
    target_compile_definitions(cubeb PRIVATE DISABLE_LIBSNDIO_DLOPEN)
    target_link_libraries(cubeb PRIVATE sndio)
  endif()

  check_include_files(aaudio/AAudio.h USE_AAUDIO)
  if(USE_AAUDIO)
    target_compile_definitions(cubeb PRIVATE DISABLE_LIBAAUDIO_DLOPEN)
    target_link_libraries(cubeb PRIVATE aaudio)
  endif()
endif()

if(USE_PULSE)
  target_sources(cubeb PRIVATE src/cubeb_pulse.c)
  target_compile_definitions(cubeb PRIVATE USE_PULSE)
endif()

if(USE_ALSA)
  target_sources(cubeb PRIVATE src/cubeb_alsa.c)
  target_compile_definitions(cubeb PRIVATE USE_ALSA)
endif()

if(USE_JACK)
  target_sources(cubeb PRIVATE src/cubeb_jack.cpp)
  target_compile_definitions(cubeb PRIVATE USE_JACK)
endif()

if(USE_SNDIO)
  target_sources(cubeb PRIVATE src/cubeb_sndio.c)
  target_compile_definitions(cubeb PRIVATE USE_SNDIO)
endif()

if(USE_AAUDIO)
  target_sources(cubeb PRIVATE src/cubeb_aaudio.cpp)
  target_compile_definitions(cubeb PRIVATE USE_AAUDIO)

  # set this definition to enable low latency mode. Possibly bad for battery
  target_compile_definitions(cubeb PRIVATE CUBEB_AAUDIO_LOW_LATENCY)

  # set this definition to enable power saving mode. Possibly resulting
  # in high latency
  # target_compile_definitions(cubeb PRIVATE CUBEB_AAUDIO_LOW_POWER_SAVING)

  # set this mode to make the backend use an exclusive stream.
  # will decrease latency.
  # target_compile_definitions(cubeb PRIVATE CUBEB_AAUDIO_EXCLUSIVE_STREAM)
endif()

check_include_files(AudioUnit/AudioUnit.h USE_AUDIOUNIT)
if(USE_AUDIOUNIT)
  target_sources(cubeb PRIVATE
    src/cubeb_audiounit.cpp
    src/cubeb_osx_run_loop.cpp)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOUNIT)
  target_link_libraries(cubeb PRIVATE "-framework AudioUnit" "-framework CoreAudio" "-framework CoreServices")
endif()

check_include_files(audioclient.h USE_WASAPI)
if(USE_WASAPI)
  target_sources(cubeb PRIVATE
    src/cubeb_wasapi.cpp)
  target_compile_definitions(cubeb PRIVATE USE_WASAPI)
  target_link_libraries(cubeb PRIVATE avrt ole32 ksuser)
endif()

check_include_files("windows.h;mmsystem.h" USE_WINMM)
if(USE_WINMM)
  target_sources(cubeb PRIVATE
    src/cubeb_winmm.c)
  target_compile_definitions(cubeb PRIVATE USE_WINMM)
  target_link_libraries(cubeb PRIVATE winmm)
endif()

check_include_files(SLES/OpenSLES.h USE_OPENSL)
if(USE_OPENSL)
  target_sources(cubeb PRIVATE
    src/cubeb_opensl.c
    src/cubeb-jni.cpp)
  target_compile_definitions(cubeb PRIVATE USE_OPENSL)
  target_link_libraries(cubeb PRIVATE OpenSLES)
endif()

check_include_files(sys/soundcard.h HAVE_SYS_SOUNDCARD_H)
if(HAVE_SYS_SOUNDCARD_H)
  try_compile(USE_OSS "${PROJECT_BINARY_DIR}/compile_tests"
    ${PROJECT_SOURCE_DIR}/cmake/compile_tests/oss_is_v4.c)
  if(USE_OSS)
    # strlcpy is not available on BSD systems that use glibc,
    # like Debian kfreebsd, so try using libbsd if available
    include(CheckSymbolExists)
    check_symbol_exists(strlcpy string.h HAVE_STRLCPY)
    if(NOT HAVE_STRLCPY)
      pkg_check_modules(libbsd-overlay IMPORTED_TARGET libbsd-overlay)
      if(libbsd-overlay_FOUND)
        target_link_libraries(cubeb PRIVATE PkgConfig::libbsd-overlay)
        set(HAVE_STRLCPY true)
      endif()
    endif()
    if (HAVE_STRLCPY)
      target_sources(cubeb PRIVATE
        src/cubeb_oss.c)
      target_compile_definitions(cubeb PRIVATE USE_OSS)
    endif()
  endif()
endif()

check_include_files(android/log.h USE_AUDIOTRACK)
if(USE_AUDIOTRACK)
  target_sources(cubeb PRIVATE
    src/cubeb_audiotrack.c)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOTRACK)
  target_link_libraries(cubeb PRIVATE log)
endif()

check_include_files(sys/audioio.h USE_SUN)
if(USE_SUN)
  target_sources(cubeb PRIVATE
    src/cubeb_sun.c)
  target_compile_definitions(cubeb PRIVATE USE_SUN)
endif()

check_include_files(kai.h USE_KAI)
if(USE_KAI)
  target_sources(cubeb PRIVATE
    src/cubeb_kai.c)
  target_compile_definitions(cubeb PRIVATE USE_KAI)
  target_link_libraries(cubeb PRIVATE kai)
endif()

if(USE_PULSE AND USE_PULSE_RUST)
  include(ExternalProject)
  set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/rust)
  ExternalProject_Add(
    cubeb_pulse_rs
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release
    BUILD_ALWAYS ON
    BINARY_DIR "${PROJECT_SOURCE_DIR}/src/cubeb-pulse-rs"
    INSTALL_COMMAND ""
    LOG_BUILD ON)
  add_dependencies(cubeb cubeb_pulse_rs)
  target_compile_definitions(cubeb PRIVATE USE_PULSE_RUST)
  target_link_libraries(cubeb PRIVATE
    debug "${PROJECT_SOURCE_DIR}/src/cubeb-pulse-rs/target/debug/libcubeb_pulse.a"
    optimized "${PROJECT_SOURCE_DIR}/src/cubeb-pulse-rs/target/release/libcubeb_pulse.a" pulse)
endif()

if(USE_AUDIOUNIT AND USE_AUDIOUNIT_RUST)
  include(ExternalProject)
  set_directory_properties(PROPERTIES EP_PREFIX ${CMAKE_BINARY_DIR}/rust)
  ExternalProject_Add(
    cubeb_coreaudio_rs
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cargo build COMMAND cargo build --release
    BUILD_ALWAYS ON
    BINARY_DIR "${PROJECT_SOURCE_DIR}/src/cubeb-coreaudio-rs"
    INSTALL_COMMAND ""
    LOG_BUILD ON)
  add_dependencies(cubeb cubeb_coreaudio_rs)
  target_compile_definitions(cubeb PRIVATE USE_AUDIOUNIT_RUST)
  target_link_libraries(cubeb PRIVATE
    debug "${PROJECT_SOURCE_DIR}/src/cubeb-coreaudio-rs/target/debug/libcubeb_coreaudio.a"
    optimized "${PROJECT_SOURCE_DIR}/src/cubeb-coreaudio-rs/target/release/libcubeb_coreaudio.a")
endif()

