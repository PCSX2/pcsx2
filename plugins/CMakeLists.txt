# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)


# make CDVDnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDnull" AND CDVDnull)
	add_subdirectory(CDVDnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDnull" AND CDVDnull)

# make dev9null
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/dev9null" AND dev9null)
	add_subdirectory(dev9null)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/dev9null" AND dev9null)

# make FWnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/FWnull" AND FWnull)
	add_subdirectory(FWnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/FWnull" AND FWnull)

# make GSnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSnull" AND GSnull)
	add_subdirectory(GSnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSnull" AND GSnull)

# make PadNull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/PadNull" AND PadNull)
	add_subdirectory(PadNull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/PadNull" AND PadNull)

# make SPU2null
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2null" AND SPU2null)
	add_subdirectory(SPU2null)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2null" AND SPU2null)

# make USBnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/USBnull" AND USBnull)
	add_subdirectory(USBnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/USBnull" AND USBnull)

# make CDVDiso
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDiso" AND CDVDiso)
	add_subdirectory(CDVDiso/src)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDiso" AND CDVDiso)

# make GSdx
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx" AND GSdx)
	add_subdirectory(GSdx)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx" AND GSdx)

# make onepad
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/onepad" AND onepad)
	add_subdirectory(onepad)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/onepad" AND onepad)

# make spu2-x
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/spu2-x" AND spu2-x)
	add_subdirectory(spu2-x/src)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/spu2-x" AND spu2-x)
