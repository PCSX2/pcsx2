#include "GS/Renderers/DX12/D3D12ShaderCompilerAsync.h"

void D3D12ShaderCompilerAsync::DoCompileJobSync(GSCompileJob* job, u32 thread_id)
{
	if (job->IsShaderJob())
	{
		D3D12ShaderJob* shader_job = static_cast<D3D12ShaderJob*>(job);
		if (shader_job->GetEntryType() == D3D::ShaderCacheEntryType::VertexShader)
		{
			shader_job->SetBlob(D3D::CompileShader(D3D::ShaderType::Vertex, m_shader_model, m_debug,
				shader_job->GetShaderCode(), shader_job->GetMacros(), shader_job->GetEntryPoint().c_str()));
		}
		else if (shader_job->GetEntryType() == D3D::ShaderCacheEntryType::PixelShader)
		{
			shader_job->SetBlob(D3D::CompileShader(D3D::ShaderType::Pixel, m_shader_model, m_debug,
				shader_job->GetShaderCode(), shader_job->GetMacros(), shader_job->GetEntryPoint().c_str()));
		}
		else
		{
			pxFailRel("Unknown shader type");
		}
	}
	else if (job->IsPipelineJob())
	{
		D3D12PipelineJob* pipeline_job = static_cast<D3D12PipelineJob*>(job);
		pxAssert(pipeline_job->HasVS() && pipeline_job->HasPS());
		pipeline_job->Create();
	}
	else
	{
		pxFailRel("Unknown job type");
	}
}
