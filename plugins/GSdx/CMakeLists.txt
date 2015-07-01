# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


# plugin name
set(Output GSdx-1.0.0)

set(CommonFlags
    -fno-operator-names # because Xbyak uses and()/xor()/or()/not() function
    -mfpmath=sse
    -fno-strict-aliasing
    -Wno-unknown-pragmas
    -Wno-parentheses
    -Wunused-variable # __dummy variable need to be investigated
    )

set(OptimizationFlags -O2)


if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(GSdxFinalFlags ${GSdxFinalFlags} ${CommonFlags} -D_DEBUG)

elseif(CMAKE_BUILD_TYPE STREQUAL Devel)
    set(GSdxFinalFlags ${GSdxFinalFlags} ${CommonFlags} ${OptimizationFlags} -D_DEVEL)

elseif(CMAKE_BUILD_TYPE STREQUAL Release)
    set(GSdxFinalFlags ${GSdxFinalFlags} ${CommonFlags} ${OptimizationFlags} -W)

endif()

if(XDG_STD)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DXDG_STD)
endif()

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DEGL_SUPPORTED)
endif()

if(LIBLZMA_FOUND)
	set(GSdxFinalFlags ${GSdxFinalFlags} -DLZMA_SUPPORTED)
endif()

if(PNGPP_FOUND)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DPNGPP_SUPPORTED)
endif()

#Clang doesn't support a few common flags that GCC does.
if(NOT USE_CLANG)
    set(GSdxFinalFlags ${GSdxFinalFlags} -fabi-version=6)
    if (_M_X86_32 AND NOT USE_ASAN)
        # Someone need to seriously test the build of GSdx without this option
        set(GSdxFinalFlags ${GSdxFinalFlags} -mpreferred-stack-boundary=2)
    endif()
endif()

set(GSdxSources
    GLLoader.cpp
    GLState.cpp
    GPU.cpp
    GPUDrawScanline.cpp
    GPUDrawScanlineCodeGenerator.cpp
    GPULocalMemory.cpp
    GPURenderer.cpp
    GPURendererSW.cpp
    GPUSetupPrimCodeGenerator.cpp
    GPUState.cpp
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
    GSDrawScanline.cpp
    GSDrawScanlineCodeGenerator.cpp
    GSDrawScanlineCodeGenerator.x86.avx.cpp
    GSDrawScanlineCodeGenerator.x86.avx2.cpp
    GSDrawScanlineCodeGenerator.x64.cpp
    GSDrawScanlineCodeGenerator.x86.cpp
    GSDrawScanlineCodeGenerator.x64.avx.cpp
    GSDump.cpp
    GSFunctionMap.cpp
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
    GSSetupPrimCodeGenerator.x86.avx.cpp
    GSSetupPrimCodeGenerator.x86.avx2.cpp
    GSSetupPrimCodeGenerator.x64.avx.cpp
    GSSetupPrimCodeGenerator.x86.cpp
    GSSetupPrimCodeGenerator.x64.cpp
    GSShaderOGL.cpp
    GSState.cpp
    GSTables.cpp
    GSTexture.cpp
    GSTextureCache.cpp
    GSTextureCacheSW.cpp
    GSTextureCacheOGL.cpp
    GSTextureFXOGL.cpp
    GSTextureOGL.cpp
    GSTextureNull.cpp
    GSTextureSW.cpp
    GSThread.cpp
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
    GPU.h
    GPUDrawScanline.h
    GPUDrawScanlineCodeGenerator.h
    GPUDrawingEnvironment.h
    GPULocalMemory.h
    GPURenderer.h
    GPURendererSW.h
    GPUScanlineEnvironment.h
    GPUSetupPrimCodeGenerator.h
    GPUState.h
    GPUVertex.h
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
    xbyak/xbyak_bin2hex.h
    xbyak/xbyak_mnemonic.h
    xbyak/xbyak_util.h
    )

set(GSdxFinalSources
    ${GSdxSources}
    ${GSdxHeaders}
)

set(GSdxFinalLibs
    ${X11_LIBRARIES}
)

set(GSdxFinalLibs ${GSdxFinalLibs}
    ${OPENGL_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${LIBC_LIBRARIES}
    ${PNG_LIBRARY}
)

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs}
        ${EGL_LIBRARIES}
        )
endif()

if(LIBLZMA_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs}
		${LIBLZMA_LIBRARIES}
		)
endif()

# Generate Glsl header file. Protect with REBUILD_SHADER to avoid build-dependency on PERL
if (REBUILD_SHADER)
    add_custom_command(OUTPUT res/glsl_source.h COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/glsl2h.pl)
endif()

add_pcsx2_plugin(${Output} "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
    set(Replay pcsx2_GSReplayLoader)
    set(GSdxReplayLoaderFinalSources
        ${GSdxFinalSources}
        linux_replay.cpp
    )
    add_pcsx2_executable(${Replay} "${GSdxReplayLoaderFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
endif(BUILD_REPLAY_LOADERS)
