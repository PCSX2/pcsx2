// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GSJobQueue.h"

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
} // namespace GSPng
