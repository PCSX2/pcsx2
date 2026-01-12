// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Image.h"
#include "FileSystem.h"
#include "Console.h"
#include "Path.h"
#include "ScopedGuard.h"
#include "StringUtil.h"

#include <common/FastJmp.h>
#include <jpeglib.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/encode.h>

static bool PNGBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size);
static bool PNGBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality);
static bool PNGFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp);
static bool PNGFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality);

static bool JPEGBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size);
static bool JPEGBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality);
static bool JPEGFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp);
static bool JPEGFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality);

static bool WebPBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size);
static bool WebPBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality);
static bool WebPFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp);
static bool WebPFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality);

static bool BMPBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size);
static bool BMPBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality);
static bool BMPFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp);
static bool BMPFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality);

struct FormatHandler
{
	const char* extension;
	bool (*buffer_loader)(RGBA8Image*, const void*, size_t);
	bool (*buffer_saver)(const RGBA8Image&, std::vector<u8>*, u8);
	bool (*file_loader)(RGBA8Image*, const char*, std::FILE*);
	bool (*file_saver)(const RGBA8Image&, const char*, std::FILE*, u8);
};

static constexpr FormatHandler s_format_handlers[] = {
	{"png", PNGBufferLoader, PNGBufferSaver, PNGFileLoader, PNGFileSaver},
	{"jpg", JPEGBufferLoader, JPEGBufferSaver, JPEGFileLoader, JPEGFileSaver},
	{"jpeg", JPEGBufferLoader, JPEGBufferSaver, JPEGFileLoader, JPEGFileSaver},
	{"webp", WebPBufferLoader, WebPBufferSaver, WebPFileLoader, WebPFileSaver},
	{"bmp", BMPBufferLoader, BMPBufferSaver, BMPFileLoader, BMPFileSaver},
};

static const FormatHandler* GetFormatHandler(const std::string_view extension)
{
	for (const FormatHandler& handler : s_format_handlers)
	{
		if (StringUtil::Strncasecmp(extension.data(), handler.extension, extension.size()) == 0)
			return &handler;
	}

	return nullptr;
}

RGBA8Image::RGBA8Image() = default;

RGBA8Image::RGBA8Image(const RGBA8Image& copy)
	: Image(copy)
{
}

RGBA8Image::RGBA8Image(u32 width, u32 height, const u32* pixels)
	: Image(width, height, pixels)
{
}

RGBA8Image::RGBA8Image(RGBA8Image&& move)
	: Image(move)
{
}

RGBA8Image::RGBA8Image(u32 width, u32 height)
	: Image(width, height)
{
}

RGBA8Image::RGBA8Image(u32 width, u32 height, std::vector<u32> pixels)
	: Image(width, height, std::move(pixels))
{
}

RGBA8Image& RGBA8Image::operator=(const RGBA8Image& copy)
{
	Image<u32>::operator=(copy);
	return *this;
}

RGBA8Image& RGBA8Image::operator=(RGBA8Image&& move)
{
	Image<u32>::operator=(move);
	return *this;
}

bool RGBA8Image::LoadFromFile(const char* filename)
{
	auto fp = FileSystem::OpenManagedCFile(filename, "rb");
	if (!fp)
		return false;

	return LoadFromFile(filename, fp.get());
}

bool RGBA8Image::SaveToFile(const char* filename, u8 quality) const
{
	auto fp = FileSystem::OpenManagedCFile(filename, "wb");
	if (!fp)
		return false;

	if (SaveToFile(filename, fp.get(), quality))
		return true;

	// save failed
	fp.reset();
	FileSystem::DeleteFilePath(filename);
	return false;
}

bool RGBA8Image::LoadFromFile(const char* filename, std::FILE* fp)
{
	const std::string_view extension(Path::GetExtension(filename));
	const FormatHandler* handler = GetFormatHandler(extension);
	if (!handler || !handler->file_loader)
	{
		Console.ErrorFmt("Unknown extension '{}'", extension);
		return false;
	}

	return handler->file_loader(this, filename, fp);
}

bool RGBA8Image::LoadFromBuffer(const char* filename, const void* buffer, size_t buffer_size)
{
	const std::string_view extension(Path::GetExtension(filename));
	const FormatHandler* handler = GetFormatHandler(extension);
	if (!handler || !handler->buffer_loader)
	{
		Console.ErrorFmt("Unknown extension '{}'", extension);
		return false;
	}

	return handler->buffer_loader(this, buffer, buffer_size);
}

bool RGBA8Image::SaveToFile(const char* filename, std::FILE* fp, u8 quality) const
{
	const std::string_view extension(Path::GetExtension(filename));
	const FormatHandler* handler = GetFormatHandler(extension);
	if (!handler || !handler->file_saver)
	{
		Console.ErrorFmt("Unknown extension '{}'", extension);
		return false;
	}

	if (!handler->file_saver(*this, filename, fp, quality))
		return false;

	return (std::fflush(fp) == 0);
}

