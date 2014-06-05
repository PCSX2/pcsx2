# Check that people use the good file
if(NOT TOP_CMAKE_WAS_SOURCED)
    message(FATAL_ERROR "
    You did not 'cmake' the good CMakeLists.txt file. Use the one in the top dir.
    It is advice to delete all wrongly generated cmake stuff => CMakeFiles & CMakeCache.txt")
endif(NOT TOP_CMAKE_WAS_SOURCED)

# ***CD/DVD plugins***

# make cdvdGigaherz
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/cdvdGigaherz" AND cdvdGigaherz)
#	add_subdirectory(CDVD/cdvdGigaherz)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/cdvdGigaherz" AND cdvdGigaherz)

# make CDVDiso
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDiso" AND CDVDiso)
	add_subdirectory(CDVD/CDVDiso/src)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDiso" AND CDVDiso)

# make CDVDisoEFP
# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDisoEFP" AND CDVDisoEFP)
# 	add_subdirectory(CDVD/CDVDisoEFP)
# endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDisoEFP" AND CDVDisoEFP)

# make CDVDlinuz
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDlinuz" AND CDVDlinuz)
	add_subdirectory(CDVD/CDVDlinuz/Src)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDlinuz" AND CDVDlinuz)

# make CDVDnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDnull" AND CDVDnull)
	add_subdirectory(CDVD/CDVDnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDnull" AND CDVDnull)

# make CDVDolio
# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDolio" AND CDVDolio)
# 	add_subdirectory(CDVD/CDVDolio)
# endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDolio" AND CDVDolio)

# make CDVDpeops
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDpeops" AND CDVDpeops)
#	add_subdirectory(CDVD/CDVDpeops)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/CDVD/CDVDpeops" AND CDVDpeops)

# ***DEV9 plugins***

# make dev9null
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/DEV9/dev9null" AND dev9null)
	add_subdirectory(DEV9/dev9null)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/DEV9/dev9null" AND dev9null)

# ***FireWire plugins***

# make FWnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/FireWire/FWnull" AND FWnull)
	add_subdirectory(FireWire/FWnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/FireWire/FWnull" AND FWnull)

# ***GS plugins***

# make GSdx
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/GSdx" AND GSdx)
	add_subdirectory(GS/GSdx)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/GSdx" AND GSdx)

# make zerogs
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zerogs" AND zerogs)
#	add_subdirectory(GS/zerogs)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zerogs" AND zerogs)

# make zzogl-pg
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg" AND zzogl)
	add_subdirectory(GS/zzogl-pg/opengl)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg" AND zzogl)

# make zzogl-pg-cg
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg-cg" AND zzoglcg AND EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg" AND zzogl)
	add_subdirectory(GS/zzogl-pg-cg/opengl)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg-cg" AND zzoglcg AND EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/zzogl-pg" AND zzogl)

# make GSnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/GSnull" AND GSnull)
	add_subdirectory(GS/GSnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/GS/GSnull" AND GSnull)

# ***Control pad plugins***

# make LilyPad
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/LilyPad" AND LilyPad)
#	add_subdirectory(Pad/LilyPad)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/LilyPad" AND LilyPad)

# make onepad
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/onepad" AND onepad)
	add_subdirectory(Pad/onepad)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/onepad" AND onepad)

# make PadNull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/PadNull" AND PadNull)
	add_subdirectory(Pad/PadNull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/PadNull" AND PadNull)

# make SSSPSXPAD
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/SSSPSXPAD" AND SSSPSXPAD)
#	add_subdirectory(Pad/SSSPSXPAD)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/SSSPSXPAD" AND SSSPSXPAD)

# make xpad
#if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/xpad" AND xpad)
#	add_subdirectory(Pad/xpad)
#endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/xpad" AND xpad)

# make zeropad
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/zeropad" AND zeropad)
	add_subdirectory(Pad/zeropad)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/Pad/zeropad" AND zeropad)

# ***Sound/Audio plugins***

# make PeopsSPU2
# if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/PeopsSPU2" AND PeopsSPU2)
# 	add_subdirectory(SPU2/PeopsSPU2)
# endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/PeopsSPU2" AND PeopsSPU2)

# make SPU2null
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/SPU2null" AND SPU2null)
	add_subdirectory(SPU2/SPU2null)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/SPU2null" AND SPU2null)

# make spu2-x
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/spu2-x" AND spu2-x)
	add_subdirectory(SPU2/spu2-x/src)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/spu2-x" AND spu2-x)

# make zerospu2
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/zerospu2" AND zerospu2)
	add_subdirectory(SPU2/zerospu2)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/SPU2/zerospu2" AND zerospu2)

# ***USB plugins***

# make USBnull
if(EXISTS "${CMAKE_SOURCE_DIR}/plugins/USB/USBnull" AND USBnull)
	add_subdirectory(USB/USBnull)
endif(EXISTS "${CMAKE_SOURCE_DIR}/plugins/USB/USBnull" AND USBnull)

