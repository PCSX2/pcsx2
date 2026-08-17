// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ScopedGuard.h"
#include "common/ZipHelpers.h"

#include "GS/Renderers/HW/GSTextureReplacements.h"

#include <cstdio>
#include <csetjmp>
#include <png.h>

struct LoaderDefinition
{
	const char* extension;
	GSTextureReplacements::ReplacementTextureLoader loader;
};

static bool PNGLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image);
static bool DDSLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image);

static constexpr LoaderDefinition s_loaders[] = {
	{"png", PNGLoader},
	{"dds", DDSLoader},
};


GSTextureReplacements::ReplacementTextureLoader GSTextureReplacements::GetLoader(const std::string_view filename)
{
	const std::string_view extension(Path::GetExtension(filename));
	if (extension.empty())
		return nullptr;

	for (const LoaderDefinition& defn : s_loaders)
	{
		if (StringUtil::Strncasecmp(extension.data(), defn.extension, extension.size()) == 0)
			return defn.loader;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static u32 GetBlockCount(u32 extent, u32 block_size)
{
	return std::max(Common::AlignUp(extent, block_size) / block_size, 1u);
}

static void CalcBlockMipmapSize(u32 block_size, u32 bytes_per_block, u32 base_width, u32 base_height, u32 mip, u32& width, u32& height, u32& pitch, u32& size)
{
	width = std::max<u32>(base_width >> mip, 1u);
	height = std::max<u32>(base_height >> mip, 1u);

	const u32 blocks_wide = GetBlockCount(width, block_size);
	const u32 blocks_high = GetBlockCount(height, block_size);

	// Pitch can't be specified with each mip level, so we have to calculate it ourselves.
	pitch = blocks_wide * bytes_per_block;
	size = blocks_high * pitch;
}

static void ConvertTexture_X8B8G8R8(u32 width, u32 height, std::vector<u8>& data, u32& pitch)
{
	for (u32 row = 0; row < height; row++)
	{
		u8* data_ptr = data.data() + row * pitch;

		for (u32 x = 0; x < width; x++)
		{
			// Set alpha channel to full intensity.
			data_ptr[3] = 0x80;
			data_ptr += sizeof(u32);
		}
	}
}

static void ConvertTexture_A8R8G8B8(u32 width, u32 height, std::vector<u8>& data, u32& pitch)
{
	for (u32 row = 0; row < height; row++)
	{
		u8* data_ptr = data.data() + row * pitch;

		for (u32 x = 0; x < width; x++)
		{
			// Byte swap ABGR -> RGBA
			u32 val;
			std::memcpy(&val, data_ptr, sizeof(val));
			val = ((val & 0xFF00FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000));
			std::memcpy(data_ptr, &val, sizeof(u32));
			data_ptr += sizeof(u32);
		}
	}
}

static void ConvertTexture_X8R8G8B8(u32 width, u32 height, std::vector<u8>& data, u32& pitch)
{
	for (u32 row = 0; row < height; row++)
	{
		u8* data_ptr = data.data() + row * pitch;

		for (u32 x = 0; x < width; x++)
		{
			// Byte swap XBGR -> RGBX, and set alpha to full intensity.
			u32 val;
			std::memcpy(&val, data_ptr, sizeof(val));
			val = ((val & 0x0000FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000)) | 0xFF000000;
			std::memcpy(data_ptr, &val, sizeof(u32));
			data_ptr += sizeof(u32);
		}
	}
}

static void ConvertTexture_R8G8B8(u32 width, u32 height, std::vector<u8>& data, u32& pitch)
{
	const u32 new_pitch = width * sizeof(u32);
	std::vector<u8> new_data(new_pitch * height);

	for (u32 row = 0; row < height; row++)
	{
		const u8* rgb_data_ptr = data.data() + row * pitch;
		u8* data_ptr = new_data.data() + row * new_pitch;

		for (u32 x = 0; x < width; x++)
		{
			// This is BGR in memory.
			u32 val;
			std::memcpy(&val, rgb_data_ptr, sizeof(val));
			val = ((val & 0x0000FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000)) | 0xFF000000;
			std::memcpy(data_ptr, &val, sizeof(u32));
			data_ptr += sizeof(u32);
			rgb_data_ptr += 3;
		}
	}

	data = std::move(new_data);
	pitch = new_pitch;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PNG Handlers
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename InitIoFunc>
static bool PNGLoaderInternal(InitIoFunc&& init_io, GSTextureReplacements::ReplacementTexture* tex)
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

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	});

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	init_io(png_ptr);

	png_read_info(png_ptr, info_ptr);

	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bitDepth = 0;
	int colorType = -1;
	if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr) != 1 ||
		width == 0 || height == 0)
	{
		return false;
	}

	const u32 pitch = width * sizeof(u32);
	tex->width = width;
	tex->height = height;
	tex->format = GSTexture::Format::Color;
	tex->pitch = pitch;
	tex->data.resize(pitch * height);

	const png_uint_32 row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	std::vector<u8> row_data(row_bytes);

	for (u32 y = 0; y < height; y++)
	{
		png_read_row(png_ptr, static_cast<png_bytep>(row_data.data()), nullptr);

		const u8* row_ptr = row_data.data();
		u8* out_ptr = tex->data.data() + y * pitch;
		if (colorType == PNG_COLOR_TYPE_RGB)
		{
			for (u32 x = 0; x < width; x++)
			{
				u32 pixel = static_cast<u32>(*(row_ptr)++);
				pixel |= static_cast<u32>(*(row_ptr)++) << 8;
				pixel |= static_cast<u32>(*(row_ptr)++) << 16;
				pixel |= 0x80000000u;
				std::memcpy(out_ptr, &pixel, sizeof(pixel));
				out_ptr += sizeof(pixel);
			}
		}
		else if (colorType == PNG_COLOR_TYPE_RGBA)
		{
			std::memcpy(out_ptr, row_ptr, pitch);
		}
	}

	return true;
}

