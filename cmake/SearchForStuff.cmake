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
	add_subdirectory(3rdparty/libwebp EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/xz EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/zstd EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/lz4 EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/D3D12MemAlloc EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/winpixeventruntime EXCLUDE_FROM_ALL)
	set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
	find_package(Vtune)

	# Don't try to build tests for WIL, it needs NuGet.
	set(WIL_BUILD_TESTS OFF CACHE BOOL "")
	set(WIL_BUILD_PACKAGING OFF CACHE BOOL "")
	add_subdirectory(3rdparty/wil EXCLUDE_FROM_ALL)
else()
	find_package(PCAP REQUIRED)
	find_package(LibLZMA REQUIRED)
	make_imported_target_if_missing(LibLZMA::LibLZMA LIBLZMA)

	# Using find_package OpenGL without either setting your opengl preference to GLVND or LEGACY
	# is deprecated as of cmake 3.11.
	if(USE_OPENGL)
		set(OpenGL_GL_PREFERENCE GLVND)
		find_package(OpenGL REQUIRED)
	endif()
	# On macOS, Mono.framework contains an ancient version of libpng.  We don't want that.
	# Avoid it by telling cmake to avoid finding frameworks while we search for libpng.
	set(FIND_FRAMEWORK_BACKUP ${CMAKE_FIND_FRAMEWORK})
	set(CMAKE_FIND_FRAMEWORK NEVER)
	find_package(PNG REQUIRED)
	find_package(CURL REQUIRED)
	set(CMAKE_FIND_FRAMEWORK ${FIND_FRAMEWORK_BACKUP})
	find_package(Vtune)

	# Use bundled ffmpeg v4.x.x headers if we can't locate it in the system.
	# We'll try to load it dynamically at runtime.
	find_package(FFMPEG COMPONENTS avcodec avformat avutil swresample swscale)
	if(NOT FFMPEG_FOUND)
		message(WARNING "FFmpeg not found, using bundled headers.")
		set(FFMPEG_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/3rdparty/ffmpeg/include")
	endif()

	find_package(ZLIB REQUIRED)
	find_package(Zstd REQUIRED)
	find_package(LZ4 REQUIRED)
	find_package(WebP REQUIRED)

	## Use CheckLib package to find module
	include(CheckLib)

	if(UNIX AND NOT APPLE)
		if(USE_OPENGL)
			check_lib(EGL EGL EGL/egl.h)
		endif()

		if(LINUX)
			check_lib(AIO aio libaio.h)
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

		find_package(Libbacktrace)
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(DBUS REQUIRED dbus-1)
	endif()
endif(WIN32)

# Require threads on all OSes.
find_package(Threads REQUIRED)

# Also need SDL2.
find_package(SDL2 2.28.5 REQUIRED)

set(ACTUALLY_ENABLE_TESTS ${ENABLE_TESTS})
if(ENABLE_TESTS)
	if(NOT EXISTS "${CMAKE_SOURCE_DIR}/3rdparty/gtest/CMakeLists.txt")
		message(WARNING "ENABLE_TESTS was on but gtest was not found, unit tests will not be enabled")
		set(ACTUALLY_ENABLE_TESTS Off)
	endif()
endif()

add_subdirectory(3rdparty/rapidyaml/rapidyaml EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/lzma EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/libchdr EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(libchdr)
add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)

# rapidyaml includes fast_float as a submodule, saves us pulling it in directly.
# Normally, we'd just pull in the cmake project, and link to it, but... it seems to enable
# permissive mode, which breaks other parts of PCSX2. So, we'll just create a target here
# for now.
#add_subdirectory(3rdparty/rapidyaml/rapidyaml/ext/c4core/src/c4/ext/fast_float EXCLUDE_FROM_ALL)
add_library(fast_float INTERFACE)
target_include_directories(fast_float INTERFACE 3rdparty/rapidyaml/rapidyaml/ext/c4core/src/c4/ext/fast_float/include)

add_subdirectory(3rdparty/jpgd EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/simpleini EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/imgui EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/cpuinfo EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cpuinfo)
add_subdirectory(3rdparty/zydis EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/libzip EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rcheevos EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/rapidjson EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/discord-rpc EXCLUDE_FROM_ALL)

if(USE_OPENGL)
	add_subdirectory(3rdparty/glad EXCLUDE_FROM_ALL)
endif()

if(USE_VULKAN)
	add_subdirectory(3rdparty/glslang EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/vulkan-headers EXCLUDE_FROM_ALL)
endif()

add_subdirectory(3rdparty/cubeb EXCLUDE_FROM_ALL)
disable_compiler_warnings_for_target(cubeb)
disable_compiler_warnings_for_target(speex)

# Find the Qt components that we need.
find_package(Qt6 6.6.0 COMPONENTS CoreTools Core GuiTools Gui WidgetsTools Widgets LinguistTools REQUIRED)

if(WIN32)
  add_subdirectory(3rdparty/rainterface EXCLUDE_FROM_ALL)
endif()

# Demangler for the debugger
add_subdirectory(3rdparty/demangler EXCLUDE_FROM_ALL)

# Prevent fmt from being built with exceptions, or being thrown at call sites.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFMT_EXCEPTIONS=0")
add_subdirectory(3rdparty/fmt/fmt EXCLUDE_FROM_ALL)

# Deliberately at the end. We don't want to set the flag on third-party projects.
if(MSVC)
	# Don't warn about "deprecated" POSIX functions.
	add_definitions("-D_CRT_NONSTDC_NO_WARNINGS" "-D_CRT_SECURE_NO_WARNINGS" "-DCRT_SECURE_NO_DEPRECATE")
endif()
