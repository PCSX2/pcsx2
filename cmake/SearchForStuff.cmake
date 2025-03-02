#-------------------------------------------------------------------------------
#                       Search all libraries on the system
#-------------------------------------------------------------------------------
find_package(Git)

# Require threads on all OSes.
find_package(Threads REQUIRED)

# Dependency libraries.
# On macOS, Mono.framework contains an ancient version of libpng.  We don't want that.
# Avoid it by telling cmake to avoid finding frameworks while we search for libpng.
set(FIND_FRAMEWORK_BACKUP ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK NEVER)
find_package(PNG 1.6.40 REQUIRED)
find_package(JPEG REQUIRED) # No version because flatpak uses libjpeg-turbo.
find_package(ZLIB REQUIRED) # v1.3, but Mac uses the SDK version.
find_package(Zstd 1.5.5 REQUIRED)
find_package(LZ4 REQUIRED)
find_package(WebP REQUIRED) # v1.3.2, spews an error on Linux because no pkg-config.
find_package(SDL3 3.2.6 REQUIRED)
find_package(Freetype 2.11.1 REQUIRED)

if(USE_VULKAN)
	find_package(Shaderc REQUIRED)
endif()

# Platform-specific dependencies.
if (WIN32)
	add_subdirectory(3rdparty/D3D12MemAlloc EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/winpixeventruntime EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/winwil EXCLUDE_FROM_ALL)
	set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
	find_package(Vtune)
else()
	find_package(CURL REQUIRED)
	find_package(PCAP REQUIRED)
	find_package(Vtune)

	# Use bundled ffmpeg v4.x.x headers if we can't locate it in the system.
	# We'll try to load it dynamically at runtime.
	find_package(FFMPEG COMPONENTS avcodec avformat avutil swresample swscale)
	if(NOT FFMPEG_FOUND)
		message(WARNING "FFmpeg not found, using bundled headers.")
		set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
	endif()

	## Use CheckLib package to find module
	include(CheckLib)

	if(UNIX AND NOT APPLE)
		if(LINUX)
			check_lib(LIBUDEV libudev libudev.h)
		endif()

		if(X11_API)
			find_package(X11 REQUIRED)
			if (NOT X11_Xrandr_FOUND)
				message(FATAL_ERROR "XRandR extension is required")
			endif()
		endif()

		if(WAYLAND_API)
			find_package(ECM REQUIRED NO_MODULE)
			list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")
			find_package(Wayland REQUIRED Egl)
		endif()

		if(USE_BACKTRACE)
			find_package(Libbacktrace REQUIRED)
		endif()

		find_package(PkgConfig REQUIRED)
		pkg_check_modules(DBUS REQUIRED dbus-1)
	endif()
endif()

set(CMAKE_FIND_FRAMEWORK ${FIND_FRAMEWORK_BACKUP})

add_subdirectory(3rdparty/fast_float EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rapidyaml EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/lzma EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/libchdr EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(libchdr)
add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simpleini EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/imgui EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cpuinfo EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cpuinfo)
add_subdirectory(3rdparty/libzip EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rcheevos EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rapidjson EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/discord-rpc EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/freesurround EXCLUDE_FROM_ALL)

if(USE_OPENGL)
	add_subdirectory(3rdparty/glad EXCLUDE_FROM_ALL)
endif()

if(USE_VULKAN)
	add_subdirectory(3rdparty/vulkan EXCLUDE_FROM_ALL)
endif()

add_subdirectory(3rdparty/cubeb EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cubeb)
disable_compiler_warnings_for_target(speex)

# Find the Qt components that we need.
find_package(Qt6 6.7.2 COMPONENTS CoreTools Core GuiTools Gui WidgetsTools Widgets LinguistTools REQUIRED)

if(WIN32)
  add_subdirectory(3rdparty/rainterface EXCLUDE_FROM_ALL)
endif()

# Demangler for the debugger.
add_subdirectory(3rdparty/demangler EXCLUDE_FROM_ALL)

# Symbol table parser.
add_subdirectory(3rdparty/ccc EXCLUDE_FROM_ALL)

# Architecture-specific.
if(_M_X86)
	add_subdirectory(3rdparty/zydis EXCLUDE_FROM_ALL)
elseif(_M_ARM64)
	add_subdirectory(3rdparty/vixl EXCLUDE_FROM_ALL)
endif()

# Prevent fmt from being built with exceptions, or being thrown at call sites.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_USE_EXCEPTIONS=0 -DFMT_USE_RTTI=0")
add_subdirectory(3rdparty/fmt EXCLUDE_FROM_ALL)

# Deliberately at the end. We don't want to set the flag on third-party projects.
if(MSVC)
	# Don't warn about "deprecated" POSIX functions.
	add_definitions("-D_CRT_NONSTDC_NO_WARNINGS" "-D_CRT_SECURE_NO_WARNINGS" "-DCRT_SECURE_NO_DEPRECATE")
endif()
