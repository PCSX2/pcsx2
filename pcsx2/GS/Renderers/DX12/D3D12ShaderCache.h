// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "GS/Renderers/DX11/D3D.h"

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include <cstdio>
#include <directx/d3d12.h>
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
		LibraryShader,
		GraphicsPipeline,
		ComputePipeline,
	};

	D3D12ShaderCache();
	~D3D12ShaderCache();

	__fi bool UsingDebugShaders() const { return m_debug; }

	bool Open(D3D::ShaderModel shader_model, bool debug);
	void Close();

	__fi ComPtr<ID3DBlob> GetVertexShader(
		const std::string_view shader_code, const char* shader_name, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", const std::unordered_map<std::string, std::string>& includes = {})
	{
		return GetShaderBlob(EntryType::VertexShader, shader_code, shader_name, macros, entry_point, includes);
	}
	__fi ComPtr<ID3DBlob> GetPixelShader(
		const std::string_view shader_code, const char* shader_name, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", const std::unordered_map<std::string, std::string>& includes = {})
	{
		return GetShaderBlob(EntryType::PixelShader, shader_code, shader_name, macros, entry_point, includes);
	}
	__fi ComPtr<ID3DBlob> GetPixelShader(
		const std::vector<std::pair<std::string, const char*>>& shader, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", const std::unordered_map<std::string, std::string>& includes = {})
	{
		return GetShaderBlob(EntryType::PixelShader, shader, macros, entry_point, includes);
	}
	__fi ComPtr<ID3DBlob> GetComputeShader(
		const std::string_view shader_code, const char* shader_name, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
	{
		return GetShaderBlob(EntryType::ComputeShader, shader_code, shader_name, macros, entry_point);
	}

	ComPtr<ID3DBlob> GetShaderBlob(EntryType type, const std::string_view shader_code, const char* shader_name,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", const std::unordered_map<std::string, std::string>& includes = {});

	ComPtr<ID3DBlob> GetShaderBlob(EntryType type, const std::vector<std::pair<std::string, const char*>>& shader,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main", const std::unordered_map<std::string, std::string>& includes = {});

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

	union MD5Hash
	{
		struct
		{
			u64 low;
			u64 high;
		};
		u8 hash[16];

		bool operator==(const MD5Hash& other) const
		{
			return (low == other.low) && (high == other.high);
		}
		bool operator!=(const MD5Hash& other) const
		{
			return !(*this == other);
		}
	};

	struct MD5HashHasher
	{
		std::size_t operator()(const MD5Hash& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, e.low, e.high);
			return h;
		}
	};

	static std::string GetCacheBaseFileName(const std::string_view type, D3D::ShaderModel shader_model, bool debug);
	static CacheIndexKey GetShaderCacheKey(
		EntryType type, const std::string_view shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point);
	static CacheIndexKey GetShaderCacheKey(
		EntryType type, const std::vector<std::pair<std::string, const char*>>& shader, const D3D_SHADER_MACRO* macros, const char* entry_point);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);
	static CacheIndexKey GetPipelineCacheKey(const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc);

	bool CreateNew(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file);
	bool ReadExisting(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
		std::FILE*& blob_file, CacheIndex& index);
	void InvalidatePipelineCache();

	void CollectIncludes(const std::string& shader_code, const std::unordered_map<std::string, std::string>& includes,
		std::vector<std::unordered_map<std::string, std::string>::const_iterator>& included_files, std::string_view parent_path = {});

	ComPtr<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, const std::string_view shader_code, const char* shader_name,
		const D3D_SHADER_MACRO* macros, const char* entry_point, const std::unordered_map<std::string, std::string>& includes = {});
	ComPtr<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, const std::vector<std::pair<std::string, const char*>>& shader,
		const D3D_SHADER_MACRO* macros, const char* entry_point, const std::unordered_map<std::string, std::string>& includes = {});
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

	D3D::ShaderModel m_shader_model = D3D::ShaderModel::SM51;
	bool m_debug = false;

	// Track defines needed for each lib of a linked shader.
	std::unordered_map<MD5Hash, std::unordered_map<std::string, bool>, MD5HashHasher> m_lib_defines;
};
