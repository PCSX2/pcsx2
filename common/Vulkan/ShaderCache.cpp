/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common/Vulkan/ShaderCache.h"
#include "common/Vulkan/ShaderCompiler.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/Util.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/MD5Digest.h"

// TODO: store the driver version and stuff in the shader header

std::unique_ptr<Vulkan::ShaderCache> g_vulkan_shader_cache;

namespace Vulkan
{
	using ShaderCompiler::SPIRVCodeType;
	using ShaderCompiler::SPIRVCodeVector;

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

		if (header.vendor_id != g_vulkan_context->GetDeviceProperties().vendorID)
		{
			Console.Error("Pipeline cache failed validation: Incorrect vendor ID (file: 0x%X, device: 0x%X)",
				header.vendor_id, g_vulkan_context->GetDeviceProperties().vendorID);
			return false;
		}

		if (header.device_id != g_vulkan_context->GetDeviceProperties().deviceID)
		{
			Console.Error("Pipeline cache failed validation: Incorrect device ID (file: 0x%X, device: 0x%X)",
				header.device_id, g_vulkan_context->GetDeviceProperties().deviceID);
			return false;
		}

		if (std::memcmp(header.uuid, g_vulkan_context->GetDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE) != 0)
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
		header->vendor_id = g_vulkan_context->GetDeviceProperties().vendorID;
		header->device_id = g_vulkan_context->GetDeviceProperties().deviceID;
		std::memcpy(header->uuid, g_vulkan_context->GetDeviceProperties().pipelineCacheUUID, VK_UUID_SIZE);
	}

	ShaderCache::ShaderCache() = default;

	ShaderCache::~ShaderCache()
	{
		CloseShaderCache();
		FlushPipelineCache();
		ClosePipelineCache();
	}

	bool ShaderCache::CacheIndexKey::operator==(const CacheIndexKey& key) const
	{
		return (source_hash_low == key.source_hash_low && source_hash_high == key.source_hash_high &&
				source_length == key.source_length && shader_type == key.shader_type);
	}

	bool ShaderCache::CacheIndexKey::operator!=(const CacheIndexKey& key) const
	{
		return (source_hash_low != key.source_hash_low || source_hash_high != key.source_hash_high ||
				source_length != key.source_length || shader_type != key.shader_type);
	}

	void ShaderCache::Create(std::string_view base_path, u32 version, bool debug)
	{
		pxAssert(!g_vulkan_shader_cache);
		g_vulkan_shader_cache.reset(new ShaderCache());
		g_vulkan_shader_cache->Open(base_path, version, debug);
	}

	void ShaderCache::Destroy() { g_vulkan_shader_cache.reset(); }

	void ShaderCache::Open(std::string_view directory, u32 version, bool debug)
	{
		m_version = version;
		m_debug = debug;

		if (!directory.empty())
		{
			m_pipeline_cache_filename = GetPipelineCacheBaseFileName(directory, debug);

			const std::string base_filename = GetShaderCacheBaseFileName(directory, debug);
			const std::string index_filename = base_filename + ".idx";
			const std::string blob_filename = base_filename + ".bin";

			if (!ReadExistingShaderCache(index_filename, blob_filename))
				CreateNewShaderCache(index_filename, blob_filename);

			if (!ReadExistingPipelineCache())
				CreateNewPipelineCache();
		}
		else
		{
			CreateNewPipelineCache();
		}
	}

	VkPipelineCache ShaderCache::GetPipelineCache(bool set_dirty /*= true*/)
	{
		if (m_pipeline_cache == VK_NULL_HANDLE)
			return VK_NULL_HANDLE;

		m_pipeline_cache_dirty |= set_dirty;
		return m_pipeline_cache;
	}

	bool ShaderCache::CreateNewShaderCache(const std::string& index_filename, const std::string& blob_filename)
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

		m_index_file = FileSystem::OpenCFile(index_filename.c_str(), "wb");
		if (!m_index_file)
		{
			Console.Error("Failed to open index file '%s' for writing", index_filename.c_str());
			return false;
		}

		const u32 index_version = FILE_VERSION;
		VK_PIPELINE_CACHE_HEADER header;
		FillPipelineCacheHeader(&header);

		if (std::fwrite(&index_version, sizeof(index_version), 1, m_index_file) != 1 ||
			std::fwrite(&m_version, sizeof(m_version), 1, m_index_file) != 1 ||
			std::fwrite(&header, sizeof(header), 1, m_index_file) != 1)
		{
			Console.Error("Failed to write header to index file '%s'", index_filename.c_str());
			std::fclose(m_index_file);
			m_index_file = nullptr;
			FileSystem::DeleteFilePath(index_filename.c_str());
			return false;
		}

		m_blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "w+b");
		if (!m_blob_file)
		{
			Console.Error("Failed to open blob file '%s' for writing", blob_filename.c_str());
			std::fclose(m_index_file);
			m_index_file = nullptr;
			FileSystem::DeleteFilePath(index_filename.c_str());
			return false;
		}

		return true;
	}

	bool ShaderCache::ReadExistingShaderCache(const std::string& index_filename, const std::string& blob_filename)
	{
		m_index_file = FileSystem::OpenCFile(index_filename.c_str(), "r+b");
		if (!m_index_file)
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
		u32 data_version = 0;
		if (std::fread(&file_version, sizeof(file_version), 1, m_index_file) != 1 || file_version != FILE_VERSION ||
			std::fread(&data_version, sizeof(data_version), 1, m_index_file) != 1 || data_version != m_version)
		{
			Console.Error("Bad file/data version in '%s'", index_filename.c_str());
			std::fclose(m_index_file);
			m_index_file = nullptr;
			return false;
		}

		VK_PIPELINE_CACHE_HEADER header;
		if (std::fread(&header, sizeof(header), 1, m_index_file) != 1 || !ValidatePipelineCacheHeader(header))
		{
			Console.Error("Mismatched pipeline cache header in '%s' (GPU/driver changed?)", index_filename.c_str());
			std::fclose(m_index_file);
			m_index_file = nullptr;
			return false;
		}

		m_blob_file = FileSystem::OpenCFile(blob_filename.c_str(), "a+b");
		if (!m_blob_file)
		{
			Console.Error("Blob file '%s' is missing", blob_filename.c_str());
			std::fclose(m_index_file);
			m_index_file = nullptr;
			return false;
		}

		std::fseek(m_blob_file, 0, SEEK_END);
		const u32 blob_file_size = static_cast<u32>(std::ftell(m_blob_file));

		for (;;)
		{
			CacheIndexEntry entry;
			if (std::fread(&entry, sizeof(entry), 1, m_index_file) != 1 ||
				(entry.file_offset + entry.blob_size) > blob_file_size)
			{
				if (std::feof(m_index_file))
					break;

				Console.Error("Failed to read entry from '%s', corrupt file?", index_filename.c_str());
				m_index.clear();
				std::fclose(m_blob_file);
				m_blob_file = nullptr;
				std::fclose(m_index_file);
				m_index_file = nullptr;
				return false;
			}

			const CacheIndexKey key{entry.source_hash_low, entry.source_hash_high, entry.source_length,
				static_cast<ShaderCompiler::Type>(entry.shader_type)};
			const CacheIndexData data{entry.file_offset, entry.blob_size};
			m_index.emplace(key, data);
		}

		// ensure we don't write before seeking
		std::fseek(m_index_file, 0, SEEK_END);

		Console.WriteLn("Read %zu entries from '%s'", m_index.size(), index_filename.c_str());
		return true;
	}

	void ShaderCache::CloseShaderCache()
	{
		if (m_index_file)
		{
			std::fclose(m_index_file);
			m_index_file = nullptr;
		}
		if (m_blob_file)
		{
			std::fclose(m_blob_file);
			m_blob_file = nullptr;
		}
	}

	bool ShaderCache::CreateNewPipelineCache()
	{
		if (!m_pipeline_cache_filename.empty() && FileSystem::FileExists(m_pipeline_cache_filename.c_str()))
		{
			Console.Warning("Removing existing pipeline cache '%s'", m_pipeline_cache_filename.c_str());
			FileSystem::DeleteFilePath(m_pipeline_cache_filename.c_str());
		}

		const VkPipelineCacheCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, 0, nullptr};
		VkResult res = vkCreatePipelineCache(g_vulkan_context->GetDevice(), &ci, nullptr, &m_pipeline_cache);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreatePipelineCache() failed: ");
			return false;
		}

		m_pipeline_cache_dirty = true;
		return true;
	}

	bool ShaderCache::ReadExistingPipelineCache()
	{
		std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(m_pipeline_cache_filename.c_str());
		if (!data.has_value())
			return false;

		if (data->size() < sizeof(VK_PIPELINE_CACHE_HEADER))
		{
			Console.Error("Pipeline cache at '%s' is too small", m_pipeline_cache_filename.c_str());
			return false;
		}

		VK_PIPELINE_CACHE_HEADER header;
		std::memcpy(&header, data->data(), sizeof(header));
		if (!ValidatePipelineCacheHeader(header))
			return false;

		const VkPipelineCacheCreateInfo ci{
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0, data->size(), data->data()};
		VkResult res = vkCreatePipelineCache(g_vulkan_context->GetDevice(), &ci, nullptr, &m_pipeline_cache);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreatePipelineCache() failed: ");
			return false;
		}

		return true;
	}

	bool ShaderCache::FlushPipelineCache()
	{
		if (m_pipeline_cache == VK_NULL_HANDLE || !m_pipeline_cache_dirty || m_pipeline_cache_filename.empty())
			return false;

		size_t data_size;
		VkResult res = vkGetPipelineCacheData(g_vulkan_context->GetDevice(), m_pipeline_cache, &data_size, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData() failed: ");
			return false;
		}

		std::vector<u8> data(data_size);
		res = vkGetPipelineCacheData(g_vulkan_context->GetDevice(), m_pipeline_cache, &data_size, data.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData() (2) failed: ");
			return false;
		}

		data.resize(data_size);

		// Save disk writes if it hasn't changed, think of the poor SSDs.
		FILESYSTEM_STAT_DATA sd;
		if (!FileSystem::StatFile(m_pipeline_cache_filename.c_str(), &sd) || sd.Size != static_cast<s64>(data_size))
		{
			Console.WriteLn("Writing %zu bytes to '%s'", data_size, m_pipeline_cache_filename.c_str());
			if (!FileSystem::WriteBinaryFile(m_pipeline_cache_filename.c_str(), data.data(), data.size()))
			{
				Console.Error("Failed to write pipeline cache to '%s'", m_pipeline_cache_filename.c_str());
				return false;
			}
		}
		else
		{
			Console.WriteLn(
				"Skipping updating pipeline cache '%s' due to no changes.", m_pipeline_cache_filename.c_str());
		}

		m_pipeline_cache_dirty = false;
		return true;
	}

	void ShaderCache::ClosePipelineCache()
	{
		if (m_pipeline_cache == VK_NULL_HANDLE)
			return;

		vkDestroyPipelineCache(g_vulkan_context->GetDevice(), m_pipeline_cache, nullptr);
		m_pipeline_cache = VK_NULL_HANDLE;
	}

	std::string ShaderCache::GetShaderCacheBaseFileName(const std::string_view& base_path, bool debug)
	{
		std::string base_filename(base_path);
		base_filename += FS_OSPATH_SEPARATOR_STR "vulkan_shaders";

		if (debug)
			base_filename += "_debug";

		return base_filename;
	}

	std::string ShaderCache::GetPipelineCacheBaseFileName(const std::string_view& base_path, bool debug)
	{
		std::string base_filename(base_path);
		base_filename += FS_OSPATH_SEPARATOR_STR "vulkan_pipelines";

		if (debug)
			base_filename += "_debug";

		base_filename += ".bin";
		return base_filename;
	}

	ShaderCache::CacheIndexKey ShaderCache::GetCacheKey(ShaderCompiler::Type type, const std::string_view& shader_code)
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

		return CacheIndexKey{h.hash_low, h.hash_high, static_cast<u32>(shader_code.length()), type};
	}

	std::optional<ShaderCompiler::SPIRVCodeVector> ShaderCache::GetShaderSPV(
		ShaderCompiler::Type type, std::string_view shader_code)
	{
		const auto key = GetCacheKey(type, shader_code);
		auto iter = m_index.find(key);
		if (iter == m_index.end())
			return CompileAndAddShaderSPV(key, shader_code);

		SPIRVCodeVector spv(iter->second.blob_size);
		if (std::fseek(m_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
			std::fread(spv.data(), sizeof(SPIRVCodeType), iter->second.blob_size, m_blob_file) !=
				iter->second.blob_size)
		{
			Console.Error("Read blob from file failed, recompiling");
			return ShaderCompiler::CompileShader(type, shader_code, m_debug);
		}

		return spv;
	}

	VkShaderModule ShaderCache::GetShaderModule(ShaderCompiler::Type type, std::string_view shader_code)
	{
		std::optional<SPIRVCodeVector> spv = GetShaderSPV(type, shader_code);
		if (!spv.has_value())
			return VK_NULL_HANDLE;

		const VkShaderModuleCreateInfo ci{
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, spv->size() * sizeof(SPIRVCodeType), spv->data()};

		VkShaderModule mod;
		VkResult res = vkCreateShaderModule(g_vulkan_context->GetDevice(), &ci, nullptr, &mod);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateShaderModule() failed: ");
			return VK_NULL_HANDLE;
		}

		return mod;
	}

	VkShaderModule ShaderCache::GetVertexShader(std::string_view shader_code)
	{
		return GetShaderModule(ShaderCompiler::Type::Vertex, std::move(shader_code));
	}

	VkShaderModule ShaderCache::GetGeometryShader(std::string_view shader_code)
	{
		return GetShaderModule(ShaderCompiler::Type::Geometry, std::move(shader_code));
	}

	VkShaderModule ShaderCache::GetFragmentShader(std::string_view shader_code)
	{
		return GetShaderModule(ShaderCompiler::Type::Fragment, std::move(shader_code));
	}

	VkShaderModule ShaderCache::GetComputeShader(std::string_view shader_code)
	{
		return GetShaderModule(ShaderCompiler::Type::Compute, std::move(shader_code));
	}

	std::optional<ShaderCompiler::SPIRVCodeVector> ShaderCache::CompileAndAddShaderSPV(
		const CacheIndexKey& key, std::string_view shader_code)
	{
		std::optional<SPIRVCodeVector> spv = ShaderCompiler::CompileShader(key.shader_type, shader_code, m_debug);
		if (!spv.has_value())
			return {};

		if (!m_blob_file || std::fseek(m_blob_file, 0, SEEK_END) != 0)
			return spv;

		CacheIndexData data;
		data.file_offset = static_cast<u32>(std::ftell(m_blob_file));
		data.blob_size = static_cast<u32>(spv->size());

		CacheIndexEntry entry = {};
		entry.source_hash_low = key.source_hash_low;
		entry.source_hash_high = key.source_hash_high;
		entry.source_length = key.source_length;
		entry.shader_type = static_cast<u32>(key.shader_type);
		entry.blob_size = data.blob_size;
		entry.file_offset = data.file_offset;

		if (std::fwrite(spv->data(), sizeof(SPIRVCodeType), entry.blob_size, m_blob_file) != entry.blob_size ||
			std::fflush(m_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, m_index_file) != 1 ||
			std::fflush(m_index_file) != 0)
		{
			Console.Error("Failed to write shader blob to file");
			return spv;
		}

		m_index.emplace(key, data);
		return spv;
	}
} // namespace Vulkan