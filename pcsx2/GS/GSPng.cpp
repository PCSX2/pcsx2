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

#include "PrecompiledHeader.h"
#include "GSPng.h"
#include "GSExtra.h"
#include "common/FileSystem.h"
#include <zlib.h>
#include <png.h>

struct
{
	int type;
	int bytes_per_pixel_in;
	int bytes_per_pixel_out;
	int channel_bit_depth;
	const char *extension[2];
} static const pixel[GSPng::Format::COUNT] = {
	{PNG_COLOR_TYPE_RGBA, 4, 4, 8 , {"_full.png",     nullptr}},         // RGBA_PNG
	{PNG_COLOR_TYPE_RGB , 4, 3, 8 , {".png",          nullptr}},         // RGB_PNG
	{PNG_COLOR_TYPE_RGB , 4, 3, 8 , {".png",          "_alpha.png"}},    // RGB_A_PNG
	{PNG_COLOR_TYPE_GRAY, 4, 1, 8 , {"_alpha.png",    nullptr}},         // ALPHA_PNG
	{PNG_COLOR_TYPE_GRAY, 1, 1, 8 , {"_R8I.png",      nullptr}},         // R8I_PNG
	{PNG_COLOR_TYPE_GRAY, 2, 2, 16, {"_R16I.png",     nullptr}},         // R16I_PNG
	{PNG_COLOR_TYPE_GRAY, 4, 2, 16, {"_R32I_lsb.png", "_R32I_msb.png"}}, // R32I_PNG
};

namespace GSPng
{

	bool SaveFile(const std::string& file, const Format fmt, const u8* const image,
		u8* const row, const int width, const int height, const int pitch,
		const int compression, const bool rb_swapped = false, const bool first_image = false)
	{
		const int channel_bit_depth = pixel[fmt].channel_bit_depth;
		const int bytes_per_pixel_in = pixel[fmt].bytes_per_pixel_in;

		const int type = first_image ? pixel[fmt].type : PNG_COLOR_TYPE_GRAY;
		const int offset = first_image ? 0 : pixel[fmt].bytes_per_pixel_out;
		const int bytes_per_pixel_out = first_image ? pixel[fmt].bytes_per_pixel_out : bytes_per_pixel_in - offset;

		FILE* fp = FileSystem::OpenCFile(file.c_str(), "wb");
		if (fp == nullptr)
			return false;

		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		png_infop info_ptr = nullptr;

		bool success;
		try
		{
			if (png_ptr == nullptr)
				throw GSRecoverableError();

			info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr == nullptr)
				throw GSRecoverableError();

			if (setjmp(png_jmpbuf(png_ptr)))
				throw GSRecoverableError();

			png_init_io(png_ptr, fp);
			png_set_compression_level(png_ptr, compression);
			png_set_IHDR(png_ptr, info_ptr, width, height, channel_bit_depth, type,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			png_write_info(png_ptr, info_ptr);

			if (channel_bit_depth > 8)
				png_set_swap(png_ptr);
			if (rb_swapped && type != PNG_COLOR_TYPE_GRAY)
				png_set_bgr(png_ptr);

			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
					for (int i = 0; i < bytes_per_pixel_out; ++i)
						row[bytes_per_pixel_out * x + i] = image[y * pitch + bytes_per_pixel_in * x + i + offset];
				png_write_row(png_ptr, row);
			}
			png_write_end(png_ptr, nullptr);

			success = true;
		}
		catch (GSRecoverableError&)
		{
			fprintf(stderr, "Failed to write image %s\n", file.c_str());

			success = false;
		}

		if (png_ptr)
			png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : nullptr);
		fclose(fp);

		return success;
	}

	bool Save(GSPng::Format fmt, const std::string& file, u8* image, int w, int h, int pitch, int compression, bool rb_swapped)
	{
		std::string root = file;
		root.replace(file.length() - 4, 4, "");

		ASSERT(fmt >= Format::START && fmt < Format::COUNT);

		if (compression < 0 || compression > Z_BEST_COMPRESSION)
			compression = Z_BEST_SPEED;

		std::unique_ptr<u8[]> row(new u8[pixel[fmt].bytes_per_pixel_out * w]);

		std::string filename = root + pixel[fmt].extension[0];
		if (!SaveFile(filename, fmt, image, row.get(), w, h, pitch, compression, rb_swapped, true))
			return false;

		// Second image
		if (pixel[fmt].extension[1] == nullptr)
			return true;

		filename = root + pixel[fmt].extension[1];
		return SaveFile(filename, fmt, image, row.get(), w, h, pitch, compression);
	}

	Transaction::Transaction(GSPng::Format fmt, const std::string& file, const u8* image, int w, int h, int pitch, int compression)
		: m_fmt(fmt), m_file(file), m_w(w), m_h(h), m_pitch(pitch), m_compression(compression)
	{
		// Note: yes it would be better to use shared pointer
		m_image = (u8*)_aligned_malloc(pitch * h, 32);
		if (m_image)
			memcpy(m_image, image, pitch * h);
	}

	Transaction::~Transaction()
	{
		if (m_image)
			_aligned_free(m_image);
	}

	void Process(std::shared_ptr<Transaction>& item)
	{
		Save(item->m_fmt, item->m_file, item->m_image, item->m_w, item->m_h, item->m_pitch, item->m_compression);
	}

} // namespace GSPng