std::optional<std::vector<u8>> RGBA8Image::SaveToBuffer(const char* filename, u8 quality) const
{
	std::optional<std::vector<u8>> ret;

	const std::string_view extension(Path::GetExtension(filename));
	const FormatHandler* handler = GetFormatHandler(extension);
	if (!handler || !handler->file_saver)
	{
		Console.ErrorFmt("Unknown extension '{}'", extension);
		return ret;
	}

	ret = std::vector<u8>();
	if (!handler->buffer_saver(*this, &ret.value(), quality))
		ret.reset();

	return ret;
}

static bool PNGCommonLoader(RGBA8Image* image, png_structp png_ptr, png_infop info_ptr, std::vector<u32>& new_data,
	std::vector<png_bytep>& row_pointers)
{
	png_read_info(png_ptr, info_ptr);

	const u32 width = png_get_image_width(png_ptr, info_ptr);
	const u32 height = png_get_image_height(png_ptr, info_ptr);
	const png_byte color_type = png_get_color_type(png_ptr, info_ptr);
	const png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if (bit_depth == 16)
		png_set_strip_16(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png_ptr);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	new_data.resize(width * height);
	row_pointers.reserve(height);
	for (u32 y = 0; y < height; y++)
		row_pointers.push_back(reinterpret_cast<png_bytep>(new_data.data() + y * width));

	png_read_image(png_ptr, row_pointers.data());
	image->SetPixels(width, height, std::move(new_data));
	return true;
}

bool PNGFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png_ptr)
		return false;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return false;
	}

	ScopedGuard cleanup([&png_ptr, &info_ptr]() { png_destroy_read_struct(&png_ptr, &info_ptr, nullptr); });

	std::vector<u32> new_data;
	std::vector<png_bytep> row_pointers;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_set_read_fn(png_ptr, fp, [](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
		std::FILE* fp = static_cast<std::FILE*>(png_get_io_ptr(png_ptr));
		if (std::fread(data_ptr, size, 1, fp) != 1)
			png_error(png_ptr, "Read error");
	});

	return PNGCommonLoader(image, png_ptr, info_ptr, new_data, row_pointers);
}

bool PNGBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png_ptr)
		return false;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return false;
	}

	ScopedGuard cleanup([&png_ptr, &info_ptr]() { png_destroy_read_struct(&png_ptr, &info_ptr, nullptr); });

	std::vector<u32> new_data;
	std::vector<png_bytep> row_pointers;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	struct IOData
	{
		const u8* buffer;
		size_t buffer_size;
		size_t buffer_pos;
	};
	IOData data = {static_cast<const u8*>(buffer), buffer_size, 0};

	png_set_read_fn(png_ptr, &data, [](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
		IOData* data = static_cast<IOData*>(png_get_io_ptr(png_ptr));
		const size_t read_size = std::min<size_t>(data->buffer_size - data->buffer_pos, size);
		if (read_size > 0)
		{
			std::memcpy(data_ptr, data->buffer + data->buffer_pos, read_size);
			data->buffer_pos += read_size;
		}
	});

	return PNGCommonLoader(image, png_ptr, info_ptr, new_data, row_pointers);
}

static void PNGSaveCommon(const RGBA8Image& image, png_structp png_ptr, png_infop info_ptr, u8 quality)
{
	png_set_compression_level(png_ptr, std::clamp(quality / 10, 0, 9));
	png_set_IHDR(png_ptr, info_ptr, image.GetWidth(), image.GetHeight(), 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	for (u32 y = 0; y < image.GetHeight(); ++y)
		png_write_row(png_ptr, (png_bytep)image.GetRowPixels(y));

	png_write_end(png_ptr, nullptr);
}

bool PNGFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality)
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info_ptr = nullptr;
	if (!png_ptr)
		return false;

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		if (png_ptr)
			png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : nullptr);
	});

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return false;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_set_write_fn(
		png_ptr, fp,
		[](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
			if (std::fwrite(data_ptr, size, 1, static_cast<std::FILE*>(png_get_io_ptr(png_ptr))) != 1)
				png_error(png_ptr, "file write error");
		},
		[](png_structp png_ptr) {});

	PNGSaveCommon(image, png_ptr, info_ptr, quality);
	return true;
}

bool PNGBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality)
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info_ptr = nullptr;
	if (!png_ptr)
		return false;

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		if (png_ptr)
			png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : nullptr);
	});

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return false;

	buffer->reserve(image.GetWidth() * image.GetHeight() * 2);

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_set_write_fn(
		png_ptr, buffer,
		[](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
			std::vector<u8>* buffer = static_cast<std::vector<u8>*>(png_get_io_ptr(png_ptr));
			const size_t old_pos = buffer->size();
			buffer->resize(old_pos + size);
			std::memcpy(buffer->data() + old_pos, data_ptr, size);
		},
		[](png_structp png_ptr) {});

	PNGSaveCommon(image, png_ptr, info_ptr, quality);
	return true;
}

namespace
{
	struct JPEGErrorHandler
	{
		jpeg_error_mgr err;
		fastjmp_buf jbuf;

		JPEGErrorHandler()
		{
			jpeg_std_error(&err);
			err.error_exit = &ErrorExit;
		}

		static void ErrorExit(j_common_ptr cinfo)
		{
			JPEGErrorHandler* eh = (JPEGErrorHandler*)cinfo->err;
			char msg[JMSG_LENGTH_MAX];
			eh->err.format_message(cinfo, msg);
			Console.ErrorFmt("libjpeg fatal error: {}", msg);
			fastjmp_jmp(&eh->jbuf, 1);
		}
	};
} // namespace

