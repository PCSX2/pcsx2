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

# The next two need to be looked at, but spam really badly in gcc 8.
# Largely class alignment in GSDevice.h and memcpy in GSVector*.h.
if(GCC_VERSION VERSION_EQUAL "8.0" OR GCC_VERSION VERSION_GREATER "8.0")
    set(CommonFlags
	    ${CommonFlags}
        -Wno-class-memaccess
        -Wno-packed-not-aligned
    )
endif()

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


set(GSdxSources
    GS.cpp
    GSAlignedClass.cpp
    GSBlock.cpp
    GSCapture.cpp
    GSClut.cpp
    GSCodeBuffer.cpp
    GSCrc.cpp
    GSDrawingContext.cpp
    GSDump.cpp
    GSLocalMemory.cpp
    GSLzma.cpp
    GSPerfMon.cpp
    GSPng.cpp
    GSState.cpp
    GSTables.cpp
    GSUtil.cpp
    GSVector.cpp
    GSdx.cpp
    GSdxResources.cpp
    stdafx.cpp
    Renderers/Common/GSDevice.cpp
    Renderers/Common/GSDirtyRect.cpp
    Renderers/Common/GSFunctionMap.cpp
    Renderers/Common/GSOsdManager.cpp
    Renderers/Common/GSRenderer.cpp
    Renderers/Common/GSTexture.cpp
    Renderers/Common/GSVertexTrace.cpp
    Renderers/Null/GSDeviceNull.cpp
    Renderers/Null/GSRendererNull.cpp
    Renderers/Null/GSTextureNull.cpp
    Renderers/HW/GSHwHack.cpp
    Renderers/HW/GSRendererHW.cpp
    Renderers/HW/GSTextureCache.cpp
    Renderers/SW/GSDrawScanline.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x64.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x64.avx.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x64.avx2.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x86.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x86.avx.cpp
    Renderers/SW/GSDrawScanlineCodeGenerator.x86.avx2.cpp
    Renderers/SW/GSRasterizer.cpp
    Renderers/SW/GSRendererSW.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x64.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x64.avx.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x64.avx2.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x86.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x86.avx.cpp
    Renderers/SW/GSSetupPrimCodeGenerator.x86.avx2.cpp
    Renderers/SW/GSTextureCacheSW.cpp
    Renderers/SW/GSTextureSW.cpp
    Renderers/OpenGL/GLLoader.cpp
    Renderers/OpenGL/GLState.cpp
    Renderers/OpenGL/GSDeviceOGL.cpp
    Renderers/OpenGL/GSRendererOGL.cpp
    Renderers/OpenGL/GSShaderOGL.cpp
    Renderers/OpenGL/GSTextureCacheOGL.cpp
    Renderers/OpenGL/GSTextureOGL.cpp
    Window/GSSetting.cpp
    Window/GSWnd.cpp
    )

set(GSdxHeaders
    config.h
    GSAlignedClass.h
    GSBlock.h
    GSCapture.h
    GSClut.h
    GSCodeBuffer.h
    GSCrc.h
    GSDrawingContext.h
    GSDrawingEnvironment.h
    GSDump.h
    GSdx.h
    GSdxResources.h
    GS.h
    GSLocalMemory.h
    GSLzma.h
    GSPerfMon.h
    GSPng.h
    GSState.h
    GSTables.h
    GSThread_CXX11.h
    GSThread.h
    GSUtil.h
    GSVector.h
    GSVector4.h
    GSVector4i.h
    GSVector8.h
    GSVector8i.h
    stdafx.h
    Renderers/Common/GSDevice.h
    Renderers/Common/GSDirtyRect.h
    Renderers/Common/GSFastList.h
    Renderers/Common/GSFunctionMap.h
    Renderers/Common/GSOsdManager.h
    Renderers/Common/GSRenderer.h
    Renderers/Common/GSTexture.h
    Renderers/Common/GSVertex.h
    Renderers/Common/GSVertexList.h
    Renderers/Common/GSVertexTrace.h
    Renderers/Null/GSDeviceNull.h
    Renderers/Null/GSRendererNull.h
    Renderers/Null/GSTextureNull.h
    Renderers/HW/GSRendererHW.h
    Renderers/HW/GSTextureCache.h
    Renderers/HW/GSVertexHW.h
    Renderers/SW/GSDrawScanlineCodeGenerator.h
    Renderers/SW/GSDrawScanline.h
    Renderers/SW/GSRasterizer.h
    Renderers/SW/GSRendererSW.h
    Renderers/SW/GSScanlineEnvironment.h
    Renderers/SW/GSSetupPrimCodeGenerator.h
    Renderers/SW/GSTextureCacheSW.h
    Renderers/SW/GSTextureSW.h
    Renderers/SW/GSVertexSW.h
    Renderers/OpenGL/GLLoader.h
    Renderers/OpenGL/GLState.h
    Renderers/OpenGL/GSDeviceOGL.h
    Renderers/OpenGL/GSRendererOGL.h
    Renderers/OpenGL/GSShaderOGL.h
    Renderers/OpenGL/GSTextureCacheOGL.h
    Renderers/OpenGL/GSTextureOGL.h
    Renderers/OpenGL/GSUniformBufferOGL.h
    Renderers/OpenGL/GSVertexArrayOGL.h
    Window/GSCaptureDlg.h
    Window/GSDialog.h
    Window/GSSetting.h
    Window/GSSettingsDlg.h
    Window/GSWnd.h
    xbyak/xbyak.h
    xbyak/xbyak_mnemonic.h
    xbyak/xbyak_util.h
    )

