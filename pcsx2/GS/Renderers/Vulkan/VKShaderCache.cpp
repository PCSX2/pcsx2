// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GSShaderCompileIndicator.h"
#include "GS/GS.h"
#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#include "GS/Renderers/Vulkan/VKBuilders.h"
#include "GS/Renderers/Vulkan/VKShadercWrapper.h"
#include "GS/Renderers/Vulkan/VKShaderCache.h"
#include "GS/Renderers/Vulkan/VKShaderCompilerAsync.h"

#include "Config.h"
#include "ShaderCacheVersion.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/DynamicLibrary.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/MD5Digest.h"
#include "common/Path.h"
#include "common/Timer.h"
#include "common/HashCombine.h"

#include "fmt/format.h"
#include "shaderc/shaderc.h"

#include <cstring>
#include <memory>

// TODO: store the driver version and stuff in the shader header

std::unique_ptr<VKShaderCache> g_vulkan_shader_cache;

static u32 s_next_bad_shader_id = 0;

namespace
{
#pragma pack(push, 4)
	struct VK_PIPELINE_CACHE_HEADER
	{
		u32 header_length;
		u32 header_version;
		u32 vendor_id;
		u32 device_id;
		u8 uuid[VK_UUID_SIZE];
	};

	struct CacheIndexEntry
	{
		u64 source_hash_low;
		u64 source_hash_high;
		u32 source_length;
		u32 shader_type;
		u32 file_offset;
		u32 blob_size;
	};
#pragma pack(pop)
} // namespace

static bool ValidatePipelineCacheHeader(const VK_PIPELINE_CACHE_HEADER& header)
{
	if (header.header_length < sizeof(VK_PIPELINE_CACHE_HEADER))
	{
		Console.Error("Pipeline cache failed validation: Invalid header length");
		return false;
	}

	if (header.header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
	{
		Console.Error("Pipeline cache failed validation: Invalid header version");
		return false;
	}

	if (header.vendor_id != GSDeviceVK::GetInstance()->GetDeviceProperties().vendorID)
	{
		Console.Error("Pipeline cache failed validation: Incorrect vendor ID (file: 0x%X, device: 0x%X)",
			header.vendor_id, GSDeviceVK::GetInstance()->GetDeviceProperties().vendorID);
		return false;
	}

	if (header.device_id != GSDeviceVK::GetInstance()->GetDeviceProperties().deviceID)
	{
		Console.Error("Pipeline cache failed validation: Incorrect device ID (file: 0x%X, device: 0x%X)",
			header.device_id, GSDeviceVK::GetInstance()->GetDeviceProperties().deviceID);
		return false;
	}

	if (std::memcmp(header.uuid, GSDeviceVK::GetInstance()->GetDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE) != 0)
	{
		Console.Error("Pipeline cache failed validation: Incorrect UUID");
		return false;
	}

	return true;
}

static void FillPipelineCacheHeader(VK_PIPELINE_CACHE_HEADER* header)
{
	header->header_length = sizeof(VK_PIPELINE_CACHE_HEADER);
	header->header_version = VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
	header->vendor_id = GSDeviceVK::GetInstance()->GetDeviceProperties().vendorID;
	header->device_id = GSDeviceVK::GetInstance()->GetDeviceProperties().deviceID;
	std::memcpy(header->uuid, GSDeviceVK::GetInstance()->GetDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE);
}

static void DumpBadShader(std::string_view code, std::string_view errors)
{
	const std::string filename = Path::Combine(EmuFolders::Logs, fmt::format("pcsx2_bad_shader_{}.txt", ++s_next_bad_shader_id));
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "wb");
	if (fp)
	{
		if (!code.empty())
			std::fwrite(code.data(), code.size(), 1, fp.get());
		std::fputs("\n\n**** ERRORS ****\n", fp.get());
		if (!errors.empty())
			std::fwrite(errors.data(), errors.size(), 1, fp.get());
	}
}

shaderc_compiler_t VKShaderCache::m_compiler_sync = {};
bool VKShaderCache::m_shaderc_failed = false;

bool VKShaderCache::InitShadercCompiler()
{
	if (!VKShadercWrapper::Open())
		return false;

	if (m_shaderc_failed)
		return false;

	if (m_compiler_sync)
		return true;

	m_compiler_sync = VKShadercWrapper::CreateCompiler();

	m_shaderc_failed = !m_compiler_sync;

	return m_compiler_sync;
}

std::optional<VKShaderCache::SPIRVCodeVector> VKShaderCache::CompileShaderToSPV(u32 stage, std::string_view source, bool debug)
{
	std::optional<VKShaderCache::SPIRVCodeVector> ret;
	if (!InitShadercCompiler())
		return ret;

	const GSShaderCompileIndicator::CompileTimer compile_timer;

	std::string errors;

	ret = VKShadercWrapper::CompileShaderToSPV(m_compiler_sync, stage, source, debug,
		GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_khr_shader_non_semantic_info, &errors);

	if (!ret)
		DumpBadShader(source, errors);

	return ret;
}

VKShaderCache::VKShaderCache() = default;

VKShaderCache::~VKShaderCache()
{
	CloseShaderCache();
	FlushPipelineCache();
	ClosePipelineCache();

	m_compiler_async.reset();
	m_queued_pipeline_jobs_async.clear();
	m_compile_jobs_async.clear();
}

bool VKShaderCache::ShaderCacheIndexKey::operator==(const ShaderCacheIndexKey& key) const
{
	return (source_hash_low == key.source_hash_low && source_hash_high == key.source_hash_high &&
			source_length == key.source_length && shader_type == key.shader_type);
}

