# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name (no version number to ease future version bump and bisect)
set(Output GSdx)

set(CommonFlags
    -fno-operator-names # because Xbyak uses and()/xor()/or()/not() function
    -Wno-unknown-pragmas
    -Wno-parentheses
    -Wunused-variable # __dummy variable need to be investigated
    )

if(USE_ICC)
	set(CommonFlags "${CommonFlags} -restrict")
endif()

set(GSdxFinalFlags ${CommonFlags})

if(XDG_STD)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DXDG_STD)
endif()

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DEGL_SUPPORTED)
endif()

if(LIBLZMA_FOUND)
	set(GSdxFinalFlags ${GSdxFinalFlags} -DLZMA_SUPPORTED)
endif()

#Clang doesn't support a few common flags that GCC does.
if(NOT USE_CLANG)
    set(GSdxFinalFlags ${GSdxFinalFlags} -fabi-version=6)
endif()

set(GSdxSources
    GLLoader.cpp
    GLState.cpp
    PSX/GPU.cpp
    PSX/GPUDrawScanline.cpp
    PSX/GPUDrawScanlineCodeGenerator.cpp
    PSX/GPULocalMemory.cpp
    PSX/GPURenderer.cpp
    PSX/GPURendererSW.cpp
    PSX/GPUSetupPrimCodeGenerator.cpp
    PSX/GPUState.cpp
    GS.cpp
    GSAlignedClass.cpp
    GSBlock.cpp
    GSCapture.cpp
    GSClut.cpp
    GSCodeBuffer.cpp
    GSCrc.cpp
    GSDevice.cpp
    GSDeviceOGL.cpp
    GSDeviceSW.cpp
    GSDeviceNull.cpp
    GSDirtyRect.cpp
    GSDrawingContext.cpp
    GSDrawScanline.cpp
    GSDrawScanlineCodeGenerator.cpp
    GSDrawScanlineCodeGenerator.x64.cpp
    GSDrawScanlineCodeGenerator.x64.avx.cpp
    GSDrawScanlineCodeGenerator.x64.avx2.cpp
    GSDrawScanlineCodeGenerator.x86.cpp
    GSDrawScanlineCodeGenerator.x86.avx.cpp
    GSDrawScanlineCodeGenerator.x86.avx2.cpp
    GSDump.cpp
    GSFunctionMap.cpp
    GSHwHack.cpp
    GSLinuxDialog.cpp
    GSLocalMemory.cpp
    GSLzma.cpp
    GSPerfMon.cpp
    GSPng.cpp
    GSRasterizer.cpp
    GSRenderer.cpp
    GSRendererCL.cpp
    GSRendererHW.cpp
    GSRendererNull.cpp
    GSRendererOGL.cpp
    GSRendererSW.cpp
    GSSetting.cpp
    GSSetupPrimCodeGenerator.cpp
    GSSetupPrimCodeGenerator.x64.cpp
    GSSetupPrimCodeGenerator.x64.avx.cpp
    GSSetupPrimCodeGenerator.x64.avx2.cpp
    GSSetupPrimCodeGenerator.x86.cpp
    GSSetupPrimCodeGenerator.x86.avx.cpp
    GSSetupPrimCodeGenerator.x86.avx2.cpp
    GSShaderOGL.cpp
    GSState.cpp
    GSTables.cpp
    GSTexture.cpp
    GSTextureCache.cpp
    GSTextureCacheSW.cpp
    GSTextureCacheOGL.cpp
    GSTextureOGL.cpp
    GSTextureNull.cpp
    GSTextureSW.cpp
    GSUtil.cpp
    GSVector.cpp
    GSVertexTrace.cpp
    GSWnd.cpp
    GSWndOGL.cpp
    GSWndEGL.cpp
    GSdx.cpp
    stdafx.cpp
    )

set(GSdxHeaders
    PSX/GPU.h
    PSX/GPUDrawScanline.h
    PSX/GPUDrawScanlineCodeGenerator.h
    PSX/GPUDrawingEnvironment.h
    PSX/GPULocalMemory.h
    PSX/GPURenderer.h
    PSX/GPURendererSW.h
    PSX/GPUScanlineEnvironment.h
    PSX/GPUSetupPrimCodeGenerator.h
    PSX/GPUState.h
    PSX/GPUVertex.h
    GS.h
    GSAlignedClass.h
    GSBlock.h
    GSCapture.h
    GSClut.h
    GSCodeBuffer.h
    GSCrc.h
    GSDevice.h
    GSDeviceOGL.h
    GSDeviceNull.h
    GSDirtyRect.h
    GSDrawScanline.h
    GSDrawScanlineCodeGenerator.h
    GSDrawingContext.h
    GSDrawingEnvironment.h
    GSDump.h
    GSFunctionMap.h
    GSLinuxLogo.h
    GSLocalMemory.h
    GSPerfMon.h
    GSRasterizer.h
    GSRenderer.h
    GSRendererNull.h
    GSRendererSW.h
    GSRendererHW.h
    GSRendererOGL.h
    GSScanlineEnvironment.h
    GSSetting.h
    GSSetupPrimCodeGenerator.h
    GSState.h
    GSTables.h
    GSTexture.h
    GSTextureCache.h
    GSTextureCacheSW.h
    GSTextureCacheOGL.h
    GSTextureNull.h
    GSThread.h
    GSThread_CXX11.h
    GSUtil.h
    GSVector.h
    GSVertex.h
    GSVertexHW.h
    GSVertexList.h
    GSVertexSW.h
    GSVertexTrace.h
    GSWnd.h
    GSWndOGL.h
    GSWndEGL.h
    GSdx.h
    res/glsl_source.h
    stdafx.h
    xbyak/xbyak.h
    xbyak/xbyak_mnemonic.h
    xbyak/xbyak_util.h
    )

set(GSdxFinalSources
    ${GSdxSources}
    ${GSdxHeaders}
)

set(GSdxFinalLibs
    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${LIBC_LIBRARIES}
    ${PNG_LIBRARIES}
)

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${EGL_LIBRARIES})
endif()

if(LIBLZMA_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${LIBLZMA_LIBRARIES})
endif()

if(USE_VTUNE)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${VTUNE_LIBRARIES})
endif()

# Generate Glsl header file. Protect with REBUILD_SHADER to avoid build-dependency on PERL
if (REBUILD_SHADER)
    add_custom_command(OUTPUT res/glsl_source.h COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/glsl2h.pl)
endif()

if(BUILTIN_GS)
    add_pcsx2_lib(${Output} "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
else()
    if (DISABLE_ADVANCE_SIMD)
        add_pcsx2_plugin("${Output}-SSE2" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
        add_pcsx2_plugin("${Output}-SSE4" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags} -mssse3 -msse4 -msse4.1")
        add_pcsx2_plugin("${Output}-AVX2" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags} -mavx -mavx2")
    else()
        add_pcsx2_plugin(${Output} "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
    endif()
endif()

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
    set(Replay pcsx2_GSReplayLoader)
    set(GSdxReplayLoaderFinalSources
        linux_replay.cpp
    )
    add_pcsx2_executable(${Replay} "${GSdxReplayLoaderFinalSources}" "${LIBC_LIBRARIES}" "${GSdxFinalFlags}")
endif(BUILD_REPLAY_LOADERS)