template <typename T>
static bool WrapJPEGDecompress(RGBA8Image* image, T setup_func)
{
	std::vector<u8> scanline;
	jpeg_decompress_struct info = {};

	// NOTE: Be **very** careful not to allocate memory after calling this function.
	// It won't get freed, because fastjmp does not unwind the stack.
	JPEGErrorHandler errhandler;
	if (fastjmp_set(&errhandler.jbuf) != 0)
	{
		jpeg_destroy_decompress(&info);
		return false;
	}
	info.err = &errhandler.err;
	jpeg_create_decompress(&info);
	setup_func(info);

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

	image->SetSize(info.image_width, info.image_height);
	scanline.resize(info.image_width * 3);

	u8* scanline_buffer[1] = {scanline.data()};
	bool result = true;
	for (u32 y = 0; y < info.image_height; y++)
	{
		if (jpeg_read_scanlines(&info, scanline_buffer, 1) != 1)
		{
			Console.ErrorFmt("jpeg_read_scanlines() failed at row {}", y);
			result = false;
			break;
		}

		// RGB -> RGBA
		const u8* src_ptr = scanline.data();
		u32* dst_ptr = image->GetRowPixels(y);
		for (u32 x = 0; x < info.image_width; x++)
		{
			*(dst_ptr++) = (static_cast<u32>(src_ptr[0]) | (static_cast<u32>(src_ptr[1]) << 8) | (static_cast<u32>(src_ptr[2]) << 16) | 0xFF000000u);
			src_ptr += 3;
		}
	}

	jpeg_finish_decompress(&info);
	jpeg_destroy_decompress(&info);
	return result;
}

bool JPEGBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size)
{
	return WrapJPEGDecompress(image, [buffer, buffer_size](jpeg_decompress_struct& info) {
		jpeg_mem_src(&info, static_cast<const unsigned char*>(buffer), buffer_size);
	});
}

bool JPEGFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp)
{
	static constexpr u32 BUFFER_SIZE = 16384;

	struct FileCallback
	{
		// Must be the first member (&this == &mgr)
		// We pass a pointer of mgr to libjpeg, and we need to be able to cast it back to FileCallback.
		jpeg_source_mgr mgr;

		std::FILE* fp;
		std::unique_ptr<u8[]> buffer;
		bool end_of_file;
	};

	FileCallback cb = {
		.mgr = {
			.init_source = [](j_decompress_ptr cinfo) {},
			.fill_input_buffer = [](j_decompress_ptr cinfo) -> boolean {
				FileCallback* cb = reinterpret_cast<FileCallback*>(cinfo->src);
				cb->mgr.next_input_byte = cb->buffer.get();
				if (cb->end_of_file)
				{
					cb->buffer[0] = 0xFF;
					cb->buffer[1] = JPEG_EOI;
					cb->mgr.bytes_in_buffer = 2;
					return TRUE;
				}

				const size_t r = std::fread(cb->buffer.get(), 1, BUFFER_SIZE, cb->fp);
				cb->end_of_file |= (std::feof(cb->fp) != 0);
				cb->mgr.bytes_in_buffer = r;
				return TRUE;
			},
			.skip_input_data =
				[](j_decompress_ptr cinfo, long num_bytes) {
					FileCallback* cb = reinterpret_cast<FileCallback*>(cinfo->src);
					const size_t skip_in_buffer = std::min<size_t>(cb->mgr.bytes_in_buffer, static_cast<size_t>(num_bytes));
					cb->mgr.next_input_byte += skip_in_buffer;
					cb->mgr.bytes_in_buffer -= skip_in_buffer;

					const size_t seek_cur = static_cast<size_t>(num_bytes) - skip_in_buffer;
					if (seek_cur > 0)
					{
						if (FileSystem::FSeek64(cb->fp, static_cast<size_t>(seek_cur), SEEK_CUR) != 0)
						{
							cb->end_of_file = true;
							return;
						}
					}
				},
			.resync_to_restart = jpeg_resync_to_restart,
			.term_source = [](j_decompress_ptr cinfo) {},
		},
		.fp = fp,
		.buffer = std::make_unique<u8[]>(BUFFER_SIZE),
		.end_of_file = false,
	};

	return WrapJPEGDecompress(image, [&cb](jpeg_decompress_struct& info) { info.src = &cb.mgr; });
}

