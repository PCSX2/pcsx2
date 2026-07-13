# Setting it to a range tells it that it supports the features on the newer
# versions as well, avoiding setting policies.
cmake_minimum_required(VERSION 3.16...3.25)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enabling this cmake policy gets rid of warnings regarding LTO.
cmake_policy(SET CMP0069 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR "PCSX2 does not support in-tree builds.  Please make a build directory that is not the PCSX2 source directory and generate your CMake project there using either `cmake -B build_directory` or by running cmake from the build directory.")
endif()

set(ARMSX2_VERSION_MAJOR 2)
set(ARMSX2_VERSION_MINOR 4)
set(ARMSX2_VERSION_PATCH 1)
set(ARMSX2_VERSION "${ARMSX2_VERSION_MAJOR}.${ARMSX2_VERSION_MINOR}.${ARMSX2_VERSION_PATCH}")
set(ARMSX2_BUILD_NUMBER 241)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
	set(CMAKE_C_COMPILER_WORKS 1)
	set(CMAKE_CXX_COMPILER_WORKS 1)

	option(ARMSX2_REAL_DEVICE "Build for real iOS device (iphoneos SDK)" OFF)
	option(ARMSX2_MAC_CATALYST "Build the ARMSX2 UIKit app for Mac Catalyst" OFF)
	if(ARMSX2_MAC_CATALYST)
		set(ARMSX2_DEFAULT_BUNDLE_IDENTIFIER "com.armsx2.macos")
	else()
		set(ARMSX2_DEFAULT_BUNDLE_IDENTIFIER "com.armsx2.ios")
	endif()
	set(ARMSX2_BUNDLE_IDENTIFIER "${ARMSX2_DEFAULT_BUNDLE_IDENTIFIER}" CACHE STRING "Apple bundle identifier")
	set(ARMSX2_DEVELOPMENT_TEAM "" CACHE STRING "Apple Development Team ID for device signing")
	if(ARMSX2_REAL_DEVICE OR ARMSX2_MAC_CATALYST)
		set(ARMSX2_JIT_ENTITLEMENTS_DEFAULT ON)
	else()
		set(ARMSX2_JIT_ENTITLEMENTS_DEFAULT OFF)
	endif()
	option(ARMSX2_ENABLE_JIT_ENTITLEMENTS "Attach the Apple JIT entitlement file for provisioning profiles which allow it" ${ARMSX2_JIT_ENTITLEMENTS_DEFAULT})
	option(ARMSX2_IOS_DSYM "Emit a .dSYM for crash symbolication (injects -g into Release; costs link time + artifact size)" OFF)
	set(ARMSX2_IOS_DEPLOYMENT_TARGET "17.0" CACHE STRING "Minimum iOS deployment target")
	set(CMAKE_OSX_DEPLOYMENT_TARGET "${ARMSX2_IOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum iOS deployment target" FORCE)
	set(ARMSX2_MACOS_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum Mac Catalyst deployment target")
	if(ARMSX2_REAL_DEVICE OR ARMSX2_MAC_CATALYST)
		set(CMAKE_OSX_SYSROOT "iphoneos")
	else()
		set(CMAKE_OSX_SYSROOT "iphonesimulator")
	endif()
	set(CMAKE_OSX_ARCHITECTURES "arm64")
	set(USE_OPENGL OFF CACHE BOOL "Enable OpenGL renderer" FORCE)
	set(USE_VULKAN OFF CACHE BOOL "Enable Vulkan renderer" FORCE)
	set(SDL_OPENGL OFF CACHE BOOL "Enable SDL OpenGL support" FORCE)
	set(SDL_OPENGLES OFF CACHE BOOL "Enable SDL OpenGL ES support" FORCE)
	# iOS uses its own SwiftUI frontend, not the desktop Qt GUI or test harnesses.
	set(ENABLE_QT_UI OFF CACHE BOOL "Enables building the PCSX2 Qt interface." FORCE)
	set(ENABLE_TESTS OFF CACHE BOOL "Enables building the unit tests" FORCE)
	set(ENABLE_GSRUNNER OFF CACHE BOOL "Enables building the GSRunner" FORCE)
endif()

# Project Name
project(ARMSX2iOS VERSION ${ARMSX2_VERSION} LANGUAGES C CXX)

add_compile_definitions(ARMSX2_VERSION_STR="${ARMSX2_VERSION}")

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
	set(_M_ARM64 TRUE)
	set(_M_X86 FALSE)
	set(CMAKE_HOST_SYSTEM_PROCESSOR "arm64")
	set(CMAKE_SYSTEM_PROCESSOR "arm64")

	if(CMAKE_GENERATOR STREQUAL "Xcode")
		if(ARMSX2_MAC_CATALYST OR NOT ARMSX2_REAL_DEVICE)
			set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
			set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
			set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual")
			set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
			set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "")
		endif()
	endif()
