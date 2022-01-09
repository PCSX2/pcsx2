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
#include "common/D3D11/ShaderCompiler.h"
#include <cstdio>
#include <d3d11.h>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <wil/com.h>

namespace D3D11
{
	class ShaderCache
	{
	public:
		ShaderCache();
		~ShaderCache();

		D3D_FEATURE_LEVEL GetFeatureLevel() const { return m_feature_level; }
		bool UsingDebugShaders() const { return m_debug; }

		bool Open(std::string_view base_path, D3D_FEATURE_LEVEL feature_level, u32 version, bool debug);

		wil::com_ptr_nothrow<ID3DBlob> GetShaderBlob(ShaderCompiler::Type type, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		wil::com_ptr_nothrow<ID3D11VertexShader> GetVertexShader(ID3D11Device* device, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		bool GetVertexShaderAndInputLayout(ID3D11Device* device,
			ID3D11VertexShader** vs, ID3D11InputLayout** il,
			const D3D11_INPUT_ELEMENT_DESC* layout, size_t layout_size,
			const std::string_view& shader_code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		wil::com_ptr_nothrow<ID3D11GeometryShader> GetGeometryShader(ID3D11Device* device, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		wil::com_ptr_nothrow<ID3D11PixelShader> GetPixelShader(ID3D11Device* device, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

		wil::com_ptr_nothrow<ID3D11ComputeShader> GetComputeShader(ID3D11Device* device, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

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
			ShaderCompiler::Type shader_type;

			bool operator==(const CacheIndexKey& key) const;
			bool operator!=(const CacheIndexKey& key) const;
		};

		struct CacheIndexEntryHasher
		{
			std::size_t operator()(const CacheIndexKey& e) const noexcept
			{
				std::size_t h = 0;
				HashCombine(h, e.entry_point_low, e.entry_point_high, e.macro_hash_low, e.macro_hash_high,
					e.source_hash_low, e.source_hash_high, e.source_length, e.shader_type);
				return h;
			}
		};

		struct CacheIndexData
		{
			u32 file_offset;
			u32 blob_size;
		};

		using CacheIndex = std::unordered_map<CacheIndexKey, CacheIndexData, CacheIndexEntryHasher>;

		static std::string GetCacheBaseFileName(const std::string_view& base_path, D3D_FEATURE_LEVEL feature_level,
			bool debug);
		static CacheIndexKey GetCacheKey(ShaderCompiler::Type type, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros, const char* entry_point);

		bool CreateNew(const std::string& index_filename, const std::string& blob_filename);
		bool ReadExisting(const std::string& index_filename, const std::string& blob_filename);
		void Close();

		wil::com_ptr_nothrow<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key, const std::string_view& shader_code,
			const D3D_SHADER_MACRO* macros, const char* entry_point);

		std::FILE* m_index_file = nullptr;
		std::FILE* m_blob_file = nullptr;

		CacheIndex m_index;

		D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;
		u32 m_version = 0;
		bool m_debug = false;
	};
} // namespace D3D11
