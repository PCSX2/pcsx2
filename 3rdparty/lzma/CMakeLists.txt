add_library(pcsx2-lzma STATIC
	include/7z.h
	include/7zAlloc.h
	include/7zBuf.h
	include/7zCrc.h
	include/7zFile.h
	include/7zTypes.h
	include/7zVersion.h
	include/Alloc.h
	include/Bcj2.h
	include/Bra.h
	include/Compiler.h
	include/CpuArch.h
	include/Delta.h
	include/LzFind.h
	include/LzHash.h
	include/Lzma2Dec.h
	include/Lzma2DecMt.h
	include/Lzma2Enc.h
	include/Lzma86.h
	include/LzmaDec.h
	include/LzmaEnc.h
	include/LzmaLib.h
	include/Ppmd.h
	include/Ppmd7.h
	include/Precomp.h
	include/RotateDefs.h
	include/Sha256.h
	include/Xz.h
	include/XzCrc64.h
	include/XzEnc.h
	src/7zAlloc.c
	src/7zArcIn.c
	src/7zBuf.c
	src/7zBuf2.c
	src/7zCrc.c
	src/7zCrcOpt.c
	src/7zDec.c
	src/7zFile.c
	src/7zStream.c
	src/Alloc.c
	src/Bcj2.c
	src/Bcj2Enc.c
	src/Bra.c
	src/Bra86.c
	src/BraIA64.c
	src/CpuArch.c
	src/Delta.c
	src/LzFind.c
	src/LzFindOpt.c
	src/Lzma2Dec.c
	src/Lzma2DecMt.c
	src/Lzma2Enc.c
	src/Lzma86Dec.c
	src/Lzma86Enc.c
	src/LzmaDec.c
	src/LzmaEnc.c
	src/LzmaLib.c
	src/Ppmd7.c
	src/Ppmd7Dec.c
	src/Ppmd7Enc.c
	src/Sha256.c
	src/Sha256Opt.c
	src/Xz.c
	src/XzCrc64.c
	src/XzCrc64Opt.c
	src/XzDec.c
	src/XzEnc.c
	src/XzIn.c
)

target_compile_definitions(pcsx2-lzma PRIVATE _7ZIP_ST)

target_include_directories(pcsx2-lzma PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(pcsx2-lzma INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(LZMA::LZMA ALIAS pcsx2-lzma)