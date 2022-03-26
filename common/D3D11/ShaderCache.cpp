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

#include "common/PrecompiledHeader.h"
#include "common/D3D11/ShaderCache.h"
#include "common/D3D11/ShaderCompiler.h"
#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/MD5Digest.h"
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

D3D11::ShaderCache::ShaderCache() = default;

D3D11::ShaderCache::~ShaderCache()
{
	if (m_index_file)
		std::fclose(m_index_file);
	if (m_blob_file)
		std::fclose(m_blob_file);
}

bool D3D11::ShaderCache::CacheIndexKey::operator==(const CacheIndexKey& key) const
{
	return (source_hash_low == key.source_hash_low && source_hash_high == key.source_hash_high &&
			macro_hash_low == key.macro_hash_low && macro_hash_high == key.macro_hash_high &&
			entry_point_low == key.entry_point_low && entry_point_high == key.entry_point_high &&
			shader_type == key.shader_type && source_length == key.source_length);
}

bool D3D11::ShaderCache::CacheIndexKey::operator!=(const CacheIndexKey& key) const
{
	return (source_hash_low != key.source_hash_low || source_hash_high != key.source_hash_high ||
			macro_hash_low != key.macro_hash_low || macro_hash_high != key.macro_hash_high ||
			entry_point_low != key.entry_point_low || entry_point_high != key.entry_point_high ||
			shader_type != key.shader_type || source_length != key.source_length);
}

bool D3D11::ShaderCache::Open(std::string_view base_path, D3D_FEATURE_LEVEL feature_level, u32 version, bool debug)
{
	m_feature_level = feature_level;
	m_version = version;
	m_debug = debug;

	if (!base_path.empty())
	{
		const std::string base_filename = GetCacheBaseFileName(base_path, feature_level, debug);
		const std::string index_filename = base_filename + ".idx";
		const std::string blob_filename = base_filename + ".bin";

		if (!ReadExisting(index_filename, blob_filename))
			return CreateNew(index_filename, blob_filename);
	}

	return true;
}

bool D3D11::ShaderCache::CreateNew(const std::string& index_filename, const std::string& blob_filename)
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
	if (std::fwrite(&index_version, sizeof(index_version), 1, m_index_file) != 1 ||
		std::fwrite(&m_version, sizeof(m_version), 1, m_index_file) != 1)
	{
		Console.Error("Failed to write version to index file '%s'", index_filename.c_str());
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

bool D3D11::ShaderCache::ReadExisting(const std::string& index_filename, const std::string& blob_filename)
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

		const CacheIndexKey key{
			entry.source_hash_low, entry.source_hash_high,
			entry.macro_hash_low, entry.macro_hash_high,
			entry.entry_point_low, entry.entry_point_high,
			entry.source_length, static_cast<ShaderCompiler::Type>(entry.shader_type)};
		const CacheIndexData data{entry.file_offset, entry.blob_size};
		m_index.emplace(key, data);
	}

	// ensure we don't write before seeking
	std::fseek(m_index_file, 0, SEEK_END);

	DevCon.WriteLn("Read %zu entries from '%s'", m_index.size(), index_filename.c_str());
	return true;
}

std::string D3D11::ShaderCache::GetCacheBaseFileName(const std::string_view& base_path, D3D_FEATURE_LEVEL feature_level,
	bool debug)
{
	std::string base_filename(base_path);
	base_filename += FS_OSPATH_SEPARATOR_STR "d3d_shaders_";

	switch (feature_level)
	{
		case D3D_FEATURE_LEVEL_10_0:
			base_filename += "sm40";
			break;
		case D3D_FEATURE_LEVEL_10_1:
			base_filename += "sm41";
			break;
		case D3D_FEATURE_LEVEL_11_0:
			base_filename += "sm50";
			break;
		default:
			base_filename += "unk";
			break;
	}

	if (debug)
		base_filename += "_debug";

	return base_filename;
}

D3D11::ShaderCache::CacheIndexKey D3D11::ShaderCache::GetCacheKey(ShaderCompiler::Type type, const std::string_view& shader_code,
	const D3D_SHADER_MACRO* macros, const char* entry_point)
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
	key.shader_type = type;

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

