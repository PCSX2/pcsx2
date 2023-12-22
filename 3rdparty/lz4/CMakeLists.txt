add_library(pcsx2-lz4
	lz4/lib/lz4.c
	lz4/lib/lz4.h
)

target_include_directories(pcsx2-lz4 PUBLIC lz4/lib)
target_compile_definitions(pcsx2-lz4 PUBLIC
	LZ4LIB_VISIBILITY=
)

add_library(LZ4::LZ4 ALIAS pcsx2-lz4)

disable_compiler_warnings_for_target(pcsx2-lz4)
