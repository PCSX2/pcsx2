#-------------------------------------------------------------------------------
#                              Dependency message print
#-------------------------------------------------------------------------------
set(msg_dep_common_libs "check these libraries -> wxWidgets (>=3.0), aio")
set(msg_dep_pcsx2       "check these libraries -> wxWidgets (>=3.0), gtk2, zlib (>=1.2.4), pcsx2 common libs")
set(msg_dep_gsdx        "check these libraries -> opengl, png (>=1.2), zlib (>=1.2.4), X11, liblzma")
set(msg_dep_onepad      "check these libraries -> sdl2, X11, gtk2")
set(msg_dep_spu2x       "check these libraries -> soundtouch (>=1.5), alsa, portaudio (optional, >=1.9), sdl (>=1.2), pcsx2 common libs")
set(msg_dep_dev         "check these libraries -> gtk2, pcap, libxml2")

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
elseif(APPLE) # Not we have but that we don't change all if(gtkn) entries
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
    print_dep("Skip build of common libraries: missing dependencies" "${msg_dep_common_libs}")
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
    print_dep("Skip build of pcsx2 core: missing dependencies" "${msg_dep_pcsx2}")
endif()
# Linux, BSD, use gtk2, but not OSX
if(UNIX AND pcsx2_core AND NOT GTKn_FOUND AND NOT APPLE)
    set(pcsx2_core FALSE)
    print_dep("Skip build of pcsx2 core: missing dependencies" "${msg_dep_pcsx2}")
endif()


#-------------------------------------------------------------------------------
#								Plugins
#-------------------------------------------------------------------------------
# Check all plugins for additional dependencies.
# If all dependencies of a plugin are available, including OS, the plugin will
# be build.
#-------------------------------------------------------------------------------


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
#           -PNG
#           -X11
#           -zlib
#---------------------------------------
if(OPENGL_FOUND AND X11_FOUND AND GTKn_FOUND AND ZLIB_FOUND AND PNG_FOUND AND FREETYPE_FOUND AND LIBLZMA_FOUND AND ((EGL_FOUND AND X11_XCB_FOUND) OR APPLE))
    set(GSdx TRUE)
elseif(NOT EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx")
    set(GSdx FALSE)
else()
    set(GSdx FALSE)
    print_dep("Skip build of GSdx: missing dependencies" "${msg_dep_gsdx}")
endif()
#---------------------------------------

#-------------------------------------------------------------------------------
