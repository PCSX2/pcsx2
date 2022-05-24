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
	add_subdirectory(3rdparty/pthreads4w EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/soundtouch EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/wil EXCLUDE_FROM_ALL)
	if (NOT PCSX2_CORE)
		add_subdirectory(3rdparty/wxwidgets3.0 EXCLUDE_FROM_ALL)
	endif()
	add_subdirectory(3rdparty/xz EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/D3D12MemAlloc EXCLUDE_FROM_ALL)
else()
	## Use cmake package to find module
	if (Linux)
		find_package(ALSA REQUIRED)
		make_imported_target_if_missing(ALSA::ALSA ALSA)
	endif()
	find_package(PCAP REQUIRED)
	find_package(Gettext) # translation tool
	find_package(LibLZMA REQUIRED)
	make_imported_target_if_missing(LibLZMA::LibLZMA LIBLZMA)

	# Using find_package OpenGL without either setting your opengl preference to GLVND or LEGACY
	# is deprecated as of cmake 3.11.
	if(USE_OPENGL)
		set(OpenGL_GL_PREFERENCE GLVND)
		find_package(OpenGL REQUIRED)
	endif()
	find_package(PNG REQUIRED)
	find_package(Vtune)

	if(NOT PCSX2_CORE)
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
	endif()

	find_package(ZLIB REQUIRED)

	## Use pcsx2 package to find module
	include(FindLibc)

	## Use pcsx2 package to find module
	include(FindPulseAudio)

	## Use CheckLib package to find module
	include(CheckLib)

	if(UNIX AND NOT APPLE)
		if(USE_OPENGL)
			check_lib(EGL EGL EGL/egl.h)
		endif()
		if(X11_API)
			check_lib(X11_XCB X11-xcb X11/Xlib-xcb.h)
			check_lib(XCB xcb xcb/xcb.h)
			check_lib(XRANDR xrandr)
		endif()

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

	check_lib(SOUNDTOUCH SoundTouch SoundTouch.h PATH_SUFFIXES soundtouch)
	check_lib(SAMPLERATE samplerate samplerate.h)

	if(NOT QT_BUILD)
		find_optional_system_library(SDL2 3rdparty/sdl2 2.0.12)
	endif()

	if(UNIX AND NOT APPLE)
		find_package(X11 REQUIRED)
		make_imported_target_if_missing(X11::X11 X11)

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
		## Use pcsx2 package to find module
		find_package(HarfBuzz)
		endif()
		if(WAYLAND_API)
			find_package(Wayland REQUIRED)
		endif()
	endif()
endif(WIN32)

# Require threads on all OSes.
find_package(Threads REQUIRED)

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

if(NOT PCSX2_CORE)
	WX_vs_SDL()
endif()

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

find_optional_system_library(fmt 3rdparty/fmt/fmt 7.1.3)
find_optional_system_library(ryml 3rdparty/rapidyaml/rapidyaml 0.4.0)
find_optional_system_library(zstd 3rdparty/zstd 1.4.5)
if (${zstd_TYPE} STREQUAL System)
	alias_library(Zstd::Zstd zstd::libzstd_shared)
	alias_library(pcsx2-zstd zstd::libzstd_shared)
endif()
find_optional_system_library(libzip 3rdparty/libzip 1.8.0)

if(QT_BUILD)
	# Default to bundled Qt6 for Windows.
	if(WIN32 AND NOT DEFINED Qt6_DIR)
		set(Qt6_DIR ${CMAKE_SOURCE_DIR}/3rdparty/qt/6.3.0/msvc2019_64/lib/cmake/Qt6)
	endif()

	# Find the Qt components that we need.
	find_package(Qt6 COMPONENTS CoreTools Core GuiTools Gui WidgetsTools Widgets Network LinguistTools REQUIRED)

	# We use the bundled (latest) SDL version for Qt.
	find_optional_system_library(SDL2 3rdparty/sdl2 2.0.22)
endif()

add_subdirectory(3rdparty/lzma EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/libchdr EXCLUDE_FROM_ALL)

if(USE_NATIVE_TOOLS)
	add_subdirectory(tools/bin2cpp EXCLUDE_FROM_ALL)
	set(BIN2CPP bin2cpp)
	set(BIN2CPPDEP bin2cpp)
else()
	set(BIN2CPP perl ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl)
	set(BIN2CPPDEP ${CMAKE_SOURCE_DIR}/linux_various/hex2h.pl)
endif()

add_subdirectory(3rdparty/simpleini EXCLUDE_FROM_ALL)
add_subdirectory(3rdparty/imgui EXCLUDE_FROM_ALL)

if(USE_OPENGL)
	add_subdirectory(3rdparty/glad EXCLUDE_FROM_ALL)
endif()

if(USE_VULKAN)
	add_subdirectory(3rdparty/glslang EXCLUDE_FROM_ALL)
	add_subdirectory(3rdparty/vulkan-headers EXCLUDE_FROM_ALL)
endif()

if(CUBEB_API)
	add_subdirectory(3rdparty/cubeb EXCLUDE_FROM_ALL)
	target_compile_options(cubeb PRIVATE "-w")
	target_compile_options(speex PRIVATE "-w")
endif()

