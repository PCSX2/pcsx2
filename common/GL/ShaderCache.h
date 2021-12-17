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
#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/GL/Program.h"
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GL
{
	class ShaderCache
	{
	public:
		using PreLinkCallback = std::function<void(Program&)>;

		ShaderCache();
		~ShaderCache();

		bool Open(bool is_gles, std::string_view base_path, u32 version);

		std::optional<Program> GetProgram(const std::string_view vertex_shader, const std::string_view geometry_shader,
			const std::string_view fragment_shader, const PreLinkCallback& callback = {});
		bool GetProgram(Program* out_program, const std::string_view vertex_shader, const std::string_view geometry_shader,
			const std::string_view fragment_shader, const PreLinkCallback& callback = {});

	private:
		static constexpr u32 FILE_VERSION = 1;

		struct CacheIndexKey
		{
			u64 vertex_source_hash_low;
			u64 vertex_source_hash_high;
			u32 vertex_source_length;
			u64 geometry_source_hash_low;
			u64 geometry_source_hash_high;
			u32 geometry_source_length;
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
				HashCombine(h,
					e.vertex_source_hash_low, e.vertex_source_hash_high, e.vertex_source_length,
					e.geometry_source_hash_low, e.geometry_source_hash_high, e.geometry_source_length,
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

		static CacheIndexKey GetCacheKey(const std::string_view& vertex_shader, const std::string_view& geometry_shader,
			const std::string_view& fragment_shader);

		std::string GetIndexFileName() const;
		std::string GetBlobFileName() const;

		bool CreateNew(const std::string& index_filename, const std::string& blob_filename);
		bool ReadExisting(const std::string& index_filename, const std::string& blob_filename);
		void Close();
		bool Recreate();

		std::optional<Program> CompileProgram(const std::string_view& vertex_shader, const std::string_view& geometry_shader,
			const std::string_view& fragment_shader, const PreLinkCallback& callback,
			bool set_retrievable);
		std::optional<Program> CompileAndAddProgram(const CacheIndexKey& key, const std::string_view& vertex_shader,
			const std::string_view& geometry_shader,
			const std::string_view& fragment_shader, const PreLinkCallback& callback);

		std::string m_base_path;
		std::FILE* m_index_file = nullptr;
		std::FILE* m_blob_file = nullptr;

		CacheIndex m_index;
		u32 m_version = 0;
		bool m_program_binary_supported = false;
	};
} // namespace GL
