/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "GS/Renderers/Common/GSTexture.h"

class GSTextureNull : public GSTexture
{
	struct
	{
		int type, w, h, format;
	} m_desc;

public:
	GSTextureNull();
	GSTextureNull(int type, int w, int h, int format);

	int GetType() const { return m_desc.type; }
	int GetFormat() const { return m_desc.format; }

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) { return true; }
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) { return false; }
	void Unmap() {}
	bool Save(const std::string& fn) { return false; }
};
