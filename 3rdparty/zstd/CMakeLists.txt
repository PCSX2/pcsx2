enable_language(C ASM)

add_library(pcsx2-zstd
	zstd/lib/common/debug.c
	zstd/lib/common/entropy_common.c
	zstd/lib/common/error_private.c
	zstd/lib/common/fse_decompress.c
	zstd/lib/common/pool.c
	zstd/lib/common/threading.c
	zstd/lib/common/xxhash.c
	zstd/lib/common/zstd_common.c
	zstd/lib/compress/fse_compress.c
	zstd/lib/compress/hist.c
	zstd/lib/compress/huf_compress.c
	zstd/lib/compress/zstd_compress.c
	zstd/lib/compress/zstd_compress_literals.c
	zstd/lib/compress/zstd_compress_sequences.c
	zstd/lib/compress/zstd_compress_superblock.c
	zstd/lib/compress/zstd_double_fast.c
	zstd/lib/compress/zstd_fast.c
	zstd/lib/compress/zstd_lazy.c
	zstd/lib/compress/zstd_ldm.c
	zstd/lib/compress/zstdmt_compress.c
	zstd/lib/compress/zstd_opt.c
	zstd/lib/decompress/huf_decompress.c
	zstd/lib/decompress/zstd_ddict.c
	zstd/lib/decompress/zstd_decompress_block.c
	zstd/lib/decompress/zstd_decompress.c
)

if(NOT MSVC AND ${PCSX2_TARGET_ARCHITECTURES} MATCHES "x86_64")
	target_sources(pcsx2-zstd PRIVATE zstd/lib/decompress/huf_decompress_amd64.S)
endif()

target_include_directories(pcsx2-zstd PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib")

add_library(Zstd::Zstd ALIAS pcsx2-zstd)