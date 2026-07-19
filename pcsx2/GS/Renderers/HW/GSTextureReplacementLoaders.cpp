// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/ScopedGuard.h"
#include "common/TextureDecompress.h" // CPU BC1/2/3/BC7 decode when the GPU lacks BC support

#include "GS/Renderers/HW/GSTextureReplacements.h"

#include <algorithm>
#include <csetjmp>
#include <cstring>
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

bool PNGLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
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

	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	if (!fp)
		return false;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_init_io(png_ptr, fp.get());
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
				pixel |= 0x80000000u; // make opaque
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
	s64 base_image_offset = 0;
	u32 base_image_size = 0;
	u32 base_image_pitch = 0;

	// Set when the file is block-compressed but the GPU cannot sample that format, so the
	// blocks are decoded to RGBA8 on the CPU at load time instead of the whole texture
	// being rejected. `format` is then Color (the uploaded format) while `decompress_format`
	// keeps the on-disk format the block reader has to decode. Sizes/pitches below stay in
	// COMPRESSED units, because they describe how much to read out of the file.
	bool decompress = false;
	GSTexture::Format decompress_format = GSTexture::Format::Color;

	std::function<void(u32 width, u32 height, std::vector<u8>& data, u32& pitch)> conversion_function;
};

static bool ParseDDSHeader(std::FILE* fp, DDSLoadInfo* info)
{
	u32 magic;
	if (std::fread(&magic, sizeof(magic), 1, fp) != 1 || magic != DDS_MAGIC)
		return false;

	DDS_HEADER header;
	u32 header_size = sizeof(header);
	if (std::fread(&header, header_size, 1, fp) != 1 || header.dwSize < header_size)
		return false;

	// We should check for DDS_HEADER_FLAGS_TEXTURE here, but some tools don't seem
	// to set it (e.g. compressonator). But we can still validate the size.
	if (header.dwWidth == 0 || header.dwWidth >= DDS_MAX_TEXTURE_SIZE ||
		header.dwHeight == 0 || header.dwHeight >= DDS_MAX_TEXTURE_SIZE)
	{
		return false;
	}

	// Image should be 2D.
	if (header.dwFlags & DDS_HEADER_FLAGS_VOLUME)
		return false;

	// Presence of width/height fields is already tested by DDS_HEADER_FLAGS_TEXTURE.
	info->width = header.dwWidth;
	info->height = header.dwHeight;
	if (info->width == 0 || info->height == 0)
		return false;

	// Check for mip levels.
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

	// Handle fourcc formats vs uncompressed formats.
	const bool has_fourcc = (header.ddspf.dwFlags & DDS_FOURCC) != 0;
	if (has_fourcc)
	{
		// Handle DX10 extension header.
		u32 dxt10_format = 0;
		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0'))
		{
			DDS_HEADER_DXT10 dxt10_header;
			if (std::fread(&dxt10_header, sizeof(dxt10_header), 1, fp) != 1)
				return false;

			// Can't handle array textures here. Doesn't make sense to use them, anyway.
			if (dxt10_header.resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10_header.arraySize != 1)
				return false;

			header_size += sizeof(dxt10_header);
			dxt10_format = dxt10_header.dxgiFormat;
		}

		const GSDevice::FeatureSupport features(g_gs_device->Features());
		// Mobile GPUs frequently expose no BC/BPTC support at all (Vulkan
		// textureCompressionBC false: Adreno 650 / Snapdragon 865, and Mesa Turnip on any
		// Adreno). Rejecting the file there silently drops the ENTIRE pack — and for packs
		// that also ship game-side data the result is worse than "no upscale": the P3P Slim
		// Font mod pairs new FONT0.FNT glyph metrics with BC7 replacement glyphs, so with
		// the textures dropped the game indexes the new narrow metrics into the old wide
		// atlas and renders letters sliced in half. Decode on the CPU instead; the decoders
		// are already built (common/TextureDecompress.cpp, used below for alpha min/max).
		const auto set_bc = [&info](GSTexture::Format fmt, u32 bytes_per_block, bool gpu_supported) {
			info->block_size = 4;
			info->bytes_per_block = bytes_per_block;
			if (gpu_supported)
			{
				info->format = fmt;
			}
			else
			{
				info->decompress = true;
				info->decompress_format = fmt;
				info->format = GSTexture::Format::Color;
				static bool logged_once = false;
				if (!logged_once)
				{
					logged_once = true;
					Console.WriteLn("Texture replacements: GPU cannot sample this block-compressed "
									"format, decoding to RGBA8 on the CPU (uses ~4x more memory per texture).");
				}
			}
		};

		if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1') || dxt10_format == 71 /*DXGI_FORMAT_BC1_UNORM*/)
		{
			set_bc(GSTexture::Format::BC1, 8, features.dxt_textures);
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '2') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3') || dxt10_format == 74 /*DXGI_FORMAT_BC2_UNORM*/)
		{
			set_bc(GSTexture::Format::BC2, 16, features.dxt_textures);
		}
		else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '4') || header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5') || dxt10_format == 77 /*DXGI_FORMAT_BC3_UNORM*/)
		{
			set_bc(GSTexture::Format::BC3, 16, features.dxt_textures);
		}
		else if (dxt10_format == 98 /*DXGI_FORMAT_BC7_UNORM*/)
		{
			set_bc(GSTexture::Format::BC7, 16, features.bptc_textures);
		}
		else
		{
			// Leave all remaining formats to SOIL.
			return false;
		}
	}
	else
	{
		if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8R8G8B8))
		{
			info->conversion_function = ConvertTexture_A8R8G8B8;
		}
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8R8G8B8))
		{
			info->conversion_function = ConvertTexture_X8R8G8B8;
		}
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8B8G8R8))
		{
			info->conversion_function = ConvertTexture_X8B8G8R8;
		}
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_R8G8B8))
		{
			info->conversion_function = ConvertTexture_R8G8B8;
		}
		else if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8B8G8R8))
		{
			// This format is already in RGBA order, so no conversion necessary.
		}
		else
		{
			return false;
		}

		// All these formats are RGBA, just with byte swapping.
		info->format = GSTexture::Format::Color;
		info->block_size = 1;
		info->bytes_per_block = header.ddspf.dwRGBBitCount / 8;
	}

	// Mip levels smaller than the block size are padded to multiples of the block size.
	const u32 blocks_wide = GetBlockCount(info->width, info->block_size);
	const u32 blocks_high = GetBlockCount(info->height, info->block_size);

	// Pitch can be specified in the header, otherwise we can derive it from the dimensions. For
	// compressed formats, both DDS_HEADER_FLAGS_LINEARSIZE and DDS_HEADER_FLAGS_PITCH should be
	// set. See https://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
	if (header.dwFlags & DDS_HEADER_FLAGS_PITCH && header.dwFlags & DDS_HEADER_FLAGS_LINEARSIZE)
	{
		// Convert pitch (in bytes) to texels/row length.
		if (header.dwPitchOrLinearSize < info->bytes_per_block)
		{
			// Likely a corrupted or invalid file.
			return false;
		}

		info->base_image_pitch = header.dwPitchOrLinearSize;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}
	else
	{
		// Assume no padding between rows of blocks.
		info->base_image_pitch = blocks_wide * info->bytes_per_block;
		info->base_image_size = info->base_image_pitch * blocks_high;
	}

	// Check for truncated or corrupted files.
	info->base_image_offset = sizeof(magic) + header_size;
	if (info->base_image_offset >= FileSystem::FSize64(fp))
		return false;

	return true;
}

