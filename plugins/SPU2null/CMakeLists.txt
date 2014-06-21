# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# SPU2null sources
set(SPU2nullSources
	SPU2.cpp)

# SPU2null headers
set(SPU2nullHeaders
	SPU2.h)

# SPU2null Linux sources
set(SPU2nullLinuxSources
	Linux/Config.cpp)

# SPU2null Linux headers
set(SPU2nullLinuxHeaders
	Linux/Config.h)

add_pcsx2_plugin(SPU2null "${SPU2nullSources} ${SPU2nullHeaders} ${SPU2nullLinuxSources} ${SPU2nullLinuxHeaders}" "${GTK2_LIBRARIES}")
