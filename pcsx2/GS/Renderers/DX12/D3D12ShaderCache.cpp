// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/DX12/D3D12ShaderCache.h"
#include "GS/Renderers/DX12/GSDevice12.h"
#include "GS/GS.h"
#include "GS/GSShaderCompileIndicator.h"

#include "ShaderCacheVersion.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/MD5Digest.h"
#include "common/Path.h"

#include <d3dcompiler.h>

#include <variant>

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

	m_compiler_async.reset();
	m_queued_pipeline_jobs_async.clear();
	m_compile_jobs_async.clear();
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

bool D3D12ShaderCache::Open(D3D::ShaderModel shader_model, bool debug)
{
	// Only support SM5.1 for now, which is the minimum for D3D12.
	pxAssert(shader_model >= D3D::ShaderModel::SM51);
	m_shader_model = shader_model;
	m_debug = debug;

	bool result = true;

	if (GSConfig.ShaderCacheType >= GSShaderCacheType::Hybrid)
	{
		for (u32 uber = 0; uber < 2; uber++)
		{
			CacheState& state = GetCacheState(uber);

			const std::string base_shader_filename = GetCacheBaseFileName("shaders", m_shader_model, debug, uber);
			const std::string shader_index_filename = base_shader_filename + ".idx";
			const std::string shader_blob_filename = base_shader_filename + ".bin";

			if (!ReadExisting(
				shader_index_filename, shader_blob_filename, state.shader_index_file, state.shader_blob_file,
				state.shader_index))
			{
				result = CreateNew(shader_index_filename, shader_blob_filename, state.shader_index_file,
					state.shader_blob_file);
			}

			if (result)
			{
				const std::string base_pipelines_filename = GetCacheBaseFileName("pipelines", m_shader_model, debug, uber);
				const std::string pipelines_index_filename = base_pipelines_filename + ".idx";
				const std::string pipelines_blob_filename = base_pipelines_filename + ".bin";

				if (!ReadExisting(pipelines_index_filename, pipelines_blob_filename, state.pipeline_index_file,
					state.pipeline_blob_file, state.pipeline_index))
				{
					result = CreateNew(
						pipelines_index_filename, pipelines_blob_filename, state.pipeline_index_file, state.pipeline_blob_file);
				}
			}
		}
	}

	return result;
}

void D3D12ShaderCache::Close()
{
	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);

		if (state.pipeline_index_file)
		{
			std::fclose(state.pipeline_index_file);
			state.pipeline_index_file = nullptr;
		}
		if (state.pipeline_blob_file)
		{
			std::fclose(state.pipeline_blob_file);
			state.pipeline_blob_file = nullptr;
		}
		if (state.shader_index_file)
		{
			std::fclose(state.shader_index_file);
			state.shader_index_file = nullptr;
		}
		if (state.shader_blob_file)
		{
			std::fclose(state.shader_blob_file);
			state.shader_blob_file = nullptr;
		}
	}
}

void D3D12ShaderCache::InvalidatePipelineCache()
{
	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);

		state.pipeline_index.clear();
		if (state.pipeline_blob_file)
		{
			std::fclose(state.pipeline_blob_file);
			state.pipeline_blob_file = nullptr;
		}

		if (state.pipeline_index_file)
		{
			std::fclose(state.pipeline_index_file);
			state.pipeline_index_file = nullptr;
		}
	}

	if (GSConfig.ShaderCacheType == GSShaderCacheType::Disabled)
		return;

	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);
		const std::string base_pipelines_filename = GetCacheBaseFileName("pipelines", m_shader_model, m_debug, uber);
		const std::string pipelines_index_filename = base_pipelines_filename + ".idx";
		const std::string pipelines_blob_filename = base_pipelines_filename + ".bin";
		CreateNew(pipelines_index_filename, pipelines_blob_filename, state.pipeline_index_file, state.pipeline_blob_file);
	}
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

