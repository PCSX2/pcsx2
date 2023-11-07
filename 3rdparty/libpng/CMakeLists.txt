add_library(png
	png.c
	pngerror.c
	pngget.c
	pngmem.c
	pngpread.c
	pngread.c
	pngrio.c
	pngrtran.c
	pngrutil.c
	pngset.c
	pngtrans.c
	pngwio.c
	pngwrite.c
	pngwtran.c
	pngwutil.c
	intel/filter_sse2_intrinsics.c
	intel/intel_init.c
)

target_compile_definitions(png PRIVATE "PNG_INTEL_SSE")
target_link_libraries(png PRIVATE ZLIB::ZLIB)
target_include_directories(png PUBLIC .)
add_library(PNG::PNG ALIAS png)
disable_compiler_warnings_for_target(png)
