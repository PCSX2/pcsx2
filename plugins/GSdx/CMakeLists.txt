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
    # The next two need to be looked at, but spam really badly in gcc 8.
    # Largely class alignment in GSDevice.h and memcpy in GSVector*.h.
    -Wno-class-memaccess 
    -Wno-packed-not-aligned
    )

if(USE_ICC)
    set(CommonFlags "${CommonFlags} -restrict")
endif()

set(GSdxFinalFlags ${CommonFlags})

if(CMAKE_COMPILER_IS_GNUCXX)
	# Newer version default to a correct ABI
	if (${GCC_VERSION} VERSION_LESS "5.1")
		set(GSdxFinalFlags ${GSdxFinalFlags} -fabi-version=6)
	endif()
endif()

if(XDG_STD)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DXDG_STD)
endif()

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DEGL_SUPPORTED)
endif()


if(OPENCL_API)
    set(GSdxFinalFlags ${GSdxFinalFlags} -DENABLE_OPENCL)
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
    GSOsdManager.cpp
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
    GSdx.cpp
    GSdxResources.cpp
    stdafx.cpp
    )

set(GSdxHeaders
    config.h
    GLLoader.h
    GLState.h
    GSAlignedClass.h
    GSBlock.h
    GSCaptureDlg.h
    GSCapture.h
    GSClut.h
    GSCodeBuffer.h
    GSCrc.h
    GSDevice.h
    GSDeviceNull.h
    GSDeviceOGL.h
    GSDialog.h
    GSDirtyRect.h
    GSDrawingContext.h
    GSDrawingEnvironment.h
    GSDrawScanlineCodeGenerator.h
    GSDrawScanline.h
    GSDump.h
    GSdx.h
    GSdxResources.h
    GSFastList.h
    GSFunctionMap.h
    GS.h
    GSLocalMemory.h
    GSLzma.h
    GSOsdManager.h
    GSPerfMon.h
    GSPng.h
    GSRasterizer.h
    GSRendererCL.h
    GSRenderer.h
    GSRendererHW.h
    GSRendererNull.h
    GSRendererOGL.h
    GSRendererSW.h
    GSScanlineEnvironment.h
    GSSetting.h
    GSSettingsDlg.h
    GSSetupPrimCodeGenerator.h
    GSShaderOGL.h
    GSState.h
    GSTables.h
    GSTextureCache.h
    GSTextureCacheOGL.h
    GSTextureCacheSW.h
    GSTexture.h
    GSTextureNull.h
    GSTextureOGL.h
    GSTextureSW.h
    GSThread_CXX11.h
    GSThread.h
    GSUniformBufferOGL.h
    GSUtil.h
    GSVector.h
    GSVector4.h
    GSVector4i.h
    GSVector8.h
    GSVector8i.h
    GSVertexArrayOGL.h
    GSVertex.h
    GSVertexHW.h
    GSVertexList.h
    GSVertexSW.h
    GSVertexTrace.h
    GSWnd.h
    PSX/GPUDrawingEnvironment.h
    PSX/GPUDrawScanlineCodeGenerator.h
    PSX/GPUDrawScanline.h
    PSX/GPU.h
    PSX/GPULocalMemory.h
    PSX/GPURenderer.h
    PSX/GPURendererSW.h
    PSX/GPUScanlineEnvironment.h
    PSX/GPUSetupPrimCodeGenerator.h
    PSX/GPUState.h
    PSX/GPUVertex.h
    stdafx.h
    xbyak/xbyak.h
    xbyak/xbyak_mnemonic.h
    xbyak/xbyak_util.h
    )

if(Windows)
    LIST(APPEND GSdxSources
        GSCaptureDlg.cpp
        GSDevice11.cpp
        GSDevice9.cpp
        GSDeviceDX.cpp
        GSDialog.cpp
        GSRendererDX11.cpp
        GSRendererDX9.cpp
        GSRendererDX.cpp
        GSSettingsDlg.cpp
        GSTexture11.cpp
        GSTexture9.cpp
        GSTextureCache11.cpp
        GSTextureCache9.cpp
        GSTextureFX11.cpp
        GSTextureFX9.cpp
        GSTextureFX.cpp
        GSVertexList.cpp
        GSVertexSW.cpp
        GSWndDX.cpp
        GSWndWGL.cpp
        PSX/GPUSettingsDlg.cpp
        )

    LIST(APPEND GSdxHeaders
        GSDevice11.h
        GSDevice9.h
        GSDeviceDX.h
        GSRendererDX11.h
        GSRendererDX9.h
        GSRendererDX.h
        GSTexture11.h
        GSTexture9.h
        GSTextureCache11.h
        GSTextureCache9.h
        GSWndDX.h
        GSWndWGL.h
        PSX/GPUSettingsDlg.h
        resource.h
        targetver.h
        )
else()
    LIST(APPEND GSdxSources
        GSLinuxDialog.cpp
        GSWndOGL.cpp
        GSWndEGL.cpp
        )

    LIST(APPEND GSdxHeaders
        GSWndEGL.h
        GSWndOGL.h
        )
endif()


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
    ${FREETYPE_LIBRARIES}
    ${LIBLZMA_LIBRARIES}
)

if(EGL_API AND EGL_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${EGL_LIBRARIES} ${X11_XCB_LIBRARIES})
endif()

if(USE_VTUNE)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${VTUNE_LIBRARIES})
endif()

if(OPENCL_FOUND)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${OPENCL_LIBRARIES})
endif()

set(RESOURCE_FILES
    res/logo-ogl.bmp
    res/fxaa.fx
    res/tfx.cl
    res/glsl/common_header.glsl
    res/glsl/convert.glsl
    res/glsl/interlace.glsl
    res/glsl/merge.glsl
    res/glsl/shadeboost.glsl
    res/glsl/tfx_fs.glsl
    res/glsl/tfx_vgs.glsl)

add_custom_glib_res("GSdxResources" "gsdx-res.xml" "GSdx_res" ${RESOURCE_FILES})

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(BUILTIN_GS)
    add_pcsx2_lib(${Output} "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
else()
    if (DISABLE_ADVANCE_SIMD)
        # Don't append -SSE2 on the first build to keep same name as SIMD build
        add_pcsx2_plugin("${Output}" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
        add_pcsx2_plugin("${Output}-SSE4" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags} -mssse3 -msse4 -msse4.1")
        add_pcsx2_plugin("${Output}-AVX2" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags} -mavx -mavx2 -mbmi -mbmi2")
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
