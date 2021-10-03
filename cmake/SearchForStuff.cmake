#-------------------------------------------------------------------------------
#                       Search all libraries on the system
#-------------------------------------------------------------------------------
if(EXISTS ${PROJECT_SOURCE_DIR}/.git)
	find_package(Git)
endif()
if (WIN32)
	# We bundle everything on Windows
	add_subdirectory(3rdparty/zlib EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/libpng EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/libjpeg EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/libsamplerate EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/baseclasses EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/freetype EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/portaudio EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/pthreads4w EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/wil EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/wxwidgets3.0 EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/xz EXCLUDE_FROM_ALL)
else()
	## Use cmake package to find module
	if (Linux)
		find_package(ALSA REQUIRED)
		make_imported_target_if_missing(ALSA::ALSA ALSA)
	endif()
	find_package(PCAP REQUIRED)
	find_package(LibXml2 REQUIRED)
	make_imported_target_if_missing(LibXml2::LibXml2 LibXml2)
	find_package(Freetype REQUIRED) # GS OSD
	find_package(Gettext) # translation tool
	find_package(LibLZMA REQUIRED)
	make_imported_target_if_missing(LibLZMA::LibLZMA LIBLZMA)

	# Using find_package OpenGL without either setting your opengl preference to GLVND or LEGACY
	# is deprecated as of cmake 3.11.
	set(OpenGL_GL_PREFERENCE GLVND)
	find_package(OpenGL REQUIRED)
	find_package(PNG REQUIRED)
	find_package(Vtune)

	# Does not require the module (allow to compile non-wx plugins)
	# Force the unicode build (the variable is only supported on cmake 2.8.3 and above)
	# Warning do not put any double-quote for the argument...
	# set(wxWidgets_CONFIG_OPTIONS --unicode=yes --debug=yes) # In case someone want to debug inside wx
	#
	# Fedora uses an extra non-standard option ... Arch must be the first option.
	# They do uname -m if missing so only fix for cross compilations.
	# http://pkgs.fedoraproject.org/cgit/wxGTK.git/plain/wx-config
	if(Fedora AND CMAKE_CROSSCOMPILING)
		set(wxWidgets_CONFIG_OPTIONS --arch ${PCSX2_TARGET_ARCHITECTURES} --unicode=yes)
	else()
		set(wxWidgets_CONFIG_OPTIONS --unicode=yes)
	endif()

	# I'm removing the version check, because it excludes newer versions and requires specifically 3.0.
	#list(APPEND wxWidgets_CONFIG_OPTIONS --version=3.0)

	# The wx version must be specified so a mix of gtk2 and gtk3 isn't used
	# as that can cause compile errors.
	if(GTK2_API AND NOT APPLE)
		list(APPEND wxWidgets_CONFIG_OPTIONS --toolkit=gtk2)
	elseif(NOT APPLE)
		list(APPEND wxWidgets_CONFIG_OPTIONS --toolkit=gtk3)
	endif()

	# wx2.8 => /usr/bin/wx-config-2.8
	# lib32-wx2.8 => /usr/bin/wx-config32-2.8
	# wx3.0 => /usr/bin/wx-config-3.0
	# I'm going to take a wild guess and predict this:
	# lib32-wx3.0 => /usr/bin/wx-config32-3.0
	# FindwxWidgets only searches for wx-config.
	if(CMAKE_CROSSCOMPILING)
		# May need to fix the filenames for lib32-wx3.0.
		if(${PCSX2_TARGET_ARCHITECTURES} MATCHES "i386")
			if (Fedora AND EXISTS "/usr/bin/wx-config-3.0")
				set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.0")
			endif()
			if (EXISTS "/usr/bin/wx-config32")
				set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config32")
			endif()
			if (EXISTS "/usr/bin/wx-config32-3.0")
				set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config32-3.0")
			endif()
		endif()
	else()
		if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/local/bin/wxgtk3u-3.0-config")
		endif()
		if(EXISTS "/usr/bin/wx-config-3.2")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.2")
		endif()
		if(EXISTS "/usr/bin/wx-config-3.1")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.1")
		endif()
		if(EXISTS "/usr/bin/wx-config-3.0")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.0")
		endif()
		if(EXISTS "/usr/bin/wx-config")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config")
		endif()
		if(NOT GTK2_API AND EXISTS "/usr/bin/wx-config-gtk3")
			set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-gtk3")
		endif()
	endif()

	find_package(wxWidgets REQUIRED base core adv)
	include(${wxWidgets_USE_FILE})
	make_imported_target_if_missing(wxWidgets::all wxWidgets)

	find_package(ZLIB REQUIRED)

	## Use pcsx2 package to find module
	include(FindLibc)

	## Use pcsx2 package to find module
	include(FindPulseAudio)

	## Use CheckLib package to find module
	include(CheckLib)

	if(UNIX AND NOT APPLE)
		check_lib(EGL EGL EGL/egl.h)
		check_lib(X11_XCB X11-xcb X11/Xlib-xcb.h)
		check_lib(XCB xcb xcb/xcb.h)

		if(Linux)
			check_lib(AIO aio libaio.h)
			# There are two udev pkg config files - udev.pc (wrong), libudev.pc (correct)
			# When cross compiling, pkg-config will be skipped so we have to look for
			# udev (it'll automatically be prefixed with lib). But when not cross
			# compiling, we have to look for libudev.pc. Argh. Hence the silliness below.
			if(CMAKE_CROSSCOMPILING)
				check_lib(LIBUDEV udev libudev.h)
			else()
				check_lib(LIBUDEV libudev libudev.h)
			endif()
		endif()
	endif()

	if(PORTAUDIO_API)
		check_lib(PORTAUDIO portaudio portaudio.h pa_linux_alsa.h)
	endif()
	check_lib(SOUNDTOUCH SoundTouch SoundTouch.h PATH_SUFFIXES soundtouch)
	check_lib(SAMPLERATE samplerate samplerate.h)

	if(SDL2_API)
		check_lib(SDL2 SDL2 SDL.h PATH_SUFFIXES SDL2)
		alias_library(SDL::SDL PkgConfig::SDL2)
	else()
		# Tell cmake that we use SDL as a library and not as an application
		set(SDL_BUILDING_LIBRARY TRUE)
		find_package(SDL REQUIRED)
	endif()

	if(UNIX AND NOT APPLE)
		find_package(X11 REQUIRED)
		make_imported_target_if_missing(X11::X11 X11)
	endif()
	if(UNIX)
		# Most plugins (if not all) and PCSX2 core need gtk2, so set the required flags
		if (GTK2_API)
			find_package(GTK2 REQUIRED gtk)
			alias_library(GTK::gtk GTK2::gtk)
		else()
		if(CMAKE_CROSSCOMPILING)
			find_package(GTK3 REQUIRED gtk)
			alias_library(GTK::gtk GTK3::gtk)
		else()
			check_lib(GTK3 gtk+-3.0 gtk/gtk.h)
			alias_library(GTK::gtk PkgConfig::GTK3)
		endif()
		endif()
	endif()

	#----------------------------------------
	#           Use system include
	#----------------------------------------
	find_package(HarfBuzz)

