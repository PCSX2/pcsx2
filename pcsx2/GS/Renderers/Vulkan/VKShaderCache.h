// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"

#include "GS/Renderers/Vulkan/VKLoader.h"
#include "GS/Renderers/Vulkan/VKShadercWrapper.h"

#include "common/HashCombine.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>

class GSCompileJob;
class GSShaderCompilerAsync;
class VKShaderJob;
class VKPipelineJob;
class VKShaderCompilerAsync;

class VKShaderCache
{
public:
	struct ShaderCacheIndexKey
	{
		u64 source_hash_low;
		u64 source_hash_high;
		u32 source_length;
		u32 shader_type;

		bool operator==(const ShaderCacheIndexKey& key) const;
		bool operator!=(const ShaderCacheIndexKey& key) const;
	};

	struct ShaderCacheIndexKeyHasher
	{
		std::size_t operator()(const ShaderCacheIndexKey& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, e.source_hash_low, e.source_hash_high, e.source_length, e.shader_type);
			return h;
		}
	};

	struct GraphicsPipelineCacheIndexKey
	{
		ShaderCacheIndexKey vs;
		ShaderCacheIndexKey fs;
		u32 renderpass;
		u64 ci_hash_low;
		u64 ci_hash_high;
		u32 ci_size;

		bool operator==(const GraphicsPipelineCacheIndexKey& key) const;
		bool operator!=(const GraphicsPipelineCacheIndexKey& key) const;
	};

	struct GraphicsPipelineCacheIndexKeyHasher
	{
		std::size_t operator()(const GraphicsPipelineCacheIndexKey& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h,
				e.vs.source_hash_low, e.vs.source_hash_high, e.vs.source_length, e.vs.shader_type,
				e.fs.source_hash_low, e.fs.source_hash_high, e.fs.source_length, e.fs.shader_type,
				e.renderpass,
				e.ci_hash_low, e.ci_hash_high, e.ci_size);
			return h;
		}
	};

	static ShaderCacheIndexKey GetShaderCacheKey(u32 type, const std::string_view shader_code);
	static GraphicsPipelineCacheIndexKey GetGraphicsPipelineCacheKey(
		const ShaderCacheIndexKey& vs_key, const ShaderCacheIndexKey& fs_key, u32 renderpass_key,
		const VkGraphicsPipelineCreateInfo& ci);

	struct ShaderCacheIndexData
	{
		u32 file_offset;
		u32 blob_size;
	};

	struct VKCachedShaderModule
	{
		VkShaderModule module;
		ShaderCacheIndexKey key;
	};

	struct VKCachedPipeline
	{
		VkPipeline pipeline;
		GraphicsPipelineCacheIndexKey key;
	};

	~VKShaderCache();

	static void Create();
	static void Destroy();

	/// Returns a handle to the pipeline cache. Set set_dirty to true if you are planning on writing to it externally.
	VkPipelineCache GetPipelineCache(bool set_dirty = true, bool uber = false);

	/// Writes pipeline cache to file, saving all newly compiled pipelines.
	bool FlushPipelineCache();

	bool HasVertexShader(std::string_view shader_code, bool uber);
	bool HasFragmentShader(std::string_view shader_code, bool uber);

	VKCachedShaderModule GetVertexShader(std::string_view shader_code, bool uber);
	VKCachedShaderModule GetFragmentShader(std::string_view shader_code, bool uber);
	VkShaderModule GetComputeShader(std::string_view shader_code);

	bool HasGraphicsPipeline(const GraphicsPipelineCacheIndexKey& key, bool uber);
	VKCachedPipeline GetGraphicsPipeline(VkDevice device, const ShaderCacheIndexKey& vs_key, const ShaderCacheIndexKey& fs_key,
		u32 renderpass_key, const VkGraphicsPipelineCreateInfo& ci, bool uber);

	void StartPipelineCompilationAsync(std::shared_ptr<GSCompileJob> job);
	void ProcessAsyncCompileJobs(); // Process jobs that have finished.
private:
	// SPIR-V compiled code type
	using SPIRVCodeType = VKShadercWrapper::SPIRVCodeType;
	using SPIRVCodeVector = VKShadercWrapper::SPIRVCodeVector;

	using ShaderCacheIndex = std::unordered_map<ShaderCacheIndexKey, ShaderCacheIndexData, ShaderCacheIndexKeyHasher>;
	using GraphicsPipelineCacheSet = std::unordered_set<GraphicsPipelineCacheIndexKey, GraphicsPipelineCacheIndexKeyHasher>;

	VKShaderCache();

	static std::string GetShaderCacheBaseFileName(bool uber, bool debug);
	static std::string GetPipelineCacheBaseFileName(bool uber, bool debug);

	void Open();

	bool CreateNewShaderCache(const std::string& index_filename, const std::string& blob_filename, bool uber);
	bool ReadExistingShaderCache(const std::string& index_filename, const std::string& blob_filename, bool uber);
	void CloseShaderCache();

	bool CreateNewPipelineCache(bool uber);
	bool ReadExistingPipelineCache(bool uber);
	void ClosePipelineCache();

	static std::optional<VKShaderCache::SPIRVCodeVector> CompileShaderToSPV(
		u32 stage, std::string_view source, bool debug);
	bool HasShaderSPV(u32 type, std::string_view shader_code, bool uber);
	std::optional<SPIRVCodeVector> GetShaderSPV(u32 type, std::string_view shader_code, bool uber);
	std::optional<SPIRVCodeVector> CompileAndAddShaderSPV(const ShaderCacheIndexKey& key, std::string_view shader_code, bool uber);
	VKCachedShaderModule GetShaderModule(u32 type, std::string_view shader_code, bool uber);
	void AddShaderSPV(u32 type, std::string_view shader_code, const SPIRVCodeVector& spv,
		bool uber, bool only_new);

	void AddGraphicsPipelineKey(const GraphicsPipelineCacheIndexKey& key, bool uber);

	static bool InitShadercCompiler();

	// Start pipeline jobs that are waiting on the given vertex and/or fragment shader.
	void StartQueuedPipelineJobs(const VKShaderJob* shader_job);

	void DoneAsyncPipelineJob(GSCompileJob* job); // Remove from queue and mark done.

	struct CacheState
	{
		std::FILE* shader_index_file = nullptr;
		std::FILE* shader_blob_file = nullptr;

		std::string pipeline_cache_filename;
		std::string pipeline_index_filename;

		ShaderCacheIndex shader_index;
		GraphicsPipelineCacheSet pipeline_index;
		std::vector<GraphicsPipelineCacheIndexKey> new_pipeline_index;

		VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
		bool pipeline_cache_dirty = false;
	} m_cache_state[2]; // Normal and uber state.

	CacheState& GetCacheState(bool uber) { return m_cache_state[uber ? 1 : 0]; }

	static shaderc_compiler_t m_compiler_sync;
	static bool m_shaderc_failed;

	std::unique_ptr<VKShaderCompilerAsync> m_compiler_async;
	std::deque<std::shared_ptr<GSCompileJob>> m_compile_jobs_async;
	std::deque<VKPipelineJob*> m_queued_pipeline_jobs_async;
	std::vector<GSCompileJob*> m_finished_compile_jobs_async;
};

extern std::unique_ptr<VKShaderCache> g_vulkan_shader_cache;