bool VKShaderCache::ShaderCacheIndexKey::operator!=(const ShaderCacheIndexKey& key) const
{
	return (source_hash_low != key.source_hash_low || source_hash_high != key.source_hash_high ||
			source_length != key.source_length || shader_type != key.shader_type);
}

bool VKShaderCache::GraphicsPipelineCacheIndexKey::operator==(const GraphicsPipelineCacheIndexKey& key) const
{
	return vs == key.vs && fs == key.fs && renderpass == key.renderpass && ci_hash_high == key.ci_hash_high &&
		ci_hash_low == key.ci_hash_low && ci_size == key.ci_size;
}

bool VKShaderCache::GraphicsPipelineCacheIndexKey::operator!=(const GraphicsPipelineCacheIndexKey& key) const
{
	return vs != key.vs || fs != key.fs || renderpass != key.renderpass || ci_hash_high != key.ci_hash_high ||
		ci_hash_low != key.ci_hash_low || ci_size != key.ci_size;
}

void VKShaderCache::Create()
{
	pxAssert(!g_vulkan_shader_cache);
	g_vulkan_shader_cache.reset(new VKShaderCache());
	g_vulkan_shader_cache->Open();
}

void VKShaderCache::Destroy()
{
	g_vulkan_shader_cache.reset();
}

void VKShaderCache::Open()
{
	if (GSConfig.ShaderCacheType != GSShaderCacheType::Disabled)
	{
		for (u32 uber = 0; uber < 2; uber++)
		{
			CacheState& state = GetCacheState(uber);

			const std::string pipeline_base_filename = GetPipelineCacheBaseFileName(uber, GSConfig.UseDebugDevice);
			state.pipeline_cache_filename = pipeline_base_filename + ".bin";
			state.pipeline_index_filename = pipeline_base_filename + "_index.bin";

			const std::string base_filename = GetShaderCacheBaseFileName(uber, GSConfig.UseDebugDevice);
			const std::string index_filename = base_filename + ".idx";
			const std::string blob_filename = base_filename + ".bin";

			if (!ReadExistingShaderCache(index_filename, blob_filename, uber))
				CreateNewShaderCache(index_filename, blob_filename, uber);

			if (!ReadExistingPipelineCache(uber))
				CreateNewPipelineCache(uber);
		}
	}
	else
	{
		for (u32 uber = 0; uber < 2; uber++)
			CreateNewPipelineCache(uber);
	}
}

VkPipelineCache VKShaderCache::GetPipelineCache(bool set_dirty /*= true*/, bool uber /*= false*/)
{
	CacheState& state = GetCacheState(uber);

	if (state.pipeline_cache == VK_NULL_HANDLE)
		return state.pipeline_cache;

	state.pipeline_cache_dirty |= set_dirty;
	return state.pipeline_cache;
}

bool VKShaderCache::CreateNewShaderCache(const std::string& index_filename, const std::string& blob_filename, bool uber)
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

	CacheState& state = GetCacheState(uber);

	state.shader_index_file = FileSystem::OpenCFile(index_filename.c_str(), "wb");
	if (!state.shader_index_file)
	{
		Console.Error("Failed to open index file '%s' for writing", index_filename.c_str());
		return false;
	}

	const u32 file_version = SHADER_CACHE_VERSION;
	VK_PIPELINE_CACHE_HEADER header;
	FillPipelineCacheHeader(&header);

	if (std::fwrite(&file_version, sizeof(file_version), 1, state.shader_index_file) != 1 ||
		std::fwrite(&header, sizeof(header), 1, state.shader_index_file) != 1)
	{
		Console.Error("Failed to write header to index file '%s'", index_filename.c_str());
		std::fclose(state.shader_index_file);
		state.shader_index_file = nullptr;
		FileSystem::DeleteFilePath(index_filename.c_str());
		return false;
	}

	state.shader_blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "w+b");
	if (!state.shader_blob_file)
	{
		Console.Error("Failed to open blob file '%s' for writing", blob_filename.c_str());
		std::fclose(state.shader_index_file);
		state.shader_index_file = nullptr;
		FileSystem::DeleteFilePath(index_filename.c_str());
		return false;
	}

	return true;
}

bool VKShaderCache::ReadExistingShaderCache(const std::string& index_filename, const std::string& blob_filename, bool uber)
{
	CacheState& state = GetCacheState(uber);

	state.shader_index_file = FileSystem::OpenCFile(index_filename.c_str(), "r+b");
	if (!state.shader_index_file)
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

	u32 file_version = 0;
	if (std::fread(&file_version, sizeof(file_version), 1, state.shader_index_file) != 1 || file_version != SHADER_CACHE_VERSION)
	{
		Console.Error("Bad file/data version in '%s'", index_filename.c_str());
		std::fclose(state.shader_index_file);
		state.shader_index_file = nullptr;
		return false;
	}

	VK_PIPELINE_CACHE_HEADER header;
	if (std::fread(&header, sizeof(header), 1, state.shader_index_file) != 1 || !ValidatePipelineCacheHeader(header))
	{
		Console.Error("Mismatched pipeline cache header in '%s' (GPU/driver changed?)", index_filename.c_str());
		std::fclose(state.shader_index_file);
		state.shader_index_file = nullptr;
		return false;
	}

	state.shader_blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "a+b");
	if (!state.shader_blob_file)
	{
		Console.Error("Blob file '%s' is missing", blob_filename.c_str());
		std::fclose(state.shader_index_file);
		state.shader_index_file = nullptr;
		return false;
	}

	std::fseek(state.shader_blob_file, 0, SEEK_END);
	const u32 blob_file_size = static_cast<u32>(std::ftell(state.shader_blob_file));

	for (;;)
	{
		CacheIndexEntry entry;
		if (std::fread(&entry, sizeof(entry), 1, state.shader_index_file) != 1 ||
			(entry.file_offset + entry.blob_size) > blob_file_size)
		{
			if (std::feof(state.shader_index_file))
				break;

			Console.Error("Failed to read entry from '%s', corrupt file?", index_filename.c_str());
			state.shader_index.clear();
			std::fclose(state.shader_blob_file);
			state.shader_blob_file = nullptr;
			std::fclose(state.shader_index_file);
			state.shader_index_file = nullptr;
			return false;
		}

		const ShaderCacheIndexKey key{entry.source_hash_low, entry.source_hash_high, entry.source_length, entry.shader_type};
		const ShaderCacheIndexData data{entry.file_offset, entry.blob_size};
		state.shader_index.emplace(key, data);
	}

	// ensure we don't write before seeking
	std::fseek(state.shader_index_file, 0, SEEK_END);

	Console.WriteLn("Read %zu entries from '%s'", state.shader_index.size(), index_filename.c_str());
	return true;
}