template <typename T>
static bool WrapJPEGCompress(const RGBA8Image& image, u8 quality, T setup_func)
{
	std::vector<u8> scanline;
	jpeg_compress_struct info = {};

	// NOTE: Be **very** careful not to allocate memory after calling this function.
	// It won't get freed, because fastjmp does not unwind the stack.
	JPEGErrorHandler errhandler;
	if (fastjmp_set(&errhandler.jbuf) != 0)
	{
		jpeg_destroy_compress(&info);
		return false;
	}
	info.err = &errhandler.err;
	jpeg_create_compress(&info);
	setup_func(info);

	info.image_width = image.GetWidth();
	info.image_height = image.GetHeight();
	info.in_color_space = JCS_RGB;
	info.input_components = 3;

	jpeg_set_defaults(&info);
	jpeg_set_quality(&info, quality, TRUE);
	jpeg_start_compress(&info, TRUE);

	scanline.resize(image.GetWidth() * 3);
	u8* scanline_buffer[1] = {scanline.data()};
	bool result = true;
	for (u32 y = 0; y < info.image_height; y++)
	{
		// RGBA -> RGB
		u8* dst_ptr = scanline.data();
		const u32* src_ptr = image.GetRowPixels(y);
		for (u32 x = 0; x < info.image_width; x++)
		{
			const u32 rgba = *(src_ptr++);
			*(dst_ptr++) = static_cast<u8>(rgba);
			*(dst_ptr++) = static_cast<u8>(rgba >> 8);
			*(dst_ptr++) = static_cast<u8>(rgba >> 16);
		}

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

bool JPEGBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality)
{
	// give enough space to avoid reallocs
	buffer->resize(image.GetWidth() * image.GetHeight() * 2);

	struct MemCallback
	{
		jpeg_destination_mgr mgr;
		std::vector<u8>* buffer;
		size_t buffer_used;
	};

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

	return WrapJPEGCompress(image, quality, [&cb](jpeg_compress_struct& info) { info.dest = &cb.mgr; });
}

bool JPEGFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality)
{
	static constexpr u32 BUFFER_SIZE = 16384;

	struct FileCallback
	{
		jpeg_destination_mgr mgr;

		std::FILE* fp;
		std::unique_ptr<u8[]> buffer;
		bool write_error;
	};

	FileCallback cb = {
		.mgr = {
			.init_destination =
				[](j_compress_ptr cinfo) {
					FileCallback* cb = reinterpret_cast<FileCallback*>(cinfo->dest);
					cb->mgr.next_output_byte = cb->buffer.get();
					cb->mgr.free_in_buffer = BUFFER_SIZE;
				},
			.empty_output_buffer = [](j_compress_ptr cinfo) -> boolean {
				FileCallback* cb = reinterpret_cast<FileCallback*>(cinfo->dest);
				if (!cb->write_error)
					cb->write_error |= (std::fwrite(cb->buffer.get(), 1, BUFFER_SIZE, cb->fp) != BUFFER_SIZE);

				cb->mgr.next_output_byte = cb->buffer.get();
				cb->mgr.free_in_buffer = BUFFER_SIZE;
				return TRUE;
			},
			.term_destination =
				[](j_compress_ptr cinfo) {
					FileCallback* cb = reinterpret_cast<FileCallback*>(cinfo->dest);
					const size_t left = BUFFER_SIZE - cb->mgr.free_in_buffer;
					if (left > 0 && !cb->write_error)
						cb->write_error |= (std::fwrite(cb->buffer.get(), 1, left, cb->fp) != left);
				},
		},
		.fp = fp,
		.buffer = std::make_unique<u8[]>(BUFFER_SIZE),
		.write_error = false,
	};

	return (WrapJPEGCompress(image, quality, [&cb](jpeg_compress_struct& info) { info.dest = &cb.mgr; }) &&
			!cb.write_error);
}

bool WebPBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size)
{
	int width, height;
	if (!WebPGetInfo(static_cast<const u8*>(buffer), buffer_size, &width, &height) || width <= 0 || height <= 0)
	{
		Console.Error("WebPGetInfo() failed");
		return false;
	}

	std::vector<u32> pixels;
	pixels.resize(static_cast<u32>(width) * static_cast<u32>(height));
	if (!WebPDecodeRGBAInto(static_cast<const u8*>(buffer), buffer_size, reinterpret_cast<u8*>(pixels.data()),
			sizeof(u32) * pixels.size(), sizeof(u32) * static_cast<u32>(width)))
	{
		Console.Error("WebPDecodeRGBAInto() failed");
		return false;
	}

	image->SetPixels(static_cast<u32>(width), static_cast<u32>(height), std::move(pixels));
	return true;
}

bool WebPBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality)
{
	u8* encoded_data;
	const size_t encoded_size =
		WebPEncodeRGBA(reinterpret_cast<const u8*>(image.GetPixels()), image.GetWidth(), image.GetHeight(),
			image.GetPitch(), static_cast<float>(quality), &encoded_data);
	if (encoded_size == 0)
		return false;

	buffer->resize(encoded_size);
	std::memcpy(buffer->data(), encoded_data, encoded_size);
	WebPFree(encoded_data);
	return true;
}

bool WebPFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp)
{
	std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(fp);
	if (!data.has_value())
		return false;

	return WebPBufferLoader(image, data->data(), data->size());
}

bool WebPFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality)
{
	std::vector<u8> buffer;
	if (!WebPBufferSaver(image, &buffer, quality))
		return false;

	return (std::fwrite(buffer.data(), buffer.size(), 1, fp) == 1);
}

// Some of this code is adapted from Qt's BMP handler (https://github.com/qt/qtbase/blob/dev/src/gui/image/qbmphandler.cpp)
#pragma pack(push, 1)
struct BMPFileHeader
{
	u16 type;
	u32 size;
	u16 reserved1;
	u16 reserved2;
	u32 offset;
};

