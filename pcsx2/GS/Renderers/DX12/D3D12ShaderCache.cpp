// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/DX12/D3D12ShaderCache.h"
#include "GS/Renderers/DX11/D3D.h"
#include "GS/GS.h"

#include "Config.h"
#include "ShaderCacheVersion.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/MD5Digest.h"
#include "common/Path.h"

#include <d3dcompiler.h>

#pragma pack(push, 1)
struct CacheIndexEntry
{
	u64 source_hash_low;
	u64 source_hash_high;
	u64 macro_hash_low;
	u64 macro_hash_high;
	u64 entry_point_low;
	u64 entry_point_high;
	u32 source_length;
	u32 shader_type;
	u32 file_offset;
	u32 blob_size;
};
#pragma pack(pop)

D3D12ShaderCache::D3D12ShaderCache() = default;

D3D12ShaderCache::~D3D12ShaderCache()
{
	Close();
}

bool D3D12ShaderCache::CacheIndexKey::operator==(const CacheIndexKey& key) const
{
	return (source_hash_low == key.source_hash_low && source_hash_high == key.source_hash_high &&
			macro_hash_low == key.macro_hash_low && macro_hash_high == key.macro_hash_high &&
			entry_point_low == key.entry_point_low && entry_point_high == key.entry_point_high && type == key.type &&
			source_length == key.source_length);
}

bool D3D12ShaderCache::CacheIndexKey::operator!=(const CacheIndexKey& key) const
{
	return (source_hash_low != key.source_hash_low || source_hash_high != key.source_hash_high ||
			macro_hash_low != key.macro_hash_low || macro_hash_high != key.macro_hash_high ||
			entry_point_low != key.entry_point_low || entry_point_high != key.entry_point_high || type != key.type ||
			source_length != key.source_length);
}

bool D3D12ShaderCache::Open(D3D_FEATURE_LEVEL feature_level, bool debug)
{
	m_feature_level = feature_level;
	m_debug = debug;

	bool result = true;

	if (!GSConfig.DisableShaderCache)
	{
		const std::string base_shader_filename = GetCacheBaseFileName("shaders", feature_level, debug);
		const std::string shader_index_filename = base_shader_filename + ".idx";
		const std::string shader_blob_filename = base_shader_filename + ".bin";

		if (!ReadExisting(
				shader_index_filename, shader_blob_filename, m_shader_index_file, m_shader_blob_file, m_shader_index))
		{
			result = CreateNew(shader_index_filename, shader_blob_filename, m_shader_index_file, m_shader_blob_file);
		}

		if (result)
		{
			const std::string base_pipelines_filename = GetCacheBaseFileName("pipelines", feature_level, debug);
			const std::string pipelines_index_filename = base_pipelines_filename + ".idx";
			const std::string pipelines_blob_filename = base_pipelines_filename + ".bin";

			if (!ReadExisting(pipelines_index_filename, pipelines_blob_filename, m_pipeline_index_file,
					m_pipeline_blob_file, m_pipeline_index))
			{
				result = CreateNew(
					pipelines_index_filename, pipelines_blob_filename, m_pipeline_index_file, m_pipeline_blob_file);
			}
		}
	}

	return result;
}

void D3D12ShaderCache::Close()
{
	if (m_pipeline_index_file)
	{
		std::fclose(m_pipeline_index_file);
		m_pipeline_index_file = nullptr;
	}
	if (m_pipeline_blob_file)
	{
		std::fclose(m_pipeline_blob_file);
		m_pipeline_blob_file = nullptr;
	}
	if (m_shader_index_file)
	{
		std::fclose(m_shader_index_file);
		m_shader_index_file = nullptr;
	}
	if (m_shader_blob_file)
	{
		std::fclose(m_shader_blob_file);
		m_shader_blob_file = nullptr;
	}
}

