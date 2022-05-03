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

#include <dxgi1_3.h>
#include <vector>
#include <string>
#include <wil/com.h>

namespace D3D
{
	// create a dxgi factory
	wil::com_ptr_nothrow<IDXGIFactory2> CreateFactory(bool debug);

	// get an adapter based on position
	// assuming no one removes/moves it, it should always have the same id
	// however in the event that the adapter is not found due to the above, use the default
	wil::com_ptr_nothrow<IDXGIAdapter1> GetAdapterFromIndex(IDXGIFactory2* factory, int index);

	// this is sort of a legacy thing that doesn't have much to do with d3d (just the easiest way)
	// checks to see if the adapter at 0 is NV and thus we should prefer OpenGL
	enum VendorID
	{
		Unknown,
		Nvidia,
		AMD,
		Intel
	};

	enum Renderer
	{
		Default,
		OpenGL,
		Vulkan,
		Direct3D11,
		Direct3D12
	};

	u8 Vendor();
	u8 ShouldPreferRenderer();
};
