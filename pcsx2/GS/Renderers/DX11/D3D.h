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

#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include "pcsx2/Config.h"

#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <string>
#include <string_view>
#include <vector>

namespace D3D
{
	// create a dxgi factory
	wil::com_ptr_nothrow<IDXGIFactory5> CreateFactory(bool debug);

	// returns a list of all adapter names
	std::vector<std::string> GetAdapterNames(IDXGIFactory5* factory);

	// returns a list of fullscreen modes for the specified adapter
	std::vector<std::string> GetFullscreenModes(IDXGIFactory5* factory, const std::string_view& adapter_name);

	// returns the fullscreen mode to use for the specified dimensions
	bool GetRequestedExclusiveFullscreenModeDesc(IDXGIFactory5* factory, const RECT& window_rect, u32 width, u32 height,
		float refresh_rate, DXGI_FORMAT format, DXGI_MODE_DESC* fullscreen_mode, IDXGIOutput** output);

	// get an adapter based on name
	wil::com_ptr_nothrow<IDXGIAdapter1> GetAdapterByName(IDXGIFactory5* factory, const std::string_view& name);

	// returns the first adapter in the system
	wil::com_ptr_nothrow<IDXGIAdapter1> GetFirstAdapter(IDXGIFactory5* factory);

	// returns the adapter specified in the configuration, or the default
	wil::com_ptr_nothrow<IDXGIAdapter1> GetChosenOrFirstAdapter(IDXGIFactory5* factory, const std::string_view& name);

	// returns a utf-8 string of the specified adapter's name
	std::string GetAdapterName(IDXGIAdapter1* adapter);

	// returns the driver version from the registry as a string
	std::string GetDriverVersionFromLUID(const LUID& luid);

	// this is sort of a legacy thing that doesn't have much to do with d3d (just the easiest way)
	// checks to see if the adapter at 0 is NV and thus we should prefer OpenGL
	enum class VendorID
	{
		Unknown,
		Nvidia,
		AMD,
		Intel
	};

	VendorID GetVendorID(IDXGIAdapter1* adapter);
	GSRendererType GetPreferredRenderer();

	// D3DCompiler wrapper.
	enum class ShaderType
	{
		Vertex,
		Pixel,
		Compute
	};

	wil::com_ptr_nothrow<ID3DBlob> CompileShader(ShaderType type, D3D_FEATURE_LEVEL feature_level, bool debug,
		const std::string_view& code, const D3D_SHADER_MACRO* macros = nullptr, const char* entry_point = "main");
}; // namespace D3D