struct BMPInfoHeader
{
	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bit_count;
	u32 compression;
	u32 size_image;
	s32 x_pels_per_meter;
	s32 y_pels_per_meter;
	u32 clr_used;
	u32 clr_important;
};
#pragma pack(pop)

bool IsSupportedBMPFormat(u32 compression, u16 bit_count)
{
	if (compression == 0)
		return (bit_count == 1 || bit_count == 4 || bit_count == 8 || bit_count == 16 || bit_count == 24 || bit_count == 32);

	if (compression == 1)
		return (bit_count == 8);

	if (compression == 2)
		return (bit_count == 4);

	if (compression == 3 || compression == 4) // BMP_BITFIELDS or BMP_ALPHABITFIELDS
		return (bit_count == 16 || bit_count == 32);

	return false;
}

bool LoadBMPPalette(std::vector<u32>& palette, const u8* data, u32 palette_offset, const BMPInfoHeader& info_header)
{
	// 1 bit format doesn't use a palette in the traditional sense
	if (info_header.bit_count == 1)
	{
		palette = {0xFFFFFFFF, 0xFF000000};
		return true;
	}

	const u32 num_colors = (info_header.clr_used > 0) ? info_header.clr_used : (1u << info_header.bit_count);

	// Make sure that we don't have an unreasonably large palette
	if (num_colors > 256)
	{
		Console.Error("Invalid palette size: %u", num_colors);
		return false;
	}

	palette.clear();
	palette.reserve(num_colors);

	const u8* palette_data = data + sizeof(BMPFileHeader) + info_header.size;

	for (u32 i = 0; i < num_colors; i++)
	{
		const u8* color = palette_data + (i * 4);
		const u8 b = color[0];
		const u8 g = color[1];
		const u8 r = color[2];
		palette.push_back(r | (g << 8) | (b << 16) | 0xFF000000u);
	}

	return true;
}