std::string D3D12ShaderCache::GetCacheBaseFileName(const std::string_view type, D3D::ShaderModel shader_model,
	bool debug, bool uber)
{
	std::string base_filename = "d3d12_";
	if (uber)
		base_filename += "uber_";
	base_filename += type;
	base_filename += "_";
	base_filename += D3D::ShaderModelToCacheString(shader_model);

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
	// Don't hash incomplete descriptions.
	pxAssert(gpdesc.VS.BytecodeLength > 0 && gpdesc.PS.BytecodeLength > 0);

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

bool D3D12ShaderCache::HasShaderBlob(EntryType type, std::string_view shader_code, const D3D_SHADER_MACRO* macros,
	const char* entry_point, bool uber)
{
	CacheState& state = GetCacheState(uber);
	const auto key = GetShaderCacheKey(type, shader_code, macros, entry_point);
	auto iter = state.shader_index.find(key);
	return iter != state.shader_index.end();
}

D3D12ShaderCache::ComPtr<ID3DBlob> D3D12ShaderCache::GetShaderBlob(EntryType type, std::string_view shader_code,
	const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */, bool uber /* = false */)
{
	CacheState& state = GetCacheState(uber);

	const auto key = GetShaderCacheKey(type, shader_code, macros, entry_point);
	auto iter = state.shader_index.find(key);
	if (iter == state.shader_index.end())
		return CompileAndAddShaderBlob(key, shader_code, macros, entry_point, uber);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(state.shader_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, state.shader_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

	return blob;
}

bool D3D12ShaderCache::HasPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, bool uber)
{
	CacheState& state = GetCacheState(uber);
	const auto key = GetPipelineCacheKey(desc);
	auto iter = state.pipeline_index.find(key);
	return iter != state.pipeline_index.end();
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::GetPipelineState(
	ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc, bool uber)
{
	CacheState& state = GetCacheState(uber);

	const auto key = GetPipelineCacheKey(desc);

	auto iter = state.pipeline_index.find(key);
	if (iter == state.pipeline_index.end())
		return CompileAndAddPipeline(device, key, desc, uber);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(state.pipeline_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, state.pipeline_blob_file) != iter->second.blob_size)
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
		pso = CompileAndAddPipeline(device, key, desc, uber);
	}

	return pso;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::GetPipelineState(
	ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc)
{
	CacheState& state = GetCacheState(false);

	const auto key = GetPipelineCacheKey(desc);

	auto iter = state.pipeline_index.find(key);
	if (iter == state.pipeline_index.end())
		return CompileAndAddPipeline(device, key, desc);

	ComPtr<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(state.pipeline_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, state.pipeline_blob_file) != iter->second.blob_size)
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
	const CacheIndexKey& key, std::string_view shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point,
	bool uber)
{
	CacheState& state = GetCacheState(uber);

	ComPtr<ID3DBlob> blob;

	switch (key.type)
	{
		case EntryType::VertexShader:
			blob =
				D3D::CompileShader(D3D::ShaderType::Vertex, m_shader_model, m_debug, shader_code, macros, entry_point);
			break;
		case EntryType::PixelShader:
			blob =
				D3D::CompileShader(D3D::ShaderType::Pixel, m_shader_model, m_debug, shader_code, macros, entry_point);
			break;
		case EntryType::ComputeShader:
			blob =
				D3D::CompileShader(D3D::ShaderType::Compute, m_shader_model, m_debug, shader_code, macros, entry_point);
			break;
		default:
			break;
	}

	if (!blob)
		return {};

	if (!state.shader_blob_file || std::fseek(state.shader_blob_file, 0, SEEK_END) != 0)
		return blob;

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(state.shader_blob_file));
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

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, state.shader_blob_file) != entry.blob_size ||
		std::fflush(state.shader_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, state.shader_index_file) != 1 ||
		std::fflush(state.shader_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
		return blob;
	}

	state.shader_index.emplace(key, data);
	return blob;
}

// FIXME: Duplication with above
void D3D12ShaderCache::AddShaderBlob(EntryType type, const std::string& shader_code, const D3D_SHADER_MACRO* macros,
	const char* entry_point, ComPtr<ID3DBlob> blob, bool uber, bool only_new)
{
	CacheState& state = GetCacheState(uber);

	if (!blob || !state.shader_blob_file || std::fseek(state.shader_blob_file, 0, SEEK_END) != 0)
		return;

	const auto key = GetShaderCacheKey(type, shader_code, macros, entry_point);

	if (only_new && state.shader_index.contains(key))
		return;

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(state.shader_blob_file));
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

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, state.shader_blob_file) != entry.blob_size ||
		std::fflush(state.shader_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, state.shader_index_file) != 1 ||
		std::fflush(state.shader_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
	}

	state.shader_index.emplace(key, data);
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::CompileAndAddPipeline(
	ID3D12Device* device, const CacheIndexKey& key, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc, bool uber)
{
	const GSShaderCompileIndicator::CompileTimer compile_timer;
	Common::Timer debug_timer;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = device->CreateGraphicsPipelineState(&gpdesc, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Error("Creating cached PSO failed: %08X", hr);
		return {};
	}

	Console.WriteLn("Sync pipeline compile: uber=%d time=%.2fms", uber, debug_timer.GetTimeMilliseconds());

	AddPipelineToBlob(key, pso.get(), uber);
	return pso;
}

