# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# SPU2null sources
set(SPU2nullSources
	SPU2.cpp)

# SPU2null Linux sources
set(SPU2nullLinuxSources
	Linux/Config.cpp)

set(SPU2nullFinalSources
	${SPU2nullSources} 
	${SPU2nullLinuxSources}
)

add_pcsx2_plugin(SPU2null "${SPU2nullFinalSources}" "${CMAKE_THREAD_LIBS_INIT} ${GTK2_LIBRARIES}")