static bool ReadDDSMipLevel(std::FILE* fp, const std::string& filename, u32 mip_level, const DDSLoadInfo& info, u32 width, u32 height, std::vector<u8>& data, u32& pitch, u32 size)
{
	// D3D11 cannot handle block compressed textures where the first mip level is
	// not a multiple of the block size.
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

	// CPU-side block decode when the GPU can't sample this compressed format (see the
	// set_bc comment in ParseDDSHeader). Decode into a separate RGBA8 buffer and hand that
	// back as the level's data, adjusting pitch to match. Levels smaller than one 4x4 block
	// are stored padded, so decode a whole block grid and only keep the valid region.
	if (info.decompress)
	{
		constexpr u32 BLOCK = 4;
		const u32 blocks_wide = GetBlockCount(width, BLOCK);
		const u32 blocks_high = GetBlockCount(height, BLOCK);
		// Source stride is this level's COMPRESSED pitch as passed in — not a recomputed
		// blocks_wide*bytes_per_block, because the base level may carry an explicit
		// dwPitchOrLinearSize from the header.
		const u32 src_pitch = pitch;
		const u32 out_pitch = width * sizeof(u32);

		std::vector<u8> rgba(static_cast<size_t>(out_pitch) * height);
		alignas(16) u8 block_pixels[BLOCK * BLOCK * sizeof(u32)];

		for (u32 by = 0; by < blocks_high; by++)
		{
			const u8* block_in = data.data() + (static_cast<size_t>(by) * src_pitch);
			for (u32 bx = 0; bx < blocks_wide; bx++, block_in += info.bytes_per_block)
			{
				switch (info.decompress_format)
				{
					case GSTexture::Format::BC1:
						DecompressBlockBC1(0, 0, sizeof(u32) * BLOCK, block_in, block_pixels);
						break;
					case GSTexture::Format::BC2:
						DecompressBlockBC2(0, 0, sizeof(u32) * BLOCK, block_in, block_pixels);
						break;
					case GSTexture::Format::BC3:
						DecompressBlockBC3(0, 0, sizeof(u32) * BLOCK, block_in, block_pixels);
						break;
					case GSTexture::Format::BC7:
						bc7decomp::unpack_bc7(block_in, reinterpret_cast<bc7decomp::color_rgba*>(block_pixels));
						break;
					default:
						return false;
				}

				// Copy the block's valid rows/columns into the destination image.
				const u32 copy_w = std::min<u32>(BLOCK, width - (bx * BLOCK));
				const u32 copy_h = std::min<u32>(BLOCK, height - (by * BLOCK));
				for (u32 row = 0; row < copy_h; row++)
				{
					std::memcpy(rgba.data() + (static_cast<size_t>(by * BLOCK + row) * out_pitch) + (static_cast<size_t>(bx) * BLOCK * sizeof(u32)),
						block_pixels + (static_cast<size_t>(row) * BLOCK * sizeof(u32)),
						copy_w * sizeof(u32));
				}
			}
		}

		data = std::move(rgba);
		pitch = out_pitch;
		return true;
	}

	// Apply conversion function for uncompressed textures.
	if (info.conversion_function)
		info.conversion_function(width, height, data, pitch);

	return true;
}

