// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Config.h"
#include "GS/GSShaderCompileIndicator.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/DX11/D3D.h"
#include "GS/GSExtra.h"
#include "Host.h"

#ifdef ARCH_X86
#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#endif

#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Path.h"

#include "IconsFontAwesome.h"

#include <array>
#include <d3d11.h>
#include <directx/d3d12.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
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

static std::string FixupDuplicateAdapterNames(const std::vector<GSAdapterInfo>& adapters, std::string adapter_name)
{
	if (std::any_of(adapters.begin(), adapters.end(),
			[&adapter_name](const GSAdapterInfo& other) { return (adapter_name == other.name); }))
	{
		std::string original_adapter_name = std::move(adapter_name);

		u32 current_extra = 2;
		do
		{
			adapter_name = fmt::format("{} ({})", original_adapter_name.c_str(), current_extra);
			current_extra++;
		} while (std::any_of(adapters.begin(), adapters.end(),
			[&adapter_name](const GSAdapterInfo& other) { return (adapter_name == other.name); }));
	}

	return adapter_name;
}

std::vector<GSAdapterInfo> D3D::GetAdapterInfo(IDXGIFactory5* factory)
{
	std::vector<GSAdapterInfo> adapters;

	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	for (u32 index = 0;; index++)
	{
		HRESULT hr = factory->EnumAdapters1(index, adapter.put());
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr))
		{
			ERROR_LOG("IDXGIFactory2::EnumAdapters() returned {:08X}", static_cast<unsigned>(hr));
			continue;
		}

		GSAdapterInfo ai;
		ai.name = FixupDuplicateAdapterNames(adapters, GetAdapterName(adapter.get()));

		// Unfortunately we can't get any properties such as feature level without creating the device.
		// So just assume a max of the D3D11 max across the board.
		ai.max_texture_size = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		ai.max_upscale_multiplier = GSGetMaxUpscaleMultiplier(ai.max_texture_size);

		wil::com_ptr_nothrow<IDXGIOutput> output;
		// Only check the first output, which would be the primary display (if any is connected)
		if (SUCCEEDED(hr = adapter->EnumOutputs(0, &output)))
		{
			UINT num_modes = 0;
			if (SUCCEEDED(hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, nullptr)))
			{
				std::vector<DXGI_MODE_DESC> dmodes(num_modes);
				if (SUCCEEDED(hr = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, dmodes.data())))
				{
					for (const DXGI_MODE_DESC& mode : dmodes)
					{
						ai.fullscreen_modes.push_back(GSDevice::GetFullscreenModeString(mode.Width, mode.Height,
							static_cast<float>(mode.RefreshRate.Numerator) / static_cast<float>(mode.RefreshRate.Denominator)));
					}
				}
				else
				{
					ERROR_LOG("GetDisplayModeList() (2) failed: {:08X}", static_cast<unsigned>(hr));
				}
			}
			else
			{
				ERROR_LOG("GetDisplayModeList() failed: {:08X}", static_cast<unsigned>(hr));
			}
		}
		else if (hr != DXGI_ERROR_NOT_FOUND)
		{
			ERROR_LOG("EnumOutputs() failed: {:08X}", static_cast<unsigned>(hr));
		}

		adapters.push_back(std::move(ai));
	}

	return adapters;
}