bool LoadUncompressedBMP(u32* pixels, const u8* src, const u8* data, u32 width, u32 height, const BMPInfoHeader& info_header, const std::vector<u32>& palette, bool flip_vertical, u32 red_mask = 0, u32 green_mask = 0, u32 blue_mask = 0, u32 alpha_mask = 0, bool use_alpha = false)
{
	const u32 row_size = ((width * info_header.bit_count + 31) / 32) * 4;

	for (u32 y = 0; y < height; y++)
	{
		u32 dst_y = flip_vertical ? (height - 1 - y) : y;
		const u8* row_src = src + (y * row_size);
		u32* row_dst = pixels + (dst_y * width);

		u32 bit_offset = 0;

		for (u32 x = 0; x < width; x++)
		{
			u32 pixel_value = 0;

			switch (info_header.bit_count)
			{
				case 1:
				{
					const u32 byte_index = bit_offset / 8;
					const u32 bit_index = 7 - (bit_offset % 8);
					pixel_value = (row_src[byte_index] >> bit_index) & 1;
					bit_offset += 1;
					break;
				}
				case 4:
				{
					const u32 byte_index = bit_offset / 8;
					const u32 nibble_index = (bit_offset % 8) / 4;
					pixel_value = (row_src[byte_index] >> (nibble_index * 4)) & 0xF;
					bit_offset += 4;
					break;
				}
				case 8:
				{
					pixel_value = row_src[bit_offset / 8];
					bit_offset += 8;
					break;
				}
				case 16:
				{
					const u32 byte_index = bit_offset / 8;
					pixel_value = row_src[byte_index] | (row_src[byte_index + 1] << 8);
					bit_offset += 16;

					if (info_header.compression == 3)
					{
						const u8* bitfields = data + sizeof(BMPFileHeader) + info_header.size;
						const u32 r_mask = *reinterpret_cast<const u32*>(bitfields);
						const u32 g_mask = *reinterpret_cast<const u32*>(bitfields + 4);
						const u32 b_mask = *reinterpret_cast<const u32*>(bitfields + 8);

						u32 r_shift = 0, g_shift = 0, b_shift = 0;
						u32 temp = r_mask;
						while (temp >>= 1)
							r_shift++;
						temp = g_mask;
						while (temp >>= 1)
							g_shift++;
						temp = b_mask;
						while (temp >>= 1)
							b_shift++;

						const u8 r = static_cast<u8>((pixel_value & r_mask) >> r_shift);
						const u8 g = static_cast<u8>((pixel_value & g_mask) >> g_shift);
						const u8 b = static_cast<u8>((pixel_value & b_mask) >> b_shift);

						const u8 r_max = static_cast<u8>(r_mask >> r_shift);
						const u8 g_max = static_cast<u8>(g_mask >> g_shift);
						const u8 b_max = static_cast<u8>(b_mask >> b_shift);

						const u8 r_scaled = (r_max > 0) ? static_cast<u8>((r * 255) / r_max) : 0;
						const u8 g_scaled = (g_max > 0) ? static_cast<u8>((g * 255) / g_max) : 0;
						const u8 b_scaled = (b_max > 0) ? static_cast<u8>((b * 255) / b_max) : 0;

						row_dst[x] = r_scaled | (g_scaled << 8) | (b_scaled << 16) | 0xFF000000u;
					}
					else
					{
						const u8 r = (pixel_value >> 10) & 0x1F;
						const u8 g = (pixel_value >> 5) & 0x1F;
						const u8 b = pixel_value & 0x1F;
						row_dst[x] = (r << 3) | (g << 11) | (b << 19) | 0xFF000000u;
					}
					continue;
				}
				case 24:
				{
					const u32 byte_index = bit_offset / 8;
					const u8 b = row_src[byte_index + 0];
					const u8 g = row_src[byte_index + 1];
					const u8 r = row_src[byte_index + 2];
					row_dst[x] = r | (g << 8) | (b << 16) | 0xFF000000u;
					bit_offset += 24;
					continue;
				}
				case 32:
				{
					const u32 byte_index = bit_offset / 8;
					u32 pixel_value = row_src[byte_index] | (row_src[byte_index + 1] << 8) | (row_src[byte_index + 2] << 16) | (row_src[byte_index + 3] << 24);
					bit_offset += 32;

					if (info_header.compression == 3 || info_header.compression == 4) // BITFIELDS or ALPHABITFIELDS
					{
						// Calculate shifts
						auto calc_shift = [](u32 mask) -> u32 {
							u32 result = 0;
							while ((mask >= 0x100) || (!(mask & 1) && mask))
							{
								result++;
								mask >>= 1;
							}
							return result;
						};

						// Calculate scales
						auto calc_scale = [](u32 low_mask) -> u32 {
							u32 result = 8;
							while (low_mask && result)
							{
								result--;
								low_mask >>= 1;
							}
							return result;
						};

						// Apply scale
						auto apply_scale = [](u32 value, u32 scale) -> u8 {
							if (!(scale & 0x07)) // scale == 8 or 0
								return static_cast<u8>(value);
							u32 filled = 8 - scale;
							u32 result = value << scale;
							do
							{
								result |= result >> filled;
								filled <<= 1;
							} while (filled < 8);
							return static_cast<u8>(result);
						};

						const u32 r_shift = calc_shift(red_mask);
						const u32 g_shift = calc_shift(green_mask);
						const u32 b_shift = calc_shift(blue_mask);
						const u32 a_shift = (alpha_mask != 0) ? calc_shift(alpha_mask) : 0;

						const u32 r_scale = calc_scale(red_mask >> r_shift);
						const u32 g_scale = calc_scale(green_mask >> g_shift);
						const u32 b_scale = calc_scale(blue_mask >> b_shift);
						const u32 a_scale = (alpha_mask != 0) ? calc_scale(alpha_mask >> a_shift) : 0;

						const u8 r = apply_scale((pixel_value & red_mask) >> r_shift, r_scale);
						const u8 g = apply_scale((pixel_value & green_mask) >> g_shift, g_scale);
						const u8 b = apply_scale((pixel_value & blue_mask) >> b_shift, b_scale);
						const u8 a = (use_alpha && alpha_mask != 0) ? apply_scale((pixel_value & alpha_mask) >> a_shift, a_scale) : 0xFF;

						row_dst[x] = r | (g << 8) | (b << 16) | (a << 24);
					}
					else
					{
						// Uncompressed 32-bit BGRA order
						const u8 b = row_src[byte_index + 0];
						const u8 g = row_src[byte_index + 1];
						const u8 r = row_src[byte_index + 2];
						const u8 a = row_src[byte_index + 3];
						row_dst[x] = r | (g << 8) | (b << 16) | (a << 24);
					}
					continue;
				}
			}
			if (info_header.bit_count <= 8)
			{
				if (pixel_value < palette.size())
					row_dst[x] = palette[pixel_value];
				else
				{
					Console.Error("Invalid palette index: %u (palette size: %zu)", pixel_value, palette.size());
					return false;
				}
			}
		}
	}
	return true;
}

bool LoadCompressedBMP(u32* pixels, const u8* src, u32 src_size, u32 width, u32 height, const BMPInfoHeader& info_header, const std::vector<u32>& palette, bool flip_vertical)
{
	u32 src_pos = 0;
	const u32 pixel_size = (info_header.bit_count == 8) ? 1 : 2;

	for (u32 y = 0; y < height; y++)
	{
		u32 dst_y = flip_vertical ? (height - 1 - y) : y;
		u32* row_dst = pixels + (dst_y * width);
		u32 x = 0;

		while (x < width)
		{
			// Check bounds before reading
			if (src_pos + 2 > src_size)
				return false;

			const u8 count = src[src_pos++];
			const u8 value = src[src_pos++];

			if (count == 0)
			{
				if (value == 0)
				{
					break;
				}
				else if (value == 1)
				{
					return true;
				}
				else if (value == 2)
				{
					// Delta (jump) need 2 more bytes
					if (src_pos + 2 > src_size)
						return false;
					const u8 dx = src[src_pos++];
					const u8 dy = src[src_pos++];
					x += dx;
					y += dy;
					if (y >= height || x >= width)
						return false;
					const u32 new_dst_y = flip_vertical ? (height - 1 - y) : y;
					row_dst = pixels + (new_dst_y * width);
				}
				else
				{
					// Absolute mode need "value" bytes of pixel data
					const u32 run_length = value;
					const u32 bytes_needed = run_length * pixel_size;
					if (src_pos + bytes_needed > src_size)
						return false;

					for (u32 i = 0; i < run_length; i++)
					{
						if (x >= width)
							break;

						u8 pixel_value = 0;
						if (info_header.bit_count == 8)
						{
							pixel_value = src[src_pos++];
						}
						else
						{
							const u8 byte_val = src[src_pos++];
							pixel_value = (i % 2 == 0) ? (byte_val >> 4) : (byte_val & 0x0F);
						}

						row_dst[x++] = (pixel_value < palette.size()) ? palette[pixel_value] : 0;
					}

					if ((run_length * pixel_size) % 2 == 1)
						src_pos++;
				}
			}
			else
			{
				u8 pixel_value = value;

				for (u32 i = 0; i < count; i++)
				{
					if (x >= width)
						break;
					row_dst[x++] = (pixel_value < palette.size()) ? palette[pixel_value] : 0;
				}
			}
		}
	}

	return true;
}

