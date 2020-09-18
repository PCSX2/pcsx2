#-------------------------------------------------------------------------------
#						Search all libraries on the system
#-------------------------------------------------------------------------------
## Use cmake package to find module
if (Linux)
    find_package(ALSA)
    find_package(PCAP)
    find_package(LibXml2)
endif()
find_package(Freetype) # GSdx OSD
find_package(Gettext) # translation tool
if(EXISTS ${PROJECT_SOURCE_DIR}/.git)
    find_package(Git)
endif()
find_package(LibLZMA)

# Using find_package OpenGL without either setting your opengl preference to GLVND or LEGACY
# is deprecated as of cmake 3.11.
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL)
find_package(PNG)
find_package(Vtune)
# The requirement of wxWidgets is checked in SelectPcsx2Plugins module
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

list(APPEND wxWidgets_CONFIG_OPTIONS --version=3.0)

if(GTK3_API AND NOT APPLE)
    list(APPEND wxWidgets_CONFIG_OPTIONS --toolkit=gtk3)
elseif(NOT APPLE)
    list(APPEND wxWidgets_CONFIG_OPTIONS --toolkit=gtk2)
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
    if(EXISTS "/usr/bin/wx-config-3.0")
        set(wxWidgets_CONFIG_EXECUTABLE "/usr/bin/wx-config-3.0")
    endif()
endif()

find_package(wxWidgets COMPONENTS base core adv)
find_package(ZLIB)

## Use pcsx2 package to find module
include(FindLibc)

## Only needed by the extra plugins
if(EXTRA_PLUGINS)
    include(FindCg)
    include(FindGlew)
    find_package(JPEG)
endif()

## Use CheckLib package to find module
include(CheckLib)
if(Linux)
    check_lib(EGL EGL EGL/egl.h)
    check_lib(X11_XCB X11-xcb X11/Xlib-xcb.h)
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
if(PORTAUDIO_API)
    check_lib(PORTAUDIO portaudio portaudio.h pa_linux_alsa.h)
endif()
check_lib(SOUNDTOUCH SoundTouch soundtouch/SoundTouch.h)

if(SDL2_API)
    check_lib(SDL2 SDL2 SDL.h PATH_SUFFIXES SDL2)
else()
    # Tell cmake that we use SDL as a library and not as an application
    set(SDL_BUILDING_LIBRARY TRUE)
    find_package(SDL)
endif()

if(UNIX)
    find_package(X11)
    # Most plugins (if not all) and PCSX2 core need gtk2, so set the required flags
    if (GTK3_API)
        if(CMAKE_CROSSCOMPILING)
            find_package(GTK3 REQUIRED gtk)
        else()
            check_lib(GTK3 gtk+-3.0 gtk/gtk.h)
        endif()
    else()
        find_package(GTK2 REQUIRED gtk)
    endif()
endif()

#----------------------------------------
#		    Use system include
#----------------------------------------
if(UNIX)
	if(GTK2_FOUND)
		include_directories(${GTK2_INCLUDE_DIRS})
    elseif(GTK3_FOUND)
		include_directories(${GTK3_INCLUDE_DIRS})
        # A lazy solution
        set(GTK2_LIBRARIES ${GTK3_LIBRARIES})
	endif()

	if(X11_FOUND)
		include_directories(${X11_INCLUDE_DIR})
	endif()
endif()

if(ALSA_FOUND)
	include_directories(${ALSA_INCLUDE_DIRS})
endif()

if(CG_FOUND)
	include_directories(${CG_INCLUDE_DIRS})
endif()

if(JPEG_FOUND)
	include_directories(${JPEG_INCLUDE_DIR})
endif()

if(GLEW_FOUND)
    include_directories(${GLEW_INCLUDE_DIR})
endif()

if(OPENGL_FOUND)
	include_directories(${OPENGL_INCLUDE_DIR})
endif()

if(SDL_FOUND AND NOT SDL2_API)
	include_directories(${SDL_INCLUDE_DIR})
endif()

if(USE_VTUNE AND VTUNE_FOUND)
    include_directories(${VTUNE_INCLUDE_DIRS})
endif()

if(wxWidgets_FOUND)
	include(${wxWidgets_USE_FILE})
endif()

if(PCAP_FOUND)
	include_directories(${PCAP_INCLUDE_DIR})
endif()

if(LIBXML2_FOUND)
	include_directories(${LIBXML2_INCLUDE_DIRS})
endif()

if(ZLIB_FOUND)
	include_directories(${ZLIB_INCLUDE_DIRS})
endif()

find_package(HarfBuzz)

if(HarfBuzz_FOUND)
include_directories(${HarfBuzz_INCLUDE_DIRS})
endif()

set(ACTUALLY_ENABLE_TESTS ${ENABLE_TESTS})
if(ENABLE_TESTS)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/gtest/CMakeLists.txt")
		message(WARNING "ENABLE_TESTS was on but gtest was not found, unit tests will not be enabled")
		set(ACTUALLY_ENABLE_TESTS Off)
	endif()
endif()

#----------------------------------------
#  Use  project-wide include directories
#----------------------------------------
include_directories(${CMAKE_SOURCE_DIR}/common/include
					${CMAKE_SOURCE_DIR}/common/include/Utilities
					${CMAKE_SOURCE_DIR}/common/include/x86emitter
                    # File generated by Cmake
                    ${CMAKE_BINARY_DIR}/common/include
                    )

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
