// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/OpenGL/GLShaderCache.h"
#include "GS/GS.h"

#include "Config.h"
#include "ShaderCacheVersion.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/MD5Digest.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

namespace {
#pragma pack(push, 1)
struct CacheIndexEntry
{
	u64 vertex_source_hash_low;
	u64 vertex_source_hash_high;
	u32 vertex_source_length;
	u64 fragment_source_hash_low;
	u64 fragment_source_hash_high;
	u32 fragment_source_length;
	u32 file_offset;
	u32 blob_size;
	u32 blob_format;
};
#pragma pack(pop)
}

GLShaderCache::GLShaderCache() = default;

GLShaderCache::~GLShaderCache()
{
	Close();
}

bool GLShaderCache::CacheIndexKey::operator==(const CacheIndexKey& key) const
{
	return (vertex_source_hash_low == key.vertex_source_hash_low &&
			vertex_source_hash_high == key.vertex_source_hash_high &&
			vertex_source_length == key.vertex_source_length &&
			fragment_source_hash_low == key.fragment_source_hash_low &&
			fragment_source_hash_high == key.fragment_source_hash_high &&
			fragment_source_length == key.fragment_source_length);
}

bool GLShaderCache::CacheIndexKey::operator!=(const CacheIndexKey& key) const
{
	return (vertex_source_hash_low != key.vertex_source_hash_low ||
			vertex_source_hash_high != key.vertex_source_hash_high ||
			vertex_source_length != key.vertex_source_length ||
			fragment_source_hash_low != key.fragment_source_hash_low ||
			fragment_source_hash_high != key.fragment_source_hash_high ||
			fragment_source_length != key.fragment_source_length);
}

bool GLShaderCache::Open()
{
	m_program_binary_supported = GLAD_GL_ARB_get_program_binary;
	if (m_program_binary_supported)
	{
		// check that there's at least one format and the extension isn't being "faked"
		GLint num_formats = 0;
		glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_formats);
		Console.WriteLn("%u program binary formats supported by driver", num_formats);
		m_program_binary_supported = (num_formats > 0);
	}

	if (!m_program_binary_supported)
	{
		Console.Warning("Your GL driver does not support program binaries. Hopefully it has a built-in cache.");
		return true;
	}

	if (!GSConfig.DisableShaderCache)
	{
		const std::string index_filename = GetIndexFileName();
		const std::string blob_filename = GetBlobFileName();

		if (ReadExisting(index_filename, blob_filename))
			return true;

		return CreateNew(index_filename, blob_filename);
	}

	return true;
}

bool GLShaderCache::CreateNew(const std::string& index_filename, const std::string& blob_filename)
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

	const u32 file_version = SHADER_CACHE_VERSION;
	if (std::fwrite(&file_version, sizeof(file_version), 1, m_index_file) != 1)
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

bool GLShaderCache::ReadExisting(const std::string& index_filename, const std::string& blob_filename)
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
	if (std::fread(&file_version, sizeof(file_version), 1, m_index_file) != 1 || file_version != SHADER_CACHE_VERSION)
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

		const CacheIndexKey key{entry.vertex_source_hash_low, entry.vertex_source_hash_high, entry.vertex_source_length,
			entry.fragment_source_hash_low, entry.fragment_source_hash_high, entry.fragment_source_length};
		const CacheIndexData data{entry.file_offset, entry.blob_size, entry.blob_format};
		m_index.emplace(key, data);
	}

	Console.WriteLn("Read %zu entries from '%s'", m_index.size(), index_filename.c_str());
	return true;
}