bool D3D::GetRequestedExclusiveFullscreenModeDesc(IDXGIFactory5* factory, HWND window_hwnd, u32 width,
	u32 height, float refresh_rate, DXGI_FORMAT format, DXGI_MODE_DESC* fullscreen_mode, IDXGIOutput** output)
{
	// We need to find which monitor the window is located on.
	// DXGI seems to use the nearest monitor if the window is out of bounds.
	const auto* monitor = MonitorFromWindow(window_hwnd, MONITOR_DEFAULTTONEAREST);

	// The monitor might be on a different adapter to which we are rendering.. so we have to enumerate them all.
	HRESULT hr;
	wil::com_ptr_nothrow<IDXGIOutput> first_output, monitor_output;

	for (u32 adapter_index = 0; !monitor_output; adapter_index++)
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

			if (output_desc.Monitor == monitor)
			{
				monitor_output = std::move(this_output);
				break;
			}

			// Fallback to the first monitor.
			if (!first_output)
				first_output = std::move(this_output);
		}
	}

	if (!monitor_output)
	{
		if (!first_output)
		{
			Console.Error("No DXGI output found. Can't use exclusive fullscreen.");
			return false;
		}

		Console.Warning("No DXGI output found for window, using first.");
		monitor_output = std::move(first_output);
	}

	DXGI_MODE_DESC request_mode = {};
	request_mode.Width = width;
	request_mode.Height = height;
	request_mode.Format = format;
	request_mode.RefreshRate.Numerator = static_cast<UINT>(std::floor(refresh_rate * 1000.0f));
	request_mode.RefreshRate.Denominator = 1000u;

	if (FAILED(hr = monitor_output->FindClosestMatchingMode(&request_mode, fullscreen_mode, nullptr)) ||
		request_mode.Format != format)
	{
		ERROR_LOG("Failed to find closest matching mode, hr={:08X}", static_cast<unsigned>(hr));
		return false;
	}

	*output = monitor_output.get();
	monitor_output->AddRef();
	return true;
}

wil::com_ptr_nothrow<IDXGIAdapter1> D3D::GetAdapterByName(IDXGIFactory5* factory, const std::string_view name)
{
	if (name.empty() || name == GetDefaultAdapter())
		return {};

	// This might seem a bit odd to cache the names.. but there's a method to the madness.
	// We might have two GPUs with the same name... :)
	std::vector<GSAdapterInfo> adapter_names;

	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	for (u32 index = 0;; index++)
	{
		const HRESULT hr = factory->EnumAdapters1(index, adapter.put());
		if (hr == DXGI_ERROR_NOT_FOUND)
			break;

		if (FAILED(hr))
		{
			ERROR_LOG("IDXGIFactory2::EnumAdapters() returned {:08X}", static_cast<unsigned>(hr));
			continue;
		}

		GSAdapterInfo ai;
		ai.name = FixupDuplicateAdapterNames(adapter_names, GetAdapterName(adapter.get()));
		if (ai.name == name)
		{
			INFO_LOG("D3D: Found adapter '{}'", ai.name);
			return adapter;
		}

		adapter_names.push_back(std::move(ai));
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

wil::com_ptr_nothrow<IDXGIAdapter1> D3D::GetChosenOrFirstAdapter(IDXGIFactory5* factory, const std::string_view name)
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
			D3D_FEATURE_LEVEL_10_0,
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
		const HRESULT hr = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(device.put()));
		if (FAILED(hr))
			Console.Error("D3D12CreateDevice() for automatic renderer failed: %08X", hr);
		return device;
	};
#ifdef ENABLE_VULKAN
	static constexpr auto check_vulkan_supported = []() {
		if (!GSDeviceVK::EnumerateGPUs().empty())
			return true;

		Host::AddIconOSDMessage("VKDriverUnsupported", ICON_FA_TV, TRANSLATE_STR("GS",
			"The Vulkan graphics API was automatically selected, but no compatible devices were found.\n"
			"       You should update all graphics drivers in your system, including any integrated GPUs\n"
			"       to use the Vulkan renderer."), Host::OSD_WARNING_DURATION);
		return false;
	};
#else
	static constexpr auto check_vulkan_supported = []() { return false; };
