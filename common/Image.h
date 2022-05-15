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
#include "Pcsx2Defs.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

namespace Common
{
	template <typename PixelType>
	class Image
	{
	public:
		Image() = default;
		Image(u32 width, u32 height, const PixelType* pixels) { SetPixels(width, height, pixels); }
		Image(const Image& copy)
		{
			m_width = copy.m_width;
			m_height = copy.m_height;
			m_pixels = copy.m_pixels;
		}
		Image(Image&& move)
		{
			m_width = move.m_width;
			m_height = move.m_height;
			m_pixels = std::move(move.m_pixels);
			move.m_width = 0;
			move.m_height = 0;
		}

		Image& operator=(const Image& copy)
		{
			m_width = copy.m_width;
			m_height = copy.m_height;
			m_pixels = copy.m_pixels;
			return *this;
		}
		Image& operator=(Image&& move)
		{
			m_width = move.m_width;
			m_height = move.m_height;
			m_pixels = std::move(move.m_pixels);
			move.m_width = 0;
			move.m_height = 0;
			return *this;
		}

		__fi bool IsValid() const { return (m_width > 0 && m_height > 0); }
		__fi u32 GetWidth() const { return m_width; }
		__fi u32 GetHeight() const { return m_height; }
		__fi u32 GetByteStride() const { return (sizeof(PixelType) * m_width); }
		__fi const PixelType* GetPixels() const { return m_pixels.data(); }
		__fi PixelType* GetPixels() { return m_pixels.data(); }
		__fi const PixelType* GetRowPixels(u32 y) const { return &m_pixels[y * m_width]; }
		__fi PixelType* GetRowPixels(u32 y) { return &m_pixels[y * m_width]; }
		__fi void SetPixel(u32 x, u32 y, PixelType pixel) { m_pixels[y * m_width + x] = pixel; }
		__fi PixelType GetPixel(u32 x, u32 y) const { return m_pixels[y * m_width + x]; }

		void Clear(PixelType fill_value = static_cast<PixelType>(0))
		{
			std::fill(m_pixels.begin(), m_pixels.end(), fill_value);
		}

		void Invalidate()
		{
			m_width = 0;
			m_height = 0;
			m_pixels.clear();
		}

		void SetSize(u32 new_width, u32 new_height, PixelType fill_value = static_cast<PixelType>(0))
		{
			m_width = new_width;
			m_height = new_height;
			m_pixels.resize(new_width * new_height);
			Clear(fill_value);
		}

		void SetPixels(u32 width, u32 height, const PixelType* pixels)
		{
			m_width = width;
			m_height = height;
			m_pixels.resize(width * height);
			std::memcpy(m_pixels.data(), pixels, width * height * sizeof(PixelType));
		}

		void SetPixels(u32 width, u32 height, std::vector<PixelType> pixels)
		{
			m_width = width;
			m_height = height;
			m_pixels = std::move(pixels);
		}

		std::vector<PixelType> TakePixels()
		{
			m_width = 0;
			m_height = 0;
			return std::move(m_pixels);
		}

	protected:
		u32 m_width = 0;
		u32 m_height = 0;
		std::vector<PixelType> m_pixels;
	};

	class RGBA8Image : public Image<u32>
	{
	public:
		static constexpr int DEFAULT_SAVE_QUALITY = 85;

		RGBA8Image();
		RGBA8Image(u32 width, u32 height, const u32* pixels);
		RGBA8Image(const RGBA8Image& copy);
		RGBA8Image(RGBA8Image&& move);

		RGBA8Image& operator=(const RGBA8Image& copy);
		RGBA8Image& operator=(RGBA8Image&& move);

		bool LoadFromFile(const char* filename);
		bool LoadFromFile(const char* filename, std::FILE* fp);
		bool LoadFromBuffer(const char* filename, const void* buffer, size_t buffer_size);

		bool SaveToFile(const char* filename, int quality = DEFAULT_SAVE_QUALITY) const;
		bool SaveToFile(const char* filename, std::FILE* fp, int quality = DEFAULT_SAVE_QUALITY) const;
		std::optional<std::vector<u8>> SaveToBuffer(const char* filename, int quality = DEFAULT_SAVE_QUALITY) const;
	};
} // namespace Common