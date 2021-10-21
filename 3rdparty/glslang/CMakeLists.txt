add_library(glslang
	glslang/glslang/CInterface/glslang_c_interface.cpp
	glslang/glslang/GenericCodeGen/CodeGen.cpp
	glslang/glslang/GenericCodeGen/Link.cpp
	glslang/glslang/MachineIndependent/attribute.cpp
	glslang/glslang/MachineIndependent/Constant.cpp
	glslang/glslang/MachineIndependent/glslang_tab.cpp
	glslang/glslang/MachineIndependent/InfoSink.cpp
	glslang/glslang/MachineIndependent/Initialize.cpp
	glslang/glslang/MachineIndependent/Intermediate.cpp
	glslang/glslang/MachineIndependent/intermOut.cpp
	glslang/glslang/MachineIndependent/IntermTraverse.cpp
	glslang/glslang/MachineIndependent/iomapper.cpp
	glslang/glslang/MachineIndependent/limits.cpp
	glslang/glslang/MachineIndependent/linkValidate.cpp
	glslang/glslang/MachineIndependent/parseConst.cpp
	glslang/glslang/MachineIndependent/ParseContextBase.cpp
	glslang/glslang/MachineIndependent/ParseHelper.cpp
	glslang/glslang/MachineIndependent/PoolAlloc.cpp
	glslang/glslang/MachineIndependent/preprocessor/Pp.cpp
	glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp
	glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp
	glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp
	glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp
	glslang/glslang/MachineIndependent/propagateNoContraction.cpp
	glslang/glslang/MachineIndependent/reflection.cpp
	glslang/glslang/MachineIndependent/RemoveTree.cpp
	glslang/glslang/MachineIndependent/Scan.cpp
	glslang/glslang/MachineIndependent/ShaderLang.cpp
	glslang/glslang/MachineIndependent/SpirvIntrinsics.cpp
	glslang/glslang/MachineIndependent/SymbolTable.cpp
	glslang/glslang/MachineIndependent/Versions.cpp
	glslang/OGLCompilersDLL/InitializeDll.cpp
	glslang/SPIRV/disassemble.cpp
	glslang/SPIRV/doc.cpp
	glslang/SPIRV/GlslangToSpv.cpp
	glslang/SPIRV/InReadableOrder.cpp
	glslang/SPIRV/Logger.cpp
	glslang/SPIRV/SpvBuilder.cpp
	glslang/SPIRV/SpvPostProcess.cpp
	glslang/SPIRV/SPVRemapper.cpp
	glslang/SPIRV/SpvTools.cpp
	glslang/StandAlone/ResourceLimits.cpp
	glslang/StandAlone/resource_limits_c.cpp
)

if(WIN32)
	target_sources(glslang PRIVATE glslang/glslang/OSDependent/Windows/ossource.cpp)
else()
	target_sources(glslang PRIVATE glslang/glslang/OSDependent/Unix/ossource.cpp)
endif()

target_include_directories(glslang PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glslang" "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Needed for macOS compile.
set_property(TARGET glslang PROPERTY CXX_STANDARD 11)
set_property(TARGET glslang PROPERTY CXX_STANDARD_REQUIRED ON)

