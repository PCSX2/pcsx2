// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include <cstdio>
#include <d3d12.h>
#include <string_view>
#include <unordered_map>
#include <vector>

class D3D12ShaderCache
{
public:
	template <typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	enum class EntryType
	{
		VertexShader,
		PixelShader,
		ComputeShader,
		GraphicsPipeline,
		ComputePipeline,
	};

	D3D12ShaderCache();
	~D3D12ShaderCache();

	__fi D3D_FEATURE_LEVEL GetFeatureLevel() const { return m_feature_level; }
	__fi bool UsingDebugShaders() const { return m_debug; }

	bool Open(D3D_FEATURE_LEVEL feature_level, bool debug);
	void Close();

	__fi ComPtr<ID3DBlob> GetVertexShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
	{
		return GetShaderBlob(EntryType::VertexShader, shader_code, macros, entry_point);
	}
	__fi ComPtr<ID3DBlob> GetPixelShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
	{
		return GetShaderBlob(EntryType::PixelShader, shader_code, macros, entry_point);
	}
	__fi ComPtr<ID3DBlob> GetComputeShader(
		std::string_view shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
	{
		return GetShaderBlob(EntryType::ComputeShader, shader_code, macros, entry_point);
	}

	ComPtr<ID3DBlob> GetShaderBlob(EntryType type, std::string_view shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	ComPtr<ID3D12PipelineState> GetPipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);
	ComPtr<ID3D12PipelineState> GetPipelineState(ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc);

private:
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

	static std::string GetCacheBaseFileName(const std::string_view& type, D3D_FEATURE_LEVEL feature_level, bool debug);
	static CacheIndexKey GetShaderCacheKey(
		EntryType type, const std::string_view& shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc);

	bool CreateNew(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file);
	bool ReadExisting(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file, CacheIndex& index);
	void InvalidatePipelineCache();

	ComPtr<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, std::string_view shader_code,
		const D3D_SHADER_MACRO* macros, const char* entry_point);
	ComPtr<ID3D12PipelineState> CompileAndAddPipeline(
		ID3D12Device* device, const CacheIndexKey& key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);
	ComPtr<ID3D12PipelineState> CompileAndAddPipeline(
		ID3D12Device* device, const CacheIndexKey& key, const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc);
	bool AddPipelineToBlob(const CacheIndexKey& key, ID3D12PipelineState* pso);

	std::FILE* m_shader_index_file = nullptr;
	std::FILE* m_shader_blob_file = nullptr;
	CacheIndex m_shader_index;

	std::FILE* m_pipeline_index_file = nullptr;
	std::FILE* m_pipeline_blob_file = nullptr;
	CacheIndex m_pipeline_index;

	D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;
	bool m_debug = false;
};
