/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "GS/Renderers/OpenGL/GLProgram.h"

#include "common/HashCombine.h"

#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class GLShaderCache
{
public:
	using PreLinkCallback = std::function<void(GLProgram&)>;

	GLShaderCache();
	~GLShaderCache();

	bool Open();
	void Close();

	std::optional<GLProgram> GetProgram(const std::string_view vertex_shader, const std::string_view fragment_shader,
		const PreLinkCallback& callback = {});
	bool GetProgram(GLProgram* out_program, const std::string_view vertex_shader,
		const std::string_view fragment_shader, const PreLinkCallback& callback = {});

	std::optional<GLProgram> GetComputeProgram(const std::string_view glsl, const PreLinkCallback& callback = {});
	bool GetComputeProgram(GLProgram* out_program, const std::string_view glsl, const PreLinkCallback& callback = {});

private:
	struct CacheIndexKey
	{
		u64 vertex_source_hash_low;
		u64 vertex_source_hash_high;
		u32 vertex_source_length;
		u64 fragment_source_hash_low;
		u64 fragment_source_hash_high;
		u32 fragment_source_length;

		bool operator==(const CacheIndexKey& key) const;
		bool operator!=(const CacheIndexKey& key) const;
	};

	struct CacheIndexEntryHasher
	{
		std::size_t operator()(const CacheIndexKey& e) const noexcept
		{
			std::size_t h = 0;
			HashCombine(h, e.vertex_source_hash_low, e.vertex_source_hash_high, e.vertex_source_length,
				e.fragment_source_hash_low, e.fragment_source_hash_high, e.fragment_source_length);
			return h;
		}
	};

	struct CacheIndexData
	{
		u32 file_offset;
		u32 blob_size;
		u32 blob_format;
	};

	using CacheIndex = std::unordered_map<CacheIndexKey, CacheIndexData, CacheIndexEntryHasher>;

	static CacheIndexKey GetCacheKey(const std::string_view& vertex_shader, const std::string_view& fragment_shader);

	std::string GetIndexFileName() const;
	std::string GetBlobFileName() const;

	bool CreateNew(const std::string& index_filename, const std::string& blob_filename);
	bool ReadExisting(const std::string& index_filename, const std::string& blob_filename);
	bool Recreate();

	bool WriteToBlobFile(const CacheIndexKey& key, const std::vector<u8>& prog_data, u32 prog_format);

	std::optional<GLProgram> CompileProgram(const std::string_view& vertex_shader,
		const std::string_view& fragment_shader, const PreLinkCallback& callback, bool set_retrievable);
	std::optional<GLProgram> CompileAndAddProgram(const CacheIndexKey& key, const std::string_view& vertex_shader,
		const std::string_view& fragment_shader, const PreLinkCallback& callback);

	std::optional<GLProgram> CompileComputeProgram(
		const std::string_view& glsl, const PreLinkCallback& callback, bool set_retrievable);
	std::optional<GLProgram> CompileAndAddComputeProgram(
		const CacheIndexKey& key, const std::string_view& glsl, const PreLinkCallback& callback);

	std::FILE* m_index_file = nullptr;
	std::FILE* m_blob_file = nullptr;

	CacheIndex m_index;
	bool m_program_binary_supported = false;
};