static bool PNGLoaderFromFile(FILE* fp, const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	return PNGLoaderInternal([fp](png_structp png_ptr) {
		png_init_io(png_ptr, fp);
	},
		tex);
}

bool PNGLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	if (!fp)
		return false;

	return PNGLoaderFromFile(fp.get(), filename, tex, only_base_image);
}

static void PNGReadFromMemory(png_structp png_ptr, png_bytep out, png_size_t count)
{
	auto* src = static_cast<std::vector<u8>*>(png_get_io_ptr(png_ptr));
	if (src->empty())
	{
		png_error(png_ptr, "unexpected end of PNG data");
		return;
	}

	const size_t available = std::min<size_t>(count, src->size());
	std::memcpy(out, src->data(), available);
	src->erase(src->begin(), src->begin() + static_cast<std::ptrdiff_t>(available));
	if (available < count)
		png_error(png_ptr, "unexpected end of PNG data");
}

static bool PNGLoaderFromBuffer(const std::vector<u8>& data, const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	std::vector<u8> buffer = data;
	return PNGLoaderInternal([&buffer](png_structp png_ptr) {
		png_set_read_fn(png_ptr, &buffer, PNGReadFromMemory);
	},
		tex);
}

bool GSTextureReplacements::SavePNGImage(const std::string& filename, u32 width, u32 height, const u8* buffer, u32 pitch)
{
	const int compression = GSConfig.PNGCompressionLevel;

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png_ptr)
		return false;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == nullptr)
	{
		png_destroy_write_struct(&png_ptr, nullptr);
		return false;
	}

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		png_destroy_write_struct(&png_ptr, &info_ptr);
	});

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "wb");
	if (!fp)
		return false;

	png_init_io(png_ptr, fp.get());
	png_set_compression_level(png_ptr, compression);
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);
	png_set_swap(png_ptr);

	for (u32 y = 0; y < height; ++y)
	{
		// cast is needed here for mac builder
		png_write_row(png_ptr, (png_bytep)(buffer + y * pitch));
	}

	png_write_end(png_ptr, nullptr);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DDS Handler
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// From https://raw.githubusercontent.com/Microsoft/DirectXTex/master/DirectXTex/DDS.h
//
// This header defines constants and structures that are useful when parsing
// DDS files.  DDS files were originally designed to use several structures
// and constants that are native to DirectDraw and are defined in ddraw.h,
// such as DDSURFACEDESC2 and DDSCAPS2.  This file defines similar
// (compatible) constants and structures so that one can use DDS files
// without needing to include ddraw.h.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248926

#pragma pack(push, 1)

static constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