void GLShaderCache::Close()
{
	m_index.clear();
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

bool GLShaderCache::Recreate()
{
	Close();

	const std::string index_filename = GetIndexFileName();
	const std::string blob_filename = GetBlobFileName();

	return CreateNew(index_filename, blob_filename);
}

GLShaderCache::CacheIndexKey GLShaderCache::GetCacheKey(
	const std::string_view vertex_shader, const std::string_view fragment_shader)
{
	union ShaderHash
	{
		struct
		{
			u64 low;
			u64 high;
		};
		u8 bytes[16];
	};

	ShaderHash vertex_hash = {};
	ShaderHash fragment_hash = {};

	MD5Digest digest;
	if (!vertex_shader.empty())
	{
		digest.Update(vertex_shader.data(), static_cast<u32>(vertex_shader.length()));
		digest.Final(vertex_hash.bytes);
	}

	if (!fragment_shader.empty())
	{
		digest.Reset();
		digest.Update(fragment_shader.data(), static_cast<u32>(fragment_shader.length()));
		digest.Final(fragment_hash.bytes);
	}

	return CacheIndexKey{vertex_hash.low, vertex_hash.high, static_cast<u32>(vertex_shader.length()), fragment_hash.low,
		fragment_hash.high, static_cast<u32>(fragment_shader.length())};
}

std::string GLShaderCache::GetIndexFileName() const
{
	return Path::Combine(EmuFolders::Cache, "gl_programs.idx");
}

std::string GLShaderCache::GetBlobFileName() const
{
	return Path::Combine(EmuFolders::Cache, "gl_programs.bin");
}

std::optional<GLProgram> GLShaderCache::GetProgram(
	const std::string_view vertex_shader, const std::string_view fragment_shader, const PreLinkCallback& callback)
{
	if (!m_program_binary_supported || !m_blob_file)
	{
#ifdef PCSX2_DEVBUILD
		Common::Timer timer;
#endif

		std::optional<GLProgram> res = CompileProgram(vertex_shader, fragment_shader, callback, false);

#ifdef PCSX2_DEVBUILD
		Console.WriteLn("Time to compile shader without caching: %.2fms", timer.GetTimeMilliseconds());
#endif
		return res;
	}

	const auto key = GetCacheKey(vertex_shader, fragment_shader);
	auto iter = m_index.find(key);
	if (iter == m_index.end())
		return CompileAndAddProgram(key, vertex_shader, fragment_shader, callback);

	std::vector<u8> data(iter->second.blob_size);
	if (std::fseek(m_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(data.data(), 1, iter->second.blob_size, m_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

#ifdef PCSX2_DEVBUILD
	Common::Timer timer;
#endif

	GLProgram prog;
	if (prog.CreateFromBinary(data.data(), static_cast<u32>(data.size()), iter->second.blob_format))
	{
#ifdef PCSX2_DEVBUILD
		Console.WriteLn("Time to create program from binary: %.2fms", timer.GetTimeMilliseconds());
#endif

		return std::optional<GLProgram>(std::move(prog));
	}

	Console.Warning(
		"Failed to create program from binary, this may be due to a driver or GPU Change. Recreating cache.");
	if (!Recreate())
		return CompileProgram(vertex_shader, fragment_shader, callback, false);
	else
		return CompileAndAddProgram(key, vertex_shader, fragment_shader, callback);
}

bool GLShaderCache::GetProgram(GLProgram* out_program, const std::string_view vertex_shader,
	const std::string_view fragment_shader, const PreLinkCallback& callback /* = */)
{
	auto prog = GetProgram(vertex_shader, fragment_shader, callback);
	if (!prog)
		return false;

	*out_program = std::move(*prog);
	return true;
}

bool GLShaderCache::WriteToBlobFile(const CacheIndexKey& key, const std::vector<u8>& prog_data, u32 prog_format)
{
	if (!m_blob_file || std::fseek(m_blob_file, 0, SEEK_END) != 0)
		return false;

	CacheIndexData data;
	data.file_offset = static_cast<u32>(std::ftell(m_blob_file));
	data.blob_size = static_cast<u32>(prog_data.size());
	data.blob_format = prog_format;

	CacheIndexEntry entry = {};
	entry.vertex_source_hash_low = key.vertex_source_hash_low;
	entry.vertex_source_hash_high = key.vertex_source_hash_high;
	entry.vertex_source_length = key.vertex_source_length;
	entry.fragment_source_hash_low = key.fragment_source_hash_low;
	entry.fragment_source_hash_high = key.fragment_source_hash_high;
	entry.fragment_source_length = key.fragment_source_length;
	entry.file_offset = data.file_offset;
	entry.blob_size = data.blob_size;
	entry.blob_format = data.blob_format;

	if (std::fwrite(prog_data.data(), 1, entry.blob_size, m_blob_file) != entry.blob_size ||
		std::fflush(m_blob_file) != 0 || std::fwrite(&entry, sizeof(entry), 1, m_index_file) != 1 ||
		std::fflush(m_index_file) != 0)
	{
		Console.Error("Failed to write shader blob to file");
		return false;
	}

	m_index.emplace(key, data);
	return true;
}

std::optional<GLProgram> GLShaderCache::CompileProgram(const std::string_view vertex_shader,
	const std::string_view fragment_shader, const PreLinkCallback& callback, bool set_retrievable)
{
	GLProgram prog;
	if (!prog.Compile(vertex_shader, fragment_shader))
		return std::nullopt;

	if (callback)
		callback(prog);

	if (set_retrievable)
		prog.SetBinaryRetrievableHint();

	if (!prog.Link())
		return std::nullopt;

	return std::optional<GLProgram>(std::move(prog));
}

std::optional<GLProgram> GLShaderCache::CompileComputeProgram(
	const std::string_view glsl, const PreLinkCallback& callback, bool set_retrievable)
{
	GLProgram prog;
	if (!prog.CompileCompute(glsl))
		return std::nullopt;

	if (callback)
		callback(prog);

	if (set_retrievable)
		prog.SetBinaryRetrievableHint();

	if (!prog.Link())
		return std::nullopt;

	return std::optional<GLProgram>(std::move(prog));
}

std::optional<GLProgram> GLShaderCache::CompileAndAddProgram(const CacheIndexKey& key,
	const std::string_view vertex_shader, const std::string_view fragment_shader, const PreLinkCallback& callback)
{
#ifdef PCSX2_DEVBUILD
	Common::Timer timer;
#endif

	std::optional<GLProgram> prog = CompileProgram(vertex_shader, fragment_shader, callback, true);
	if (!prog)
		return std::nullopt;

#ifdef PCSX2_DEVBUILD
	const float compile_time = timer.GetTimeMilliseconds();
	timer.Reset();
#endif

	std::vector<u8> prog_data;
	u32 prog_format = 0;
	if (!prog->GetBinary(&prog_data, &prog_format))
		return std::nullopt;

#ifdef PCSX2_DEVBUILD
	const float binary_time = timer.GetTimeMilliseconds();
	timer.Reset();
#endif

	WriteToBlobFile(key, prog_data, prog_format);

#ifdef PCSX2_DEVBUILD
	const float write_time = timer.GetTimeMilliseconds();
	Console.WriteLn("Compiled and cached shader: Compile: %.2fms, Binary: %.2fms, Write: %.2fms", compile_time,
		binary_time, write_time);
#endif

	return prog;
}

std::optional<GLProgram> GLShaderCache::GetComputeProgram(const std::string_view glsl, const PreLinkCallback& callback)
{
	if (!m_program_binary_supported || !m_blob_file)
	{
#ifdef PCSX2_DEVBUILD
		Common::Timer timer;
#endif

		std::optional<GLProgram> res = CompileComputeProgram(glsl, callback, false);

#ifdef PCSX2_DEVBUILD
		Console.WriteLn("Time to compile shader without caching: %.2fms", timer.GetTimeMilliseconds());
#endif
		return res;
	}

	const auto key = GetCacheKey(glsl, std::string_view());
	auto iter = m_index.find(key);
	if (iter == m_index.end())
		return CompileAndAddComputeProgram(key, glsl, callback);

	std::vector<u8> data(iter->second.blob_size);
	if (std::fseek(m_blob_file, iter->second.file_offset, SEEK_SET) != 0 ||
		std::fread(data.data(), 1, iter->second.blob_size, m_blob_file) != iter->second.blob_size)
	{
		Console.Error("Read blob from file failed");
		return {};
	}

#ifdef PCSX2_DEVBUILD
	Common::Timer timer;
#endif

	GLProgram prog;
	if (prog.CreateFromBinary(data.data(), static_cast<u32>(data.size()), iter->second.blob_format))
	{
#ifdef PCSX2_DEVBUILD
		Console.WriteLn("Time to create program from binary: %.2fms", timer.GetTimeMilliseconds());
#endif

		return std::optional<GLProgram>(std::move(prog));
	}

	Console.Warning(
		"Failed to create program from binary, this may be due to a driver or GPU Change. Recreating cache.");
	if (!Recreate())
		return CompileComputeProgram(glsl, callback, false);
	else
		return CompileAndAddComputeProgram(key, glsl, callback);
}

bool GLShaderCache::GetComputeProgram(
	GLProgram* out_program, const std::string_view glsl, const PreLinkCallback& callback)
{
	auto prog = GetComputeProgram(glsl, callback);
	if (!prog)
		return false;

	*out_program = std::move(*prog);
	return true;
}

std::optional<GLProgram> GLShaderCache::CompileAndAddComputeProgram(
	const CacheIndexKey& key, const std::string_view glsl, const PreLinkCallback& callback)
{
#ifdef PCSX2_DEVBUILD
	Common::Timer timer;
#endif

	std::optional<GLProgram> prog = CompileComputeProgram(glsl, callback, true);
	if (!prog)
		return std::nullopt;

#ifdef PCSX2_DEVBUILD
	const float compile_time = timer.GetTimeMilliseconds();
	timer.Reset();
#endif

	std::vector<u8> prog_data;
	u32 prog_format = 0;
	if (!prog->GetBinary(&prog_data, &prog_format))
		return std::nullopt;

#ifdef PCSX2_DEVBUILD
	const float binary_time = timer.GetTimeMilliseconds();
	timer.Reset();
#endif

	WriteToBlobFile(key, prog_data, prog_format);

#ifdef PCSX2_DEVBUILD
	const float write_time = timer.GetTimeMilliseconds();
	Console.WriteLn("Compiled and cached compute shader: Compile: %.2fms, Binary: %.2fms, Write: %.2fms", compile_time,
		binary_time, write_time);
#endif

	return prog;
}