void VKShaderCache::CloseShaderCache()
{
	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);

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

bool VKShaderCache::CreateNewPipelineCache(bool uber)
{
	CacheState& state = GetCacheState(uber);

	// Create new pipeline index.
	{
		if (FileSystem::FileExists(state.pipeline_index_filename.c_str()))
		{
			Console.Warning("Removing existing index file '%s'", state.pipeline_index_filename.c_str());
			FileSystem::DeleteFilePath(state.pipeline_index_filename.c_str());
		}

		FILE* index_file = FileSystem::OpenCFile(state.pipeline_index_filename.c_str(), "wb");
		if (!index_file)
		{
			Console.Error("Failed to open index file '%s' for writing", state.pipeline_index_filename.c_str());
			return false;
		}

		const u32 file_version = SHADER_CACHE_VERSION;
		VK_PIPELINE_CACHE_HEADER header;
		FillPipelineCacheHeader(&header);

		if (std::fwrite(&file_version, sizeof(file_version), 1, index_file) != 1 ||
			std::fwrite(&header, sizeof(header), 1, index_file) != 1)
		{
			Console.Error("Failed to write header to index file '%s'", state.pipeline_index_filename.c_str());
			std::fclose(index_file);
			FileSystem::DeleteFilePath(state.pipeline_index_filename.c_str());
			return false;
		}

		std::fclose(index_file);
	}

	// Create new pipeline cache.
	{
		if (!state.pipeline_cache_filename.empty() && FileSystem::FileExists(state.pipeline_cache_filename.c_str()))
		{
			Console.Warning("Removing existing pipeline cache '%s'", state.pipeline_cache_filename.c_str());
			FileSystem::DeleteFilePath(state.pipeline_cache_filename.c_str());
		}

		const VkPipelineCacheCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, 0, nullptr };
		VkResult res = vkCreatePipelineCache(GSDeviceVK::GetInstance()->GetDevice(), &ci, nullptr, &state.pipeline_cache);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreatePipelineCache() failed: ");
			return false;
		}
	}
	
	state.pipeline_cache_dirty = true;
	return true;
}

bool VKShaderCache::ReadExistingPipelineCache(bool uber)
{
	CacheState& state = GetCacheState(uber);

	FILE* index_file = FileSystem::OpenCFile(state.pipeline_index_filename.c_str(), "r+b");
	if (!index_file)
	{
		// special case here: when there's a sharing violation (i.e. two instances running),
		// we don't want to blow away the cache. so just continue without a cache.
		if (errno == EACCES)
		{
			Console.WriteLn("Failed to open pipeline cache index with EACCES, are you running two instances?");
			return true;
		}

		return false;
	}

	u32 file_version = 0;
	if (std::fread(&file_version, sizeof(file_version), 1, index_file) != 1 || file_version != SHADER_CACHE_VERSION)
	{
		Console.Error("Bad file/data version in '%s'", state.pipeline_index_filename.c_str());
		std::fclose(index_file);
		index_file = nullptr;
		return false;
	}

	VK_PIPELINE_CACHE_HEADER header;
	if (std::fread(&header, sizeof(header), 1, index_file) != 1 || !ValidatePipelineCacheHeader(header))
	{
		Console.Error("Mismatched pipeline cache header in '%s' (GPU/driver changed?)", state.pipeline_index_filename.c_str());
		std::fclose(index_file);
		return false;
	}

	// Read existing pipeline index
	for (;;)
	{
		GraphicsPipelineCacheIndexKey key;
		if (std::fread(&key, sizeof(key), 1, index_file) != 1)
		{
			if (std::feof(index_file))
				break;

			Console.Error("Failed to read key from '%s', corrupt file?", state.pipeline_index_filename.c_str());
			state.pipeline_index.clear();
			std::fclose(index_file);
			return false;
		}

		state.pipeline_index.insert(key);
	}

	std::fclose(index_file);

	Console.WriteLn("Read %zu entries from '%s'", state.pipeline_index.size(), state.pipeline_index_filename.c_str());

	std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(state.pipeline_cache_filename.c_str());
	if (!data.has_value())
		return false;

	if (data->size() < sizeof(VK_PIPELINE_CACHE_HEADER))
	{
		Console.Error("Pipeline cache at '%s' is too small", state.pipeline_cache_filename.c_str());
		return false;
	}

	std::memcpy(&header, data->data(), sizeof(header));
	if (!ValidatePipelineCacheHeader(header))
		return false;

	const VkPipelineCacheCreateInfo ci{
		VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, data->size(), data->data()};
	VkResult res = vkCreatePipelineCache(GSDeviceVK::GetInstance()->GetDevice(), &ci, nullptr, &state.pipeline_cache);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreatePipelineCache() failed: ");
		return false;
	}

	return true;
}