D3D12ShaderCache::ComPtr<ID3D12PipelineState> D3D12ShaderCache::CompileAndAddPipeline(
	ID3D12Device* device, const CacheIndexKey& key, const D3D12_COMPUTE_PIPELINE_STATE_DESC& gpdesc)
{
	const GSShaderCompileIndicator::CompileTimer compile_timer;

	ComPtr<ID3D12PipelineState> pso;
	HRESULT hr = device->CreateComputePipelineState(&gpdesc, IID_PPV_ARGS(pso.put()));
	if (FAILED(hr))
	{
		Console.Error("Creating cached compute PSO failed: %08X", hr);
		return {};
	}

	AddPipelineToBlob(key, pso.get(), false);
	return pso;
}

bool D3D12ShaderCache::AddPipelineToBlob(const CacheIndexKey& key, ID3D12PipelineState* pso, bool uber, bool only_new)
{
	CacheState& state = GetCacheState(uber);

	if (!state.pipeline_blob_file || std::fseek(state.pipeline_blob_file, 0, SEEK_END) != 0 ||
		(only_new && state.pipeline_index.contains(key)))
	{
		return false;
	}

	ComPtr<ID3DBlob> blob;
	HRESULT hr = pso->GetCachedBlob(blob.put());
	if (FAILED(hr))
	{
		Console.Warning("Failed to get cached PSO data: %08X", hr);
		return false;
	}

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(state.pipeline_blob_file));
	data.blob_size = static_cast<u32>(blob->GetBufferSize());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, state.pipeline_blob_file) != entry.blob_size ||
		std::fflush(state.pipeline_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, state.pipeline_index_file) != 1 ||
		std::fflush(state.pipeline_index_file) != 0)
	{
		Console.Error("Failed to write pipeline blob to file");
		return false;
	}

	state.pipeline_index.emplace(key, data);
	return true;
}

void D3D12ShaderCache::StartPipelineCompilationAsync(std::shared_ptr<GSCompileJob> job)
{
	if (!m_compiler_async)
		m_compiler_async = std::make_unique<D3D12ShaderCompilerAsync>(
			GSConfig.HybridShaderCacheThreads, GSConfig.HybridShaderCacheLatencyMS, m_shader_model, m_debug);

	if (job->IsShaderJob())
	{
		D3D12ShaderJob* shader_job = static_cast<D3D12ShaderJob*>(job.get());
		pxAssert(shader_job->GetEntryType() == EntryType::VertexShader || shader_job->GetEntryType() == EntryType::PixelShader);
		const char* entry_type_str = (shader_job->GetEntryType() == EntryType::VertexShader) ? "vertex" : "pixel";
		Console.WriteLn("Async %s shader compile: started hash=0x%016llX uber=%d",
			entry_type_str, shader_job->GetHash(), shader_job->IsUber());
		m_compiler_async->StartCompileJobAsync(shader_job);
		m_compile_jobs_async.push_back(std::move(job));
	}
	else if (job->IsPipelineJob())
	{
		D3D12PipelineJob* pipeline_job = static_cast<D3D12PipelineJob*>(job.get());
		if (pipeline_job->HasVS() && pipeline_job->HasPS())
		{
			Console.WriteLn("Async pipeline compile: started hash=0x%016llX uber=%d", pipeline_job->GetHash(), pipeline_job->IsUber());
			m_compiler_async->StartCompileJobAsync(pipeline_job);
			m_compile_jobs_async.emplace_back(std::move(job));
		}
		else
		{
			// Need to wait for vertex and/or pixel shader.
			pxAssert((pipeline_job->GetVSJob() || pipeline_job->HasVS()) &&
				(pipeline_job->GetPSJob() || pipeline_job->HasPS()));
			Console.WriteLn("Async pipeline compile: queued hash=0x%016llX uber=%d vs_hash=0x%016llX ps_hash=0x%016llX",
				pipeline_job->GetHash(), pipeline_job->IsUber(),
				pipeline_job->GetVSJob() ? pipeline_job->GetVSJob()->GetHash() : 0,
				pipeline_job->GetPSJob() ? pipeline_job->GetPSJob()->GetHash() : 0);
			m_queued_pipeline_jobs_async.push_back(pipeline_job);
			m_compile_jobs_async.emplace_back(std::move(job));
		}
	}
	else
	{
		pxFailRel("Unknown job type");
	}
}

