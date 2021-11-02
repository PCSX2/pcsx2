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

#include "PrecompiledHeader.h"
#include "GSDeviceNull.h"

bool GSDeviceNull::Create(const WindowInfo& wi)
{
	if (!GSDevice::Create(wi))
		return false;

	Reset(1, 1);

	return true;
}

bool GSDeviceNull::Reset(int w, int h)
{
	return GSDevice::Reset(w, h);
}

GSTexture* GSDeviceNull::CreateSurface(GSTexture::Type type, int w, int h, GSTexture::Format format)
{
	return new GSTextureNull(type, w, h, format);
}
