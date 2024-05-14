// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Image.h"
#include "FileSystem.h"
#include "Console.h"
#include "Path.h"
#include "ScopedGuard.h"
#include "StringUtil.h"

#include <jpeglib.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/encode.h>

// Compute the address of a base type given a field offset.
#define BASE_FROM_RECORD_FIELD(ptr, base_type, field) ((base_type*)(((char*)ptr) - offsetof(base_type, field)))

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

template <typename T>
static bool WrapJPEGDecompress(RGBA8Image* image, T setup_func)
{
	std::vector<u8> scanline;

	JPEGErrorHandler err;
	if (!HandleJPEGError(&err))
		return false;

	jpeg_decompress_struct info;
	info.err = &err.err;
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
		jpeg_source_mgr mgr;

		std::FILE* fp;
		std::unique_ptr<u8[]> buffer;
		bool end_of_file;
	};

	FileCallback cb = {
		.mgr = {
			.init_source = [](j_decompress_ptr cinfo) {},
			.fill_input_buffer = [](j_decompress_ptr cinfo) -> boolean {
				FileCallback* cb = BASE_FROM_RECORD_FIELD(cinfo->src, FileCallback, mgr);
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
					FileCallback* cb = BASE_FROM_RECORD_FIELD(cinfo->src, FileCallback, mgr);
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

	JPEGErrorHandler err;
	if (!HandleJPEGError(&err))
		return false;

	jpeg_compress_struct info;
	info.err = &err.err;
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
					FileCallback* cb = BASE_FROM_RECORD_FIELD(cinfo->dest, FileCallback, mgr);
					cb->mgr.next_output_byte = cb->buffer.get();
					cb->mgr.free_in_buffer = BUFFER_SIZE;
				},
			.empty_output_buffer = [](j_compress_ptr cinfo) -> boolean {
				FileCallback* cb = BASE_FROM_RECORD_FIELD(cinfo->dest, FileCallback, mgr);
				if (!cb->write_error)
					cb->write_error |= (std::fwrite(cb->buffer.get(), 1, BUFFER_SIZE, cb->fp) != BUFFER_SIZE);

				cb->mgr.next_output_byte = cb->buffer.get();
				cb->mgr.free_in_buffer = BUFFER_SIZE;
				return TRUE;
			},
			.term_destination =
				[](j_compress_ptr cinfo) {
					FileCallback* cb = BASE_FROM_RECORD_FIELD(cinfo->dest, FileCallback, mgr);
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