void D3D12ShaderCache::InvalidatePipelineCache()
{
	m_pipeline_index.clear();
	if (m_pipeline_blob_file)
	{
		std::fclose(m_pipeline_blob_file);
		m_pipeline_blob_file = nullptr;
	}

	if (m_pipeline_index_file)
	{
		std::fclose(m_pipeline_index_file);
		m_pipeline_index_file = nullptr;
	}

	if (GSConfig.DisableShaderCache)
		return;

	const std::string base_pipelines_filename = GetCacheBaseFileName("pipelines", m_feature_level, m_debug);
	const std::string pipelines_index_filename = base_pipelines_filename + ".idx";
	const std::string pipelines_blob_filename = base_pipelines_filename + ".bin";
	CreateNew(pipelines_index_filename, pipelines_blob_filename, m_pipeline_index_file, m_pipeline_blob_file);
}

bool D3D12ShaderCache::CreateNew(
	const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file, std::FILE*& blob_file)
{
	if (FileSystem::FileExists(index_filename.c_str()))
	{
		Console.Warning("Removing existing index file '%s'", index_filename.c_str());
		FileSystem::DeleteFilePath(index_filename.c_str());
	}
	if (FileSystem::FileExists(blob_filename.c_str()))
	{
		Console.Warning("Removing existing blob file '%s'", blob_filename.c_str());
		FileSystem::DeleteFilePath(blob_filename.c_str());
	}

	index_file = FileSystem::OpenCFile(index_filename.c_str(), "wb");
	if (!index_file)
	{
		Console.Error("Failed to open index file '%s' for writing", index_filename.c_str());
		return false;
	}

	const u32 file_version = SHADER_CACHE_VERSION;
	if (std::fwrite(&file_version, sizeof(file_version), 1, index_file) != 1)
	{
		Console.Error("Failed to write version to index file '%s'", index_filename.c_str());
		std::fclose(index_file);
		index_file = nullptr;
		FileSystem::DeleteFilePath(index_filename.c_str());
		return false;
	}

	blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "w+b");
	if (!blob_file)
	{
		Console.Error("Failed to open blob file '%s' for writing", blob_filename.c_str());
		std::fclose(blob_file);
		blob_file = nullptr;
		FileSystem::DeleteFilePath(index_filename.c_str());
		return false;
	}

	return true;
}

bool D3D12ShaderCache::ReadExisting(const std::string& index_filename, const std::string& blob_filename,
	std::FILE*& index_file, std::FILE*& blob_file, CacheIndex& index)
{
	index_file = FileSystem::OpenCFile(index_filename.c_str(), "r+b");
	if (!index_file)
	{
		// special case here: when there's a sharing violation (i.e. two instances running),
		// we don't want to blow away the cache. so just continue without a cache.
		if (errno == EACCES)
		{
			Console.WriteLn("Failed to open shader cache index with EACCES, are you running two instances?");
			return true;
		}

		return false;
	}

	u32 file_version;
	if (std::fread(&file_version, sizeof(file_version), 1, index_file) != 1 || file_version != SHADER_CACHE_VERSION)
	{
		Console.Error("Bad file version in '%s'", index_filename.c_str());
		std::fclose(index_file);
		index_file = nullptr;
		return false;
	}

	blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "a+b");
	if (!blob_file)
	{
		Console.Error("Blob file '%s' is missing", blob_filename.c_str());
		std::fclose(index_file);
		index_file = nullptr;
		return false;
	}

	std::fseek(blob_file, 0, SEEK_END);
	const u32 blob_file_size = static_cast<u32>(std::ftell(blob_file));

	for (;;)
	{
		CacheIndexEntry entry;
		if (std::fread(&entry, sizeof(entry), 1, index_file) != 1 ||
			(entry.file_offset + entry.blob_size) > blob_file_size)
		{
			if (std::feof(index_file))
				break;

			Console.Error("Failed to read entry from '%s', corrupt file?", index_filename.c_str());
			index.clear();
			std::fclose(blob_file);
			blob_file = nullptr;
			std::fclose(index_file);
			index_file = nullptr;
			return false;
		}

		const CacheIndexKey key{entry.source_hash_low, entry.source_hash_high, entry.macro_hash_low,
			entry.macro_hash_high, entry.entry_point_low, entry.entry_point_high, entry.source_length,
			static_cast<EntryType>(entry.shader_type)};
		const CacheIndexData data{entry.file_offset, entry.blob_size};
		index.emplace(key, data);
	}

	// ensure we don't write before seeking
	std::fseek(index_file, 0, SEEK_END);

	DevCon.WriteLn("Read %zu entries from '%s'", index.size(), index_filename.c_str());
	return true;
}