struct DDS_PIXELFORMAT
{
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwFourCC;
	uint32_t dwRGBBitCount;
	uint32_t dwRBitMask;
	uint32_t dwGBitMask;
	uint32_t dwBBitMask;
	uint32_t dwABitMask;
};

#define DDS_FOURCC 0x00000004 // DDPF_FOURCC
#define DDS_RGB 0x00000040 // DDPF_RGB
#define DDS_RGBA 0x00000041 // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE 0x00020000 // DDPF_LUMINANCE
#define DDS_LUMINANCEA 0x00020001 // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
#define DDS_ALPHA 0x00000002 // DDPF_ALPHA
#define DDS_PAL8 0x00000020 // DDPF_PALETTEINDEXED8
#define DDS_PAL8A 0x00000021 // DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
#define DDS_BUMPDUDV 0x00080000 // DDPF_BUMPDUDV

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
	((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | \
		((uint32_t)(uint8_t)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */

#define DDS_HEADER_FLAGS_TEXTURE \
	0x00001007 // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP 0x00020000 // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME 0x00800000 // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH 0x00000008 // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE 0x00080000 // DDSD_LINEARSIZE
#define DDS_MAX_TEXTURE_SIZE 32768

// Subset here matches D3D10_RESOURCE_DIMENSION and D3D11_RESOURCE_DIMENSION
enum DDS_RESOURCE_DIMENSION
{
	DDS_DIMENSION_TEXTURE1D = 2,
	DDS_DIMENSION_TEXTURE2D = 3,
	DDS_DIMENSION_TEXTURE3D = 4,
};

struct DDS_HEADER
{
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwHeight;
	uint32_t dwWidth;
	uint32_t dwPitchOrLinearSize;
	uint32_t dwDepth; // only if DDS_HEADER_FLAGS_VOLUME is set in dwFlags
	uint32_t dwMipMapCount;
	uint32_t dwReserved1[11];
	DDS_PIXELFORMAT ddspf;
	uint32_t dwCaps;
	uint32_t dwCaps2;
	uint32_t dwCaps3;
	uint32_t dwCaps4;
	uint32_t dwReserved2;
};

struct DDS_HEADER_DXT10
{
	uint32_t dxgiFormat;
	uint32_t resourceDimension;
	uint32_t miscFlag; // see DDS_RESOURCE_MISC_FLAG
	uint32_t arraySize;
	uint32_t miscFlags2; // see DDS_MISC_FLAGS2
};

#pragma pack(pop)

static_assert(sizeof(DDS_HEADER) == 124, "DDS Header size mismatch");
static_assert(sizeof(DDS_HEADER_DXT10) == 20, "DDS DX10 Extended Header size mismatch");

constexpr DDS_PIXELFORMAT DDSPF_A8R8G8B8 = {
	sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000};
constexpr DDS_PIXELFORMAT DDSPF_X8R8G8B8 = {
	sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000};
constexpr DDS_PIXELFORMAT DDSPF_A8B8G8R8 = {
	sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000};
constexpr DDS_PIXELFORMAT DDSPF_X8B8G8R8 = {
	sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000};
constexpr DDS_PIXELFORMAT DDSPF_R8G8B8 = {
	sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000};

// End of Microsoft code from DDS.h.

static bool DDSPixelFormatMatches(const DDS_PIXELFORMAT& pf1, const DDS_PIXELFORMAT& pf2)
{
	return std::tie(pf1.dwSize, pf1.dwFlags, pf1.dwFourCC, pf1.dwRGBBitCount, pf1.dwRBitMask, pf1.dwGBitMask, pf1.dwGBitMask, pf1.dwBBitMask, pf1.dwABitMask) ==
	       std::tie(pf2.dwSize, pf2.dwFlags, pf2.dwFourCC, pf2.dwRGBBitCount, pf2.dwRBitMask, pf2.dwGBitMask, pf2.dwGBitMask, pf2.dwBBitMask, pf2.dwABitMask);
}

struct DDSLoadInfo
{
	u32 block_size = 1;
	u32 bytes_per_block = 4;
	u32 width = 0;
	u32 height = 0;
	u32 mip_count = 0;
	GSTexture::Format format = GSTexture::Format::Color;
	size_t base_image_offset = 0;
	u32 base_image_size = 0;
	u32 base_image_pitch = 0;

	std::function<void(u32 width, u32 height, std::vector<u8>& data, u32& pitch)> conversion_function;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions for reading from a file
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ParseDDSHeader(std::FILE* fp, DDSLoadInfo* info)
{
	u32 magic;
	if (std::fread(&magic, sizeof(magic), 1, fp) != 1 || magic != DDS_MAGIC)
		return false;

	DDS_HEADER header;
	u32 header_size = sizeof(header);
	if (std::fread(&header, header_size, 1, fp) != 1 || header.dwSize < header_size)
		return false;

	if (header.dwWidth == 0 || header.dwWidth >= DDS_MAX_TEXTURE_SIZE ||
		header.dwHeight == 0 || header.dwHeight >= DDS_MAX_TEXTURE_SIZE)
	{
		return false;
	}

	if (header.dwFlags & DDS_HEADER_FLAGS_VOLUME)
		return false;

	info->width = header.dwWidth;
	info->height = header.dwHeight;
	if (info->width == 0 || info->height == 0)
		return false;

	if (header.dwFlags & DDS_HEADER_FLAGS_MIPMAP)
	{
		info->mip_count = header.dwMipMapCount;
		if (header.dwMipMapCount != 0)
			info->mip_count = header.dwMipMapCount;
		else
			info->mip_count = GSTextureReplacements::CalcMipmapLevelsForReplacement(info->width, info->height);
	}
	else
	{
		info->mip_count = 1;
	}

	const bool has_fourcc = (header.ddspf.dwFlags & DDS_FOURCC) != 0;
	if (has_fourcc)
	{
		u32 dxt10_format = 0;
		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0'))
		{
			DDS_HEADER_DXT10 dxt10_header;
			if (std::fread(&dxt10_header, sizeof(dxt10_header), 1, fp) != 1)
				return false;

			if (dxt10_header.resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10_header.arraySize != 1)
				return false;

			header_size += sizeof(dxt10_header);
			dxt10_format = dxt10_header.dxgiFormat;
		}

		const GSDevice::FeatureSupport features(g_gs_device->Features());
		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1') || dxt10_format == 71)
		{
			info->format = GSTexture::Format::BC1;
			info->block_size = 4;
			info->bytes_per_block = 8;
			if (!features.dxt_textures)
				return false;
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '2') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3') || dxt10_format == 74)
		{
			info->format = GSTexture::Format::BC2;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.dxt_textures)
				return false;
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '4') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5') || dxt10_format == 77)
		{
			info->format = GSTexture::Format::BC3;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.dxt_textures)
				return false;
		}
		else if (dxt10_format == 98)
		{
			info->format = GSTexture::Format::BC7;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.bptc_textures)
				return false;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8R8G8B8))
			info->conversion_function = ConvertTexture_A8R8G8B8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8R8G8B8))
			info->conversion_function = ConvertTexture_X8R8G8B8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8B8G8R8))
			info->conversion_function = ConvertTexture_X8B8G8R8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_R8G8B8))
			info->conversion_function = ConvertTexture_R8G8B8;
		else if (!DDSPixelFormatMatches(header.ddspf, DDSPF_A8B8G8R8))
			return false;

		info->format = GSTexture::Format::Color;
		info->block_size = 1;
		info->bytes_per_block = header.ddspf.dwRGBBitCount / 8;
	}

	const u32 blocks_wide = GetBlockCount(info->width, info->block_size);
	const u32 blocks_high = GetBlockCount(info->height, info->block_size);

	if (header.dwFlags & DDS_HEADER_FLAGS_PITCH && header.dwFlags & DDS_HEADER_FLAGS_LINEARSIZE)
	{
		if (header.dwPitchOrLinearSize < info->bytes_per_block)
			return false;

		info->base_image_pitch = header.dwPitchOrLinearSize;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}
	else
	{
		info->base_image_pitch = blocks_wide * info->bytes_per_block;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}

	info->base_image_offset = sizeof(magic) + header_size;
	if (info->base_image_offset >= FileSystem::FSize64(fp))
		return false;

	return true;
}

