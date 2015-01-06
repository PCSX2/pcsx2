set(wx_sdl_c_code "
#include <wx/setup.h>

#if (wxUSE_LIBSDL != 0)
#error cmake_WX_SDL
#endif
")

set(wx_gtk3_c_code "
#include <wx/setup.h>

#ifdef __WXGTK3__
#error cmake_WX_GTK3
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

function(WX_vs_GTK3)
    file(WRITE "${CMAKE_BINARY_DIR}/wx_gtk3.c" "${wx_gtk3_c_code}")
    enable_language(C)

    try_run(
            run_result_unused
            compile_result_unused
            "${CMAKE_BINARY_DIR}"
            "${CMAKE_BINARY_DIR}/wx_gtk3.c"
            COMPILE_OUTPUT_VARIABLE OUT
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${wxWidgets_INCLUDE_DIRS}"
        )

    if (${OUT} MATCHES "cmake_WX_GTK3")
        if (NOT GTK3_API)
            message(FATAL_ERROR "Your current WxWidget version requires GTK3. Please use -DGTK3_API=TRUE option")
        endif()
    else()
        if (GTK3_API)
            message(FATAL_ERROR "Your current WxWidget version doesn't support GTK3. Please use -DGTK3_API=FALSE option")
        endif()
    endif()

endfunction()

function(WX_version)
    EXECUTE_PROCESS(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} --version OUTPUT_VARIABLE version)
    STRING(STRIP ${version} version)
    if (${version} MATCHES "3.0" AND WX28_API)
        message(FATAL_ERROR "Mismatch between wx-config version (${version}) and requested WX API (2.8.x)")
    endif()
    if (${version} MATCHES "2.8" AND NOT WX28_API)
        message(FATAL_ERROR "Mismatch between wx-config version (${version}) and requested WX API (3.0.x)")
    endif()
endfunction()
