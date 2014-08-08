# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# Allow to build the shader files
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zzogl-pg/opengl/ZeroGSShaders" AND REBUILD_SHADER AND NOT GLSL_API)
    add_subdirectory(ZeroGSShaders)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zzogl-pg/opengl/ZeroGSShaders" AND REBUILD_SHADER AND NOT GLSL_API)

# plugin name
set(Output zzogl-0.4.0)

set(CommonFlags
    -pthread
    -DZEROGS_SSE2
    -fno-strict-aliasing
    -Wstrict-aliasing # Allow to track strict aliasing issue.
    -Wunused-variable
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

#Clang doesn't support a few common flags that GCC does.
if(NOT USE_CLANG)
	set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        ${CommonFlags} -fno-regmove
    )
endif(NOT USE_CLANG) 

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # add defines
    set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        ${CommonFlags} -g -Wall -D_DEBUG
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
    # add defines
    set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        ${CommonFlags} ${OptimizationFlags} -g -W -DZEROGS_DEVBUILD
    )
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
    # add defines
    set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        ${CommonFlags} ${OptimizationFlags} -W
    )
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# Select the shader API 
if(GLSL_API)
    set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        -DGLSL4_API -DOGL4_LOG
    )
    #-DGLSL_API
else(GLSL_API)
    set(zzoglFinalFlags 
        ${zzoglFinalFlags} 
        -DNVIDIA_CG_API
    )
endif(GLSL_API)

# Select the EGL API
if(EGL_API AND EGL_FOUND)
    if (EGL_GL_CONTEXT_SUPPORT)
        set(zzoglFinalFlags 
            ${zzoglFinalFlags} 
            -DEGL_API
        )
    else()
        message(WARNING "Current EGL implementation doesn't support openGL context. Fallback to standard GLX.")
    endif()
endif()


# zzogl sources
set(zzoglSources
    GifTransfer.cpp
    GLWin32.cpp
    GLWinX11.cpp
    GSDump.cpp
    GSmain.cpp
    HostMemory.cpp
    Mem.cpp
    Mem_Swizzle.cpp
    Mem_Tables.cpp
    Profile.cpp
    rasterfont.cpp
    Regs.cpp
    targets.cpp
    zerogs.cpp
    zpipe.cpp
    ZZDepthTargets.cpp
    ZZMemoryTargets.cpp
    ZZRenderTargets.cpp
    ZZClut.cpp
    ZZHacks.cpp
    ZZKeyboard.cpp
    ZZoglDrawing.cpp
    ZZLog.cpp
    ZZoglCreate.cpp
    ZZoglCRTC.cpp
    ZZoglFlush.cpp
    ZZoglFlushHack.cpp
    ZZoglMem.cpp
    ZZoglSave.cpp
    ZZoglShaders.cpp
    ZZoglShadersGLSL.cpp
    ZZoglShadersGLSL4.cpp
    ZZoglShoots.cpp
    ZZoglVB.cpp
    )

# zzogl headers
set(zzoglHeaders
    common.h
    CRC.h
    GifTransfer.h
    # glprocs.h
    GS.h
    GSDump.h
    HostMemory.h
    ps2hw_gl4.h
    Mem.h
    Mem_Swizzle.h
    Mem_Transmit.h
    Profile.h
    rasterfont.h
    Regs.h
    targets.h
    Util.h
    x86.h
    zerogs.h
    zpipe.h
    ZZClut.h
    ZZoglFlush.h
    ZZGl.h
    ZZHacks.h
    ZZoglDrawing.h
    ZZLog.h
    ZZoglCRTC.h
    ZZoglMath.h
    ZZoglMem.h
    ZZoglShaders.h
    ZZoglShoots.h
    ZZoglVB.h
    )

# zzogl shader sources
set(zzoglShaderSources
    ctx0/ps2hw_ctx.fx
    ctx1/ps2hw_ctx.fx)

# zzogl Linux sources
set(zzoglLinuxSources
    Linux/Conf.cpp
    Linux/Linux.cpp)

# zzogl Linux headers
set(zzoglLinuxHeaders
    Linux/Linux.h)

set(zzoglFinalSources 
    ${zzoglSources}
    ${zzoglHeaders}
    ${zzoglShaderSources}
    ${zzoglLinuxSources}
    ${zzoglLinuxHeaders}
)

set(zzoglFinalLibs 
    Utilities
    ${OPENGL_LIBRARIES}
)

# add additional include directories
include_directories(Linux)

# Generate Glsl header file. Protect with REBUILD_SHADER to avoid build-dependency on PERL
if (REBUILD_SHADER)
    add_custom_command(OUTPUT ps2hw_gl4.h COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/glsl2h.pl)
endif()

if(EGL_API AND EGL_FOUND AND EGL_GL_CONTEXT_SUPPORT)
    set(zzoglFinalLibs 
        ${zzoglFinalLibs}
        ${EGL_LIBRARIES}
    )
endif()

if(NOT GLSL_API)
    set(zzoglFinalLibs 
        ${zzoglFinalLibs}
        ${CG_LIBRARIES}
    )
endif(NOT GLSL_API)

set(zzoglFinalLibs 
    ${zzoglFinalLibs}
    ${GLEW_LIBRARY}
    ${X11_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LIBC_LIBRARIES}
)

add_pcsx2_plugin(${Output} "${zzoglFinalSources}" "${zzoglFinalLibs}" "${zzoglFinalFlags}")

# Trick that allow to compile zzogl with GSOPEN2 and the replayer with GSOPEN
set_target_properties(${Output} PROPERTIES COMPILE_DEFINITIONS USE_GSOPEN2)

if(NOT GLSL_API AND NOT REBUILD_SHADER)
    if(PACKAGE_MODE)
        install(FILES ${CMAKE_SOURCE_DIR}/plugins/zzogl-pg/opengl/ps2hw.dat DESTINATION ${PLUGIN_DIR})
    else()
        install(FILES ${CMAKE_SOURCE_DIR}/plugins/zzogl-pg/opengl/ps2hw.dat DESTINATION ${CMAKE_SOURCE_DIR}/bin/plugins)
    endif()
endif()

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
	set(Replay pcsx2_ZZReplayLoader)
    set(zzoglReplayLoaderFinalSources 
        ${zzoglFinalSources}
        linux_replay.cpp
    )
    add_pcsx2_executable(${Replay} "${zzoglReplayLoaderFinalSources}" "${zzoglFinalLibs}" "${zzoglFinalFlags}")
endif(BUILD_REPLAY_LOADERS)