wil::com_ptr_nothrow<ID3DBlob> D3D11::ShaderCache::GetShaderBlob(ShaderCompiler::Type type, const std::string_view& shader_code,
	const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	const auto key = GetCacheKey(type, shader_code, macros, entry_point);
	auto iter = m_index.find(key);
	if (iter == m_index.end())
		return CompileAndAddShaderBlob(key, shader_code, macros, entry_point);

	wil::com_ptr_nothrow<ID3DBlob> blob;
	HRESULT hr = D3DCreateBlob(iter->second.blob_size, blob.put());
	if (FAILED(hr) || std::fseek(m_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(blob->GetBufferPointer(), 1, iter->second.blob_size, m_blob_file) != iter->second.blob_size)
	{
		Console.Error("(D3D11::ShaderCache::GetShaderBlob): Read blob from file failed");
		return {};
	}

	return blob;
}

wil::com_ptr_nothrow<ID3D11VertexShader> D3D11::ShaderCache::GetVertexShader(ID3D11Device* device,
	const std::string_view& shader_code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = GetShaderBlob(ShaderCompiler::Type::Vertex, shader_code, macros, entry_point);
	if (!blob)
		return {};

	return D3D11::ShaderCompiler::CreateVertexShader(device, blob.get());
}

bool D3D11::ShaderCache::GetVertexShaderAndInputLayout(ID3D11Device* device,
	ID3D11VertexShader** vs, ID3D11InputLayout** il,
	const D3D11_INPUT_ELEMENT_DESC* layout, size_t layout_size, const std::string_view& shader_code,
	const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = GetShaderBlob(ShaderCompiler::Type::Vertex, shader_code, macros, entry_point);
	if (!blob)
		return false;

	wil::com_ptr_nothrow<ID3D11VertexShader> actual_vs = D3D11::ShaderCompiler::CreateVertexShader(device, blob.get());
	if (!actual_vs)
		return false;

	HRESULT hr = device->CreateInputLayout(layout, layout_size, blob->GetBufferPointer(), blob->GetBufferSize(), il);
	if (FAILED(hr))
	{
		Console.Error("(GetVertexShaderAndInputLayout) Failed to create input layout: %08X", hr);
		return false;
	}

	*vs = actual_vs.detach();
	return true;
}

wil::com_ptr_nothrow<ID3D11GeometryShader> D3D11::ShaderCache::GetGeometryShader(ID3D11Device* device,
	const std::string_view& shader_code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = GetShaderBlob(ShaderCompiler::Type::Geometry, shader_code, macros, entry_point);
	if (!blob)
		return {};

	return D3D11::ShaderCompiler::CreateGeometryShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11PixelShader> D3D11::ShaderCache::GetPixelShader(ID3D11Device* device,
	const std::string_view& shader_code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = GetShaderBlob(ShaderCompiler::Type::Pixel, shader_code, macros, entry_point);
	if (!blob)
		return {};

	return D3D11::ShaderCompiler::CreatePixelShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3D11ComputeShader> D3D11::ShaderCache::GetComputeShader(ID3D11Device* device,
	const std::string_view& shader_code, const D3D_SHADER_MACRO* macros /* = nullptr */, const char* entry_point /* = "main" */)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = GetShaderBlob(ShaderCompiler::Type::Compute, shader_code, macros, entry_point);
	if (!blob)
		return {};

	return D3D11::ShaderCompiler::CreateComputeShader(device, blob.get());
}

wil::com_ptr_nothrow<ID3DBlob> D3D11::ShaderCache::CompileAndAddShaderBlob(const CacheIndexKey& key,
	const std::string_view& shader_code, const D3D_SHADER_MACRO* macros, const char* entry_point)
{
	wil::com_ptr_nothrow<ID3DBlob> blob = ShaderCompiler::CompileShader(key.shader_type, m_feature_level, m_debug, shader_code, macros, entry_point);
	if (!blob)
		return {};

	if (!m_blob_file || std::fseek(m_blob_file, 0, SEEK_END) != 0)
		return blob;

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(m_blob_file));
	data.blob_size = static_cast<u32>(blob->GetBufferSize());

	CacheIndexEntry entry = {};
	entry.source_hash_low = key.source_hash_low;
	entry.source_hash_high = key.source_hash_high;
	entry.macro_hash_low = key.macro_hash_low;
	entry.macro_hash_high = key.macro_hash_high;
	entry.entry_point_low = key.entry_point_low;
	entry.entry_point_high = key.entry_point_high;
	entry.source_length = key.source_length;
	entry.shader_type = static_cast<u32>(key.shader_type);
	entry.blob_size = data.blob_size;
	entry.file_offset = data.file_offset;

	if (std::fwrite(blob->GetBufferPointer(), 1, entry.blob_size, m_blob_file) != entry.blob_size ||
		std::fflush(m_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, m_index_file) != 1 ||
		std::fflush(m_index_file) != 0)
	{
		Console.Error("(D3D11::ShaderCache::CompileAndAddShaderBlob) Failed to write shader blob to file");
		return blob;
	}

	m_index.emplace(key, data);
	return blob;
}
