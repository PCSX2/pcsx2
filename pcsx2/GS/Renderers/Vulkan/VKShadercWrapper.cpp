#include "GS/Renderers/Vulkan/VKShadercWrapper.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"

namespace VKShadercWrapper
{
	static DynamicLibrary s_library;
	static std::vector<shaderc_compiler_t> s_compilers;

#define DEFINE_FUNC(F) decltype(&::F) F;
	SHADERC_FUNCTIONS(DEFINE_FUNC)
#undef DEFINE_FUNC
};

bool VKShadercWrapper::Open()
{
	if (s_library.IsOpen())
		return true;

	Error error;

#ifdef _WIN32
	const std::string libname = DynamicLibrary::GetVersionedFilename("shaderc_shared");
#else
	// Use versioned, bundle post-processing adds it..
	const std::string libname = DynamicLibrary::GetVersionedFilename("shaderc_shared", 1);
#endif
	if (!s_library.Open(libname.c_str(), &error))
	{
		ERROR_LOG("Failed to load shaderc: {}", error.GetDescription());
		return false;
	}

#define LOAD_FUNC(F) \
	if (!s_library.GetSymbol(#F, &F)) \
	{ \
		ERROR_LOG("Failed to find function {}", #F); \
		Close(); \
		return false; \
	}

	SHADERC_FUNCTIONS(LOAD_FUNC)
#undef LOAD_FUNC

	std::atexit(&VKShadercWrapper::Close);

	return true;
}

void VKShadercWrapper::Close()
{
	// Release shaderc compilers.
	for (shaderc_compiler_t compiler : s_compilers)
		shaderc_compiler_release(compiler);
	s_compilers.clear();

#define UNLOAD_FUNC(F) F = nullptr;
	SHADERC_FUNCTIONS(UNLOAD_FUNC)
#undef UNLOAD_FUNC

	// Close dynamic library.
	s_library.Close();
}

shaderc_compiler_t VKShadercWrapper::CreateCompiler()
{
	shaderc_compiler_t compiler = shaderc_compiler_initialize();
	if (!compiler)
		ERROR_LOG("shaderc_compiler_initialize() failed");
	s_compilers.push_back(compiler);
	return compiler;
}

static const char* compilation_status_to_string(shaderc_compilation_status status)
{
	switch (status)
	{
#define CASE(x) case shaderc_compilation_status_##x: return #x
		CASE(success);
		CASE(invalid_stage);
		CASE(compilation_error);
		CASE(internal_error);
		CASE(null_result_object);
		CASE(invalid_assembly);
		CASE(validation_error);
		CASE(transformation_error);
		CASE(configuration_error);
#undef CASE
	}
	return "unknown_error";
}

std::optional<VKShadercWrapper::SPIRVCodeVector> VKShadercWrapper::CompileShaderToSPV(
	shaderc_compiler_t compiler, u32 stage, std::string_view source, bool debug, bool enable_non_semantic,
	std::string* errors_out)
{
	std::optional<SPIRVCodeVector> ret;

	shaderc_compile_options_t options = shaderc_compile_options_initialize();
	pxAssertRel(options, "shaderc_compile_options_initialize() failed");

	shaderc_compile_options_set_source_language(options, shaderc_source_language_glsl);
	shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, 0);
#ifdef SHADERC_PCSX2_CUSTOM
	shaderc_compile_options_set_generate_debug_info(options, debug, debug && enable_non_semantic);
#else
	if (debug)
		shaderc_compile_options_set_generate_debug_info(options);
#endif
	shaderc_compile_options_set_optimization_level(
		options, debug ? shaderc_optimization_level_zero : shaderc_optimization_level_performance);

	const shaderc_compilation_result_t result = shaderc_compile_into_spv(
		compiler, source.data(), source.length(), static_cast<shaderc_shader_kind>(stage), "source",
		"main", options);

	shaderc_compilation_status status = shaderc_compilation_status_null_result_object;
	if (!result || (status = shaderc_result_get_compilation_status(result)) != shaderc_compilation_status_success)
	{
		std::string_view errors(result ? shaderc_result_get_error_message(result) : "null result object");
		ERROR_LOG("Failed to compile shader to SPIR-V: {}\n{}", compilation_status_to_string(status), errors);
		if (errors_out)
			*errors_out = errors;
	}
	else
	{
		const size_t num_warnings = shaderc_result_get_num_warnings(result);
		if (num_warnings > 0)
			WARNING_LOG("Shader compiled with warnings:\n{}", shaderc_result_get_error_message(result));

		const size_t spirv_size = shaderc_result_get_length(result);
		const char* bytes = shaderc_result_get_bytes(result);
		pxAssert(spirv_size > 0 && ((spirv_size % sizeof(u32)) == 0));
		ret = SPIRVCodeVector(reinterpret_cast<const u32*>(bytes),
			reinterpret_cast<const u32*>(bytes + spirv_size));
	}

	shaderc_result_release(result);
	shaderc_compile_options_release(options);
	return ret;
}