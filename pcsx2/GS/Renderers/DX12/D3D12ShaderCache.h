// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "GS/Renderers/DX11/D3D.h"
#include "GS/Renderers/DX12/D3D12ShaderCompilerAsync.h"
#include "GS/Renderers/DX12/D3D12Builders.h"

#include "Config.h"

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include <cstdio>
#include <directx/d3d12.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <deque>
#include <thread>
#include <unordered_set>

class D3D12ShaderCache
{
public:
	template <typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	using EntryType = D3D::ShaderCacheEntryType;

	D3D12ShaderCache();
	~D3D12ShaderCache();

	__fi bool UsingDebugShaders() const { return m_debug; }

	bool Open(D3D::ShaderModel shader_model, bool debug);
	void Close();

	__fi bool HasVertexShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false)
	{
		return HasShaderBlob(EntryType::VertexShader, shader_code, macros, entry_point, uber);
	}
	__fi bool HasPixelShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false)
	{
		return HasShaderBlob(EntryType::PixelShader, shader_code, macros, entry_point, uber);
	}
	__fi ComPtr<ID3DBlob> GetVertexShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false)
	{
		return GetShaderBlob(EntryType::VertexShader, shader_code, macros, entry_point, uber);
	}
	__fi ComPtr<ID3DBlob> GetPixelShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false)
	{
		return GetShaderBlob(EntryType::PixelShader, shader_code, macros, entry_point, uber);
	}
	__fi ComPtr<ID3DBlob> GetComputeShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false)
	{
		return GetShaderBlob(EntryType::ComputeShader, shader_code, macros, entry_point, uber);
	}

	bool HasShaderBlob(EntryType type, std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr,
		const char* entry_point = "main", bool uber = false);
	ComPtr<ID3DBlob> GetShaderBlob(EntryType type, std::string_view shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", bool uber = false);

	bool HasPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, bool uber);
	ComPtr<ID3D12PipelineState> GetPipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, bool uber);
	ComPtr<ID3D12PipelineState> GetPipelineState(ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);

	void StartPipelineCompilationAsync(std::shared_ptr<GSCompileJob> job);
	void ProcessAsyncCompileJobs(); // Process jobs that have finished.

	struct CacheIndexKey
	{
		u64 source_hash_low;
		u64 source_hash_high;
		u64 macro_hash_low;
		u64 macro_hash_high;
		u64 entry_point_low;
		u64 entry_point_high;
		u32 source_length;
		EntryType type;

		bool operator==(const CacheIndexKey& key) const;
		bool operator!=(const CacheIndexKey& key) const;
	};

	struct CacheIndexEntryHasher
	{
		std::size_t operator()(const CacheIndexKey& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, e.entry_point_low, e.entry_point_high, e.macro_hash_low, e.macro_hash_high,
				e.source_hash_low, e.source_hash_high, e.source_length, e.type);
			return h;
		}
	};

	struct CacheIndexData
	{
		u32 file_offset;
		u32 blob_size;
	};

	using CacheIndex = std::unordered_map<CacheIndexKey, CacheIndexData, CacheIndexEntryHasher>;

private:
	static std::string GetCacheBaseFileName(const std::string_view type, D3D::ShaderModel shader_model, bool debug, bool uber);
	static CacheIndexKey GetShaderCacheKey(
		EntryType type, const std::string_view shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc);

	bool CreateNew(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file);
	bool ReadExisting(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file, CacheIndex& index);
	void InvalidatePipelineCache();

	ComPtr<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, std::string_view shader_code,
		const D3D_SHADER_MACRO* macros, const char* entry_point, bool uber);
	ComPtr<ID3D12PipelineState> CompileAndAddPipeline(ID3D12Device* device, const CacheIndexKey& key,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc, bool uber);
	ComPtr<ID3D12PipelineState> CompileAndAddPipeline(
		ID3D12Device* device, const CacheIndexKey& key, const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc);
	bool AddPipelineToBlob(const CacheIndexKey& key, ID3D12PipelineState* pso, bool uber, bool only_new = false);

	void AddShaderBlob(EntryType type, const std::string& source, const D3D_SHADER_MACRO* macros,
		const char* entry_point, ComPtr<ID3DBlob> blob, bool uber, bool only_new = false);

	// Start pipeline jobs that are waiting on the given vertex and/or pixel shader.
	void StartQueuedPipelineJobs(const D3D12ShaderJob* shader_job);

	void DoneAsyncPipelineJob(GSCompileJob* job); // Remove from queue and mark done.

	struct CacheState
	{
		std::FILE* shader_index_file = nullptr;
		std::FILE* shader_blob_file = nullptr;
		CacheIndex shader_index;

		std::FILE* pipeline_index_file = nullptr;
		std::FILE* pipeline_blob_file = nullptr;
		CacheIndex pipeline_index;
	} m_cache_state[2]; // Normal and uber.

	CacheState& GetCacheState(bool uber) { return m_cache_state[uber ? 1 : 0]; }

	D3D::ShaderModel m_shader_model = D3D::ShaderModel::SM51;
	bool m_debug = false;

	std::unique_ptr<D3D12ShaderCompilerAsync> m_compiler_async;
	std::deque<std::shared_ptr<GSCompileJob>> m_compile_jobs_async;
	std::deque<D3D12PipelineJob*> m_queued_pipeline_jobs_async;
	std::vector<GSCompileJob*> m_finished_compile_jobs_async;
};
