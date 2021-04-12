/*
 *	Copyright (C) 2015-2015 Gregory hainaut
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSThread_CXX11.h"

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
		uint8* m_image;
		int m_w;
		int m_h;
		int m_pitch;
		int m_compression;

		Transaction(GSPng::Format fmt, const std::string& file, const uint8* image, int w, int h, int pitch, int compression);
		~Transaction();
	};

	bool Save(GSPng::Format fmt, const std::string& file, uint8* image, int w, int h, int pitch, int compression, bool rb_swapped = false);

	void Process(std::shared_ptr<Transaction>& item);

	using Worker = GSJobQueue<std::shared_ptr<Transaction>, 16>;
} // namespace GSPng
