// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Config.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/DX11/D3D.h"
#include "GS/GSExtra.h"
#include "Host.h"

#ifdef _M_X86
#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#endif

#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Path.h"

#include "IconsFontAwesome5.h"

#include <appmodel.h>
#include <array>
#include <d3d11.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <fstream>

#include "fmt/format.h"

static u32 s_next_bad_shader_id = 1;

wil::com_ptr_nothrow<IDXGIFactory5> D3D::CreateFactory(bool debug)
{
	UINT flags = 0;
	if (debug)
		flags |= DXGI_CREATE_FACTORY_DEBUG;

	wil::com_ptr_nothrow<IDXGIFactory5> factory;
	const HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(factory.put()));
	if (FAILED(hr))
		Console.Error("D3D: Failed to create DXGI factory: %08X", hr);

	return factory;
}

static std::string FixupDuplicateAdapterNames(const std::vector<std::string>& adapter_names, std::string adapter_name)
{
	if (std::any_of(adapter_names.begin(), adapter_names.end(),
			[&adapter_name](const std::string& other) { return (adapter_name == other); }))
	{
		std::string original_adapter_name = std::move(adapter_name);

		u32 current_extra = 2;
		do
		{
			adapter_name = fmt::format("{} ({})", original_adapter_name.c_str(), current_extra);
			current_extra++;
		} while (std::any_of(adapter_names.begin(), adapter_names.end(),
			[&adapter_name](const std::string& other) { return (adapter_name == other); }));
	}

	return adapter_name;
}

std::vector<std::string> D3D::GetAdapterNames(IDXGIFactory5* factory)
{
	std::vector<std::string> adapter_names;

	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	for (u32 index = 0;; index++)
	{
		const HRESULT hr = factory->EnumAdapters1(index, adapter.put());
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr))
		{
			Console.Error(fmt::format("IDXGIFactory2::EnumAdapters() returned %08X", hr));
			continue;
		}

		adapter_names.push_back(FixupDuplicateAdapterNames(adapter_names, GetAdapterName(adapter.get())));
	}

	return adapter_names;
}

std::vector<std::string> D3D::GetFullscreenModes(IDXGIFactory5* factory, const std::string_view& adapter_name)
{
	std::vector<std::string> modes;
	HRESULT hr;

	wil::com_ptr_nothrow<IDXGIAdapter1> adapter = GetChosenOrFirstAdapter(factory, adapter_name);
	if (!adapter)
		return modes;

	wil::com_ptr_nothrow<IDXGIOutput> output;
	if (FAILED(hr = adapter->EnumOutputs(0, &output)))
	{
		Console.Error("EnumOutputs() failed: %08X", hr);
		return modes;
	}

	UINT num_modes = 0;
	if (FAILED(hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, nullptr)))
	{
		Console.Error("GetDisplayModeList() failed: %08X", hr);
		return modes;
	}

	std::vector<DXGI_MODE_DESC> dmodes(num_modes);
	if (FAILED(hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, dmodes.data())))
	{
		Console.Error("GetDisplayModeList() (2) failed: %08X", hr);
		return modes;
	}

	for (const DXGI_MODE_DESC& mode : dmodes)
	{
		modes.push_back(GSDevice::GetFullscreenModeString(mode.Width, mode.Height,
			static_cast<float>(mode.RefreshRate.Numerator) / static_cast<float>(mode.RefreshRate.Denominator)));
	}

	return modes;
}

