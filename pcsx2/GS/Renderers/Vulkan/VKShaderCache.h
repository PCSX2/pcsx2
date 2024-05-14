// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Vulkan/VKLoader.h"

#include "common/HashCombine.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class VKShaderCache
{
public:
	~VKShaderCache();

	static void Create();
	static void Destroy();

	/// Returns a handle to the pipeline cache. Set set_dirty to true if you are planning on writing to it externally.
	VkPipelineCache GetPipelineCache(bool set_dirty = true);

	/// Writes pipeline cache to file, saving all newly compiled pipelines.
	bool FlushPipelineCache();

	VkShaderModule GetVertexShader(std::string_view shader_code);
	VkShaderModule GetFragmentShader(std::string_view shader_code);
	VkShaderModule GetComputeShader(std::string_view shader_code);

private:
	// SPIR-V compiled code type
	using SPIRVCodeType = u32;
	using SPIRVCodeVector = std::vector<SPIRVCodeType>;

	struct CacheIndexKey
	{
		u64 source_hash_low;
		u64 source_hash_high;
		u32 source_length;
		u32 shader_type;

		bool operator==(const CacheIndexKey& key) const;
		bool operator!=(const CacheIndexKey& key) const;
	};

	struct CacheIndexEntryHasher
	{
		std::size_t operator()(const CacheIndexKey& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, e.source_hash_low, e.source_hash_high, e.source_length, e.shader_type);
			return h;
		}
	};

	struct CacheIndexData
	{
		u32 file_offset;
		u32 blob_size;
	};

	using CacheIndex = std::unordered_map<CacheIndexKey, CacheIndexData, CacheIndexEntryHasher>;

	VKShaderCache();

	static std::string GetShaderCacheBaseFileName(bool debug);
	static std::string GetPipelineCacheBaseFileName(bool debug);
	static CacheIndexKey GetCacheKey(u32 type, const std::string_view shader_code);
	static std::optional<VKShaderCache::SPIRVCodeVector> CompileShaderToSPV(
		u32 stage, std::string_view source, bool debug);

	void Open();

	bool CreateNewShaderCache(const std::string& index_filename, const std::string& blob_filename);
	bool ReadExistingShaderCache(const std::string& index_filename, const std::string& blob_filename);
	void CloseShaderCache();

	bool CreateNewPipelineCache();
	bool ReadExistingPipelineCache();
	void ClosePipelineCache();

	std::optional<SPIRVCodeVector> GetShaderSPV(u32 type, std::string_view shader_code);
	std::optional<SPIRVCodeVector> CompileAndAddShaderSPV(const CacheIndexKey& key, std::string_view shader_code);
	VkShaderModule GetShaderModule(u32 type, std::string_view shader_code);

	std::FILE* m_index_file = nullptr;
	std::FILE* m_blob_file = nullptr;
	std::string m_pipeline_cache_filename;

	CacheIndex m_index;

	VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
	bool m_pipeline_cache_dirty = false;
};

extern std::unique_ptr<VKShaderCache> g_vulkan_shader_cache;