bool DDSLoader(const std::string& filename, GSTextureReplacements::ReplacementTexture* tex, bool only_base_image)
{
	auto fp = FileSystem::OpenManagedCFile(filename.c_str(), "rb");
	if (!fp)
		return false;

	DDSLoadInfo info;
	if (!ParseDDSHeader(fp.get(), &info))
		return false;

	// always load the base image
	if (FileSystem::FSeek64(fp.get(), info.base_image_offset, SEEK_SET) != 0)
		return false;

	tex->format = info.format;
	tex->width = info.width;
	tex->height = info.height;
	tex->pitch = info.base_image_pitch;
	if (!ReadDDSMipLevel(fp.get(), filename, 0, info, tex->width, tex->height, tex->data, tex->pitch, info.base_image_size))
		return false;

	// Read in any remaining mip levels in the file.
	if (!only_base_image)
	{
		// info.mip_count is the DDS dwMipMapCount, which INCLUDES the base image
		// (loaded above as level 0), so there are at most (mip_count - 1) EXTRA
		// mip levels here. The old `level <= info.mip_count` bound loaded one
		// level too many: many packs leave trailing padding after the last real
		// mip (or over-declare dwMipMapCount), so that extra read succeeds and
		// the texture is then created with mips.size()+1 levels — more than the
		// dimensions allow — which trips GSDevice::CreateTexture's assert and
		// aborts the GS thread (SIGABRT) in debug, or in a release build (assert
		// compiled out) ships a garbage trailing mip that shows as corrupted
		// graphics on minification. The whole DDS pack breaks even though BC
		// support and the textures themselves are fine (works on NetherSX2, broken
		// on us). Cap the level count at what the size can actually hold (the same
		// bound the assert uses). Regressed when da04d9ef2 reverted the GS subsystem
		// to canonical; restored here.
		const u32 max_levels = static_cast<u32>(
			GSDevice::GetMipmapLevelsForSize(static_cast<int>(info.width), static_cast<int>(info.height)));
		const u32 mip_levels = std::min(info.mip_count, max_levels);
		for (u32 level = 1; level < mip_levels; level++)
		{
			GSTextureReplacements::ReplacementTexture::MipData md;
			u32 mip_size;
			CalcBlockMipmapSize(info.block_size, info.bytes_per_block, info.width, info.height, level, md.width, md.height, md.pitch, mip_size);
			if (!ReadDDSMipLevel(fp.get(), filename, level, info, md.width, md.height, md.data, md.pitch, mip_size))
				break;

			tex->mips.push_back(std::move(md));
		}
	}

	return true;
}
