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

class GSTextureNull final : public GSTexture
{
	struct
	{
		Type type;
		Format format;
		int w, h;
	} m_desc;

public:
	GSTextureNull();
	GSTextureNull(Type type, int w, int h, Format format);

	Type GetType() const { return m_desc.type; }
	Format GetFormat() const { return m_desc.format; }

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override { return true; }
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override { return false; }
	void Unmap() override {}
	bool Save(const std::string& fn) override { return false; }
	void Swap(GSTexture* tex) override;
	void* GetNativeHandle() const override;
};