endif()

# Variable to check that people use the good file
set(TOP_CMAKE_WAS_SOURCED TRUE)

# Absolute path to the repository root (single source of truth for the core).
# The PCSX2 core / common / 3rdparty are NO LONGER vendored under this iOS app;
# they are built from the repo root. See REFACTOR_STATUS.md.
get_filename_component(ARMSX2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

# Use the iOS-aware cmake modules vendored beside this entry point (copies of
# the known-good modules: Pcsx2Utils has the IOS branch + a cross-compile-safe
# page-size detect, BuildParameters sets Apple-Silicon ARM64 flags, SearchForStuff
# builds all deps from vendored sources instead of desktop find_package). The
# root cmake/ modules are desktop-only and do system find_package() calls iOS
# cannot satisfy, so we point at our LOCAL dir (mirroring how Android does it).
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Write binaries to the bin directory under the build directory.
# This makes it simple to run development builds directly out of the build directory.
if (UNIX AND NOT APPLE)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

# include some generic functions to ensure correctness of the env
include(Pcsx2Utils)

check_no_parenthesis_in_path()
detect_operating_system()
detect_compiler()

#-------------------------------------------------------------------------------
# Include specific module
include(BuildParameters)
include(SearchForStuff)

option(USE_DISCORD_SDK "Enable Discord SDK / Discord Rich Presence support" OFF)

if(ANDROID)
	if(USE_DISCORD_SDK)
		find_package(discord_partner_sdk CONFIG)
	endif()
	# Link all native binaries with 16KB page support for newer devices.
	add_link_options(-Wl,-z,max-page-size=16384)
	# Belt-and-suspenders: ensure every shared/module target inherits the flag.
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-z,max-page-size=16384")
endif()

# Must be done after SearchForStuff
get_git_version_info()
write_svnrev_h()

# Build the shared core from the repository root. Explicit binary dirs are
# required because the source dirs live outside this iOS app tree.
# NOTE: the individual 3rdparty libs are pulled in by SearchForStuff (above),
# sourced from ${ARMSX2_ROOT}/3rdparty and the Android-vendored tree. The root
# 3rdparty/ has no aggregate CMakeLists, so add_subdirectory'ing it would
# FATAL; do not re-add it here.

# make common
add_subdirectory(${ARMSX2_ROOT}/common ${CMAKE_BINARY_DIR}/common)

# make pcsx2
add_subdirectory(${ARMSX2_ROOT}/pcsx2 ${CMAKE_BINARY_DIR}/pcsx2)
#add_subdirectory(pcsx2-qt)

# Updater is Windows only for now.
if (WIN32)
	add_subdirectory(updater)
endif()

## tests
#if(ENABLE_TESTS)
#	add_subdirectory(3rdparty/googletest)
#	add_subdirectory(tests/ctest)
#endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
	enable_language(Swift)

	# Swift sources are globbed so newly-added files are picked up automatically at
	# configure time. CONFIGURE_DEPENDS makes CMake re-scan when files change.
	file(GLOB_RECURSE SWIFT_SOURCES CONFIGURE_DEPENDS
		${CMAKE_SOURCE_DIR}/../swift/*.swift
	)

	configure_file("${CMAKE_SOURCE_DIR}/Info.plist.in" "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)

	if(ARMSX2_REAL_DEVICE OR ARMSX2_MAC_CATALYST)
		set(ARMSX2_CODE_SIGNING_ALLOWED "YES")
		set(ARMSX2_CODE_SIGNING_REQUIRED "YES")
		set(ARMSX2_CODE_SIGN_STYLE "Automatic")
		set(ARMSX2_CODE_SIGN_IDENTITY "Apple Development")
	else()
		set(ARMSX2_CODE_SIGNING_ALLOWED "NO")
		set(ARMSX2_CODE_SIGNING_REQUIRED "NO")
		set(ARMSX2_CODE_SIGN_STYLE "Manual")
		set(ARMSX2_CODE_SIGN_IDENTITY "")
	endif()

	if(ARMSX2_ENABLE_JIT_ENTITLEMENTS)
		set(ARMSX2_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/Entitlements.plist")
	else()
		set(ARMSX2_CODE_SIGN_ENTITLEMENTS "")
	endif()

	if(ARMSX2_MAC_CATALYST)
		set(ARMSX2_SUPPORTS_MACCATALYST "YES")
		set(ARMSX2_DERIVE_MACCATALYST_BUNDLE_IDENTIFIER "NO")
		set(ARMSX2_SUPPORTED_PLATFORMS "iphoneos iphonesimulator macosx")
		set(ARMSX2_TARGETED_DEVICE_FAMILY "1,2,6")
		set(ARMSX2_ENABLE_HARDENED_RUNTIME "YES")
	else()
		set(ARMSX2_SUPPORTS_MACCATALYST "NO")
		set(ARMSX2_DERIVE_MACCATALYST_BUNDLE_IDENTIFIER "YES")
		set(ARMSX2_SUPPORTED_PLATFORMS "iphoneos iphonesimulator")
		set(ARMSX2_TARGETED_DEVICE_FAMILY "1,2")
		set(ARMSX2_ENABLE_HARDENED_RUNTIME "NO")
	endif()

	set(IOS_RUNTIME_SOURCES
		ios_main.mm
		IOS/AppDelegate.mm
		IOS/GamepadHaptics.mm
		IOS/HostImpls.mm
		IOS/PlaySoundAsync.mm
		IOS/SceneDelegate.mm
	)
	add_executable(ARMSX2iOS ${IOS_RUNTIME_SOURCES} ARMSX2Bridge.mm ${SWIFT_SOURCES})
	set_target_properties(ARMSX2iOS PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_GUI_IDENTIFIER "${ARMSX2_BUNDLE_IDENTIFIER}"
		MACOSX_BUNDLE_BUNDLE_NAME "ARMSX2 iOS"
		MACOSX_BUNDLE_BUNDLE_VERSION "${ARMSX2_BUILD_NUMBER}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${ARMSX2_VERSION}"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
		MACOSX_BUNDLE_EXECUTABLE_NAME "ARMSX2iOS"
		OUTPUT_NAME "ARMSX2iOS"
		XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ARMSX2_BUNDLE_IDENTIFIER}"
		XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${ARMSX2_TARGETED_DEVICE_FAMILY}"
		XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${ARMSX2_IOS_DEPLOYMENT_TARGET}"
		XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET "${ARMSX2_MACOS_DEPLOYMENT_TARGET}"
		XCODE_ATTRIBUTE_SDKROOT "${CMAKE_OSX_SYSROOT}"
		XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "${ARMSX2_SUPPORTED_PLATFORMS}"
		XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST "${ARMSX2_SUPPORTS_MACCATALYST}"
		XCODE_ATTRIBUTE_DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER "${ARMSX2_DERIVE_MACCATALYST_BUNDLE_IDENTIFIER}"
		XCODE_ATTRIBUTE_SUPPORTS_MAC_DESIGNED_FOR_IPHONE_IPAD "NO"
		XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "${ARMSX2_ENABLE_HARDENED_RUNTIME}"
		XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "${ARMSX2_CODE_SIGNING_ALLOWED}"
		XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "${ARMSX2_CODE_SIGNING_REQUIRED}"
		XCODE_ATTRIBUTE_CODE_SIGN_STYLE "${ARMSX2_CODE_SIGN_STYLE}"
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "${ARMSX2_CODE_SIGN_IDENTITY}"
		XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${ARMSX2_DEVELOPMENT_TEAM}"
		XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER ""
		XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${ARMSX2_CODE_SIGN_ENTITLEMENTS}"
			XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_SOURCE_DIR}/ARMSX2-Bridging-Header.h"
			XCODE_ATTRIBUTE_SWIFT_VERSION "6.0"
			XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES "YES"
			XCODE_ATTRIBUTE_ENABLE_ON_DEMAND_RESOURCES "NO"
			XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks /usr/lib/swift"
			XCODE_ATTRIBUTE_LIBRARY_SEARCH_PATHS "$(inherited) $(TOOLCHAIN_DIR)/usr/lib/swift/$(PLATFORM_NAME) /usr/lib/swift"
			XCODE_ATTRIBUTE_OTHER_SWIFT_FLAGS ""
			XCODE_ATTRIBUTE_MTL_HEADER_SEARCH_PATHS "${ARMSX2_ROOT}/pcsx2/GS/Renderers/Metal ${ARMSX2_ROOT}/3rdparty/include ${CMAKE_SOURCE_DIR}/../assets ${CMAKE_SOURCE_DIR}/../assets/resources"
			XCODE_ATTRIBUTE_MTL_ENABLE_DEBUG_INFO "$<IF:$<CONFIG:Debug>,INCLUDE_SOURCE,>"
		)

		set(BOOT_SPLASH_VIDEO "${CMAKE_SOURCE_DIR}/../res/raw/boot_intro.mp4")
		set(IOS_APP_ICON_FILES
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-20.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-29.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-40.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-58.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-60.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-76.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-80.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-87.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-120.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-152.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-167.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-180.png"
			"${CMAKE_SOURCE_DIR}/../assets/Assets.xcassets/AppIcon.appiconset/icon-1024.png"
		)
		foreach(IOS_APP_ICON_FILE IN LISTS IOS_APP_ICON_FILES)
			if(EXISTS "${IOS_APP_ICON_FILE}")
				target_sources(ARMSX2iOS PRIVATE "${IOS_APP_ICON_FILE}")
				set_source_files_properties("${IOS_APP_ICON_FILE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
			endif()
		endforeach()
		# Alternate app icons (user-selectable from Settings). Bundled as loose
		# Resources and referenced by name in Info.plist CFBundleAlternateIcons.
		file(GLOB IOS_ALTERNATE_APP_ICON_FILES "${CMAKE_SOURCE_DIR}/../assets/app_icons/alternate_app_icons/*.png")
		foreach(IOS_ALTERNATE_APP_ICON_FILE IN LISTS IOS_ALTERNATE_APP_ICON_FILES)
			if(EXISTS "${IOS_ALTERNATE_APP_ICON_FILE}")
				target_sources(ARMSX2iOS PRIVATE "${IOS_ALTERNATE_APP_ICON_FILE}")
				set_source_files_properties("${IOS_ALTERNATE_APP_ICON_FILE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
			endif()
		endforeach()
		# Preview thumbnails for the SwiftUI icon picker.
		file(GLOB IOS_ALTERNATE_APP_ICON_PREVIEWS "${CMAKE_SOURCE_DIR}/../assets/app_icons/alternate_app_icons/previews/*.png")
		foreach(IOS_ALTERNATE_APP_ICON_PREVIEW IN LISTS IOS_ALTERNATE_APP_ICON_PREVIEWS)
			if(EXISTS "${IOS_ALTERNATE_APP_ICON_PREVIEW}")
				target_sources(ARMSX2iOS PRIVATE "${IOS_ALTERNATE_APP_ICON_PREVIEW}")
				set_source_files_properties("${IOS_ALTERNATE_APP_ICON_PREVIEW}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
			endif()
		endforeach()
		# Full-resolution icon images exported for Home Screen shortcuts.
		file(GLOB IOS_ALTERNATE_APP_ICON_EXPORTS "${CMAKE_SOURCE_DIR}/../assets/app_icons/alternate_app_icons/exports/*.png")
		foreach(IOS_ALTERNATE_APP_ICON_EXPORT IN LISTS IOS_ALTERNATE_APP_ICON_EXPORTS)
			if(EXISTS "${IOS_ALTERNATE_APP_ICON_EXPORT}")
				target_sources(ARMSX2iOS PRIVATE "${IOS_ALTERNATE_APP_ICON_EXPORT}")
				set_source_files_properties("${IOS_ALTERNATE_APP_ICON_EXPORT}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
			endif()
		endforeach()
		if(EXISTS "${BOOT_SPLASH_VIDEO}")
			target_sources(ARMSX2iOS PRIVATE "${BOOT_SPLASH_VIDEO}")
			set_source_files_properties("${BOOT_SPLASH_VIDEO}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		endif()

		file(GLOB IOS_CONTROLLER_ICON_FILES "${CMAKE_SOURCE_DIR}/../assets/app_icons/controller_icons_nether/*.png")
		if(IOS_CONTROLLER_ICON_FILES)
			target_sources(ARMSX2iOS PRIVATE ${IOS_CONTROLLER_ICON_FILES})
			set_source_files_properties(${IOS_CONTROLLER_ICON_FILES} PROPERTIES
				MACOSX_PACKAGE_LOCATION Resources)
		endif()

		file(GLOB_RECURSE IOS_CONTROLLER_SKIN_FILES "${CMAKE_SOURCE_DIR}/../assets/app_icons/controller_skins/*.png")
		foreach(IOS_CONTROLLER_SKIN_FILE IN LISTS IOS_CONTROLLER_SKIN_FILES)
			file(RELATIVE_PATH IOS_CONTROLLER_SKIN_RELATIVE_FILE
				"${CMAKE_SOURCE_DIR}/../assets/app_icons"
				"${IOS_CONTROLLER_SKIN_FILE}")
			get_filename_component(IOS_CONTROLLER_SKIN_RELATIVE_DIR
				"${IOS_CONTROLLER_SKIN_RELATIVE_FILE}" DIRECTORY)
			target_sources(ARMSX2iOS PRIVATE "${IOS_CONTROLLER_SKIN_FILE}")
			set_source_files_properties("${IOS_CONTROLLER_SKIN_FILE}" PROPERTIES
				MACOSX_PACKAGE_LOCATION "Resources/${IOS_CONTROLLER_SKIN_RELATIVE_DIR}")
		endforeach()

	set(ARMSX2_APPLE_FRAMEWORKS
		"-framework UIKit"
		"-framework AVFoundation"
		"-framework Metal"
		"-framework MetalKit"
		"-framework QuartzCore"
		"-framework CoreText"
		"-framework CoreGraphics"
		"-framework ImageIO"
		"-framework GameController"
		"-framework CoreHaptics"
	)
	if(NOT ARMSX2_MAC_CATALYST)
		list(APPEND ARMSX2_APPLE_FRAMEWORKS "-framework OpenGLES")
	endif()

	target_link_libraries(ARMSX2iOS PRIVATE
		PCSX2
		SDL3::SDL3
		${ARMSX2_APPLE_FRAMEWORKS}
	)

	target_compile_options(ARMSX2iOS PRIVATE $<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-pipe>)

	execute_process(
		COMMAND git rev-parse --short HEAD
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
		OUTPUT_VARIABLE ARMSX2_GIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	if(NOT ARMSX2_GIT_HASH)
		set(ARMSX2_GIT_HASH "unknown")
	endif()

	target_compile_definitions(ARMSX2iOS PRIVATE
		PCSX2_NO_PCAP=1
		ARMSX2_GIT_HASH="${ARMSX2_GIT_HASH}"
		ARMSX2_VERSION_STR="${ARMSX2_VERSION}"
	)
	if(ARMSX2_MAC_CATALYST)
		target_compile_definitions(ARMSX2iOS PRIVATE ARMSX2_MAC_CATALYST_BUILD=1)
	endif()

	target_include_directories(ARMSX2iOS PRIVATE
		${CMAKE_SOURCE_DIR}
		${ARMSX2_ROOT}
		${ARMSX2_ROOT}/common
		${ARMSX2_ROOT}/pcsx2
		${ARMSX2_ROOT}/pcsx2/GS/Renderers/Metal
		${ARMSX2_ROOT}/3rdparty/SDL3/include
		${ARMSX2_ROOT}/3rdparty/fmt/include
		${ARMSX2_ROOT}/3rdparty/fast_float/include
		${ARMSX2_ROOT}/3rdparty/vixl/include
		${ARMSX2_ROOT}/3rdparty/include
		${ARMSX2_ROOT}/3rdparty/vulkan/include
		${ARMSX2_ROOT}/3rdparty/glslang/glslang
		${ARMSX2_ROOT}/3rdparty/glslang/include
		${CMAKE_BINARY_DIR}/common/include
		${CMAKE_BINARY_DIR}/pcsx2
		${ARMSX2_ROOT}/3rdparty/simpleini/include
		${ARMSX2_ROOT}/3rdparty/imgui/include
		${ARMSX2_ROOT}/3rdparty/rapidyaml/include
		${ARMSX2_ROOT}/3rdparty/cpuinfo/include
		${ARMSX2_ROOT}/3rdparty/libchdr/include
		${ARMSX2_ROOT}/3rdparty/libzip/lib
		${CMAKE_BINARY_DIR}/3rdparty/libzip
		${ARMSX2_ROOT}/3rdparty/cubeb/include
		${ARMSX2_ROOT}/3rdparty/rcheevos/include
		${ARMSX2_ROOT}/3rdparty/freesurround/include
		${ARMSX2_ROOT}/3rdparty/freetype/include
		${ARMSX2_ROOT}/3rdparty/plutosvg1/source
		${ARMSX2_ROOT}/3rdparty/plutosvg1/plutovg/include
		${CMAKE_BINARY_DIR}/3rdparty/freetype/include
		${ARMSX2_ROOT}/3rdparty/soundtouch/soundtouch
		${ARMSX2_ROOT}/3rdparty/lz4/lz4/lib
		${ARMSX2_ROOT}/3rdparty/lzma/include
		${ARMSX2_ROOT}/3rdparty/zstd/zstd/lib
		${ARMSX2_ROOT}/3rdparty/demangler/include
		${ARMSX2_ROOT}/3rdparty/ccc/src
		${CMAKE_BINARY_DIR}/3rdparty/SDL3/include-revision
	)

	file(GLOB METAL_SHADERS "${ARMSX2_ROOT}/pcsx2/GS/Renderers/Metal/*.metal")
	file(GLOB FONTS "${CMAKE_SOURCE_DIR}/../assets/resources/fonts/*.ttf" "${CMAKE_SOURCE_DIR}/../assets/resources/fonts/*.otf")
	file(GLOB_RECURSE IOS_SHADER_RESOURCES "${CMAKE_SOURCE_DIR}/../assets/resources/shaders/*")
	file(GLOB_RECURSE IOS_SOUND_RESOURCES "${CMAKE_SOURCE_DIR}/../assets/resources/sounds/*")
	set(IOS_TOP_LEVEL_RESOURCES
		"${CMAKE_SOURCE_DIR}/../assets/resources/GameIndex.yaml"
		"${CMAKE_SOURCE_DIR}/../assets/resources/RedumpDatabase.yaml"
		"${CMAKE_SOURCE_DIR}/../assets/resources/patches.zip"
		"${CMAKE_SOURCE_DIR}/../assets/resources/game_controller_db.txt"
	)

	target_sources(ARMSX2iOS PRIVATE ${METAL_SHADERS} ${FONTS} ${IOS_SHADER_RESOURCES})
	set_source_files_properties(${METAL_SHADERS} PROPERTIES
		XCODE_EXPLICIT_FILE_TYPE "sourcecode.metal"
		MACOSX_PACKAGE_LOCATION Resources
	)
	set_source_files_properties(${FONTS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts)
	foreach(IOS_TOP_LEVEL_RESOURCE IN LISTS IOS_TOP_LEVEL_RESOURCES)
		if(EXISTS "${IOS_TOP_LEVEL_RESOURCE}")
			target_sources(ARMSX2iOS PRIVATE "${IOS_TOP_LEVEL_RESOURCE}")
			set_source_files_properties("${IOS_TOP_LEVEL_RESOURCE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		endif()
	endforeach()
	foreach(IOS_SHADER_RESOURCE IN LISTS IOS_SHADER_RESOURCES)
		get_filename_component(IOS_SHADER_DIR "${IOS_SHADER_RESOURCE}" DIRECTORY)
		file(RELATIVE_PATH IOS_SHADER_SUBDIR "${CMAKE_SOURCE_DIR}/../assets/resources" "${IOS_SHADER_DIR}")
		set_source_files_properties("${IOS_SHADER_RESOURCE}" PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources/${IOS_SHADER_SUBDIR}")
	endforeach()
	target_sources(ARMSX2iOS PRIVATE ${IOS_SOUND_RESOURCES})
	foreach(IOS_SOUND_RESOURCE IN LISTS IOS_SOUND_RESOURCES)
		get_filename_component(IOS_SOUND_DIR "${IOS_SOUND_RESOURCE}" DIRECTORY)
		file(RELATIVE_PATH IOS_SOUND_SUBDIR "${CMAKE_SOURCE_DIR}/../assets/resources" "${IOS_SOUND_DIR}")
		set_source_files_properties("${IOS_SOUND_RESOURCE}" PROPERTIES
			MACOSX_PACKAGE_LOCATION "Resources/${IOS_SOUND_SUBDIR}")
	endforeach()

	if(CMAKE_GENERATOR STREQUAL "Xcode" AND ARMSX2_MAC_CATALYST)
		function(armsx2_disable_signing_for_non_app_targets directory)
			get_property(armsx2_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
			foreach(armsx2_target IN LISTS armsx2_targets)
				if(NOT armsx2_target STREQUAL "ARMSX2iOS")
					set_target_properties(${armsx2_target} PROPERTIES
						XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
						XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
						XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
						XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
						XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
						XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS ""
					)
				endif()
			endforeach()

			get_property(armsx2_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
			foreach(armsx2_subdirectory IN LISTS armsx2_subdirectories)
				armsx2_disable_signing_for_non_app_targets("${armsx2_subdirectory}")
			endforeach()
		endfunction()

		armsx2_disable_signing_for_non_app_targets("${CMAKE_SOURCE_DIR}")
	endif()
endif()

# gsrunner
if(ENABLE_GSRUNNER)
	if (NOT WIN32)
		message(WARNING "GSRunner is only supported on Windows and may not build on your system")
	endif()
	add_subdirectory(pcsx2-gsrunner)
endif()

#-------------------------------------------------------------------------------
if(NOT IS_SUPPORTED_COMPILER)
	message(WARNING "
*************** UNSUPPORTED CONFIGURATION ***************
You are not compiling PCSX2 with a supported compiler.
It may not even build successfully.
PCSX2 only supports the Clang and MSVC compilers.
No support will be provided, continue at your own risk.
*********************************************************")
endif()

#if(_M_ARM64)
#	message(WARNING "
#*************** UNSUPPORTED CONFIGURATION ***************
#
#Apple Silicon support in PCSX2 is INCOMPLETE. There are
#currently no EE/VU/IOP recompilers, and games will run
#VERY slow. There is no date for completion yet, you
#should set -DCMAKE_OSX_ARCHITECTURES=x86_64 for now,
#unless you want to work on the recompilers.
#
#We also ask that you read https://dont-ship.it/.
#
#*********************************************************")
#endif()