std::string D3D12ShaderCache::GetCacheBaseFileName(const std::string_view type, D3D_FEATURE_LEVEL feature_level, bool debug)
{
	std::string base_filename = "d3d12_";
	base_filename += type;
	base_filename += "_";

	switch (feature_level)
	{
		case D3D_FEATURE_LEVEL_11_0:
			base_filename += "sm50";
			break;
		default:
			base_filename += "unk";
			break;
	}

	if (debug)
		base_filename += "_debug";

	return Path::Combine(EmuFolders::Cache, base_filename);
}

union MD5Hash
{
	struct
	{
		u64 low;
		u64 high;
	};
	u8 hash[16];
};

D3D12ShaderCache::CacheIndexKey D3D12ShaderCache::GetShaderCacheKey(
	EntryType type, const std::string_view shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point)
{
	union
	{
		struct
		{
			u64 hash_low;
			u64 hash_high;
		};
		u8 hash[16];
	};

	CacheIndexKey key = {};
	key.type = type;

	MD5Digest digest;
	digest.Update(shader_code.data(), static_cast<u32>(shader_code.length()));
	digest.Final(hash);
	key.source_hash_low = hash_low;
	key.source_hash_high = hash_high;
	key.source_length = static_cast<u32>(shader_code.length());

	if (macros)
	{
		digest.Reset();
		for (const D3D_SHADER_MACRO* macro = macros; macro->Name != nullptr; macro++)
		{
			digest.Update(macro->Name, std::strlen(macro->Name));
			digest.Update(macro->Definition, std::strlen(macro->Definition));
		}
		digest.Final(hash);
		key.macro_hash_low = hash_low;
		key.macro_hash_high = hash_high;
	}

	digest.Reset();
	digest.Update(entry_point, static_cast<u32>(std::strlen(entry_point)));
	digest.Final(hash);
	key.entry_point_low = hash_low;
	key.entry_point_high = hash_high;

	return key;
}