bool VKShaderCache::FlushPipelineCache()
{
	bool flushed = false;
	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);

		if (state.pipeline_cache == VK_NULL_HANDLE || !state.pipeline_cache_dirty || state.pipeline_cache_filename.empty())
			continue;

		size_t data_size;

		// Write the pipeline index.
		data_size = sizeof(state.new_pipeline_index[0]) * state.new_pipeline_index.size();
		Console.WriteLn("Writing %zu bytes to '%s'", data_size, state.pipeline_index_filename.c_str());
		
		FILE* index_file = FileSystem::OpenCFile(state.pipeline_index_filename.c_str(), "a+b");
		if (!index_file)
		{
			// FIXME: Duplication with opening.
			// special case here: when there's a sharing violation (i.e. two instances running),
			// we don't want to blow away the cache. so just continue without a cache.
			if (errno == EACCES)
			{
				Console.WriteLn("Failed to open pipeline cache index with EACCES, are you running two instances?");
				return true;
			}

			return false;
		}

		if (std::fwrite(state.new_pipeline_index.data(), sizeof(state.new_pipeline_index[0]), state.new_pipeline_index.size(), index_file) !=
			state.new_pipeline_index.size())
		{
			Console.Error("Failed to write pipeline index to '%s'", state.pipeline_index_filename.c_str());
			return false;
		}

		std::fclose(index_file);
		state.new_pipeline_index.clear();

		// Write the pipeline data.
		VkResult res =
			vkGetPipelineCacheData(GSDeviceVK::GetInstance()->GetDevice(), state.pipeline_cache, &data_size, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData() failed: ");
			return false;
		}

		std::vector<u8> data(data_size);
		res = vkGetPipelineCacheData(GSDeviceVK::GetInstance()->GetDevice(), state.pipeline_cache, &data_size, data.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData() (2) failed: ");
			return false;
		}

		data.resize(data_size);

		// Save disk writes if it hasn't changed, think of the poor SSDs.
		FILESYSTEM_STAT_DATA sd;
		if (!FileSystem::StatFile(state.pipeline_cache_filename.c_str(), &sd) || sd.Size != static_cast<s64>(data_size))
		{
			Console.WriteLn("Writing %zu bytes to '%s'", data_size, state.pipeline_cache_filename.c_str());
			if (!FileSystem::WriteBinaryFile(state.pipeline_cache_filename.c_str(), data.data(), data.size()))
			{
				Console.Error("Failed to write pipeline cache to '%s'", state.pipeline_cache_filename.c_str());
				return false;
			}
		}
		else
		{
			Console.WriteLn("Skipping updating pipeline cache '%s' due to no changes.", state.pipeline_cache_filename.c_str());
		}

		state.pipeline_cache_dirty = false;
	}
	return flushed;
}

void VKShaderCache::ClosePipelineCache()
{
	for (u32 uber = 0; uber < 2; uber++)
	{
		CacheState& state = GetCacheState(uber);

		if (state.pipeline_cache == VK_NULL_HANDLE)
			return;

		vkDestroyPipelineCache(GSDeviceVK::GetInstance()->GetDevice(), state.pipeline_cache, nullptr);
		state.pipeline_cache = VK_NULL_HANDLE;
	}
}

std::string VKShaderCache::GetShaderCacheBaseFileName(bool uber, bool debug)
{
	std::string base_filename = "vulkan_shaders";

	if (uber)
		base_filename += "_uber";

	if (debug)
		base_filename += "_debug";

	return Path::Combine(EmuFolders::Cache, base_filename);
}

std::string VKShaderCache::GetPipelineCacheBaseFileName(bool uber, bool debug)
{
	std::string base_filename = "vulkan_pipelines";

	if (uber)
		base_filename += "_uber";

	if (debug)
		base_filename += "_debug";

	return Path::Combine(EmuFolders::Cache, base_filename);
}

VKShaderCache::ShaderCacheIndexKey VKShaderCache::GetShaderCacheKey(u32 type, const std::string_view shader_code)
{
	union HashParts
	{
		struct
		{
			u64 hash_low;
			u64 hash_high;
		};
		u8 hash[16];
	};
	HashParts h;

	MD5Digest digest;
	digest.Update(shader_code.data(), static_cast<u32>(shader_code.length()));
	digest.Final(h.hash);

	return ShaderCacheIndexKey{h.hash_low, h.hash_high, static_cast<u32>(shader_code.length()), type};
}