static bool ReadDDSMipLevel(std::FILE* fp, const std::string& filename, u32 mip_level, const DDSLoadInfo& info, u32 width, u32 height, std::vector<u8>& data, u32& pitch, u32 size)
{
	if (mip_level == 0 && info.block_size > 1 &&
		((width % info.block_size) != 0 || (height % info.block_size) != 0))
	{
		Console.Error(
			"Invalid dimensions for DDS texture %s. For compressed textures of this format, "
			"the width/height of the first mip level must be a multiple of %u.",
			filename.c_str(), info.block_size);
		return false;
	}

	data.resize(size);
	if (std::fread(data.data(), size, 1, fp) != 1)
		return false;

	if (info.conversion_function)
		info.conversion_function(width, height, data, pitch);

	return true;
}

static bool DDSLoaderFromFile(FILE* fp, const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	DDSLoadInfo info;
	if (!ParseDDSHeader(fp, &info))
		return false;

	if (FileSystem::FSeek64(fp, info.base_image_offset, SEEK_SET) != 0)
		return false;

	tex->format = info.format;
	tex->width = info.width;
	tex->height = info.height;
	tex->pitch = info.base_image_pitch;
	if (!ReadDDSMipLevel(fp, filename, 0, info, tex->width, tex->height, tex->data, tex->pitch, info.base_image_size))
		return false;

	if (!only_base_image)
	{
		for (u32 level = 1; level <= info.mip_count; level++)
		{
			GSTextureReplacements::ReplacementTexture::MipData md;
			u32 mip_size;
			CalcBlockMipmapSize(info.block_size, info.bytes_per_block, info.width, info.height, level, md.width, md.height, md.pitch, mip_size);
			if (!ReadDDSMipLevel(fp, filename, level, info, md.width, md.height, md.data, md.pitch, mip_size))
				break;

			tex->mips.push_back(std::move(md));
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions for reading from a memory buffer (e.g., from a zip file)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ParseDDSHeaderFromBuffer(const u8* buffer_data, size_t buffer_size, DDSLoadInfo* info)
{
	size_t offset = 0;

	if (buffer_size < sizeof(u32) + sizeof(DDS_HEADER))
		return false;

	u32 magic;
	std::memcpy(&magic, buffer_data + offset, sizeof(magic));
	offset += sizeof(magic);

	if (magic != DDS_MAGIC)
		return false;

	DDS_HEADER header;
	u32 header_size = sizeof(header);
	std::memcpy(&header, buffer_data + offset, header_size);
	offset += header_size;

	if (header.dwSize < sizeof(DDS_HEADER))
		return false;

	if (header.dwWidth == 0 || header.dwWidth >= DDS_MAX_TEXTURE_SIZE ||
		header.dwHeight == 0 || header.dwHeight >= DDS_MAX_TEXTURE_SIZE)
	{
		return false;
	}

	if (header.dwFlags & DDS_HEADER_FLAGS_VOLUME)
		return false;

	info->width = header.dwWidth;
	info->height = header.dwHeight;

	if (header.dwFlags & DDS_HEADER_FLAGS_MIPMAP)
	{
		info->mip_count = (header.dwMipMapCount != 0) ?
		                      header.dwMipMapCount :
		                      GSTextureReplacements::CalcMipmapLevelsForReplacement(info->width, info->height);
	}
	else
	{
		info->mip_count = 1;
	}

	const bool has_fourcc = (header.ddspf.dwFlags & DDS_FOURCC) != 0;
	if (has_fourcc)
	{
		u32 dxt10_format = 0;
		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0'))
		{
			if (buffer_size < offset + sizeof(DDS_HEADER_DXT10))
				return false;

			DDS_HEADER_DXT10 dxt10_header;
			std::memcpy(&dxt10_header, buffer_data + offset, sizeof(dxt10_header));
			offset += sizeof(dxt10_header);

			if (dxt10_header.resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10_header.arraySize != 1)
				return false;

			dxt10_format = dxt10_header.dxgiFormat;
		}

		const GSDevice::FeatureSupport features(g_gs_device->Features());
		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1') || dxt10_format == 71)
		{
			info->format = GSTexture::Format::BC1;
			info->block_size = 4;
			info->bytes_per_block = 8;
			if (!features.dxt_textures)
				return false;
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '2') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3') || dxt10_format == 74)
		{
			info->format = GSTexture::Format::BC2;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.dxt_textures)
				return false;
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '4') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5') || dxt10_format == 77)
		{
			info->format = GSTexture::Format::BC3;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.dxt_textures)
				return false;
		}
		else if (dxt10_format == 98)
		{
			info->format = GSTexture::Format::BC7;
			info->block_size = 4;
			info->bytes_per_block = 16;
			if (!features.bptc_textures)
				return false;
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8R8G8B8))
			info->conversion_function = ConvertTexture_A8R8G8B8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8R8G8B8))
			info->conversion_function = ConvertTexture_X8R8G8B8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8B8G8R8))
			info->conversion_function = ConvertTexture_X8B8G8R8;
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_R8G8B8))
			info->conversion_function = ConvertTexture_R8G8B8;
		else if (!DDSPixelFormatMatches(header.ddspf, DDSPF_A8B8G8R8))
			return false;

		info->format = GSTexture::Format::Color;
		info->block_size = 1;
		info->bytes_per_block = header.ddspf.dwRGBBitCount / 8;
	}

	const u32 blocks_wide = GetBlockCount(info->width, info->block_size);
	const u32 blocks_high = GetBlockCount(info->height, info->block_size);

	if (header.dwFlags & DDS_HEADER_FLAGS_PITCH && header.dwFlags & DDS_HEADER_FLAGS_LINEARSIZE)
	{
		if (header.dwPitchOrLinearSize < info->bytes_per_block)
			return false;

		info->base_image_pitch = header.dwPitchOrLinearSize;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}
	else
	{
		info->base_image_pitch = blocks_wide * info->bytes_per_block;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}

	info->base_image_offset = offset;
	if (info->base_image_offset >= buffer_size)
		return false;

	return true;
}

