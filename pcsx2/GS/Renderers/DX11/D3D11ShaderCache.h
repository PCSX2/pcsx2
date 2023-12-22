// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "GS/Renderers/DX11/D3D.h"

#include "common/HashCombine.h"

#include <string_view>
#include <unordered_map>
#include <vector>

class D3D11ShaderCache
{
public:
	D3D11ShaderCache();
	~D3D11ShaderCache();

	D3D_FEATURE_LEVEL GetFeatureLevel() const { return m_feature_level; }
	bool UsingDebugShaders() const { return m_debug; }

	bool Open(D3D_FEATURE_LEVEL feature_level, bool debug);
	void Close();

	wil::com_ptr_nothrow<ID3DBlob> GetShaderBlob(D3D::ShaderType type, const std::string_view& shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	wil::com_ptr_nothrow<ID3D11VertexShader> GetVertexShader(ID3D11Device* device, const std::string_view& shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	bool GetVertexShaderAndInputLayout(ID3D11Device* device, ID3D11VertexShader** vs, ID3D11InputLayout** il,
		const D3D11_INPUT_ELEMENT_DESC* layout, size_t layout_size, const std::string_view& shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	wil::com_ptr_nothrow<ID3D11PixelShader> GetPixelShader(ID3D11Device* device, const std::string_view& shader_code,
		const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");

	wil::com_ptr_nothrow<ID3D11ComputeShader> GetComputeShader(ID3D11Device* device,
		const std::string_view& shader_code, const D3D_SHADER_MACRO* macros = nullptr,
		const char* entry_point = "main");

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
		D3D::ShaderType shader_type;

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

	static std::string GetCacheBaseFileName(D3D_FEATURE_LEVEL feature_level, bool debug);
	static CacheIndexKey GetCacheKey(D3D::ShaderType type, const std::string_view& shader_code,
		const D3D_SHADER_MACRO* macros, const char* entry_point);

	bool CreateNew(const std::string& index_filename, const std::string& blob_filename);
	bool ReadExisting(const std::string& index_filename, const std::string& blob_filename);

	wil::com_ptr_nothrow<ID3DBlob> CompileAndAddShaderBlob(const CacheIndexKey& key,
		const std::string_view& shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point);

	std::FILE* m_index_file = nullptr;
	std::FILE* m_blob_file = nullptr;

	CacheIndex m_index;

	D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;
	bool m_debug = false;
};