VKShaderCache::GraphicsPipelineCacheIndexKey VKShaderCache::GetGraphicsPipelineCacheKey(
	const ShaderCacheIndexKey& vs_key, const ShaderCacheIndexKey& fs_key, u32 renderpass_key, const VkGraphicsPipelineCreateInfo& ci)
{
	MD5Digest ci_digest;
	u32 ci_size = 0;

	const auto UpdateDigest = [&ci_digest, &ci_size](const void* data, size_t size) {
		ci_digest.Update(data, size);
		ci_size += static_cast<u32>(size);
	};

	UpdateDigest(&ci.sType, sizeof(ci.sType));

	UpdateDigest(&ci.stageCount, sizeof(ci.stageCount));
	bool has_vs = false;
	bool has_fs = false;
	if (ci.pStages)
	{
		// First hash VS then PS.
		for (u32 vs_or_fs = 0; vs_or_fs < 2; vs_or_fs++)
		{
			for (u32 i = 0; i < ci.stageCount; i++)
			{
				const VkPipelineShaderStageCreateInfo& stage = ci.pStages[i];
				pxAssert(stage.stage == VK_SHADER_STAGE_VERTEX_BIT || stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT);
				if ((vs_or_fs == 0 && stage.stage == VK_SHADER_STAGE_VERTEX_BIT) ||
					(vs_or_fs == 1 && stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT))
				{
					UpdateDigest(&stage.flags, sizeof(stage.flags));
					UpdateDigest(&stage.stage, sizeof(stage.stage));
					if (stage.pName)
						UpdateDigest(stage.pName, strnlen(stage.pName, 128));
					has_vs = has_vs || (vs_or_fs == 0);
					has_fs = has_fs || (vs_or_fs == 1);
				}
			}
		}
	}
	pxAssert(has_vs && has_fs); // Don't hash incomplete descriptions.

	if (ci.pVertexInputState)
	{
		const VkPipelineVertexInputStateCreateInfo& vsci = *ci.pVertexInputState;
		UpdateDigest(&vsci.sType, sizeof(vsci.sType));
		UpdateDigest(&vsci.vertexBindingDescriptionCount, sizeof(vsci.vertexBindingDescriptionCount));
		UpdateDigest(&vsci.vertexAttributeDescriptionCount, sizeof(vsci.vertexAttributeDescriptionCount));
		if (vsci.pVertexBindingDescriptions)
			UpdateDigest(vsci.pVertexBindingDescriptions,
				sizeof(VkVertexInputBindingDescription) * vsci.vertexBindingDescriptionCount);
		if (vsci.pVertexAttributeDescriptions)
			UpdateDigest(vsci.pVertexAttributeDescriptions,
				sizeof(VkVertexInputAttributeDescription) * vsci.vertexAttributeDescriptionCount);
	}

	if (ci.pInputAssemblyState)
	{
		const VkPipelineInputAssemblyStateCreateInfo& ia = *ci.pInputAssemblyState;
		UpdateDigest(&ia.sType, sizeof(ia.sType));
		UpdateDigest(&ia.flags, sizeof(ia.flags));
		UpdateDigest(&ia.topology, sizeof(ia.topology));
		UpdateDigest(&ia.primitiveRestartEnable, sizeof(ia.primitiveRestartEnable));
	}

	if (ci.pRasterizationState)
	{
		const VkPipelineRasterizationStateCreateInfo& rast = *ci.pRasterizationState;
		UpdateDigest(&rast.sType, sizeof(rast.sType));
		UpdateDigest(&rast.flags, sizeof(rast.flags));
		UpdateDigest(&rast.depthClampEnable, sizeof(rast.depthClampEnable));
		UpdateDigest(&rast.rasterizerDiscardEnable, sizeof(rast.rasterizerDiscardEnable));
		UpdateDigest(&rast.polygonMode, sizeof(rast.polygonMode));
		UpdateDigest(&rast.cullMode, sizeof(rast.cullMode));
		UpdateDigest(&rast.frontFace, sizeof(rast.frontFace));
		UpdateDigest(&rast.depthBiasEnable, sizeof(rast.depthBiasEnable));
		UpdateDigest(&rast.depthBiasConstantFactor, sizeof(rast.depthBiasConstantFactor));
		UpdateDigest(&rast.depthBiasClamp, sizeof(rast.depthBiasClamp));
		UpdateDigest(&rast.depthBiasSlopeFactor, sizeof(rast.depthBiasSlopeFactor));
		UpdateDigest(&rast.lineWidth, sizeof(rast.lineWidth));

		// First hash line rasterization state then provoking vertex. Fail if anything else is encountered.
		for (u32 stage = 0; stage < 2; stage++)
		{
			const VkBaseInStructure* base = reinterpret_cast<const VkBaseInStructure*>(ci.pRasterizationState);
			while (base->pNext)
			{
				base = base->pNext;
				if (base->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT)
				{
					if (stage == 0)
					{
						const VkPipelineRasterizationLineStateCreateInfoEXT* line_rast =
							reinterpret_cast<const VkPipelineRasterizationLineStateCreateInfoEXT*>(base);
						UpdateDigest(&line_rast->sType, sizeof(line_rast->sType));
						UpdateDigest(&line_rast->lineRasterizationMode, sizeof(line_rast->lineRasterizationMode));
						UpdateDigest(&line_rast->stippledLineEnable, sizeof(line_rast->stippledLineEnable));
						UpdateDigest(&line_rast->lineStippleFactor, sizeof(line_rast->lineStippleFactor));
						UpdateDigest(&line_rast->lineStipplePattern, sizeof(line_rast->lineStipplePattern));
					}
				}
				else if (base->sType == VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT)
				{
					if (stage == 1)
					{
						const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT* provoke =
							reinterpret_cast<const VkPipelineRasterizationProvokingVertexStateCreateInfoEXT*>(base);
						UpdateDigest(&provoke->sType, sizeof(provoke->sType));
						UpdateDigest(&provoke->provokingVertexMode, sizeof(provoke->provokingVertexMode));
					}
				}
				else
				{
					pxAssert(false);
				}
			}
		}

		if (ci.pMultisampleState)
		{
			const VkPipelineMultisampleStateCreateInfo& ms = *ci.pMultisampleState;
			UpdateDigest(&ms.sType, sizeof(ms.sType));
			UpdateDigest(&ms.flags, sizeof(ms.flags));
			UpdateDigest(&ms.rasterizationSamples, sizeof(ms.rasterizationSamples));
			UpdateDigest(&ms.sampleShadingEnable, sizeof(ms.sampleShadingEnable));
			UpdateDigest(&ms.minSampleShading, sizeof(ms.minSampleShading));
			UpdateDigest(&ms.alphaToCoverageEnable, sizeof(ms.alphaToCoverageEnable));
			UpdateDigest(&ms.alphaToOneEnable, sizeof(ms.alphaToOneEnable));
			if (ms.pSampleMask)
				UpdateDigest(ms.pSampleMask, sizeof(VkSampleMask) * ((ms.rasterizationSamples + 31) / 32));
		}
	}

	if (ci.pViewportState)
	{
		const VkPipelineViewportStateCreateInfo& view = *ci.pViewportState;
		UpdateDigest(&view.sType, sizeof(view.sType));
		UpdateDigest(&view.flags, sizeof(view.flags));
		UpdateDigest(&view.viewportCount, sizeof(view.viewportCount));
		UpdateDigest(&view.scissorCount, sizeof(view.scissorCount));
		if (view.pViewports)
			UpdateDigest(view.pViewports, sizeof(VkViewport) * view.viewportCount);
		if (view.pScissors)
			UpdateDigest(view.pScissors, sizeof(VkRect2D)* view.viewportCount);
	}

	if (ci.pDynamicState)
	{
		const VkPipelineDynamicStateCreateInfo& dynamic = *ci.pDynamicState;
		UpdateDigest(&dynamic.sType, sizeof(dynamic.sType));
		UpdateDigest(&dynamic.flags, sizeof(dynamic.flags));
		UpdateDigest(&dynamic.dynamicStateCount, sizeof(dynamic.dynamicStateCount));
		if (dynamic.pDynamicStates)
			UpdateDigest(dynamic.pDynamicStates, sizeof(VkDynamicState) * dynamic.dynamicStateCount);
	}

	if (ci.pDepthStencilState)
	{
		const VkPipelineDepthStencilStateCreateInfo& ds = *ci.pDepthStencilState;
		UpdateDigest(&ds.sType, sizeof(ds.sType));
		UpdateDigest(&ds.flags, sizeof(ds.flags));
		UpdateDigest(&ds.depthTestEnable, sizeof(ds.depthTestEnable));
		UpdateDigest(&ds.depthWriteEnable, sizeof(ds.depthWriteEnable));
		UpdateDigest(&ds.depthCompareOp, sizeof(ds.depthCompareOp));
		UpdateDigest(&ds.depthBoundsTestEnable, sizeof(ds.depthBoundsTestEnable));
		UpdateDigest(&ds.stencilTestEnable, sizeof(ds.stencilTestEnable));
		UpdateDigest(&ds.front, sizeof(ds.front));
		UpdateDigest(&ds.back, sizeof(ds.back));
		UpdateDigest(&ds.minDepthBounds, sizeof(ds.minDepthBounds));
		UpdateDigest(&ds.maxDepthBounds, sizeof(ds.maxDepthBounds));
	}

	if (ci.pColorBlendState)
	{
		const VkPipelineColorBlendStateCreateInfo& blend = *ci.pColorBlendState;
		UpdateDigest(&blend.sType, sizeof(blend.sType));
		UpdateDigest(&blend.flags, sizeof(blend.flags));
		UpdateDigest(&blend.logicOpEnable, sizeof(blend.logicOpEnable));
		UpdateDigest(&blend.logicOp, sizeof(blend.logicOp));
		UpdateDigest(&blend.attachmentCount, sizeof(blend.attachmentCount));
		UpdateDigest(&blend.blendConstants, sizeof(blend.blendConstants));
		if (blend.pAttachments)
			UpdateDigest(blend.pAttachments, sizeof(VkPipelineColorBlendAttachmentState)* blend.attachmentCount);
	}

	// Note: render pass and pipeline layout are not handled since they are opaque.

	union HashParts
	{
		struct
		{
			u64 hash_low;
			u64 hash_high;
		};
		u8 hash[16];
	};
	HashParts ci_hash;

	ci_digest.Final(ci_hash.hash);

	return GraphicsPipelineCacheIndexKey{ vs_key, fs_key, renderpass_key, ci_hash.hash_low, ci_hash.hash_high,
		static_cast<u32>(ci_size) };
}

