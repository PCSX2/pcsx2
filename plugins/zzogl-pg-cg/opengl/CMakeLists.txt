# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# plugin name
set(Output zzogl-cg-0.3.0)

set(CommonFlags
    -pthread
    -DZEROGS_SSE2
    -fno-regmove
    -fno-strict-aliasing
    -Wstrict-aliasing # Allow to track strict aliasing issue.
    -Wunused-variable
    )

set(OptimizationFlags
    -O2
    -DNDEBUG
    )

# Debug - Build
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    # add defines
    set(zzoglFinalFlags 
        ${CommonFlags} -g -Wall -D_DEBUG
    )
endif(CMAKE_BUILD_TYPE STREQUAL Debug)

# Devel - Build
if(CMAKE_BUILD_TYPE STREQUAL Devel)
    # add defines
    set(zzoglFinalFlags 
        ${CommonFlags} ${OptimizationFlags} -g -W -DZEROGS_DEVBUILD
    )
endif(CMAKE_BUILD_TYPE STREQUAL Devel)

# Release - Build
if(CMAKE_BUILD_TYPE STREQUAL Release)
    # add defines
    set(zzoglFinalFlags 
        ${CommonFlags} ${OptimizationFlags} -W
    )
endif(CMAKE_BUILD_TYPE STREQUAL Release)

# zzogl sources
set(zzoglSources
    GifTransfer.cpp
    GLWin32.cpp
    GLWinX11.cpp
    GSDump.cpp
    GSmain.cpp
    HostMemory.cpp
    Mem.cpp
    # memcpy_amd.cpp
    Mem_Swizzle.cpp
    Mem_Tables.cpp
    Profile.cpp
    rasterfont.cpp
    NewRegs.cpp
    Regs.cpp
    targets.cpp
    x86.cpp
    zerogs.cpp
    zpipe.cpp
    ZZClut.cpp
    ZZHacks.cpp
    ZZKeyboard.cpp
    ZZoglDrawing.cpp
    ZZLog.cpp
    ZZoglCreate.cpp
    ZZoglCRTC.cpp
    ZZoglFlush.cpp
    ZZoglFlushHack.cpp
    ZZoglSave.cpp
    ZZoglShaders.cpp
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
    Mem.h
    Mem_Swizzle.h
    Mem_Transmit.h
    Profile.h
    rasterfont.h
    NewRegs.h
    Regs.h
    targets.h
    Util.h
    x86.h
    zerogs.h
    zpipe.h
    ZZClut.h
    ZZGl.h
    ZZHacks.h
    ZZoglDrawing.h
    ZZLog.h
    ZZoglCRTC.h
    ZZoglMath.h
    ZZoglShaders.h
    ZZoglShoots.h
    ZZoglVB.h
    )

# zzogl S sources
set(zzoglSSources
    x86-32.S)

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
    ${zzoglSSources}
    ${zzoglShaderSources}
    ${zzoglLinuxSources}
    ${zzoglLinuxHeaders}
)

set(zzoglFinalLibs 
    Utilities
    ${CG_LIBRARIES}
    ${GLEW_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${X11_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

# change language of .S-files to c++
set_source_files_properties(${zzoglSSources} PROPERTIES LANGUAGE CXX)

# add additional include directories
include_directories(Linux)

add_pcsx2_plugin(${Output} "${zzoglFinalSources}" "${zzoglFinalLibs}" "${zzoglFinalFlags}")

# Trick that allow to compile zzogl with GSOPEN2 and the replayer with GSOPEN
set_target_properties(${Output} PROPERTIES COMPILE_DEFINITIONS USE_GSOPEN2)

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
	set(Replay pcsx2_ZZCGReplayLoader)
    set(zzoglReplayLoaderFinalSources 
        ${zzoglFinalSources}
        linux_replay.cpp
    )
    # Broken for now
    #add_pcsx2_executable(${Replay} "${zzoglReplayLoaderFinalSources}" "${zzoglFinalLibs}" "${zzoglFinalFlags}")
endif(BUILD_REPLAY_LOADERS)