bool D3D::GetRequestedExclusiveFullscreenModeDesc(IDXGIFactory5* factory, const RECT& window_rect, u32 width,
	u32 height, float refresh_rate, DXGI_FORMAT format, DXGI_MODE_DESC* fullscreen_mode, IDXGIOutput** output)
{
	// We need to find which monitor the window is located on.
	const GSVector4i client_rc_vec = GSVector4i::load<false>(&window_rect);

	// The window might be on a different adapter to which we are rendering.. so we have to enumerate them all.
	HRESULT hr;
	wil::com_ptr_nothrow<IDXGIOutput> first_output, intersecting_output;

	for (u32 adapter_index = 0; !intersecting_output; adapter_index++)
	{
		wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
		hr = factory->EnumAdapters1(adapter_index, adapter.put());
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;
		else if (FAILED(hr))
			continue;

		for (u32 output_index = 0;; output_index++)
		{
			wil::com_ptr_nothrow<IDXGIOutput> this_output;
			DXGI_OUTPUT_DESC output_desc;
			hr = adapter->EnumOutputs(output_index, this_output.put());
			if (hr == DXGI_ERROR_NOT_FOUND)
				break;
			else if (FAILED(hr) || FAILED(this_output->GetDesc(&output_desc)))
				continue;

			const GSVector4i output_rc = GSVector4i::load<false>(&output_desc.DesktopCoordinates);
			if (!client_rc_vec.rintersect(output_rc).rempty())
			{
				intersecting_output = std::move(this_output);
				break;
			}

			// Fallback to the first monitor.
			if (!first_output)
				first_output = std::move(this_output);
		}
	}

	if (!intersecting_output)
	{
		if (!first_output)
		{
			Console.Error("No DXGI output found. Can't use exclusive fullscreen.");
			return false;
		}

		Console.Warning("No DXGI output found for window, using first.");
		intersecting_output = std::move(first_output);
	}

	DXGI_MODE_DESC request_mode = {};
	request_mode.Width = width;
	request_mode.Height = height;
	request_mode.Format = format;
	request_mode.RefreshRate.Numerator = static_cast<UINT>(std::floor(refresh_rate * 1000.0f));
	request_mode.RefreshRate.Denominator = 1000u;

	if (FAILED(hr = intersecting_output->FindClosestMatchingMode(&request_mode, fullscreen_mode, nullptr)) ||
		request_mode.Format != format)
	{
		Console.Error("Failed to find closest matching mode, hr=%08X", hr);
		return false;
	}

	*output = intersecting_output.get();
	intersecting_output->AddRef();
	return true;
}

wil::com_ptr_nothrow<IDXGIAdapter1> D3D::GetAdapterByName(IDXGIFactory5* factory, const std::string_view& name)
{
	if (name.empty())
		return {};

	// This might seem a bit odd to cache the names.. but there's a method to the madness.
	// We might have two GPUs with the same name... :)
	std::vector<std::string> adapter_names;

	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	for (u32 index = 0;; index++)
	{
		const HRESULT hr = factory->EnumAdapters1(index, adapter.put());
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr))
		{
			Console.Error(fmt::format("IDXGIFactory2::EnumAdapters() returned %08X", hr));
			continue;
		}

		std::string adapter_name = FixupDuplicateAdapterNames(adapter_names, GetAdapterName(adapter.get()));
		if (adapter_name == name)
		{
			Console.WriteLn(fmt::format("D3D: Found adapter '{}'", adapter_name));
			return adapter;
		}

		adapter_names.push_back(std::move(adapter_name));
	}

	Console.Warning(fmt::format("Adapter '{}' not found.", name));
	return {};
}

wil::com_ptr_nothrow<IDXGIAdapter1> D3D::GetFirstAdapter(IDXGIFactory5* factory)
{
	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	HRESULT hr = factory->EnumAdapters1(0, adapter.put());
	if (FAILED(hr))
		Console.Error(fmt::format("IDXGIFactory2::EnumAdapters() for first adapter returned %08X", hr));

	return adapter;
}

wil::com_ptr_nothrow<IDXGIAdapter1> D3D::GetChosenOrFirstAdapter(IDXGIFactory5* factory, const std::string_view& name)
{
	wil::com_ptr_nothrow<IDXGIAdapter1> adapter = GetAdapterByName(factory, name);
	if (!adapter)
		adapter = GetFirstAdapter(factory);

	return adapter;
}

std::string D3D::GetAdapterName(IDXGIAdapter1* adapter)
{
	std::string ret;

	DXGI_ADAPTER_DESC1 desc;
	HRESULT hr = adapter->GetDesc1(&desc);
	if (SUCCEEDED(hr))
	{
		ret = StringUtil::WideStringToUTF8String(desc.Description);
	}
	else
	{
		Console.Error(fmt::format("IDXGIAdapter1::GetDesc() returned {:08X}", hr));
	}

	if (ret.empty())
		ret = "(Unknown)";

	return ret;
}

