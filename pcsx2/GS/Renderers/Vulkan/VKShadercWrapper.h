#pragma once

#include "common/Pcsx2Defs.h"
#include "common/DynamicLibrary.h"

#include "shaderc/shaderc.h"

#include <vector>
#include <optional>

#define SHADERC_FUNCTIONS(X) \
	X(shaderc_compiler_initialize) \
	X(shaderc_compiler_release) \
	X(shaderc_compile_options_initialize) \
	X(shaderc_compile_options_release) \
	X(shaderc_compile_options_set_source_language) \
	X(shaderc_compile_options_set_generate_debug_info) \
	X(shaderc_compile_options_set_optimization_level) \
	X(shaderc_compile_options_set_target_env) \
	X(shaderc_compile_into_spv) \
	X(shaderc_result_release) \
	X(shaderc_result_get_length) \
	X(shaderc_result_get_num_warnings) \
	X(shaderc_result_get_bytes) \
	X(shaderc_result_get_error_message) \
	X(shaderc_result_get_compilation_status)

namespace VKShadercWrapper
{
	using SPIRVCodeType = u32;
	using SPIRVCodeVector = std::vector<SPIRVCodeType>;

	bool Open();
	void Close();
	shaderc_compiler_t CreateCompiler();

	std::optional<SPIRVCodeVector> CompileShaderToSPV(
		shaderc_compiler_t compiler, u32 stage, std::string_view source, bool debug, bool enable_non_semantic,
		std::string* errors_out = nullptr);

#define DECLARE_FUNC(F) extern decltype(&::F) F;
	SHADERC_FUNCTIONS(DECLARE_FUNC)
#undef DECLARE_FUNC

} // namespace VKShadercWrapper