void D3D12ShaderCache::StartQueuedPipelineJobs(const D3D12ShaderJob* shader_job)
{
	for (auto it = m_queued_pipeline_jobs_async.begin(); it != m_queued_pipeline_jobs_async.end(); )
	{
		D3D12PipelineJob* queued_job = *it;

		bool pipeline_job_failed = false;

		if (shader_job->GetEntryType() == EntryType::VertexShader)
		{
			if (!queued_job->HasVS() && queued_job->GetVSJob() == shader_job)
			{
				if (!shader_job->GetBlob())
					pipeline_job_failed = true; // Shader failed to compile so pipeline also fails.
				else
					queued_job->SetVS(shader_job->GetBlob().get());
			}
		}
		else if (shader_job->GetEntryType() == EntryType::PixelShader)
		{
			if (!queued_job->HasPS() && queued_job->GetPSJob() == shader_job)
			{
				if (!shader_job->GetBlob())
					pipeline_job_failed = true; // Shader failed to compile so pipeline also fails.
				else
					queued_job->SetPS(shader_job->GetBlob().get());
			}
		}
		else
		{
			pxFailRel("Unknown shader type");
		}

		if (pipeline_job_failed)
		{
			Console.WriteLn("Async pipeline shader failed: %s=%016llX failed for pipeline=%016llX uber=%d",
				shader_job->GetEntryType() == EntryType::VertexShader ? "vs" : "ps",
				queued_job->GetHash(), queued_job->IsUber());

			DoneAsyncPipelineJob(queued_job);

			it = m_queued_pipeline_jobs_async.erase(it);
		}
		else if (queued_job->HasVS() && queued_job->HasPS())
		{
			// Vertex and pixel shaders compiled so start pipeline creating.
			Console.WriteLn("Async pipeline compile: got vs=%016llX and ps=%016llX for pipeline=%016llX uber=%d",
				queued_job->GetVSJob() ? queued_job->GetVSJob()->GetHash() : 0,
				queued_job->GetPSJob() ? queued_job->GetPSJob()->GetHash() : 0,
				queued_job->GetHash(), queued_job->IsUber());
			m_compiler_async->StartCompileJobAsync(queued_job);
			it = m_queued_pipeline_jobs_async.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void D3D12ShaderCache::ProcessAsyncCompileJobs()
{
	if (m_compiler_async)
	{
		m_finished_compile_jobs_async.clear();

		m_compiler_async->GetCompletedJobs(m_finished_compile_jobs_async);

		if (!m_finished_compile_jobs_async.empty())
			Console.WriteLn("Async pipeline compile: processing %u jobs",
				static_cast<u32>(m_finished_compile_jobs_async.size()));

		for (GSCompileJob* job : m_finished_compile_jobs_async)
		{
			if (job->IsShaderJob())
			{
				// Add shader code to the cache.
				D3D12ShaderJob* shader_job = static_cast<D3D12ShaderJob*>(job);
				AddShaderBlob(shader_job->GetEntryType(), shader_job->GetShaderCode(), shader_job->GetMacros(),
					shader_job->GetEntryPoint().c_str(), shader_job->GetBlob(), shader_job->IsUber(), true);

				pxAssert(shader_job->GetEntryType() == EntryType::VertexShader || shader_job->GetEntryType() == EntryType::PixelShader);
				const char* kind_str = (shader_job->GetEntryType() == EntryType::VertexShader) ? "vertex" : "fragment";
				Console.WriteLn("Async %s shader compile: finished hash=0x%016llX uber=%d time=%.2fms thread_id=%d",
					kind_str, shader_job->GetHash(), shader_job->IsUber(), shader_job->GetCompileTimeMS(), shader_job->GetThreadID());

				// Notify any pipelines waiting on this shader.
				StartQueuedPipelineJobs(shader_job);
			}
			else if (job->IsPipelineJob())
			{
				// Add pipeline to cache.
				D3D12PipelineJob* pipeline_job = static_cast<D3D12PipelineJob*>(job);
				AddPipelineToBlob(GetPipelineCacheKey(pipeline_job->GetPipelineDesc()),
					pipeline_job->GetPipeline().get(), pipeline_job->IsUber(), true);

				Console.WriteLn("Async pipeline compile: finished hash=0x%016llX uber=%d time=%.2fms thread_id=%d",
					pipeline_job->GetHash(), pipeline_job->IsUber(), pipeline_job->GetCompileTimeMS(),
					pipeline_job->GetThreadID());
			}
			else
			{
				pxFailRel("Unknown job type");
			}

			DoneAsyncPipelineJob(job);
		}
	}
}

void D3D12ShaderCache::DoneAsyncPipelineJob(GSCompileJob* job)
{
	// Let GSDevice know we're done.
	job->SetDone();

	// Remove reference from the queue.
	const auto it = std::find_if(
		m_compile_jobs_async.begin(),
		m_compile_jobs_async.end(),
		[job](const std::shared_ptr<GSCompileJob>& other) { return other.get() == job; });
	pxAssert(it != m_compile_jobs_async.end());
	m_compile_jobs_async.erase(it);
}