bool VKShaderCache::HasShaderSPV(u32 type, std::string_view shader_code, bool uber)
{
	CacheState& state = GetCacheState(uber);
	const auto key = GetShaderCacheKey(type, shader_code);
	auto iter = state.shader_index.find(key);
	return iter != state.shader_index.end();
}

std::optional<VKShaderCache::SPIRVCodeVector> VKShaderCache::GetShaderSPV(u32 type, std::string_view shader_code, bool uber)
{
	CacheState& state = GetCacheState(uber);
	const auto key = GetShaderCacheKey(type, shader_code);
	auto iter = state.shader_index.find(key);
	if (iter == state.shader_index.end())
		return CompileAndAddShaderSPV(key, shader_code, uber);

	std::optional<SPIRVCodeVector> spv = SPIRVCodeVector(iter->second.blob_size);

	if (std::fseek(state.shader_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(spv->data(), sizeof(SPIRVCodeType), iter->second.blob_size, state.shader_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed, recompiling");
		spv = CompileShaderToSPV(type, shader_code, GSConfig.UseDebugDevice);
	}

	return spv;
}

VKShaderCache::VKCachedShaderModule VKShaderCache::GetShaderModule(u32 type, std::string_view shader_code, bool uber)
{
	std::optional<SPIRVCodeVector> spv = GetShaderSPV(type, shader_code, uber);
	if (!spv.has_value())
		return {};

	const VkShaderModuleCreateInfo ci{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, spv->size() * sizeof(SPIRVCodeType), spv->data()};

	VkShaderModule mod;
	VkResult res = vkCreateShaderModule(GSDeviceVK::GetInstance()->GetDevice(), &ci, nullptr, &mod);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateShaderModule() failed: ");
		return {};
	}

	return VKCachedShaderModule{ mod, GetShaderCacheKey(type, shader_code) };
}

bool VKShaderCache::HasVertexShader(std::string_view shader_code, bool uber)
{
	return HasShaderSPV(shaderc_glsl_vertex_shader, shader_code, uber);
}

bool VKShaderCache::HasFragmentShader(std::string_view shader_code, bool uber)
{
	return HasShaderSPV(shaderc_glsl_fragment_shader, shader_code, uber);
}

VKShaderCache::VKCachedShaderModule VKShaderCache::GetVertexShader(std::string_view shader_code, bool uber)
{
	return GetShaderModule(shaderc_glsl_vertex_shader, std::move(shader_code), uber);
}

VKShaderCache::VKCachedShaderModule VKShaderCache::GetFragmentShader(std::string_view shader_code, bool uber)
{
	return GetShaderModule(shaderc_glsl_fragment_shader, std::move(shader_code), uber);
}

VkShaderModule VKShaderCache::GetComputeShader(std::string_view shader_code)
{
	return GetShaderModule(shaderc_glsl_compute_shader, std::move(shader_code), false).module;
}

bool VKShaderCache::HasGraphicsPipeline(const GraphicsPipelineCacheIndexKey& key, bool uber)
{
	return GetCacheState(uber).pipeline_index.contains(key);
}

VKShaderCache::VKCachedPipeline VKShaderCache::GetGraphicsPipeline(VkDevice device,
	const ShaderCacheIndexKey& vs_key, const ShaderCacheIndexKey& fs_key, u32 renderpass_key,
	const VkGraphicsPipelineCreateInfo& ci, bool uber)
{
	const auto key = GetGraphicsPipelineCacheKey(vs_key, fs_key, renderpass_key, ci);

	AddGraphicsPipelineKey(key, uber);

	Common::Timer debug_timer;

	VkPipeline pipeline;
	VkResult res = vkCreateGraphicsPipelines(device, GetPipelineCache(true, uber), 1, &ci, nullptr, &pipeline);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines() failed: ");
		return {};
	}

	Console.WriteLn("Sync pipeline compile: uber=%d time=%.2fms", uber, debug_timer.GetTimeMilliseconds());

	return { pipeline, key };
}

