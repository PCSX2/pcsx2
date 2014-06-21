# - Try to find Portaudio
# Once done this will define
#
#  PORTAUDIO_FOUND - system has Portaudio
#  PORTAUDIO_INCLUDE_DIRS - the Portaudio include directory
#  PORTAUDIO_LIBRARIES - Link these to use Portaudio

include(FindPkgConfig)
pkg_check_modules(PC_PORTAUDIO portaudio-2.0)

find_path(PORTAUDIO_INCLUDE_DIRS
  NAMES
    portaudio.h
  PATHS
      /usr/local/include
      /usr/include
  HINTS
    ${PC_PORTAUDIO_INCLUDEDIR}
)

find_library(PORTAUDIO_LIBRARIES
  NAMES
    portaudio
  PATHS
      /usr/local/lib
      /usr/lib
      /usr/lib64
  HINTS
    ${PC_PORTAUDIO_LIBDIR}
)

mark_as_advanced(PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES)

# Found PORTAUDIO, but it may be version 18 which is not acceptable.
if(EXISTS ${PORTAUDIO_INCLUDE_DIRS}/portaudio.h)
  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES ${PORTAUDIO_INCLUDE_DIRS})
  CHECK_CXX_SOURCE_COMPILES(
    "#include <portaudio.h>\nPaDeviceIndex pa_find_device_by_name(const char *name); int main () {return 0;}"
    PORTAUDIO2_FOUND)
  cmake_pop_check_state()
  if(PORTAUDIO2_FOUND)
    INCLUDE(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(PORTAUDIO DEFAULT_MSG PORTAUDIO_INCLUDE_DIRS PORTAUDIO_LIBRARIES)
  else(PORTAUDIO2_FOUND)
    message(STATUS
      "  portaudio.h not compatible (requires API 2.0)")
    set(PORTAUDIO_FOUND FALSE)
  endif(PORTAUDIO2_FOUND)
endif()