D3D12ShaderCache::CacheIndexKey D3D12ShaderCache::GetPipelineCacheKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc)
{
	MD5Digest digest;
	u32 length = sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC);

	if (gpdesc.VS.BytecodeLength > 0)
	{
		digest.Update(gpdesc.VS.pShaderBytecode, static_cast<u32>(gpdesc.VS.BytecodeLength));
		length += static_cast<u32>(gpdesc.VS.BytecodeLength);
	}
	if (gpdesc.GS.BytecodeLength > 0)
	{
		digest.Update(gpdesc.GS.pShaderBytecode, static_cast<u32>(gpdesc.GS.BytecodeLength));
		length += static_cast<u32>(gpdesc.GS.BytecodeLength);
	}
	if (gpdesc.PS.BytecodeLength > 0)
	{
		digest.Update(gpdesc.PS.pShaderBytecode, static_cast<u32>(gpdesc.PS.BytecodeLength));
		length += static_cast<u32>(gpdesc.PS.BytecodeLength);
	}

	digest.Update(&gpdesc.BlendState, sizeof(gpdesc.BlendState));
	digest.Update(&gpdesc.SampleMask, sizeof(gpdesc.SampleMask));
	digest.Update(&gpdesc.RasterizerState, sizeof(gpdesc.RasterizerState));
	digest.Update(&gpdesc.DepthStencilState, sizeof(gpdesc.DepthStencilState));

	for (u32 i = 0; i < gpdesc.InputLayout.NumElements; i++)
	{
		const D3D12_INPUT_ELEMENT_DESC& ie = gpdesc.InputLayout.pInputElementDescs[i];
		digest.Update(ie.SemanticName, static_cast<u32>(std::strlen(ie.SemanticName)));
		digest.Update(&ie.SemanticIndex, sizeof(ie.SemanticIndex));
		digest.Update(&ie.Format, sizeof(ie.Format));
		digest.Update(&ie.InputSlot, sizeof(ie.InputSlot));
		digest.Update(&ie.AlignedByteOffset, sizeof(ie.AlignedByteOffset));
		digest.Update(&ie.InputSlotClass, sizeof(ie.InputSlotClass));
		digest.Update(&ie.InstanceDataStepRate, sizeof(ie.InstanceDataStepRate));
		length += sizeof(D3D12_INPUT_ELEMENT_DESC);
	}

	digest.Update(&gpdesc.IBStripCutValue, sizeof(gpdesc.IBStripCutValue));
	digest.Update(&gpdesc.PrimitiveTopologyType, sizeof(gpdesc.PrimitiveTopologyType));
	digest.Update(&gpdesc.NumRenderTargets, sizeof(gpdesc.NumRenderTargets));
	digest.Update(gpdesc.RTVFormats, sizeof(gpdesc.RTVFormats));
	digest.Update(&gpdesc.DSVFormat, sizeof(gpdesc.DSVFormat));
	digest.Update(&gpdesc.SampleDesc, sizeof(gpdesc.SampleDesc));
	digest.Update(&gpdesc.Flags, sizeof(gpdesc.Flags));

	MD5Hash h;
	digest.Final(h.hash);

	return CacheIndexKey{h.low, h.high, 0, 0, 0, 0, length, EntryType::GraphicsPipeline};
}

D3D12ShaderCache::CacheIndexKey D3D12ShaderCache::GetPipelineCacheKey(const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc)
{
	MD5Digest digest;
	u32 length = sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC);

	if (gpdesc.CS.BytecodeLength > 0)
	{
		digest.Update(gpdesc.CS.pShaderBytecode, static_cast<u32>(gpdesc.CS.BytecodeLength));
		length += static_cast<u32>(gpdesc.CS.BytecodeLength);
	}

	MD5Hash h;
	digest.Final(h.hash);

	return CacheIndexKey{h.low, h.high, 0, 0, 0, 0, length, EntryType::ComputePipeline};
}

