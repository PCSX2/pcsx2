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
if(wxWidgets_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/common/src/Utilities" AND EXISTS "${CMAKE_SOURCE_DIR}/common/src/x86emitter")
    set(common_libs TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/common/src/Utilities" OR NOT EXISTS "${CMAKE_SOURCE_DIR}/common/src/x86emitter")
    set(common_libs FALSE)
else()
    message(FATAL_ERROR "Common libraries miss some dependencies: ${msg_dep_common_libs}")
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
if(BUILD_CORE)
    if(wxWidgets_FOUND AND ZLIB_FOUND AND common_libs AND NOT (Linux AND NOT AIO_FOUND))
        set(pcsx2_core TRUE)
    else()
        set(pcsx2_core FALSE)
        message(FATAL_ERROR "PCSX2 core miss some dependencies: ${msg_dep_pcsx2}")
    endif()
    # Linux need also gtk2
    if(UNIX AND pcsx2_core AND NOT GTKn_FOUND)
        set(pcsx2_core FALSE)
        message(FATAL_ERROR "PCSX2 core miss some dependencies: ${msg_dep_pcsx2}")
    endif()
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
if(BUILD_CDVDNULL)
    if(GTKn_FOUND)
        set(CDVDnull TRUE)
    else()
        message(FATAL_ERROR "CDVDnull miss GTK dependency")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			CDVDiso
#---------------------------------------
# requires: -BZip2
#           -gtk2 (linux)
#---------------------------------------
if(BUILD_CDVDISO)
    if(BZIP2_FOUND AND GTKn_FOUND)
        set(CDVDiso TRUE)
    else()
        set(CDVDiso FALSE)
        message(FATAL_ERROR "CDVDiso miss some dependencies: ${msg_dep_cdvdiso}")
    endif()
endif()

#---------------------------------------
#			CDVDlinuz
#---------------------------------------
if(BUILD_CDVDLINUZ)
    set(CDVDlinuz TRUE)
endif()

#---------------------------------------
#			dev9null
#---------------------------------------
if(BUILD_DEV9NULL)
    if(GTKn_FOUND)
        set(dev9null TRUE)
    else()
        message(FATAL_ERROR "dev9null miss GTK dependency")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			FWnull
#---------------------------------------
if(BUILD_FWNULL)
    if(GTKn_FOUND)
        set(FWnull TRUE)
    else()
        message(FATAL_ERROR "fwnull miss GTK dependency")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			GSnull
#---------------------------------------
if(BUILD_GSNULL)
    if(GTKn_FOUND)
        set(GSnull TRUE)
    else()
        message(FATAL_ERROR "GSnull miss GTK dependency")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			GSdx
#---------------------------------------
# requires: -OpenGL
#			-X11
#---------------------------------------
if(BUILD_GSDX)
    if(OPENGL_FOUND AND X11_FOUND AND GTKn_FOUND AND PNG_FOUND AND (EGL_FOUND OR NOT EGL_API))
        set(GSdx TRUE)
    else()
        set(GSdx FALSE)
        message(FATAL_ERROR "GSdx miss some dependencies: ${msg_dep_gsdx}")
    endif()
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
if(BUILD_ZEROGS)
    if(GLEW_FOUND AND OPENGL_FOUND AND X11_FOUND AND CG_FOUND)
        set(zerogs TRUE)
    else()
        set(zerogs FALSE)
        message(FATAL_ERROR "zerogs miss some dependencies: ${msg_dep_zerogs}")
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
if(BUILD_ZZOGL-PG)
    if((GLEW_FOUND AND OPENGL_FOUND AND X11_FOUND AND JPEG_FOUND AND common_libs AND GTKn_FOUND) AND (CG_FOUND OR GLSL_API))
        set(zzogl TRUE)
    else()
        set(zzogl FALSE)
        message(FATAL_ERROR "zzogl miss some dependencies: ${msg_dep_zzogl}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			PadNull
#---------------------------------------
if(BUILD_PADNULL)
    if(GTKn_FOUND)
        set(PadNull TRUE)
    else()
        message(FATAL_ERROR "PadNull miss GTK dependency")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			LilyPad
# requires: -X11
#---------------------------------------
if(BUILD_LILYPAD)
    if(GTKn_FOUND AND X11_FOUND)
        set(LilyPad TRUE)
    else()
        message(FATAL_ERROR "LilyPad miss GTK or X11 dependencies")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			onepad
#---------------------------------------
# requires: -SDL
#			-X11
#---------------------------------------
if(BUILD_ONEPAD)
    if(SDLn_FOUND AND X11_FOUND)
        set(onepad TRUE)
    else()
        set(onepad FALSE)
        message(FATAL_ERROR "onepad miss some dependencies: ${msg_dep_onepad}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			SPU2null
#---------------------------------------
if(BUILD_SPU2NULL)
    if(GTKn_FOUND)
        set(SPU2null TRUE)
    else()
        message(FATAL_ERROR "SPU2null miss GTK dependency")
    endif()
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
if(BUILD_SPU2-X)
    if(ALSA_FOUND AND PORTAUDIO_FOUND AND SOUNDTOUCH_FOUND AND SDLn_FOUND AND common_libs)
        set(spu2-x TRUE)
    else()
        set(spu2-x FALSE)
        message(FATAL_ERROR "spu2-x miss some dependencies: ${msg_dep_spu2x}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			zerospu2
#---------------------------------------
# requires: -SoundTouch
#			-ALSA
#			-PortAudio
#---------------------------------------
if(BUILD_ZEROSPU2)
    if(SOUNDTOUCH_FOUND AND ALSA_FOUND)
        set(zerospu2 TRUE)
        message(STATUS "Don't build zerospu2. It is super-seeded by spu2x")
    else()
        set(zerospu2 FALSE)
        message(FATAL_ERROR "zerospu2 miss some dependencies: ${msg_dep_zerospu2}")
    endif()
endif()
#---------------------------------------

#---------------------------------------
#			USBnull
#---------------------------------------
if(BUILD_USBNULL)
    if(GTKn_FOUND)
        set(USBnull TRUE)
    else()
        message(FATAL_ERROR "USBnull miss GTK dependency")
    endif()
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
