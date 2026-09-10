add_library(groovymister STATIC
	groovymister.cpp
	groovymister_wrapper.cpp
	nlc_codec.cpp
)

# Static link into PCSX2: makes the gmw_* wrapper symbols non-dllexport, so there
# is no runtime dlopen surface.
target_compile_definitions(groovymister PUBLIC
	GROOVYMISTER_WIN32_STATIC
)

target_include_directories(groovymister SYSTEM PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

# LZ4: use PCSX2's, do NOT bundle our own.
#
# Upstream Groovy_MiSTer ships a copy of LZ4 under api/lz4/. PCSX2 already links LZ4
# (find_package(LZ4) / lz4.lib on Windows), so compiling a second static copy defines every
# LZ4 symbol twice and the final link fails (LNK2005 / duplicate symbol). Upstream's
# GM_SYSTEM_LZ4 switch selects <lz4.h>/<lz4hc.h> over the bundled copy, which we do not
# vendor. The client only calls LZ4_compress_default and LZ4_compress_HC, both long-stable API.
target_compile_definitions(groovymister PRIVATE GM_SYSTEM_LZ4)
target_link_libraries(groovymister PUBLIC LZ4::LZ4)

if(WIN32)
	# Winsock. RIO (the low-latency UDP send path) is resolved at runtime via
	# WSAIoctl into a function-pointer table, so mswsock.lib is not required.
	target_link_libraries(groovymister PUBLIC ws2_32)
endif()

disable_compiler_warnings_for_target(groovymister)
