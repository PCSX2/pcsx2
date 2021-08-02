# - Find PulseAudio includes and libraries
#
#   PULSEAUDIO_FOUND        - True if PULSEAUDIO_INCLUDE_DIR &
#                             PULSEAUDIO_LIBRARY are found
#   PulseAudio::PulseAudio  - Imported target for PulseAudio, if found
#   PULSEAUDIO_LIBRARIES    - Set when PULSEAUDIO_LIBRARY is found
#   PULSEAUDIO_INCLUDE_DIRS - Set when PULSEAUDIO_INCLUDE_DIR is found
#
#   PULSEAUDIO_INCLUDE_DIR - where to find pulse/pulseaudio.h, etc.
#   PULSEAUDIO_LIBRARY     - the pulse library
#   PULSEAUDIO_VERSION_STRING - the version of PulseAudio found
#

find_path(PULSEAUDIO_INCLUDE_DIR
          NAMES pulse/pulseaudio.h
          DOC "The PulseAudio include directory"
)

find_library(PULSEAUDIO_LIBRARY
             NAMES pulse
             DOC "The PulseAudio library"
)

if(PULSEAUDIO_INCLUDE_DIR AND EXISTS "${PULSEAUDIO_INCLUDE_DIR}/pulse/version.h")
	file(STRINGS "${PULSEAUDIO_INCLUDE_DIR}/pulse/version.h" pulse_version_str
	     REGEX "^#define[\t ]+pa_get_headers_version\\(\\)[\t ]+\\(\".*\"\\)")

	string(REGEX REPLACE "^.*pa_get_headers_version\\(\\)[\t ]+\\(\"([^\"]*)\"\\).*$" "\\1"
	       PULSEAUDIO_VERSION_STRING "${pulse_version_str}")
	unset(pulse_version_str)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PulseAudio
	REQUIRED_VARS PULSEAUDIO_LIBRARY PULSEAUDIO_INCLUDE_DIR
	VERSION_VAR PULSEAUDIO_VERSION_STRING
)

if(PULSEAUDIO_LIBRARY AND NOT TARGET PulseAudio::PulseAudio)
	add_library(PulseAudio::PulseAudio UNKNOWN IMPORTED GLOBAL)
	set_target_properties(PulseAudio::PulseAudio PROPERTIES
		IMPORTED_LOCATION "${PULSEAUDIO_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${PULSEAUDIO_INCLUDE_DIR}")
endif()

if(PULSEAUDIO_FOUND)
	set(PULSEAUDIO_LIBRARIES ${PULSEAUDIO_LIBRARY})
	set(PULSEAUDIO_INCLUDE_DIRS ${PULSEAUDIO_INCLUDE_DIR})
endif()

mark_as_advanced(PULSEAUDIO_INCLUDE_DIR PULSEAUDIO_LIBRARY)
