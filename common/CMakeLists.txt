# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
	message(FATAL_ERROR "
	You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
	It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

add_library(common)

# x86emitter sources
target_sources(common PRIVATE
	AlignedMalloc.cpp
	Assertions.cpp
	Console.cpp
	CrashHandler.cpp
	DynamicLibrary.cpp
	Error.cpp
	FastJmp.cpp
	FileSystem.cpp
	HostSys.cpp
	Image.cpp
	HTTPDownloader.cpp
	MemorySettingsInterface.cpp
	MD5Digest.cpp
	PrecompiledHeader.cpp
	Perf.cpp
	ProgressCallback.cpp
	ReadbackSpinManager.cpp
	Semaphore.cpp
	SettingsWrapper.cpp
	SmallString.cpp
	StringUtil.cpp
	TextureDecompress.cpp
	Timer.cpp
	WAVWriter.cpp
	WindowInfo.cpp
)

# x86emitter headers
target_sources(common PRIVATE
	AlignedMalloc.h
	Assertions.h
	boost_spsc_queue.hpp
	BitUtils.h
	ByteSwap.h
	Console.h
	CrashHandler.h
	DynamicLibrary.h
	Easing.h
	EnumOps.h
	Error.h
	FPControl.h
	FastJmp.h
	FileSystem.h
	HashCombine.h
	HostSys.h
	HeterogeneousContainers.h
	Image.h
	LRUCache.h
	HeapArray.h
	HTTPDownloader.h
	MemorySettingsInterface.h
	MD5Digest.h
	MRCHelpers.h
	Path.h
	PrecompiledHeader.h
	ProgressCallback.h
	ReadbackSpinManager.h
	RedtapeWilCom.h
	RedtapeWindows.h
	ScopedGuard.h
	SettingsInterface.h
	SettingsWrapper.h
	SingleRegisterTypes.h
	SmallString.h
	StringUtil.h
	Timer.h
	TextureDecompress.h
	Threading.h
	TraceLog.h
	VectorIntrin.h
	WAVWriter.h
	WindowInfo.h
	WrappedMemCopy.h
)

if(_M_X86)
	target_sources(common PRIVATE
		emitter/avx.cpp
		emitter/bmi.cpp
		emitter/fpu.cpp
		emitter/groups.cpp
		emitter/jmp.cpp
		emitter/legacy.cpp
		emitter/legacy_sse.cpp
		emitter/movs.cpp
		emitter/simd.cpp
		emitter/x86emitter.cpp
	)

	target_sources(common PRIVATE
		emitter/implement/dwshift.h
		emitter/implement/group1.h
		emitter/implement/group2.h
		emitter/implement/group3.h
		emitter/implement/helpers.h
		emitter/implement/incdec.h
		emitter/implement/jmpcall.h
		emitter/implement/movs.h
		emitter/implement/simd_arithmetic.h
		emitter/implement/simd_comparisons.h
		emitter/implement/simd_helpers.h
		emitter/implement/simd_moremovs.h
		emitter/implement/simd_shufflepack.h
		emitter/implement/simd_templated_helpers.h
		emitter/implement/test.h
		emitter/implement/xchg.h
		emitter/instructions.h
		emitter/internal.h
		emitter/legacy_instructions.h
		emitter/legacy_internal.h
		emitter/legacy_types.h
		emitter/x86emitter.h
		emitter/x86types.h
	)
endif()

if(WIN32)
	enable_language(ASM_MASM)
	target_sources(common PRIVATE
		FastJmp.asm
		HTTPDownloaderWinHTTP.cpp
		HTTPDownloaderWinHTTP.h
		StackWalker.cpp
		StackWalker.h
		Windows/WinThreads.cpp
		Windows/WinHostSys.cpp
		Windows/WinMisc.cpp
	)
	target_link_libraries(common PUBLIC
		WIL::WIL
		winmm
		pathcch
	)
elseif(APPLE)
	target_sources(common PRIVATE
		CocoaTools.mm
		CocoaTools.h
		Darwin/DarwinThreads.cpp
		Darwin/DarwinMisc.cpp
		Darwin/DarwinMisc.h
	)
	target_compile_options(common PRIVATE -fobjc-arc)
	target_link_options(common PRIVATE -fobjc-link-runtime)
	target_link_libraries(common PRIVATE
		"-framework Foundation"
		"-framework IOKit"
	)
else()
	target_sources(common PRIVATE
		Linux/LnxHostSys.cpp
		Linux/LnxThreads.cpp
		Linux/LnxMisc.cpp
	)
	target_include_directories(common PRIVATE
		${DBUS_INCLUDE_DIRS}
	)
	target_link_libraries(common PRIVATE
		${DBUS_LINK_LIBRARIES}
		X11::X11
		X11::Xrandr
	)
	if(USE_BACKTRACE)
		target_compile_definitions(common PRIVATE "HAS_LIBBACKTRACE=1")
		target_link_libraries(common PRIVATE libbacktrace::libbacktrace)
	endif()
	if(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
		target_link_libraries(common PRIVATE cpuinfo)
	endif()
endif()

set_source_files_properties(PrecompiledHeader.cpp PROPERTIES HEADER_FILE_ONLY TRUE)

if(USE_VTUNE)
	target_link_libraries(common PUBLIC Vtune::Vtune)
endif()

if (USE_GCC AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
	# GCC LTO doesn't work with asm statements
	set_source_files_properties(FastJmp.cpp PROPERTIES COMPILE_FLAGS -fno-lto)
endif()

if(NOT WIN32)
	# libcurl-based HTTPDownloader
	target_sources(common PRIVATE
		HTTPDownloaderCurl.cpp
		HTTPDownloaderCurl.h
	)
	target_link_libraries(common PRIVATE
		CURL::libcurl
	)
endif()

target_link_libraries(common PRIVATE
	${LIBC_LIBRARIES}
	JPEG::JPEG
	PNG::PNG
	WebP::libwebp
)

target_link_libraries(common PUBLIC
	fmt::fmt
	fast_float
)

fixup_file_properties(common)
target_compile_features(common PUBLIC cxx_std_17)
target_include_directories(common PUBLIC ../3rdparty/include ../)
target_compile_definitions(common PUBLIC "${PCSX2_DEFS}")
target_compile_options(common PRIVATE "${PCSX2_WARNINGS}")

if(COMMAND target_precompile_headers)
	target_precompile_headers(common PRIVATE PrecompiledHeader.h)
endif()
