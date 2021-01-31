#pragma once

#include <string>
#include <vector>
#include <fstream>

namespace DDS
{
	struct DDSHeader
	{
		uint32_t Magic;
		uint32_t Offset;
		uint32_t Format;
		uint32_t Height;
		uint32_t Width;
		char Unknown01[0x08];
		uint32_t MipmapCount;
		char Unknown02[0x60];
	};

	struct DDSFile
	{
		DDSHeader Header;
		std::vector<unsigned char> Data;
	};

	DDSFile CatchDDS(const char* fileName);
}