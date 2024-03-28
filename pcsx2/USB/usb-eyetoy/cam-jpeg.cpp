// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "cam-jpeg.h"

#include "common/Console.h"

#include <jpeglib.h>

#include <csetjmp>

namespace
{
	struct JPEGErrorHandler
	{
		jpeg_error_mgr err;
		jmp_buf jbuf;
	};
} // namespace

static bool HandleJPEGError(JPEGErrorHandler* eh)
{
	jpeg_std_error(&eh->err);

	eh->err.error_exit = [](j_common_ptr cinfo) {
		JPEGErrorHandler* eh = (JPEGErrorHandler*)cinfo->err;
		char msg[JMSG_LENGTH_MAX];
		eh->err.format_message(cinfo, msg);
		Console.ErrorFmt("libjpeg fatal error: {}", msg);
		longjmp(eh->jbuf, 1);
	};

	if (setjmp(eh->jbuf) == 0)
		return true;

	return false;
}

bool CompressCamJPEG(std::vector<u8>* buffer, const u8* image, u32 width, u32 height, int quality)
{
	struct MemCallback
	{
		jpeg_destination_mgr mgr;
		std::vector<u8>* buffer;
		size_t buffer_used;
	};

	JPEGErrorHandler err;
	if (!HandleJPEGError(&err))
		return false;

	MemCallback cb;
	cb.buffer = buffer;
	cb.buffer_used = 0;
	cb.mgr.next_output_byte = buffer->data();
	cb.mgr.free_in_buffer = buffer->size();
	cb.mgr.init_destination = [](j_compress_ptr cinfo) {};
	cb.mgr.empty_output_buffer = [](j_compress_ptr cinfo) -> boolean {
		MemCallback* cb = (MemCallback*)cinfo->dest;

		// double size
		cb->buffer_used = cb->buffer->size();
		cb->buffer->resize(cb->buffer->size() * 2);
		cb->mgr.next_output_byte = cb->buffer->data() + cb->buffer_used;
		cb->mgr.free_in_buffer = cb->buffer->size() - cb->buffer_used;
		return TRUE;
	};
	cb.mgr.term_destination = [](j_compress_ptr cinfo) {
		MemCallback* cb = (MemCallback*)cinfo->dest;

		// get final size
		cb->buffer->resize(cb->buffer->size() - cb->mgr.free_in_buffer);
	};

	jpeg_compress_struct info;
	info.err = &err.err;
	jpeg_create_compress(&info);
	info.dest = &cb.mgr;

	info.image_width = width;
	info.image_height = height;
	info.in_color_space = JCS_RGB;
	info.input_components = 3;

	jpeg_set_defaults(&info);
	jpeg_set_quality(&info, quality, TRUE);

	// H2V1
	info.comp_info[0].h_samp_factor = 2;
	info.comp_info[0].v_samp_factor = 1;
	info.comp_info[1].h_samp_factor = 1;
	info.comp_info[1].v_samp_factor = 1;
	info.comp_info[2].h_samp_factor = 1;
	info.comp_info[2].v_samp_factor = 1;

	jpeg_start_compress(&info, TRUE);

	bool result = true;
	for (u32 y = 0; y < info.image_height; y++)
	{
		u8* scanline_buffer[1] = { const_cast<u8*>(image + (y * width * 3)) };
		if (jpeg_write_scanlines(&info, scanline_buffer, 1) != 1)
		{
			Console.ErrorFmt("jpeg_write_scanlines() failed at row {}", y);
			result = false;
			break;
		}
	}

	jpeg_finish_compress(&info);
	jpeg_destroy_compress(&info);
	return result;
}

bool DecompressCamJPEG(std::vector<u8>* buffer, u32* width, u32* height, const u8* data, size_t data_size)
{
	JPEGErrorHandler err;
	if (!HandleJPEGError(&err))
		return false;

	jpeg_decompress_struct info;
	info.err = &err.err;
	jpeg_create_decompress(&info);
	jpeg_mem_src(&info, static_cast<const unsigned char*>(data), data_size);

	const int herr = jpeg_read_header(&info, TRUE);
	if (herr != JPEG_HEADER_OK)
	{
		Console.ErrorFmt("jpeg_read_header() returned {}", herr);
		return false;
	}

	if (info.image_width == 0 || info.image_height == 0 || info.num_components < 3)
	{
		Console.ErrorFmt("Invalid image dimensions: {}x{}x{}", info.image_width, info.image_height, info.num_components);
		return false;
	}

	info.out_color_space = JCS_RGB;
	info.out_color_components = 3;

	if (!jpeg_start_decompress(&info))
	{
		Console.ErrorFmt("jpeg_start_decompress() returned failure");
		return false;
	}

	buffer->resize(info.image_width * info.image_height * 3);

	bool result = true;
	for (u32 y = 0; y < info.image_height; y++)
	{
		u8* scanline_buffer[1] = { buffer->data() + (y * info.image_width * 3) };
		if (jpeg_read_scanlines(&info, scanline_buffer, 1) != 1)
		{
			Console.ErrorFmt("jpeg_read_scanlines() failed at row {}", y);
			result = false;
			break;
		}
	}

	jpeg_finish_decompress(&info);
	jpeg_destroy_decompress(&info);
	return result;
}
