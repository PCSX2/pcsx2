#include "GS/Renderers/Vulkan/VKShaderCompilerAsync.h"
#include "GS/Renderers/Vulkan/GSDeviceVK.h"

#include "common/Assertions.h"

void VKShaderCompilerAsync::DoCompileJobSync(GSCompileJob* job, u32 thread_id)
{
	if (m_shaderc_compilers.empty())
		return;

	if (job->IsShaderJob())
	{
		VKShaderJob* shader_job = static_cast<VKShaderJob*>(job);

		shaderc_compiler_t compiler = m_shaderc_compilers[thread_id];

		std::optional<SPIRVCodeVector> spv =
			VKShadercWrapper::CompileShaderToSPV(compiler, shader_job->GetKind(),
				shader_job->GetShaderCode(), m_debug, m_debug && m_non_semantic);

		if (spv)
		{
			const VkShaderModuleCreateInfo ci{
				VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, spv->size() * sizeof(SPIRVCodeType), spv->data() };

			VkShaderModule mod;
			vkCreateShaderModule(shader_job->GetDevice(), &ci, nullptr, &mod);

			if (mod != VK_NULL_HANDLE)
			{
				shader_job->SetModule(mod);
				shader_job->SetSPV(std::move(*spv));
			}
		}
	}
	else if (job->IsPipelineJob())
	{
		VKPipelineJob* pipeline_job = static_cast<VKPipelineJob*>(job);
		pipeline_job->Create();
	}
	else
	{
		pxFailRel("Unknown job type");
	}
}

void VKShaderCompilerAsync::OnWorkersStarted()
{
	if (!VKShadercWrapper::Open())
		return;

	m_shaderc_compilers.resize(GetNumThreads());
	for (shaderc_compiler_t& compiler : m_shaderc_compilers)
		compiler = VKShadercWrapper::CreateCompiler();
}