if(Windows)
    LIST(APPEND GSdxSources
        Renderers/DX11/GSDevice11.cpp
        Window/GSDialog.cpp
        Renderers/DX11/GSRendererDX11.cpp
        Renderers/DX11/GSTexture11.cpp
        Renderers/DX11/GSTextureCache11.cpp
        Renderers/DX11/GSTextureFX11.cpp
        GSVertexList.cpp
        Renderers/SW/GSVertexSW.cpp
        Window/GSCaptureDlg.cpp
        Window/GSSettingsDlg.cpp
        Window/GSWndDX.cpp
        Window/GSWndWGL.cpp
        )

    LIST(APPEND GSdxHeaders
        Renderers/DX11/GSDevice11.h
        Renderers/DX11/GSRendererDX11.h
        Renderers/DX11/GSTexture11.h
        Renderers/DX11/GSTextureCache11.h
        Window/GSWndDX.h
        Window/GSWndWGL.h
        resource.h
        targetver.h
        )
elseif(APPLE)
    LIST(APPEND GSdxSources
        Window/GSLinuxDialog.cpp
        )
else()
    LIST(APPEND GSdxSources
        Window/GSLinuxDialog.cpp
        Window/GSWndEGL.cpp
        )

    LIST(APPEND GSdxHeaders
        Window/GSWndEGL.h
        )
endif()


set(GSdxFinalSources
    ${GSdxSources}
    ${GSdxHeaders}
)

set(GSdxFinalLibs
    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${EGL_LIBRARIES}
    ${X11_XCB_LIBRARIES}
    ${GTK2_LIBRARIES}
    ${LIBC_LIBRARIES}
    ${PNG_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${LIBLZMA_LIBRARIES}
)

if(USE_VTUNE)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${VTUNE_LIBRARIES})
endif()

if(APPLE)
    find_library(APPKIT_LIBRARY AppKit)
    set(GSdxFinalFlags ${GSdxFinalFlags} -fobjc-arc)
    set(GSdxFinalLibs ${GSdxFinalLibs} ${APPKIT_LIBRARY} -fobjc-link-runtime)
endif()

set(RESOURCE_FILES
    res/logo-ogl.bmp
    res/fxaa.fx
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
        target_compile_features("${Output}-SSE4" PRIVATE cxx_std_17)
        add_pcsx2_plugin("${Output}-AVX2" "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags} -mavx -mavx2 -mbmi -mbmi2")
        target_compile_features("${Output}-AVX2" PRIVATE cxx_std_17)
    else()
        add_pcsx2_plugin(${Output} "${GSdxFinalSources}" "${GSdxFinalLibs}" "${GSdxFinalFlags}")
    endif()
endif()

target_compile_features(${Output} PRIVATE cxx_std_17)

################################### Replay Loader
if(BUILD_REPLAY_LOADERS)
    set(Replay pcsx2_GSReplayLoader)
    set(GSdxReplayLoaderFinalSources
        linux_replay.cpp
    )
    add_pcsx2_executable(${Replay} "${GSdxReplayLoaderFinalSources}" "${LIBC_LIBRARIES}" "${GSdxFinalFlags}")
    target_compile_features(${Replay} PRIVATE cxx_std_17)
endif(BUILD_REPLAY_LOADERS)