std::string D3D::GetDriverVersionFromLUID(const LUID& luid)
{
	std::string ret;

	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\DirectX", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD max_key_len = 0, adapter_count = 0;
		if (RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &adapter_count, &max_key_len, nullptr, nullptr, nullptr,
				nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			std::vector<TCHAR> current_name(max_key_len + 1);
			for (DWORD i = 0; i < adapter_count; ++i)
			{
				DWORD subKeyLength = static_cast<DWORD>(current_name.size());
				if (RegEnumKeyExW(hKey, i, current_name.data(), &subKeyLength, nullptr, nullptr, nullptr, nullptr) ==
					ERROR_SUCCESS)
				{
					LUID current_luid = {};
					DWORD current_luid_size = sizeof(uint64_t);
					if (RegGetValueW(hKey, current_name.data(), L"AdapterLuid", RRF_RT_QWORD, nullptr, &current_luid,
							&current_luid_size) == ERROR_SUCCESS &&
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

#ifdef _M_X86

D3D::VendorID D3D::GetVendorID(IDXGIAdapter1* adapter)
{
	DXGI_ADAPTER_DESC1 desc;
	const HRESULT hr = adapter->GetDesc1(&desc);
	if (FAILED(hr))
	{
		Console.Error(fmt::format("IDXGIAdapter1::GetDesc() returned {:08X}", hr));
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

GSRendererType D3D::GetPreferredRenderer()
{
	const auto factory = CreateFactory(false);
	const auto adapter = GetChosenOrFirstAdapter(factory.get(), GSConfig.Adapter);

	// If we somehow can't get a D3D11 device, it's unlikely any of the renderers are going to work.
	if (!adapter)
		return GSRendererType::DX11;

	const auto get_d3d11_feature_level = [&adapter]() -> std::optional<D3D_FEATURE_LEVEL> {
		static const D3D_FEATURE_LEVEL check[] = {
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_0,
		};

		D3D_FEATURE_LEVEL feature_level;
		const HRESULT hr = D3D11CreateDevice(adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, std::data(check),
			std::size(check), D3D11_SDK_VERSION, nullptr, &feature_level, nullptr);

		if (FAILED(hr))
		{
			Console.Error("D3D11CreateDevice() for automatic renderer failed: %08X", hr);
			return std::nullopt;
		}

		Console.WriteLn("D3D11 feature level for autodetection: %x", static_cast<unsigned>(feature_level));
		return feature_level;
	};
	const auto get_d3d12_device = [&adapter]() {
		wil::com_ptr_nothrow<ID3D12Device> device;
		const HRESULT hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.put()));
		if (FAILED(hr))
			Console.Error("D3D12CreateDevice() for automatic renderer failed: %08X", hr);
		return device;
	};
	static constexpr auto check_for_mapping_layers = []() {
		PCWSTR familyName = L"Microsoft.D3DMappingLayers_8wekyb3d8bbwe";
		UINT32 numPackages = 0, bufferLength = 0;
		const DWORD error = GetPackagesByPackageFamily(familyName, &numPackages, nullptr, &bufferLength, nullptr);
		if (error == ERROR_INSUFFICIENT_BUFFER || numPackages > 0)
		{
			Host::AddIconOSDMessage("VKDriverUnsupported", ICON_FA_TV,
				TRANSLATE_STR("GS",
					"Your system has the \"OpenCL, OpenGL, and Vulkan Compatibility Pack\" installed.\n"
					"This Vulkan driver crashes PCSX2 on some GPUs.\n" 
					"To use the Vulkan renderer, you should remove this app package."),
				Host::OSD_WARNING_DURATION);
			return true;
		}

		return false;
	};
	static constexpr auto check_vulkan_supported = []() {
		// Don't try to enumerate Vulkan devices if the DX12 Vulkan driver is present.
		// It crashes on AMD GPUs.
		if (check_for_mapping_layers())
			return false;

		std::vector<std::string> vk_adapter_names;
		GSDeviceVK::GetAdaptersAndFullscreenModes(&vk_adapter_names, nullptr);
		if (!vk_adapter_names.empty())
			return true;

		Host::AddIconOSDMessage("VKDriverUnsupported", ICON_FA_TV, TRANSLATE_STR("GS",
			"The Vulkan renderer was automatically selected, but no compatible devices were found.\n"
			"       You should update all graphics drivers in your system, including any integrated GPUs\n"
			"       to use the Vulkan renderer."), Host::OSD_WARNING_DURATION);
		return false;
	};

	switch (GetVendorID(adapter.get()))
	{
		case VendorID::Nvidia:
		{
			const std::optional<D3D_FEATURE_LEVEL> feature_level = get_d3d11_feature_level();
			if (!feature_level.has_value())
				return GSRendererType::DX11;
			else if (feature_level == D3D_FEATURE_LEVEL_12_0)
				return check_vulkan_supported() ? GSRendererType::VK : GSRendererType::OGL;
			else if (feature_level == D3D_FEATURE_LEVEL_11_0)
				return GSRendererType::OGL;
			else
				return GSRendererType::DX11;
		}

		case VendorID::AMD:
		{
			const std::optional<D3D_FEATURE_LEVEL> feature_level = get_d3d11_feature_level();
			if (!feature_level.has_value())
				return GSRendererType::DX11;
			else if (feature_level == D3D_FEATURE_LEVEL_12_0)
				return check_vulkan_supported() ? GSRendererType::VK : GSRendererType::DX11;
			else
				return GSRendererType::DX11;
		}

		case VendorID::Intel:
		{
			// Older Intel GPUs prior to Xe seem to have broken OpenGL drivers which choke
			// on some of our shaders, causing what appears to be GPU timeouts+device removals.
			// Vulkan has broken barriers, also prior to Xe.

			// Sampler feedback Tier 0.9 is only present in Tiger Lake/Xe/Arc, so we can use that to
			// differentiate between them. Unfortunately, that requires a D3D12 device.
			const auto device12 = get_d3d12_device();
			if (device12)
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts = {};
				if (SUCCEEDED(device12->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &opts, sizeof(opts))) &&
					opts.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_0_9)
				{
					Console.WriteLn("Sampler feedback tier 0.9 found for Intel GPU, defaulting to Vulkan.");
					return check_vulkan_supported() ? GSRendererType::VK : GSRendererType::DX11;
				}
			}

			Console.WriteLn("Sampler feedback tier 0.9 or Direct3D 12 not found for Intel GPU, using Direct3D 11.");
			return GSRendererType::DX11;
		}
		break;

		default:
		{
			// Default is D3D11
			return GSRendererType::DX11;
		}
	}
}

#endif // _M_X86

wil::com_ptr_nothrow<ID3DBlob> D3D::CompileShader(D3D::ShaderType type, D3D_FEATURE_LEVEL feature_level, bool debug,
	const std::string_view& code, const D3D_SHADER_MACRO* macros /* = nullptr */,
	const char* entry_point /* = "main" */)
{
	const char* target;
	switch (feature_level)
	{
		case D3D_FEATURE_LEVEL_11_0:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_0", "ps_5_0", "cs_5_0"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case D3D_FEATURE_LEVEL_11_1:
		default:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_1", "ps_5_1", "cs_5_1"}};
			target = targets[static_cast<int>(type)];
		}
		break;
	}

	static constexpr UINT flags_non_debug = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	static constexpr UINT flags_debug = D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;

	wil::com_ptr_nothrow<ID3DBlob> blob;
	wil::com_ptr_nothrow<ID3DBlob> error_blob;
	const HRESULT hr = D3DCompile(code.data(), code.size(), "0", macros, nullptr, entry_point, target,
		debug ? flags_debug : flags_non_debug, 0, blob.put(), error_blob.put());

	std::string error_string;
	if (error_blob)
	{
		error_string.append(static_cast<const char*>(error_blob->GetBufferPointer()), error_blob->GetBufferSize());
		error_blob.reset();
	}

	if (FAILED(hr))
	{
		Console.WriteLn("Failed to compile '%s':\n%s", target, error_string.c_str());

		std::ofstream ofs(Path::Combine(EmuFolders::Logs, fmt::format("pcsx2_bad_shader_{}.txt", s_next_bad_shader_id++)),
			std::ofstream::out | std::ofstream::binary);
		if (ofs.is_open())
		{
			ofs << code;
			ofs << "\n\nCompile as " << target << " failed: " << hr << "\n";
			ofs.write(error_string.c_str(), error_string.size());
			ofs << "\n";
			if (macros)
			{
				for (const D3D_SHADER_MACRO* macro = macros; macro->Name != nullptr; macro++)
					ofs << "#define " << macro->Name << " " << macro->Definition << "\n";
			}
			ofs.close();
		}

		return {};
	}

	if (!error_string.empty())
		Console.Warning("'%s' compiled with warnings:\n%s", target, error_string.c_str());

	return blob;
}