bool BMPBufferLoader(RGBA8Image* image, const void* buffer, size_t buffer_size)
{
	if (buffer_size < sizeof(BMPFileHeader) + sizeof(BMPInfoHeader))
	{
		Console.Error("BMP file too small");
		return false;
	}

	const u8* data = static_cast<const u8*>(buffer);
	BMPFileHeader file_header;
	BMPInfoHeader info_header;

	std::memcpy(&file_header, data, sizeof(BMPFileHeader));
	std::memcpy(&info_header, data + sizeof(BMPFileHeader), sizeof(BMPInfoHeader));

	if (file_header.type != 0x4D42)
	{
		Console.Error("Invalid BMP signature");
		return false;
	}

	// Check for extended header versions (V4=108 bytes, V5=124 bytes)
	// We read as BITMAPINFOHEADER (40 bytes) regardless, since extended headers just add fields at the end
	if (info_header.size == 108)
	{
		Console.Warning("BITMAPV4HEADER detected, reading as BITMAPINFOHEADER");
	}
	else if (info_header.size == 124)
	{
		Console.Warning("BITMAPV5HEADER detected, reading as BITMAPINFOHEADER");
	}
	else if (info_header.size != 40)
	{
		Console.Warning("Unknown BMP header size: %u, attempting to read as BITMAPINFOHEADER", info_header.size);
	}

	if (!IsSupportedBMPFormat(info_header.compression, info_header.bit_count))
	{
		Console.Error("Unsupported BMP format: compression=%u, bit_count=%u", info_header.compression, info_header.bit_count);
		return false;
	}

	const u32 width = static_cast<u32>(std::abs(info_header.width));
	const u32 height = static_cast<u32>(std::abs(info_header.height));
	const bool flip_vertical = (info_header.height > 0);

	if (width == 0 || height == 0)
	{
		Console.Error("Invalid BMP dimensions: %ux%u", width, height);
		return false;
	}

	if (width >= 65536 || height >= 65536)
	{
		Console.Error("BMP dimensions too large: %ux%u", width, height);
		return false;
	}

	Console.WriteLn("BMP: %ux%u, %u-bit, compression=%u", width, height, info_header.bit_count, info_header.compression);

	// Read color masks from header or bitfields
	u32 red_mask = 0;
	u32 green_mask = 0;
	u32 blue_mask = 0;
	u32 alpha_mask = 0;
	const bool bitfields = (info_header.compression == 3 || info_header.compression == 4); // BMP_BITFIELDS or BMP_ALPHABITFIELDS
	const u8* header_start = data + sizeof(BMPFileHeader);
	const u32 header_base_offset = sizeof(BMPFileHeader) + 40; // Base header is 40 bytes

	if (info_header.size >= 108) // BMP_WIN4 (108) or BMP_WIN5 (124)
	{
		// V4/V5 headers masks come right after the 40-byte base header
		// Masks are at offsets from header_start: red=40, green=44, blue=48, alpha=52
		if (buffer_size >= header_base_offset + 16) // Need space for 4 masks
		{
			red_mask = *reinterpret_cast<const u32*>(header_start + 40);
			green_mask = *reinterpret_cast<const u32*>(header_start + 44);
			blue_mask = *reinterpret_cast<const u32*>(header_start + 48);
			alpha_mask = *reinterpret_cast<const u32*>(header_start + 52);
		}
	}
	else if (bitfields && (info_header.bit_count == 16 || info_header.bit_count == 32))
	{
		const u32 bitfields_offset = sizeof(BMPFileHeader) + info_header.size;
		if (buffer_size >= bitfields_offset + 12) // Need space for at least r/g/b masks
		{
			red_mask = *reinterpret_cast<const u32*>(data + bitfields_offset);
			green_mask = *reinterpret_cast<const u32*>(data + bitfields_offset + 4);
			blue_mask = *reinterpret_cast<const u32*>(data + bitfields_offset + 8);
			if (info_header.compression == 4) // BMP_ALPHABITFIELDS
			{
				// Read alpha mask: r, g, b, a
				if (buffer_size >= bitfields_offset + 16)
					alpha_mask = *reinterpret_cast<const u32*>(data + bitfields_offset + 12);
			}
			// For BMP_BITFIELDS (3), alpha_mask stays 0
		}
	}

	bool use_alpha = bitfields || (info_header.compression == 0 && info_header.bit_count == 32 && alpha_mask == 0xff000000);
	use_alpha = use_alpha && (alpha_mask != 0);

	const u32 bytes_per_pixel = info_header.bit_count / 8;
	const u32 row_size = ((width * bytes_per_pixel + 3) / 4) * 4;

	// For uncompressed BMPs, verify we have enough data
	// For RLE-compressed BMPs, size is variable so we check differently
	if (info_header.compression == 0)
	{
		if (file_header.offset + (row_size * height) > buffer_size)
		{
			Console.Error("BMP file data incomplete");
			return false;
		}
	}
	else
	{
		// For RLE-compressed BMPs, check that we have at least the offset and some data
		// Use biSizeImage if available, otherwise just verify offset is valid
		if (file_header.offset >= buffer_size)
		{
			Console.Error("BMP file data incomplete");
			return false;
		}
		if (info_header.size_image > 0)
		{
			if (file_header.offset + info_header.size_image > buffer_size)
			{
				Console.Error("BMP file data incomplete");
				return false;
			}
		}
	}

	std::vector<u32> pixels;
	pixels.resize(width * height);

	const u8* src = data + file_header.offset;
	const u32 src_size = buffer_size - file_header.offset;

	std::vector<u32> palette;
	if (info_header.bit_count <= 8)
	{
		if (!LoadBMPPalette(palette, data, file_header.offset, info_header))
		{
			Console.Error("Failed to load BMP palette");
			return false;
		}
	}

	if (info_header.compression == 0 || info_header.compression == 3 || info_header.compression == 4)
	{
		if (!LoadUncompressedBMP(pixels.data(), src, data, width, height, info_header, palette, flip_vertical, red_mask, green_mask, blue_mask, alpha_mask, use_alpha))
		{
			Console.Error("Failed to load uncompressed BMP data");
			return false;
		}
	}
	else
	{
		if (!LoadCompressedBMP(pixels.data(), src, src_size, width, height, info_header, palette, flip_vertical))
		{
			Console.Error("Failed to load compressed BMP data");
			return false;
		}
	}

	// Handle alpha channel for 32-bit BMPs
	// Only use alpha if alpha_mask is explicitly set in header/bitfields
	if (info_header.bit_count == 32 && !use_alpha)
	{
		// Alpha mask not set or zero - set all pixels to fully opaque
		for (u32& pixel : pixels)
			pixel |= 0xFF000000u;
	}

	image->SetPixels(width, height, std::move(pixels));
	return true;
}

