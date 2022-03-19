/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"

#include <cstdio>
#include <d3d12.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <wil/com.h>

namespace D3D12
{
	class ShaderCache
	{
	public:
		template <typename T>
		using ComPtr = wil::com_ptr_nothrow<T>;

		enum class EntryType
		{
			VertexShader,
			GeometryShader,
			PixelShader,
			ComputeShader,
			GraphicsPipeline,
		};

		ShaderCache();
		~ShaderCache();

		__fi D3D_FEATURE_LEVEL GetFeatureLevel() const { return m_feature_level; }
		__fi u32 GetDataVersion() const { return m_data_version; }
		__fi bool UsingPipelineCache() const { return m_use_pipeline_cache; }
		__fi bool UsingDebugShaders() const { return m_debug; }

		bool Open(std::string_view base_path, D3D_FEATURE_LEVEL feature_level, u32 version, bool debug);

		__fi ComPtr<ID3DBlob> GetVertexShader(std::string_view shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
		{
			return GetShaderBlob(EntryType::VertexShader, shader_code, macros, entry_point);
		}
		__fi ComPtr<ID3DBlob> GetGeometryShader(std::string_view shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
		{
			return GetShaderBlob(EntryType::GeometryShader, shader_code, macros, entry_point);
		}
		__fi ComPtr<ID3DBlob> GetPixelShader(std::string_view shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
		{
			return GetShaderBlob(EntryType::PixelShader, shader_code, macros, entry_point);
		}
		__fi ComPtr<ID3DBlob> GetComputeShader(std::string_view shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main")
		{
			return GetShaderBlob(EntryType::ComputeShader, shader_code, macros, entry_point);
		}

		ComPtr<ID3DBlob> GetShaderBlob(EntryType type, std::string_view shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		ComPtr<ID3D12PipelineState> GetPipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

	private:
		static constexpr u32 FILE_VERSION = 1;

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

		static std::string GetCacheBaseFileName(const std::string_view& base_path, const std::string_view& type,
			D3D_FEATURE_LEVEL feature_level, bool debug);
		static CacheIndexKey GetShaderCacheKey(EntryType type, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros, const char* entry_point);
		static CacheIndexKey GetPipelineCacheKey(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);

		bool CreateNew(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
			std::FILE*& blob_file);
		bool ReadExisting(const std::string& index_filename, const std::string& blob_filename, std::FILE*& index_file,
			std::FILE*& blob_file, CacheIndex& index);
		void InvalidatePipelineCache();
		void Close();

		ComPtr<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, std::string_view shader_code,
			const D3D_SHADER_MACRO* macros, const char* entry_point);
		ComPtr<ID3D12PipelineState> CompileAndAddPipeline(ID3D12Device* device, const CacheIndexKey& key,
			const D3D12_GRAPHICS_PIPELINE_STATE_DESC& gpdesc);

		std::string m_base_path;

		std::FILE* m_shader_index_file = nullptr;
		std::FILE* m_shader_blob_file = nullptr;
		CacheIndex m_shader_index;

		std::FILE* m_pipeline_index_file = nullptr;
		std::FILE* m_pipeline_blob_file = nullptr;
		CacheIndex m_pipeline_index;

		D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;
		u32 m_data_version = 0;
		bool m_use_pipeline_cache = false;
		bool m_debug = false;
	};
} // namespace D3D12
