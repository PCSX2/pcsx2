# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

if(XDG_STD)
    add_definitions(-DXDG_STD)
endif(XDG_STD)

if(GLES_API AND GLESV2_FOUND)
    add_definitions(-DENABLE_GLES)
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
    GSDrawScanlineCodeGenerator.x64.cpp
    GSDrawScanlineCodeGenerator.x86.cpp
    GSDrawScanlineCodeGenerator.x64.avx.cpp
    GSDump.cpp
    GSFunctionMap.cpp
    GSLinuxDialog.cpp
    GSLocalMemory.cpp
    GSPerfMon.cpp
    GSRasterizer.cpp
    GSRenderer.cpp
    GSRendererHW.cpp
    GSRendererNull.cpp
    GSRendererOGL.cpp
    GSRendererSW.cpp
    GSSetting.cpp
    GSSetupPrimCodeGenerator.cpp
    GSSetupPrimCodeGenerator.x86.avx.cpp
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

# Generate Glsl header file. Protect with REBUILD_SHADER to avoid build-dependency on PERL
if (REBUILD_SHADER)
    add_custom_command(OUTPUT res/glsl_source.h COMMAND perl ${CMAKE_SOURCE_DIR}/linux_various/glsl2h.pl)
endif()

add_pcsx2_plugin(GSdx "${GSdxSources} ${GSdxHeaders}" "${GTK2_LIBRARIES} ${X11_LIBRARIES} ${EGL_LIBRARIES} ${OPENGL_LIBRARIES}")

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
    add_pcsx2_executable(pcsx2_GSReplayLoader "linux_replay.cpp" "")
endif(BUILD_REPLAY_LOADERS)
