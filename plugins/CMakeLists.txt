# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif()


#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/cdvdGigaherz" AND cdvdGigaherz)
#	add_subdirectory(cdvdGigaherz)
#endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDiso" AND CDVDiso)
	add_subdirectory(CDVDiso/src)
endif()

# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDisoEFP" AND CDVDisoEFP)
# 	add_subdirectory(CDVDisoEFP)
# endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDlinuz" AND CDVDlinuz)
	add_subdirectory(CDVDlinuz/Src)
endif()

# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDolio" AND CDVDolio)
# 	add_subdirectory(CDVDolio)
# endif()

#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVDpeops" AND CDVDpeops)
#	add_subdirectory(CDVDpeops)
#endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GSdx" AND GSdx)
	add_subdirectory(GSdx)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/LilyPad" AND LilyPad)
	add_subdirectory(LilyPad)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/onepad" AND onepad)
	add_subdirectory(onepad)
endif()

# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/PeopsSPU2" AND PeopsSPU2)
# 	add_subdirectory(PeopsSPU2)
# endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/spu2-x" AND spu2-x)
	add_subdirectory(spu2-x/src)
endif()

#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SSSPSXPAD" AND SSSPSXPAD)
#	add_subdirectory(SSSPSXPAD)
#endif()


#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/xpad" AND xpad)
#	add_subdirectory(xpad)
#endif()

#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zerogs" AND zerogs)
#	add_subdirectory(zerogs)
#endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zzogl-pg" AND zzogl)
	add_subdirectory(zzogl-pg/opengl)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zeropad" AND zeropad)
	add_subdirectory(zeropad)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/zerospu2" AND zerospu2)
	add_subdirectory(zerospu2)
endif()