#endif

	switch (GetVendorID(adapter.get()))
	{
		case VendorID::Nvidia:
		{
			const std::optional<D3D_FEATURE_LEVEL> feature_level = get_d3d11_feature_level();
			if (!feature_level.has_value())
				return GSRendererType::DX11;
			else if (feature_level == D3D_FEATURE_LEVEL_12_0)
				//return check_vulkan_supported() ? GSRendererType::VK : GSRendererType::OGL;
				return GSRendererType::DX12;
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
				//return check_vulkan_supported() ? GSRendererType::VK : GSRendererType::DX12;
				return GSRendererType::DX12;
			else if (feature_level == D3D_FEATURE_LEVEL_11_1)
				return GSRendererType::DX12;
			else
				return GSRendererType::DX11;
		}

		case VendorID::Intel:
		{
			// Vulkan has broken barriers, prior to Xe.

			// Sampler feedback Tier 0.9 is only present in Tiger Lake/Xe/Arc, so we can use that to
			// differentiate between them. Unfortunately, that requires a D3D12 device.
			const auto device12 = get_d3d12_device();
			if (device12)
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts = {};
				if (SUCCEEDED(device12->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &opts, sizeof(opts))) &&
					(opts.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_0_9) &&
					check_vulkan_supported())
				{
					Console.WriteLn("Sampler feedback tier 0.9 found for Intel GPU, defaulting to Vulkan.");
					return GSRendererType::VK;
				}
				else
				{
					Console.WriteLn("Sampler feedback tier 0.9 or Vulkan not found for Intel GPU, using OpenGL.");
					return GSRendererType::OGL;
				}
			}

			Console.WriteLn("Sampler feedback tier 0.9 or Direct3D 12 not found for Intel GPU, using Direct3D 11.");
			return GSRendererType::DX11;
		}
		break;

		default:
		{
			// Default is D3D11, but prefer DX12 on ARM (better drivers).
#ifdef ARCH_ARM64
			return GSRendererType::DX12;
#else
			return GSRendererType::DX11;
#endif
		}
	}
}

const char* D3D::ShaderModelToCacheString(D3D::ShaderModel shader_model)
{
	switch (shader_model)
	{
		case ShaderModel::SM40:
			return "sm40";
		case ShaderModel::SM41:
			return "sm41";
		case ShaderModel::SM50:
			return "sm50";
		case ShaderModel::SM51:
		case ShaderModel::SM60:
		case ShaderModel::SM61:
		case ShaderModel::SM62:
		case ShaderModel::SM63:
		case ShaderModel::SM64:
			return "sm51";
		case ShaderModel::SM65:
			return "sm65";
		default:
			return "unk";
	}
}

// Not COM
struct FxcResourceIncludeHandler final : ID3DInclude
{
private:
	std::unordered_map<std::string, std::string> includes;
	std::map<const char*, std::string> file_dir;
	std::vector<std::unique_ptr<const char[]>> opened_files;

public:
	explicit FxcResourceIncludeHandler(const std::unordered_map<std::string, std::string>& includes)
	{
		this->includes = includes;
	}
	virtual ~FxcResourceIncludeHandler() = default;

	virtual HRESULT Open(D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
	{
		if (ppData == nullptr || pBytes == nullptr)
			return E_POINTER;

		const std::string filename = Path::Canonicalize(pFileName);

		std::unordered_map<std::string, std::string>::const_iterator iter;
		if (pParentData != nullptr)
		{
			// Search same directory as file.
			const std::string path = Path::Canonicalize(Path::Combine(file_dir[reinterpret_cast<const char*>(pParentData)], filename));

			iter = includes.find(path);
			if (iter == includes.end())
			{
				// Search root directory.
				iter = includes.find(filename);
			}
		}
		else
			iter = includes.find(filename);

		if (iter != includes.end())
		{
			std::unique_ptr<char[]> source_data = std::make_unique_for_overwrite<char[]>(iter->second.length());
			std::memcpy(source_data.get(), iter->second.c_str(), iter->second.length());
			*ppData = source_data.get();
			*pBytes = iter->second.length();

			file_dir.emplace(source_data.get(), Path::GetDirectory(iter->first));
			opened_files.push_back(std::move(source_data));
			return S_OK;
		}

		*ppData = nullptr;
		*pBytes = 0;
		return E_FAIL;
	}

	virtual HRESULT Close(LPCVOID pData) override
	{
		for (std::vector<std::unique_ptr<const char[]>>::iterator it = opened_files.begin(); it != opened_files.end();)
		{
			if (pData == it->get())
			{
				file_dir.erase(it->get());
				opened_files.erase(it);
				return S_OK;
			}
		}

		return E_FAIL;
	}
};

// COM
struct DxcResourceIncludeHandler final : IDxcIncludeHandler
{
private:
	std::unordered_map<std::string, std::string> includes;
	wil::com_ptr_nothrow<IDxcUtils> utils;
	u32 ref_count;

public:
	explicit DxcResourceIncludeHandler(const std::unordered_map<std::string, std::string>& includes)
	{
		this->includes = includes;
		const HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.put()));
		if (FAILED(hr))
			utils = nullptr;
		ref_count = 1;
	}
	virtual ~DxcResourceIncludeHandler() = default;

	virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (ppvObject == nullptr)
			return E_POINTER;

		if (riid == IID_IUnknown || riid == __uuidof(IDxcIncludeHandler))
		{
			*ppvObject = this;
			AddRef();
			return S_OK;
		}
		else
		{
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}
	}

	virtual ULONG AddRef() override
	{
		ref_count++;
		return ref_count;
	}

	virtual ULONG Release() override
	{
		const u32 ret = ref_count--;
		if (ref_count == 0)
			delete this;
		return ret;
	}

	virtual HRESULT LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
	{
		if (ppIncludeSource == nullptr)
			return E_POINTER;

		if (utils != nullptr)
		{
			std::string filename = StringUtil::WideStringToUTF8String(pFilename);
			Path::Canonicalize(&filename);

			const auto iter = includes.find(filename);
			if (iter != includes.end())
			{
				IDxcBlobEncoding* source_blob;
				const HRESULT hr = utils->CreateBlob(iter->second.c_str(), iter->second.length(), CP_UTF8, &source_blob);
				if (SUCCEEDED(hr))
				{
					*ppIncludeSource = source_blob;
					return S_OK;
				}
			}
		}

		*ppIncludeSource = nullptr;
		return E_FAIL;
	}
};

