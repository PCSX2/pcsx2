#-------------------------------------------------------------------------------
#                              Dependency message print
#-------------------------------------------------------------------------------
set(msg_dep_common_libs "check these libraries -> wxWidgets (>=2.8.10), aio")
set(msg_dep_pcsx2       "check these libraries -> wxWidgets (>=2.8.10), gtk2 (>=2.16), zlib (>=1.2.4), pcsx2 common libs")
set(msg_dep_cdvdiso     "check these libraries -> bzip2 (>=1.0.5), gtk2 (>=2.16)")
set(msg_dep_zerogs      "check these libraries -> glew (>=1.6), opengl, X11, nvidia-cg-toolkit (>=2.1)")
set(msg_dep_gsdx        "check these libraries -> opengl, png++, X11")
set(msg_dep_onepad      "check these libraries -> sdl (>=1.2), X11")
set(msg_dep_spu2x       "check these libraries -> soundtouch (>=1.5), alsa, portaudio (>=1.9), sdl (>=1.2) pcsx2 common libs")
set(msg_dep_zerospu2    "check these libraries -> soundtouch (>=1.5), alsa")
if(GLSL_API)
	set(msg_dep_zzogl       "check these libraries -> glew (>=1.6), jpeg (>=6.2), opengl, X11, pcsx2 common libs")
else(GLSL_API)
	set(msg_dep_zzogl       "check these libraries -> glew (>=1.6), jpeg (>=6.2), opengl, X11, nvidia-cg-toolkit (>=2.1), pcsx2 common libs")
endif()

macro(print_dep str dep)
    if (PACKAGE_MODE)
        message(FATAL_ERROR "${str}:${dep}")
    else()
        message(STATUS "${str}:${dep}")
    endif()
endmacro(print_dep)

#-------------------------------------------------------------------------------
#								Pcsx2 core & common libs
#-------------------------------------------------------------------------------
# Check for additional dependencies.
# If all dependencies are available, including OS, build it
#-------------------------------------------------------------------------------
if (GTK2_FOUND OR GTK3_FOUND)
    set(GTKn_FOUND TRUE)
else()
    set(GTKn_FOUND FALSE)
endif()

if(SDL_FOUND OR SDL2_FOUND)
    set(SDLn_FOUND TRUE)
else()
    set(SDLn_FOUND FALSE)
endif()

