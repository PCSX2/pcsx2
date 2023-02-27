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
#include "GS/Renderers/DX11/D3D.h"
#include "GS/GSExtra.h"

#include "common/StringUtil.h"

#include <d3d11.h>

#include "fmt/format.h"

namespace D3D
{
	wil::com_ptr_nothrow<IDXGIFactory2> CreateFactory(bool debug)
	{
		UINT flags = 0;
		if (debug)
			flags |= DXGI_CREATE_FACTORY_DEBUG;

		// we use CreateDXGIFactory2 because we assume at least windows 8.1 anyway
		wil::com_ptr_nothrow<IDXGIFactory2> factory;
		HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(factory.put()));

		// if we failed to create a factory with debug support
		// try one without
		if (FAILED(hr) && debug)
		{
			fprintf(stderr, "D3D: failed to create debug dxgi factory, trying without debugging\n");
			hr = CreateDXGIFactory2(0, IID_PPV_ARGS(factory.put()));;
		}

		if (FAILED(hr))
		{
			fprintf(stderr, "D3D: failed to create dxgi factory\n"
				"check that your system meets our minimum requirements:\n"
				"https://github.com/PCSX2/pcsx2#system-requirements\n");
		}

		return factory;
	}

	wil::com_ptr_nothrow<IDXGIAdapter1> GetAdapterFromIndex(IDXGIFactory2* factory, int index)
	{
		ASSERT(factory);

		wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
		if (index < 0 || factory->EnumAdapters1(index, adapter.put()) == DXGI_ERROR_NOT_FOUND)
		{
			// try index 0 (default adapter)
			if (index >= 0)
				fprintf(stderr, "D3D: adapter not found, falling back to the default\n");
			if (FAILED(factory->EnumAdapters1(0, adapter.put())))
			{
				// either there are no adapters connected or something major is wrong with the system
				fprintf(stderr, "D3D: failed to EnumAdapters\n");
			}
		}

		return adapter;
	}

	std::string GetDriverVersionFromLUID(const LUID& luid)
	{
		std::string ret;

		HKEY hKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\DirectX", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD max_key_len = 0, adapter_count = 0;
			if (RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &adapter_count, &max_key_len, nullptr, nullptr,
					nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				std::vector<TCHAR> current_name(max_key_len + 1);
				for (DWORD i = 0; i < adapter_count; ++i)
				{
					DWORD subKeyLength = static_cast<DWORD>(current_name.size());
					if (RegEnumKeyExW(hKey, i, current_name.data(), &subKeyLength, nullptr, nullptr, nullptr,
							nullptr) == ERROR_SUCCESS)
					{
						LUID current_luid = {};
						DWORD current_luid_size = sizeof(uint64_t);
						if (RegGetValueW(hKey, current_name.data(), L"AdapterLuid", RRF_RT_QWORD, nullptr,
								&current_luid, &current_luid_size) == ERROR_SUCCESS &&
							current_luid.HighPart == luid.HighPart && current_luid.LowPart == luid.LowPart)
						{
							LARGE_INTEGER driver_version = {};
							DWORD driver_version_size = sizeof(driver_version);
							if (RegGetValueW(hKey, current_name.data(), L"DriverVersion", RRF_RT_QWORD, nullptr,
									&driver_version, &driver_version_size) == ERROR_SUCCESS)
							{
								WORD nProduct = HIWORD(driver_version.HighPart);
								WORD nVersion = LOWORD(driver_version.HighPart);
								WORD nSubVersion = HIWORD(driver_version.LowPart);
								WORD nBuild = LOWORD(driver_version.LowPart);
								ret = fmt::format("{}.{}.{}.{}", nProduct, nVersion, nSubVersion, nBuild);
							}
						}
					}
				}
			}

			RegCloseKey(hKey);
		}

		return ret;
	}

	u8 Vendor()
	{
		auto factory = CreateFactory(false);
		auto adapter = GetAdapterFromIndex(factory.get(), 0);

		ASSERT(adapter);

		DXGI_ADAPTER_DESC1 desc = {};
		if (FAILED(adapter->GetDesc1(&desc)))
		{
			fprintf(stderr, "D3D: failed to get the adapter description\n");
			return VendorID::Unknown;
		}

		switch (desc.VendorId)
		{
			case 0x10DE:
				return VendorID::Nvidia;
			case 0x1002:
			case 0x1022:
				return VendorID::AMD;
			case 0x163C:
			case 0x8086:
			case 0x8087:
				return VendorID::Intel;
			default:
				return VendorID::Unknown;
		}
	}

	u8 ShouldPreferRenderer()
	{
		auto factory = CreateFactory(false);
		auto adapter = GetAdapterFromIndex(factory.get(), 0);

		ASSERT(adapter);

		D3D_FEATURE_LEVEL feature_level;

		static const D3D_FEATURE_LEVEL check[] = {
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_0,
		};

		const HRESULT hr = D3D11CreateDevice(
			adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0,
			std::data(check), std::size(check), D3D11_SDK_VERSION, nullptr, &feature_level, nullptr
		);

		if (FAILED(hr))
			return Renderer::Default;

		switch (Vendor())
		{
			case VendorID::Nvidia:
			{
				if (feature_level == D3D_FEATURE_LEVEL_12_0)
					return Renderer::Vulkan;
				else if (feature_level == D3D_FEATURE_LEVEL_11_0)
					return Renderer::OpenGL;
				else
					return Renderer::Direct3D11;
			}

			case VendorID::AMD:
			{
				if (feature_level == D3D_FEATURE_LEVEL_12_0)
					return Renderer::Vulkan;
				else
					return Renderer::Direct3D11;
			}

			case VendorID::Intel:
			{
				// Older Intel GPUs prior to Xe seem to have broken OpenGL drivers which choke
				// on some of our shaders, causing what appears to be GPU timeouts+device removals.
				// Vulkan has broken barriers, also prior to Xe. So just fall back to DX11 everywhere,
				// unless we can find some way of differentiating Xe.
				return Renderer::Direct3D11;
			}

			default:
			{
				// Default is D3D11
				return Renderer::Direct3D11;
			}
		}
	}
}