wil::com_ptr_nothrow<ID3DBlob> D3D::CompileShaderDXBC(D3D::ShaderType type, D3D::ShaderModel shader_model, bool debug,
	const std::string_view code, const char* name, const D3D_SHADER_MACRO* macros /* = nullptr */,
	const char* entry_point /* = "main" */, const std::unordered_map<std::string, std::string>& includes /*= {} */)
{
	const GSShaderCompileIndicator::CompileTimer compile_timer;

	const char* target;
	switch (shader_model)
	{
		case ShaderModel::SM40:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_4_0", "ps_4_0", "cs_4_0"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case ShaderModel::SM41:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_4_1", "ps_4_1", "cs_4_1"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case ShaderModel::SM50:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_0", "ps_5_0", "cs_5_0"}};
			target = targets[static_cast<int>(type)];
		}
		break;

		case ShaderModel::SM51:
		default:
		{
			static constexpr std::array<const char*, 4> targets = {{"vs_5_1", "ps_5_1", "cs_5_1"}};
			target = targets[static_cast<int>(type)];
		}
		break;
	}

	static constexpr UINT flags_non_debug = D3DCOMPILE_OPTIMIZATION_LEVEL3;
	static constexpr UINT flags_debug = D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG | D3DCOMPILE_DEBUG_NAME_FOR_SOURCE;

	// Untested
	std::unique_ptr<ID3DInclude> pInclude{nullptr};
	if (!includes.empty())
		pInclude = std::make_unique<FxcResourceIncludeHandler>(includes);

	wil::com_ptr_nothrow<ID3DBlob> blob;
	wil::com_ptr_nothrow<ID3DBlob> error_blob;
	const HRESULT hr = D3DCompile(code.data(), code.size(), name, macros, pInclude.get(), entry_point, target,
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

wil::com_ptr_nothrow<ID3DBlob> D3D::CompileShaderDXIL(D3D::ShaderType type, D3D::ShaderModel shader_model, bool debug,
	const std::string_view code, const char* name, const D3D_SHADER_MACRO* macros /* = nullptr */,
	const char* entry_point /* = "main" */, const std::unordered_map<std::string, std::string>& includes /*= {} */)
{
	const GSShaderCompileIndicator::CompileTimer compile_timer;

	const wchar_t* target;
	switch (shader_model)
	{
		case ShaderModel::SM60:
		case ShaderModel::SM61:
		case ShaderModel::SM62:
		case ShaderModel::SM63:
		case ShaderModel::SM64:
			pxAssert(false);
			break;
		case ShaderModel::SM65:
		default:
		{
			static constexpr std::array<const wchar_t*, 4> targets = {{L"vs_6_5", L"ps_6_5", L"cs_6_5"}};
			target = targets[static_cast<int>(type)];
		}
		break;
	}

	// Build arguments.
	std::vector<const wchar_t*> args;
	std::wstring wentry_point;
	std::wstring wname;
	std::vector<std::wstring> wdefines; // cppcheck-suppress variableScope
	if (entry_point)
	{
		wentry_point = StringUtil::UTF8StringToWideString(entry_point);
		args.push_back(L"-E");
		args.push_back(wentry_point.c_str());
	}
	args.push_back(L"-T");
	args.push_back(target);

	if (debug)
	{
		args.push_back(L"-INPUT");
		wname = StringUtil::UTF8StringToWideString(name);
		args.push_back(wname.c_str());
		args.push_back(L"-Od");
		args.push_back(L"-Zi");
		args.push_back(L"-Zss");
		args.push_back(L"-Qembed_debug");
	}
	else
	{
		args.push_back(L"-O3");
		args.push_back(L"-Qstrip_reflect");
	}

	if (macros)
	{
		for (const D3D_SHADER_MACRO* macro = macros; macro->Name != nullptr; macro++)
			wdefines.push_back(StringUtil::UTF8StringToWideString(fmt::format("{}={}", macro->Name, macro->Definition)));

		// Use a seperate loop to avoid invalidating the pointer returned from c_str().
		for (const std::wstring& define : wdefines)
		{
			args.push_back(L"-D");
			args.push_back(define.c_str());
		}
	}

	wil::com_ptr_nothrow<IDxcIncludeHandler> pInclude;
	if (!includes.empty())
		*pInclude.put() = new DxcResourceIncludeHandler(includes);

	// Compile Shader.
	wil::com_ptr_nothrow<IDxcCompiler3> pCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(pCompiler.put()));

	const DxcBuffer source{code.data(), code.length(), DXC_CP_UTF8};
	wil::com_ptr_nothrow<IDxcResult> pResults;
	HRESULT hr = pCompiler->Compile(&source, args.data(), args.size(), pInclude.get(), IID_PPV_ARGS(pResults.put()));

	if (FAILED(hr))
	{
		Console.WriteLn("Compiler Failed");
		return {};
	}

	wil::com_ptr_nothrow<IDxcBlobUtf8> error_string = nullptr;
	pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(error_string.put()), nullptr);

	pResults->GetStatus(&hr);
	if (FAILED(hr))
	{
		std::string target_utf8 = StringUtil::WideStringToUTF8String(target);
		Console.WriteLn("Failed to compile '%s':\n%s", target_utf8.c_str(), error_string->GetStringPointer());

		std::ofstream ofs(Path::Combine(EmuFolders::Logs, fmt::format("pcsx2_bad_shader_{}.txt", s_next_bad_shader_id++)),
			std::ofstream::out | std::ofstream::binary);
		if (ofs.is_open())
		{
			ofs << code;
			ofs << "\n\nCompile as " << target_utf8.c_str() << " failed: " << hr << "\n";
			ofs.write(error_string->GetStringPointer(), error_string->GetStringLength());
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

	if (error_string->GetStringLength() != 0)
		Console.Warning("'%s' compiled with warnings:\n%s", StringUtil::WideStringToUTF8String(target).c_str(), error_string->GetStringPointer());

	wil::com_ptr_nothrow<ID3DBlob> blob = nullptr;
	pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(blob.put()), nullptr);

	return blob;
}

wil::com_ptr_nothrow<ID3DBlob> D3D::CompileShader(D3D::ShaderType type, D3D::ShaderModel shader_model, bool debug,
	const std::string_view code, const char* name, const D3D_SHADER_MACRO* macros /* = nullptr */,
	const char* entry_point /* = "main" */, const std::unordered_map<std::string, std::string>& includes /*= {} */)
{
	if (static_cast<int>(shader_model) < 0x65)
		return CompileShaderDXBC(type, shader_model, debug, code, name, macros, entry_point, includes);
	else
		return CompileShaderDXIL(type, shader_model, debug, code, name, macros, entry_point, includes);
}