#---------------------------------------
#			Common libs
# requires: -wx
#---------------------------------------
if(wxWidgets_FOUND)
    set(common_libs TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/common/src")
    set(common_libs FALSE)
else()
    set(common_libs FALSE)
    print_dep("Skip build of common libraries: miss some dependencies" "${msg_dep_common_libs}")
endif()

#---------------------------------------
#			Pcsx2 core
# requires: -wx
#           -gtk2 (linux)
#           -zlib
#           -common_libs
#           -aio
#---------------------------------------
# Common dependancy
if(wxWidgets_FOUND AND ZLIB_FOUND AND common_libs AND NOT (Linux AND NOT AIO_FOUND))
    set(pcsx2_core TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/pcsx2")
    set(pcsx2_core FALSE)
else()
    set(pcsx2_core FALSE)
    print_dep("Skip build of pcsx2 core: miss some dependencies" "${msg_dep_pcsx2}")
endif()
# Linux need also gtk2
if(UNIX AND pcsx2_core AND NOT GTKn_FOUND)
    set(pcsx2_core FALSE)
    print_dep("Skip build of pcsx2 core: miss some dependencies" "${msg_dep_pcsx2}")
endif()


#-------------------------------------------------------------------------------
#								Plugins
#-------------------------------------------------------------------------------
# Check all plugins for additional dependencies.
# If all dependencies of a plugin are available, including OS, the plugin will
# be build.
#-------------------------------------------------------------------------------

#---------------------------------------
#			CDVDnull
#---------------------------------------
if(GTKn_FOUND)
    set(CDVDnull TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			CDVDiso
#---------------------------------------
# requires: -BZip2
#           -gtk2 (linux)
#---------------------------------------
if(EXTRA_PLUGINS)
    if(BZIP2_FOUND AND GTKn_FOUND)
        set(CDVDiso TRUE)
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDiso")
        set(CDVDiso FALSE)
    else()
        set(CDVDiso FALSE)
        print_dep("Skip build of CDVDiso: miss some dependencies" "${msg_dep_cdvdiso}")
    endif()
endif()

#---------------------------------------
#			CDVDlinuz
#---------------------------------------
if(EXTRA_PLUGINS)
    set(CDVDlinuz TRUE)
endif()

#---------------------------------------
#			dev9null
#---------------------------------------
if(GTKn_FOUND)
    set(dev9null TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			FWnull
#---------------------------------------
if(GTKn_FOUND)
    set(FWnull TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			GSnull
#---------------------------------------
if(GTKn_FOUND AND EXTRA_PLUGINS)
    set(GSnull TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			GSdx
#---------------------------------------
# requires: -OpenGL
#			-X11
#---------------------------------------
if(OPENGL_FOUND AND X11_FOUND AND GTKn_FOUND AND PNG_FOUND AND (EGL_FOUND OR NOT EGL_API))
    set(GSdx TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx")
    set(GSdx FALSE)
else()
    set(GSdx FALSE)
    print_dep("Skip build of GSdx: miss some dependencies" "${msg_dep_gsdx}")
endif()
#---------------------------------------

#---------------------------------------
#			zerogs
#---------------------------------------
# requires:	-GLEW
#			-OpenGL
#			-X11
#			-CG
#---------------------------------------
if(EXTRA_PLUGINS)
    if(GLEW_FOUND AND OPENGL_FOUND AND X11_FOUND AND CG_FOUND)
        set(zerogs TRUE)
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/zerogs")
        set(zerogs FALSE)
    else()
        set(zerogs FALSE)
        print_dep("Skip build of zerogs: miss some dependencies" "${msg_dep_zerogs}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			zzogl-pg
#---------------------------------------
# requires:	-GLEW
#			-OpenGL
#			-X11
#			-CG (only with cg build)
#			-JPEG
#           -common_libs
#---------------------------------------
if(EXTRA_PLUGINS)
    if((GLEW_FOUND AND OPENGL_FOUND AND X11_FOUND AND JPEG_FOUND AND common_libs AND GTKn_FOUND) AND (CG_FOUND OR GLSL_API))
        set(zzogl TRUE)
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/zzogl-pg")
        set(zzogl FALSE)
    else()
        set(zzogl FALSE)
        print_dep("Skip build of zzogl: miss some dependencies" "${msg_dep_zzogl}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			PadNull
#---------------------------------------
if(GTKn_FOUND AND EXTRA_PLUGINS)
    set(PadNull TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			LilyPad
# requires: -X11
#---------------------------------------
# Not ready to be packaged
if(EXTRA_PLUGINS OR NOT PACKAGE_MODE)
    if(GTKn_FOUND AND X11_FOUND)
        set(LilyPad TRUE)
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			onepad
#---------------------------------------
# requires: -SDL
#			-X11
#---------------------------------------
if(SDLn_FOUND AND X11_FOUND)
	set(onepad TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/onepad")
	set(onepad FALSE)
else()
	set(onepad FALSE)
    print_dep("Skip build of onepad: miss some dependencies" "${msg_dep_onepad}")
endif()
#---------------------------------------

#---------------------------------------
#			SPU2null
#---------------------------------------
if(GTKn_FOUND AND EXTRA_PLUGINS)
    set(SPU2null TRUE)
endif()
#---------------------------------------

#---------------------------------------
#			spu2-x
#---------------------------------------
# requires: -SoundTouch
#			-ALSA
#           -Portaudio
#           -SDL
#           -common_libs
#---------------------------------------
if(ALSA_FOUND AND PORTAUDIO_FOUND AND SOUNDTOUCH_FOUND AND SDLn_FOUND AND common_libs)
	set(spu2-x TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/spu2-x")
	set(spu2-x FALSE)
else()
	set(spu2-x FALSE)
    print_dep("Skip build of spu2-x: miss some dependencies" "${msg_dep_spu2x}")
endif()
#---------------------------------------

#---------------------------------------
#			zerospu2
#---------------------------------------
# requires: -SoundTouch
#			-ALSA
#			-PortAudio
#---------------------------------------
if(EXTRA_PLUGINS)
    if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zerospu2" AND SOUNDTOUCH_FOUND AND ALSA_FOUND)
        set(zerospu2 TRUE)
        # Comment the next line, if you want to compile zerospu2
        set(zerospu2 FALSE)
        message(STATUS "Don't build zerospu2. It is super-seeded by spu2x")
    elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/zerospu2")
        set(zerospu2 FALSE)
    else()
        set(zerospu2 FALSE)
        print_dep("Skip build of zerospu2: miss some dependencies" "${msg_dep_zerospu2}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			USBnull
#---------------------------------------
if(GTKn_FOUND)
    set(USBnull TRUE)
endif()
#---------------------------------------

#-------------------------------------------------------------------------------
#			[TODO] Write CMakeLists.txt for these plugins.
set(cdvdGigaherz FALSE)
set(CDVDisoEFP FALSE)
set(CDVDolio FALSE)
set(CDVDpeops FALSE)
set(PeopsSPU2 FALSE)
set(SSSPSXPAD FALSE)
set(xpad FALSE)
#-------------------------------------------------------------------------------