std::optional<VKShaderCache::SPIRVCodeVector> VKShaderCache::CompileAndAddShaderSPV(
	const ShaderCacheIndexKey& key, std::string_view shader_code, bool uber)
{
	std::optional<SPIRVCodeVector> spv = CompileShaderToSPV(key.shader_type, shader_code, GSConfig.UseDebugDevice);
	if (!spv.has_value())
		return {};

	CacheState& state = GetCacheState(uber);

	if (!state.shader_blob_file || std::fseek(state.shader_blob_file, 0, SEEK_END) != 0)
		return spv;

	ShaderCacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(state.shader_blob_file));
	data.blob_size = static_cast<u32>(spv->size());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.shader_type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(spv->data(), sizeof(SPIRVCodeType), entry.blob_size, state.shader_blob_file) != entry.blob_size ||
		std::fflush(state.shader_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, state.shader_index_file) != 1 ||
		std::fflush(state.shader_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
		return spv;
	}

	state.shader_index.emplace(key, data);
	return spv;
}

void VKShaderCache::AddShaderSPV(u32 type, std::string_view shader_code, const SPIRVCodeVector& spv,
	bool uber, bool only_new)
{
	// FIXME: Duplication with CompileAndAddShaderSPV();
	CacheState& state = GetCacheState(uber);

	const auto key = GetShaderCacheKey(type, shader_code);

	if (only_new && state.shader_index.contains(key))
		return;

	if (!state.shader_blob_file || std::fseek(state.shader_blob_file, 0, SEEK_END) != 0)
		return;

	ShaderCacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(state.shader_blob_file));
	data.blob_size = static_cast<u32>(spv.size());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.shader_type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(spv.data(), sizeof(SPIRVCodeType), entry.blob_size, state.shader_blob_file) != entry.blob_size ||
		std::fflush(state.shader_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, state.shader_index_file) != 1 ||
		std::fflush(state.shader_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
	}

	state.shader_index.emplace(key, data);
}

void VKShaderCache::AddGraphicsPipelineKey(const GraphicsPipelineCacheIndexKey& key, bool uber)
{
	CacheState& state = GetCacheState(uber);
	bool is_new = state.pipeline_index.insert(key).second;
	if (is_new)
		state.new_pipeline_index.push_back(key);
}