D3D12ShaderCache::ComPtr<ID3DBlob> D3D12ShaderCache::GetShaderBlob(EntryType type, std::string_view shader_code,
	const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	const auto key = GetShaderCacheKey(type, shader_code, macros, entry_point);
	auto iter = m_shader_index.find(key);
	if (iter == m_shader_index.end())
		return CompileAndAddShaderBlob(key, shader_code, macros, entry_point);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(m_shader_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, m_shader_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

	return blob;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::GetPipelineState(
	ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
	const auto key = GetPipelineCacheKey(desc);

	auto iter = m_pipeline_index.find(key);
	if (iter == m_pipeline_index.end())
		return CompileAndAddPipeline(device, key, desc);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(m_pipeline_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, m_pipeline_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_with_blob(desc);
	desc_with_blob.CachedPSO.pCachedBlob = blob->GetBufferPointer();
	desc_with_blob.CachedPSO.CachedBlobSizeInBytes = blob->GetBufferSize();

	ComPtr<ID3D12PipelineState> pso;
	hr = device->CreateGraphicsPipelineState(&desc_with_blob, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Warning("Creating cached PSO failed: %08X. Invalidating cache.", hr);
		InvalidatePipelineCache();
		pso = CompileAndAddPipeline(device, key, desc);
	}

	return pso;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::GetPipelineState(
	ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc)
{
	const auto key = GetPipelineCacheKey(desc);

	auto iter = m_pipeline_index.find(key);
	if (iter == m_pipeline_index.end())
		return CompileAndAddPipeline(device, key, desc);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(m_pipeline_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, m_pipeline_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC desc_with_blob(desc);
	desc_with_blob.CachedPSO.pCachedBlob = blob->GetBufferPointer();
	desc_with_blob.CachedPSO.CachedBlobSizeInBytes = blob->GetBufferSize();

	ComPtr<ID3D12PipelineState> pso;
	hr = device->CreateComputePipelineState(&desc_with_blob, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Warning("Creating cached PSO failed: %08X. Invalidating cache.", hr);
		InvalidatePipelineCache();
		pso = CompileAndAddPipeline(device, key, desc);
	}

	return pso;
}

D3D12ShaderCache::ComPtr<ID3DBlob> D3D12ShaderCache::CompileAndAddShaderBlob(
	const CacheIndexKey& key, std::string_view shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point)
{
	ComPtr<ID3DBlob> blob;

	switch (key.type)
	{
		case EntryType::VertexShader:
			blob =
				D3D::CompileShader(D3D::ShaderType::Vertex, m_feature_level, m_debug, shader_code, macros, entry_point);
			break;
		case EntryType::PixelShader:
			blob =
				D3D::CompileShader(D3D::ShaderType::Pixel, m_feature_level, m_debug, shader_code, macros, entry_point);
			break;
		case EntryType::ComputeShader:
			blob = D3D::CompileShader(
				D3D::ShaderType::Compute, m_feature_level, m_debug, shader_code, macros, entry_point);
			break;
		default:
			break;
	}

	if (!blob)
		return {};

	if (!m_shader_blob_file || std::fseek(m_shader_blob_file, 0, SEEK_END) != 0)
		return blob;

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(m_shader_blob_file));
	data.blob_size = static_cast<u32>(blob->GetBufferSize());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.macro_hash_low = key.macro_hash_low;
	entry.macro_hash_high = key.macro_hash_high;
	entry.entry_point_low = key.entry_point_low;
	entry.entry_point_high = key.entry_point_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, m_shader_blob_file) != entry.blob_size ||
		std::fflush(m_shader_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, m_shader_index_file) != 1 ||
		std::fflush(m_shader_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
		return blob;
	}

	m_shader_index.emplace(key, data);
	return blob;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::CompileAndAddPipeline(
	ID3D12Device* device, const CacheIndexKey& key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc)
{
	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = device->CreateGraphicsPipelineState(&gpdesc, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Error("Creating cached PSO failed: %08X", hr);
		return {};
	}

	AddPipelineToBlob(key, pso.get());
	return pso;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::CompileAndAddPipeline(
	ID3D12Device* device, const CacheIndexKey& key, const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc)
{
	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = device->CreateComputePipelineState(&gpdesc, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Error("Creating cached compute PSO failed: %08X", hr);
		return {};
	}

	AddPipelineToBlob(key, pso.get());
	return pso;
}

bool D3D12ShaderCache::AddPipelineToBlob(const CacheIndexKey& key, ID3D12PipelineState* pso)
{
	if (!m_pipeline_blob_file || std::fseek(m_pipeline_blob_file, 0, SEEK_END) != 0)
		return false;

	ComPtr<ID3DBlob> blob;
	HRESULT hr = pso->GetCachedBlob(blob.put());
	if (FAILED(hr))
	{
		Console.Warning("Failed to get cached PSO data: %08X", hr);
		return false;
	}

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(m_pipeline_blob_file));
	data.blob_size = static_cast<u32>(blob->GetBufferSize());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, m_pipeline_blob_file) != entry.blob_size ||
		std::fflush(m_pipeline_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, m_pipeline_index_file) != 1 ||
		std::fflush(m_pipeline_index_file) != 0)
	{
		Console.Error("Failed to write pipeline blob to file");
		return false;
	}

	m_shader_index.emplace(key, data);
	return true;
}
