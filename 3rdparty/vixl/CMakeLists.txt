add_library(vixl
	include/vixl/aarch64/abi-aarch64.h
	include/vixl/aarch64/assembler-aarch64.h
	include/vixl/aarch64/constants-aarch64.h
	include/vixl/aarch64/cpu-aarch64.h
	include/vixl/aarch64/cpu-features-auditor-aarch64.h
	include/vixl/aarch64/decoder-aarch64.h
	include/vixl/aarch64/decoder-constants-aarch64.h
	include/vixl/aarch64/decoder-visitor-map-aarch64.h
	include/vixl/aarch64/disasm-aarch64.h
	include/vixl/aarch64/instructions-aarch64.h
	include/vixl/aarch64/macro-assembler-aarch64.h
	include/vixl/aarch64/operands-aarch64.h
	include/vixl/aarch64/registers-aarch64.h
	include/vixl/aarch64/simulator-aarch64.h
	include/vixl/aarch64/simulator-constants-aarch64.h
	include/vixl/assembler-base-vixl.h
	include/vixl/code-buffer-vixl.h
	include/vixl/code-generation-scopes-vixl.h
	include/vixl/compiler-intrinsics-vixl.h
	include/vixl/cpu-features.h
	include/vixl/globals-vixl.h
	include/vixl/invalset-vixl.h
	include/vixl/macro-assembler-interface.h
	include/vixl/platform-vixl.h
	include/vixl/pool-manager-impl.h
	include/vixl/pool-manager.h
	include/vixl/utils-vixl.h
	src/aarch64/assembler-aarch64.cc
	src/aarch64/assembler-sve-aarch64.cc
	src/aarch64/cpu-aarch64.cc
	src/aarch64/cpu-features-auditor-aarch64.cc
	src/aarch64/decoder-aarch64.cc
	src/aarch64/disasm-aarch64.cc
	src/aarch64/instructions-aarch64.cc
	src/aarch64/logic-aarch64.cc
	src/aarch64/macro-assembler-aarch64.cc
	src/aarch64/macro-assembler-sve-aarch64.cc
	src/aarch64/operands-aarch64.cc
	src/aarch64/pointer-auth-aarch64.cc
	src/aarch64/registers-aarch64.cc
	src/code-buffer-vixl.cc
	src/compiler-intrinsics-vixl.cc
	src/cpu-features.cc
	src/utils-vixl.cc
)

target_include_directories(vixl PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(vixl PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/vixl
	${CMAKE_CURRENT_SOURCE_DIR}/include/vixl/aarch64
)
target_compile_definitions(vixl PUBLIC
	VIXL_INCLUDE_TARGET_A64
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	message("Enabling vixl debug assertions")
	target_compile_definitions(vixl PUBLIC VIXL_DEBUG)
endif()

