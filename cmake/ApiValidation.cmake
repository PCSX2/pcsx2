set(wx_sdl_c_code "
#include <wx/setup.h>

#if (wxUSE_LIBSDL != 0)
#error cmake_WX_SDL
#endif
")

function(WX_vs_SDL)
    file(WRITE "${CMAKE_BINARY_DIR}/wx_sdl.c" "${wx_sdl_c_code}")
    enable_language(C)

    try_run(
            run_result_unused
            compile_result_unused
            "${CMAKE_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/wx_sdl.c"
            COMPILE_OUTPUT_VARIABLE OUT
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${wxWidgets_INCLUDE_DIRS}"
        )

    if (${OUT} MATCHES "cmake_WX_SDL" AND SDL2_API)
        message(FATAL_ERROR "WxWidget is linked to SDL (wxUSE_LIBSDL) and it is likely SDL1.2.
        Unfortunately you try to build PCSX2 with SDL2 support which is not compatible
        Please use -DSDL2_API=FALSE")
    endif()
endfunction()
