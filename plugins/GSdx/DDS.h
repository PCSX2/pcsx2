#pragma once

#include <string>
#include <vector>
#include <fstream>

namespace DDS
{
	struct DDSFormat
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

	struct DDSHeader {
		uint32_t dwMagic;
		uint32_t dwSize;
		uint32_t dwFlags;
		uint32_t dwHeight;
		uint32_t dwWidth;
		uint32_t dwPitch;
		uint32_t dwDepth;
		uint32_t dwMipCount;
		uint32_t dwReserved1[11];
		DDSFormat ddspf;
		uint32_t dwCaps;
		uint32_t dwCaps2;
		uint32_t dwCaps3;
		uint32_t dwCaps4;
		uint32_t dwReserved2;
	};

	struct DDSFile {
		DDSHeader Header;
		std::vector<unsigned char> Data;
	};

	DDSFile CatchDDS(const char* fileName);
}