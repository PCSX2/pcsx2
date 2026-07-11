#pragma once

#include "GS/Renderers/Common/GSShaderCompilerAsync.h"

#include "GS/Renderers/DX11/D3D.h"
#include "GS/Renderers/DX12/D3D12Builders.h"
#include <d3d12.h>

#include <thread>
#include <mutex>
#include <variant>

class D3D12ShaderJob : public GSCompileJob
{
public:
	template<typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	D3D12ShaderJob(D3D::ShaderCacheEntryType entry_type, std::string_view shader_code,
		D3D::ShaderMacro macros, std::string_view entry_point, u64 hash, bool uber)
		: GSCompileJob(SHADER), m_entry_type(entry_type), m_shader_code(shader_code)
		, m_macros(std::move(macros)), m_entry_point(entry_point), m_hash(hash), m_uber(uber)
	{
	}
	D3D::ShaderCacheEntryType GetEntryType() const { return m_entry_type; }
	const std::string& GetShaderCode() const { return m_shader_code; }
	const D3D_SHADER_MACRO* GetMacros() { return m_macros.GetPtr(); }
	const std::string& GetEntryPoint() const { return m_entry_point; }
	const u64 GetHash() const { return m_hash; }
	const bool IsUber() const { return m_uber; }
	void SetBlob(ComPtr<ID3DBlob> blob) { m_blob = std::move(blob); }
	ComPtr<ID3DBlob> GetBlob() const { return m_blob; }
private:
	// Inputs
	D3D::ShaderCacheEntryType m_entry_type;
	std::string m_shader_code;
	D3D::ShaderMacro m_macros;
	std::string m_entry_point;
	u64 m_hash;
	bool m_uber;

	// Output
	ComPtr<ID3DBlob> m_blob;
};

class D3D12PipelineJob : public GSCompileJob
{
public:
	template<typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	D3D12PipelineJob(ID3D12Device* device, const D3D12::GraphicsPipelineBuilder& gpb, u64 hash, bool uber)
		: GSCompileJob(PIPELINE), m_device(device), m_gpb(gpb), m_hash(hash), m_uber(uber)
	{
	}

	ID3D12Device* GetDevice() const { return m_device; }
	u64 GetHash() const { return m_hash; }
	bool IsUber() const { return m_uber; }

	bool HasVS() const { return m_gpb.HasVertexShader(); }
	bool HasPS() const { return m_gpb.HasPixelShader(); }
	void SetVS(const ID3DBlob* vs) { m_gpb.SetVertexShader(vs); }
	void SetPS(const ID3DBlob* ps) { m_gpb.SetPixelShader(ps); }

	D3D12ShaderJob* GetVSJob() const { return m_vs_job.get(); }
	D3D12ShaderJob* GetPSJob() const { return m_ps_job.get(); }
	void SetVSJob(std::shared_ptr<D3D12ShaderJob> vs_job) { m_vs_job = std::move(vs_job); }
	void SetPSJob(std::shared_ptr<D3D12ShaderJob> ps_job) { m_ps_job = std::move(ps_job); }

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetPipelineDesc() { return m_gpb.GetDesc(); }

	void Create()
	{
		pxAssert(m_gpb.HasVertexShader() && m_gpb.HasPixelShader());
		m_pipeline = m_gpb.Create(m_device, false);
	}

	ComPtr<ID3D12PipelineState> GetPipeline() const { return m_pipeline; }
private:
	// Inputs
	ID3D12Device* m_device;
	D3D12::GraphicsPipelineBuilder m_gpb;
	ComPtr<ID3DBlob> m_vs_blob;
	ComPtr<ID3DBlob> m_ps_blob;
	u64 m_hash;
	bool m_uber;

	// Optional - shader jobs this pipeline is waiting on.
	// Use shared_ptr to preserve lifetime until this job is finished.
	std::shared_ptr<D3D12ShaderJob> m_vs_job;
	std::shared_ptr<D3D12ShaderJob> m_ps_job;

	// Output
	ComPtr<ID3D12PipelineState> m_pipeline;
};

class D3D12ShaderCompilerAsync : public GSShaderCompilerAsync
{
public:
	template<typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	D3D12ShaderCompilerAsync(u32 num_threads, u32 check_latency_ms, D3D::ShaderModel shader_model, bool debug)
		: GSShaderCompilerAsync(num_threads, check_latency_ms)
		, m_shader_model(shader_model)
		, m_debug(debug)
	{
	}

protected:
	void DoCompileJobSync(GSCompileJob* job, u32 thread_id) override;

private:
	D3D::ShaderModel m_shader_model;
	bool m_debug;
};