static bool ReadDDSMipLevelFromBuffer(const u8* buffer_data, size_t buffer_size, size_t& offset, u32 mip_level, const DDSLoadInfo& info, u32 width, u32 height, std::vector<u8>& data, u32& pitch, u32 size)
{
	if (mip_level == 0 && info.block_size > 1 &&
		((width % info.block_size) != 0 || (height % info.block_size) != 0))
	{
		return false;
	}

	if (offset + size > buffer_size)
		return false;

	data.resize(size);
	std::memcpy(data.data(), buffer_data + offset, size);
	offset += size;

	if (info.conversion_function)
		info.conversion_function(width, height, data, pitch);

	return true;
}

static bool DDSLoaderFromBuffer(const std::vector<u8>& data, const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	DDSLoadInfo info;
	if (!ParseDDSHeaderFromBuffer(data.data(), data.size(), &info))
		return false;

	size_t current_offset = info.base_image_offset;

	tex->format = info.format;
	tex->width = info.width;
	tex->height = info.height;
	tex->pitch = info.base_image_pitch;

	if (!ReadDDSMipLevelFromBuffer(data.data(), data.size(), current_offset, 0, info, tex->width, tex->height, tex->data, tex->pitch, info.base_image_size))
		return false;

	if (!only_base_image)
	{
		for (u32 level = 1; level <= info.mip_count; level++)
		{
			GSTextureReplacements::ReplacementTexture::MipData md;
			u32 mip_size;
			CalcBlockMipmapSize(info.block_size, info.bytes_per_block, info.width, info.height, level, md.width, md.height, md.pitch, mip_size);

			if (!ReadDDSMipLevelFromBuffer(data.data(), data.size(), current_offset, level, info, md.width, md.height, md.data, md.pitch, mip_size))
				break;

			tex->mips.push_back(std::move(md));
		}
	}

	return true;
}

bool DDSLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	if (!fp)
		return false;

	return DDSLoaderFromFile(fp.get(), filename, tex, only_base_image);
}

bool GSTextureReplacements::LoadImageFromArchiveEntry(const std::string& archive_path,
	const std::string& normalized_name,
	GSTextureReplacements::ReplacementTexture* tex,
	bool only_base_image)
{
	const auto data = ReadArchiveEntryBytes(archive_path, normalized_name);
	if (!data.has_value())
		return false;

	const std::string_view ext = Path::GetExtension(normalized_name);
	if (StringUtil::Strncasecmp(ext.data(), "png", ext.size()) == 0)
		return PNGLoaderFromBuffer(*data, normalized_name, tex, only_base_image);
	if (StringUtil::Strncasecmp(ext.data(), "dds", ext.size()) == 0)
		return DDSLoaderFromBuffer(*data, normalized_name, tex, only_base_image);

	return false;
}