void VKShaderCache::ProcessAsyncCompileJobs()
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
				VKShaderJob* shader_job = static_cast<VKShaderJob*>(job);
				AddShaderSPV(shader_job->GetKind(), shader_job->GetShaderCode(), shader_job->GetSPV(), shader_job->IsUber(), true);

				pxAssert(shader_job->GetKind() == shaderc_vertex_shader || shader_job->GetKind() == shaderc_fragment_shader);
				const char* kind_str = (shader_job->GetKind() == shaderc_vertex_shader) ? "vertex" : "fragment";
				Console.WriteLn("Async %s shader compile: finished hash=0x%016llX uber=%d time=%.2fms thread_id=%d",
					kind_str, shader_job->GetHash(), shader_job->IsUber(), shader_job->GetCompileTimeMS(), shader_job->GetThreadID());

				// Notify any pipelines waiting on this shader.
				StartQueuedPipelineJobs(shader_job);
			}
			else if (job->IsPipelineJob())
			{
				VKPipelineJob* pipeline_job = static_cast<VKPipelineJob*>(job);
				Console.WriteLn("Async pipeline compile: finished hash=0x%016llX uber=%d time=%.2fms thread_id=%d",
					pipeline_job->GetHash(), pipeline_job->IsUber(), pipeline_job->GetCompileTimeMS(),
					pipeline_job->GetThreadID());

				const GraphicsPipelineCacheIndexKey pipeline_key = pipeline_job->GetPipelineCacheKey();
				AddGraphicsPipelineKey(pipeline_key, pipeline_job->IsUber());
			}
			else
			{
				pxFailRel("Unknown job type");
			}

			DoneAsyncPipelineJob(job);
		}
	}
}

void VKShaderCache::DoneAsyncPipelineJob(GSCompileJob* job)
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

void VKShaderCache::StartPipelineCompilationAsync(std::shared_ptr<GSCompileJob> job)
{
	if (!m_compiler_async)
		m_compiler_async = std::unique_ptr<VKShaderCompilerAsync>(
			new VKShaderCompilerAsync(GSConfig.HybridShaderCacheThreads, GSConfig.HybridShaderCacheLatencyMS,
			GSConfig.UseDebugDevice, GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_khr_shader_non_semantic_info));

	if (job->IsShaderJob())
	{
		VKShaderJob* shader_job = static_cast<VKShaderJob*>(job.get());
		pxAssert(shader_job->GetKind() == shaderc_vertex_shader || shader_job->GetKind() == shaderc_fragment_shader);
		const char* kind_str = (shader_job->GetKind() == shaderc_vertex_shader) ? "vertex" : "fragment";
		Console.WriteLn("Async %s shader compile: started hash=0x%016llX uber=%d",
			kind_str, shader_job->GetHash(), shader_job->IsUber());
		m_compiler_async->StartCompileJobAsync(shader_job);
		m_compile_jobs_async.emplace_back(std::move(job));
	}
	else if (job->IsPipelineJob())
	{
		VKPipelineJob* pipeline_job = static_cast<VKPipelineJob*>(job.get());
		if (pipeline_job->HasVS() && pipeline_job->HasFS())
		{
			Console.WriteLn("Async pipeline compile: started hash=0x%016llX uber=%d", pipeline_job->GetHash(), pipeline_job->IsUber());
			m_compiler_async->StartCompileJobAsync(pipeline_job);
			m_compile_jobs_async.emplace_back(std::move(job));
		}
		else
		{
			// Need to wait for vertex and/or fragment shader.
			pxAssert((pipeline_job->GetVSJob() || pipeline_job->HasVS()) &&
				(pipeline_job->GetFSJob() || pipeline_job->HasFS()));
			Console.WriteLn("Async pipeline compile: queued hash=0x%016llX uber=%d vs_hash=0x%016llX fs_hash=0x%016llX",
				pipeline_job->GetHash(), pipeline_job->IsUber(),
				pipeline_job->GetVSJob() ? pipeline_job->GetVSJob()->GetHash() : 0,
				pipeline_job->GetFSJob() ? pipeline_job->GetFSJob()->GetHash() : 0);
			m_queued_pipeline_jobs_async.push_back(pipeline_job);
			m_compile_jobs_async.emplace_back(std::move(job));
		}
	}
	else
	{
		pxFailRel("Unknown job type");
	}
}

void VKShaderCache::StartQueuedPipelineJobs(const VKShaderJob* shader_job)
{
	for (auto it = m_queued_pipeline_jobs_async.begin(); it != m_queued_pipeline_jobs_async.end(); )
	{
		VKPipelineJob* queued_job = *it;

		bool pipeline_job_failed = false;
		
		if (shader_job->GetKind() == shaderc_vertex_shader)
		{
			if (!queued_job->HasVS() && queued_job->GetVSJob() == shader_job)
			{
				if (shader_job->GetModule() == VK_NULL_HANDLE)
					pipeline_job_failed = true; // Shader failed to compile so pipeline also fails.
				else
					queued_job->SetVS(shader_job->GetModule());
			}
		}
		else if (shader_job->GetKind() == shaderc_fragment_shader)
		{
			if (!queued_job->HasFS() && queued_job->GetFSJob() == shader_job)
			{
				if (shader_job->GetModule() == VK_NULL_HANDLE)
					pipeline_job_failed = true; // Shader failed to compile so pipeline also fails.
				else
					queued_job->SetFS(shader_job->GetModule());
			}
		}
		else
		{
			pxFailRel("Unknown shader type");
		}

		if (pipeline_job_failed)
		{
			Console.WriteLn("Async pipeline shader failed: %s=%016llX failed for pipeline=%016llX uber=%d",
				shader_job->GetKind() == shaderc_vertex_shader ? "vs" : "fs",
				queued_job->GetHash(), queued_job->IsUber());

			DoneAsyncPipelineJob(queued_job);

			it = m_queued_pipeline_jobs_async.erase(it);
		}
		else if (queued_job->HasVS() && queued_job->HasFS())
		{
			// Vertex and pixel shaders compiled so start pipeline creating.
			Console.WriteLn("Async pipeline compile: got vs=%016llX and fs=%016llX for pipeline=%016llX uber=%d",
				queued_job->GetVSJob() ? queued_job->GetVSJob()->GetHash() : 0,
				queued_job->GetFSJob() ? queued_job->GetFSJob()->GetHash() : 0,
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
