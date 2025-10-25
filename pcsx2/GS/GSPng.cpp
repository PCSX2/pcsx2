// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSPng.h"
#include "GSExtra.h"
#include "common/FileSystem.h"
#include <zlib.h>

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

		auto fp = FileSystem::OpenManagedCFile(file.c_str(), "wb");
		if (!fp)
			return false;

		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		png_infop info_ptr = nullptr;

		if (png_ptr == nullptr)
			return false;

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == nullptr)
			return false;

		if (setjmp(png_jmpbuf(png_ptr)))
			return false;

		png_set_write_fn(
			png_ptr, fp.get(),
			[](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
				if (std::fwrite(data_ptr, size, 1, static_cast<std::FILE*>(png_get_io_ptr(png_ptr))) != 1)
					png_error(png_ptr, "file write error");
			},
			[](png_structp png_ptr) {});

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

		if (png_ptr)
			png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : nullptr);

		return true;
	}

	bool Save(GSPng::Format fmt, const std::string& file, const u8* image, int w, int h, int pitch, int compression, bool rb_swapped)
	{
		std::string root = file;
		root.replace(file.length() - 4, 4, "");

		pxAssert(fmt >= Format::START && fmt < Format::COUNT);

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
