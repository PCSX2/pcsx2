// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GSJobQueue.h"
#include <png.h>

namespace GSPng
{
	enum Format
	{
		START = 0,
		RGBA_PNG = 0,
		RGB_PNG,
		RGB_A_PNG,
		ALPHA_PNG,
		R8I_PNG,
		R16I_PNG,
		R32I_PNG,
		COUNT
	};

	class Transaction
	{
	public:
		Format m_fmt;
		const std::string m_file;
		u8* m_image;
		int m_w;
		int m_h;
		int m_pitch;
		int m_compression;

		Transaction(GSPng::Format fmt, const std::string& file, const u8* image, int w, int h, int pitch, int compression);
		~Transaction();
	};

	bool Save(GSPng::Format fmt, const std::string& file, const u8* image, int w, int h, int pitch, int compression, bool rb_swapped = false);

	void Process(std::shared_ptr<Transaction>& item);

	using Worker = GSJobQueue<std::shared_ptr<Transaction>, 16>;

	constexpr struct
	{
		int type;
		int bytes_per_pixel_in;
		int bytes_per_pixel_out;
		int channel_bit_depth;
		const char* extension[2];
	} const pixel[GSPng::Format::COUNT] = {
		{PNG_COLOR_TYPE_RGBA, 4, 4, 8, {"_full.png", nullptr}}, // RGBA_PNG
		{PNG_COLOR_TYPE_RGB, 4, 3, 8, {".png", nullptr}}, // RGB_PNG
		{PNG_COLOR_TYPE_RGB, 4, 3, 8, {".png", "_alpha.png"}}, // RGB_A_PNG
		{PNG_COLOR_TYPE_GRAY, 4, 1, 8, {"_alpha.png", nullptr}}, // ALPHA_PNG
		{PNG_COLOR_TYPE_GRAY, 1, 1, 8, {"_R8I.png", nullptr}}, // R8I_PNG
		{PNG_COLOR_TYPE_GRAY, 2, 2, 16, {"_R16I.png", nullptr}}, // R16I_PNG
		{PNG_COLOR_TYPE_GRAY, 4, 2, 16, {"_R32I_lsb.png", "_R32I_msb.png"}}, // R32I_PNG
	};
} // namespace GSPng