endif(WIN32)

set(ACTUALLY_ENABLE_TESTS ${ENABLE_TESTS})
if(ENABLE_TESTS)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/gtest/CMakeLists.txt")
		message(WARNING "ENABLE_TESTS was on but gtest was not found, unit tests will not be enabled")
		set(ACTUALLY_ENABLE_TESTS Off)
	endif()
endif()

#----------------------------------------
# Check correctness of the parameter
# Note: wxWidgets_INCLUDE_DIRS must be defined
#----------------------------------------
include(ApiValidation)

WX_vs_SDL()

# Blacklist bad GCC
if(GCC_VERSION VERSION_EQUAL "7.0" OR GCC_VERSION VERSION_EQUAL "7.1")
	GCC7_BUG()
endif()

if((GCC_VERSION VERSION_EQUAL "9.0" OR GCC_VERSION VERSION_GREATER "9.0") AND GCC_VERSION LESS "9.2")
	message(WARNING "
	It looks like you are compiling with 9.0.x or 9.1.x. Using these versions is not recommended,
	as there is a bug known to cause the compiler to segfault while compiling. See patch
	https://gitweb.gentoo.org/proj/gcc-patches.git/commit/?id=275ab714637a64672c6630cfd744af2c70957d5a
	Even with that patch, compiling with LTO may still segfault. Use at your own risk!
	This text being in a compile log in an open issue may cause it to be closed.")
endif()

find_package(fmt "7.1.3" QUIET)
if(NOT fmt_FOUND)
	if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/fmt/fmt/CMakeLists.txt")
		message(STATUS "No system fmt was found. Using bundled")
		add_subdirectory(3rdparty/fmt/fmt)
	else()
		message(FATAL_ERROR "No system or bundled fmt was found")
	endif()
else()
	message(STATUS "Found fmt: ${fmt_VERSION}")
endif()

if(USE_SYSTEM_YAML)
	find_package(yaml-cpp "0.6.3" QUIET)
	if(NOT yaml-cpp_FOUND)
		message(STATUS "No system yaml-cpp was found")
		set(USE_SYSTEM_YAML OFF)
	else()
		message(STATUS "Found yaml-cpp: ${yaml-cpp_VERSION}")
		message(STATUS "Note that the latest release of yaml-cpp is very outdated, and the bundled submodule in the repo has over a year of bug fixes and as such is preferred.")
	endif()
endif()

if(NOT USE_SYSTEM_YAML)
	if(EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/yaml-cpp/yaml-cpp/CMakeLists.txt")
		message(STATUS "Using bundled yaml-cpp")
		add_subdirectory(3rdparty/yaml-cpp/yaml-cpp EXCLUDE_FROM_ALL)
		if (NOT MSVC)
			# Remove once https://github.com/jbeder/yaml-cpp/pull/815 is merged
			target_compile_options(yaml-cpp PRIVATE -Wno-shadow)
		endif()
	else()
		message(FATAL_ERROR "No bundled yaml-cpp was found")
	endif()
endif()

add_subdirectory(3rdparty/libchdr/libchdr EXCLUDE_FROM_ALL)

if(USE_NATIVE_TOOLS)
	add_subdirectory(tools/bin2cpp EXCLUDE_FROM_ALL)
	set(BIN2CPP bin2cpp)
	set(BIN2CPPDEP bin2cpp)
else()
	set(BIN2CPP perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl)
	set(BIN2CPPDEP ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl)
endif()