bool BMPFileLoader(RGBA8Image* image, const char* filename, std::FILE* fp)
{
	std::optional<std::vector<u8>> data = FileSystem::ReadBinaryFile(fp);
	if (!data.has_value())
		return false;

	return BMPBufferLoader(image, data->data(), data->size());
}

bool BMPBufferSaver(const RGBA8Image& image, std::vector<u8>* buffer, u8 quality)
{
	const u32 width = image.GetWidth();
	const u32 height = image.GetHeight();

	// Check dimensions
	if (width == 0 || height == 0)
	{
		Console.Error("Invalid BMP dimensions: %ux%u", width, height);
		return false;
	}

	const u32 row_size = ((width * 3 + 3) / 4) * 4;
	const u32 image_size = row_size * height;
	const u32 file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size;

	buffer->resize(file_size);
	u8* data = buffer->data();

	BMPFileHeader file_header = {};
	file_header.type = 0x4D42;
	file_header.size = file_size;
	file_header.reserved1 = 0;
	file_header.reserved2 = 0;
	file_header.offset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
	std::memcpy(data, &file_header, sizeof(BMPFileHeader));

	BMPInfoHeader info_header = {};
	info_header.size = sizeof(BMPInfoHeader);
	info_header.width = static_cast<s32>(width);
	info_header.height = static_cast<s32>(height);
	info_header.planes = 1;
	info_header.bit_count = 24;
	info_header.compression = 0;
	info_header.size_image = image_size;
	info_header.x_pels_per_meter = 0;
	info_header.y_pels_per_meter = 0;
	info_header.clr_used = 0;
	info_header.clr_important = 0;
	std::memcpy(data + sizeof(BMPFileHeader), &info_header, sizeof(BMPInfoHeader));

	u8* pixel_data = data + file_header.offset;
	for (u32 y = 0; y < height; y++)
	{
		const u32 src_y = height - 1 - y;
		const u32* row_src = image.GetRowPixels(src_y);
		u8* row_dst = pixel_data + (y * row_size);

		for (u32 x = 0; x < width; x++)
		{
			const u32 rgba = row_src[x];
			row_dst[x * 3 + 0] = static_cast<u8>((rgba >> 16) & 0xFF);
			row_dst[x * 3 + 1] = static_cast<u8>((rgba >> 8) & 0xFF);
			row_dst[x * 3 + 2] = static_cast<u8>(rgba & 0xFF);
		}
	}

	return true;
}

bool BMPFileSaver(const RGBA8Image& image, const char* filename, std::FILE* fp, u8 quality)
{
	std::vector<u8> buffer;
	if (!BMPBufferSaver(image, &buffer, quality))
		return false;

	return (std::fwrite(buffer.data(), buffer.size(), 1, fp) == 1);
}
