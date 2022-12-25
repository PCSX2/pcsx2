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

#include "Frontend/D3D11HostDisplay.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <array>
#include <dxgi1_5.h>

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/RedtapeWindows.h"

#include "Config.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class D3D11HostDisplayTexture : public HostDisplayTexture
{
public:
	D3D11HostDisplayTexture(wil::com_ptr_nothrow<ID3D11Texture2D> texture,
		wil::com_ptr_nothrow<ID3D11ShaderResourceView> srv, u32 width, u32 height, bool dynamic)
		: m_texture(std::move(texture))
		, m_srv(std::move(srv))
		, m_width(width)
		, m_height(height)
		, m_dynamic(dynamic)
	{
	}
	~D3D11HostDisplayTexture() override = default;

	void* GetHandle() const override { return m_srv.get(); }
	u32 GetWidth() const override { return m_width; }
	u32 GetHeight() const override { return m_height; }

	__fi ID3D11Texture2D* GetD3DTexture() const { return m_texture.get(); }
	__fi ID3D11ShaderResourceView* GetD3DSRV() const { return m_srv.get(); }
	__fi ID3D11ShaderResourceView* const* GetD3DSRVArray() const { return m_srv.addressof(); }
	__fi bool IsDynamic() const { return m_dynamic; }

private:
	wil::com_ptr_nothrow<ID3D11Texture2D> m_texture;
	wil::com_ptr_nothrow<ID3D11ShaderResourceView> m_srv;
	u32 m_width;
	u32 m_height;
	bool m_dynamic;
};

D3D11HostDisplay::D3D11HostDisplay() = default;

D3D11HostDisplay::~D3D11HostDisplay()
{
	D3D11HostDisplay::DestroySurface();
	m_context.reset();
	m_device.reset();
}

RenderAPI D3D11HostDisplay::GetRenderAPI() const
{
	return RenderAPI::D3D11;
}

void* D3D11HostDisplay::GetDevice() const
{
	return m_device.get();
}

void* D3D11HostDisplay::GetContext() const
{
	return m_context.get();
}

void* D3D11HostDisplay::GetSurface() const
{
	return m_swap_chain.get();
}

bool D3D11HostDisplay::HasDevice() const
{
	return static_cast<bool>(m_device);
}

bool D3D11HostDisplay::HasSurface() const
{
	return static_cast<bool>(m_swap_chain);
}

std::unique_ptr<HostDisplayTexture> D3D11HostDisplay::CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic /* = false */)
{
	const CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1u, 1u,
		D3D11_BIND_SHADER_RESOURCE, dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT,
		dynamic ? D3D11_CPU_ACCESS_WRITE : 0, 1, 0, 0);
	const D3D11_SUBRESOURCE_DATA srd{data, data_stride, data_stride * height};
	ComPtr<ID3D11Texture2D> texture;
	HRESULT hr = m_device->CreateTexture2D(&desc, data ? &srd : nullptr, texture.addressof());
	if (FAILED(hr))
		return {};

	const CD3D11_SHADER_RESOURCE_VIEW_DESC srv_desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, 0,
		1);
	ComPtr<ID3D11ShaderResourceView> srv;
	hr = m_device->CreateShaderResourceView(texture.get(), &srv_desc, srv.addressof());
	if (FAILED(hr))
		return {};

	return std::make_unique<D3D11HostDisplayTexture>(std::move(texture), std::move(srv), width, height, dynamic);
}

void D3D11HostDisplay::UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* texture_data, u32 texture_data_stride)
{
	D3D11HostDisplayTexture* d3d11_texture = static_cast<D3D11HostDisplayTexture*>(texture);
	if (!d3d11_texture->IsDynamic())
	{
		const CD3D11_BOX dst_box(x, y, 0, x + width, y + height, 1);
		m_context->UpdateSubresource(d3d11_texture->GetD3DTexture(), 0, &dst_box, texture_data, texture_data_stride,
			texture_data_stride * height);
	}
	else
	{
		D3D11_MAPPED_SUBRESOURCE sr;
		HRESULT hr = m_context->Map(d3d11_texture->GetD3DTexture(), 0, D3D11_MAP_WRITE_DISCARD, 0, &sr);
		pxAssertMsg(SUCCEEDED(hr), "Failed to map dynamic host display texture");

		char* dst_ptr = static_cast<char*>(sr.pData) + (y * sr.RowPitch) + (x * sizeof(u32));
		const char* src_ptr = static_cast<const char*>(texture_data);
		if (sr.RowPitch == texture_data_stride)
		{
			std::memcpy(dst_ptr, src_ptr, texture_data_stride * height);
		}
		else
		{
			for (u32 row = 0; row < height; row++)
			{
				std::memcpy(dst_ptr, src_ptr, width * sizeof(u32));
				src_ptr += texture_data_stride;
				dst_ptr += sr.RowPitch;
			}
		}

		m_context->Unmap(d3d11_texture->GetD3DTexture(), 0);
	}
}

bool D3D11HostDisplay::GetHostRefreshRate(float* refresh_rate)
{
	if (m_swap_chain && IsFullscreen())
	{
		DXGI_SWAP_CHAIN_DESC desc;
		if (SUCCEEDED(m_swap_chain->GetDesc(&desc)) && desc.BufferDesc.RefreshRate.Numerator > 0 &&
			desc.BufferDesc.RefreshRate.Denominator > 0)
		{
			DevCon.WriteLn("using fs rr: %u %u", desc.BufferDesc.RefreshRate.Numerator,
				desc.BufferDesc.RefreshRate.Denominator);
			*refresh_rate = static_cast<float>(desc.BufferDesc.RefreshRate.Numerator) /
							static_cast<float>(desc.BufferDesc.RefreshRate.Denominator);
			return true;
		}
	}

	return HostDisplay::GetHostRefreshRate(refresh_rate);
}

void D3D11HostDisplay::SetVSync(VsyncMode mode)
{
	m_vsync_mode = mode;
}

bool D3D11HostDisplay::CreateDevice(const WindowInfo& wi, VsyncMode vsync)
{
	UINT create_flags = 0;
	if (EmuConfig.GS.UseDebugDevice)
		create_flags |= D3D11_CREATE_DEVICE_DEBUG;

	ComPtr<IDXGIFactory> temp_dxgi_factory;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(temp_dxgi_factory.put()));
	if (FAILED(hr))
	{
		Console.Error("Failed to create DXGI factory: 0x%08X", hr);
		return false;
	}

	u32 adapter_index;
	if (!EmuConfig.GS.Adapter.empty())
	{
		AdapterAndModeList adapter_info(GetAdapterAndModeList(temp_dxgi_factory.get()));
		for (adapter_index = 0; adapter_index < static_cast<u32>(adapter_info.adapter_names.size()); adapter_index++)
		{
			if (EmuConfig.GS.Adapter == adapter_info.adapter_names[adapter_index])
				break;
		}
		if (adapter_index == static_cast<u32>(adapter_info.adapter_names.size()))
		{
			Console.Warning("Could not find adapter '%s', using first (%s)", EmuConfig.GS.Adapter.c_str(),
				adapter_info.adapter_names[0].c_str());
			adapter_index = 0;
		}
	}
	else
	{
		Console.WriteLn("No adapter selected, using first.");
		adapter_index = 0;
	}

	ComPtr<IDXGIAdapter> dxgi_adapter;
	hr = temp_dxgi_factory->EnumAdapters(adapter_index, dxgi_adapter.put());
	if (FAILED(hr))
		Console.Warning("Failed to enumerate adapter %u, using default", adapter_index);

	static constexpr std::array<D3D_FEATURE_LEVEL, 3> requested_feature_levels = {
		{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0}};

	hr =
		D3D11CreateDevice(dxgi_adapter.get(), dxgi_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
			create_flags, requested_feature_levels.data(), static_cast<UINT>(requested_feature_levels.size()),
			D3D11_SDK_VERSION, m_device.put(), nullptr, m_context.put());

	// we re-grab these later, see below
	dxgi_adapter.reset();
	temp_dxgi_factory.reset();

	if (FAILED(hr))
	{
		Console.Error("Failed to create D3D device: 0x%08X", hr);
		return false;
	}

	if (EmuConfig.GS.UseDebugDevice && IsDebuggerPresent())
	{
		ComPtr<ID3D11InfoQueue> info;
		if (m_device.try_query_to(&info))
		{
			info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
			info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, TRUE);
		}
	}

	// we need the specific factory for the device, otherwise MakeWindowAssociation() is flaky.
	ComPtr<IDXGIDevice> dxgi_device;
	if (!m_device.try_query_to(&dxgi_device) || FAILED(dxgi_device->GetParent(IID_PPV_ARGS(dxgi_adapter.put()))) ||
		FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(m_dxgi_factory.put()))))
	{
		Console.Warning("Failed to get parent adapter/device/factory");
		return false;
	}

	DXGI_ADAPTER_DESC adapter_desc;
	if (SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc)))
	{
		char adapter_name_buffer[128];
		const int name_length =
			WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, static_cast<int>(std::wcslen(adapter_desc.Description)),
				adapter_name_buffer, sizeof(adapter_name_buffer), 0, nullptr);
		if (name_length >= 0)
		{
			adapter_name_buffer[name_length] = 0;
			Console.WriteLn("D3D Adapter: %s", adapter_name_buffer);
		}
	}

	m_allow_tearing_supported = false;
	ComPtr<IDXGIFactory5> dxgi_factory5;
	if (m_dxgi_factory.try_query_to(&dxgi_factory5))
	{
		BOOL allow_tearing_supported = false;
		hr = dxgi_factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing_supported,
			sizeof(allow_tearing_supported));
		if (SUCCEEDED(hr))
			m_allow_tearing_supported = (allow_tearing_supported == TRUE);
	}

	m_window_info = wi;
	m_vsync_mode = vsync;

	if (m_window_info.type != WindowInfo::Type::Surfaceless && !CreateSwapChain(nullptr))
		return false;

	return true;
}

bool D3D11HostDisplay::SetupDevice()
{
	return true;
}

bool D3D11HostDisplay::MakeCurrent()
{
	return true;
}

bool D3D11HostDisplay::DoneCurrent()
{
	return true;
}

bool D3D11HostDisplay::CreateSwapChain(const DXGI_MODE_DESC* fullscreen_mode)
{
	if (m_window_info.type != WindowInfo::Type::Win32)
		return false;

	m_using_flip_model_swap_chain = !EmuConfig.GS.UseBlitSwapChain || fullscreen_mode;

	const HWND window_hwnd = reinterpret_cast<HWND>(m_window_info.window_handle);
	RECT client_rc{};
	GetClientRect(window_hwnd, &client_rc);
	const u32 width = static_cast<u32>(client_rc.right - client_rc.left);
	const u32 height = static_cast<u32>(client_rc.bottom - client_rc.top);

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
	swap_chain_desc.BufferDesc.Width = width;
	swap_chain_desc.BufferDesc.Height = height;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.BufferCount = 3;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = window_hwnd;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = m_using_flip_model_swap_chain ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;

	m_using_allow_tearing = (m_allow_tearing_supported && m_using_flip_model_swap_chain && !fullscreen_mode);
	if (m_using_allow_tearing)
		swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	if (fullscreen_mode)
	{
		swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swap_chain_desc.Windowed = FALSE;
		swap_chain_desc.BufferDesc = *fullscreen_mode;
	}

	Console.WriteLn("Creating a %dx%d %s %s swap chain", swap_chain_desc.BufferDesc.Width,
		swap_chain_desc.BufferDesc.Height, m_using_flip_model_swap_chain ? "flip-discard" : "discard",
		swap_chain_desc.Windowed ? "windowed" : "full-screen");

	HRESULT hr = m_dxgi_factory->CreateSwapChain(m_device.get(), &swap_chain_desc, m_swap_chain.put());
	if (FAILED(hr) && m_using_flip_model_swap_chain)
	{
		Console.Warning("Failed to create a flip-discard swap chain, trying discard.");
		swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swap_chain_desc.Flags = 0;
		m_using_flip_model_swap_chain = false;
		m_using_allow_tearing = false;

		hr = m_dxgi_factory->CreateSwapChain(m_device.get(), &swap_chain_desc, m_swap_chain.put());
		if (FAILED(hr))
		{
			Console.Error("CreateSwapChain failed: 0x%08X", hr);
			return false;
		}
	}

	ComPtr<IDXGIFactory> dxgi_factory;
	hr = m_swap_chain->GetParent(IID_PPV_ARGS(dxgi_factory.put()));
	if (SUCCEEDED(hr))
	{
		hr = dxgi_factory->MakeWindowAssociation(swap_chain_desc.OutputWindow, DXGI_MWA_NO_WINDOW_CHANGES);
		if (FAILED(hr))
			Console.Warning("MakeWindowAssociation() to disable ALT+ENTER failed");
	}

	return CreateSwapChainRTV();
}

bool D3D11HostDisplay::CreateSwapChainRTV()
{
	ComPtr<ID3D11Texture2D> backbuffer;
	HRESULT hr = m_swap_chain->GetBuffer(0, IID_PPV_ARGS(backbuffer.put()));
	if (FAILED(hr))
	{
		Console.Error("GetBuffer for RTV failed: 0x%08X", hr);
		return false;
	}

	D3D11_TEXTURE2D_DESC backbuffer_desc;
	backbuffer->GetDesc(&backbuffer_desc);

	CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D, backbuffer_desc.Format, 0, 0,
		backbuffer_desc.ArraySize);
	hr = m_device->CreateRenderTargetView(backbuffer.get(), &rtv_desc, m_swap_chain_rtv.put());
	if (FAILED(hr))
	{
		Console.Error("CreateRenderTargetView for swap chain failed: 0x%08X", hr);
		return false;
	}

	m_window_info.surface_width = backbuffer_desc.Width;
	m_window_info.surface_height = backbuffer_desc.Height;
	DevCon.WriteLn("Swap chain buffer size: %ux%u", m_window_info.surface_width, m_window_info.surface_height);

	if (m_window_info.type == WindowInfo::Type::Win32)
	{
		BOOL fullscreen = FALSE;
		DXGI_SWAP_CHAIN_DESC desc;
		if (SUCCEEDED(m_swap_chain->GetFullscreenState(&fullscreen, nullptr)) && fullscreen &&
			SUCCEEDED(m_swap_chain->GetDesc(&desc)))
		{
			m_window_info.surface_refresh_rate = static_cast<float>(desc.BufferDesc.RefreshRate.Numerator) /
												 static_cast<float>(desc.BufferDesc.RefreshRate.Denominator);
		}
		else
		{
			m_window_info.surface_refresh_rate = 0.0f;
		}
	}

	return true;
}

bool D3D11HostDisplay::ChangeWindow(const WindowInfo& new_wi)
{
	DestroySurface();

	m_window_info = new_wi;
	return CreateSwapChain(nullptr);
}

void D3D11HostDisplay::DestroySurface()
{
	if (IsFullscreen())
		SetFullscreen(false, 0, 0, 0.0f);

	m_swap_chain_rtv.reset();
	m_swap_chain.reset();
}

static std::string GetDriverVersionFromLUID(const LUID& luid)
{
	std::string ret;

	HKEY hKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\DirectX", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		DWORD max_key_len = 0, adapter_count = 0;
		if (RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &adapter_count, &max_key_len,
				nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
		{
			std::vector<TCHAR> current_name(max_key_len + 1);
			for (DWORD i = 0; i < adapter_count; ++i)
			{
				DWORD subKeyLength = static_cast<DWORD>(current_name.size());
				if (RegEnumKeyExW(hKey, i, current_name.data(), &subKeyLength, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
				{
					LUID current_luid = {};
					DWORD current_luid_size = sizeof(uint64_t);
					if (RegGetValueW(hKey, current_name.data(), L"AdapterLuid", RRF_RT_QWORD, nullptr, &current_luid, &current_luid_size) == ERROR_SUCCESS &&
						current_luid.HighPart == luid.HighPart && current_luid.LowPart == luid.LowPart)
					{
						LARGE_INTEGER driver_version = {};
						DWORD driver_version_size = sizeof(driver_version);
						if (RegGetValueW(hKey, current_name.data(), L"DriverVersion", RRF_RT_QWORD, nullptr, &driver_version, &driver_version_size) == ERROR_SUCCESS)
						{
							WORD nProduct = HIWORD(driver_version.HighPart);
							WORD nVersion = LOWORD(driver_version.HighPart);
							WORD nSubVersion = HIWORD(driver_version.LowPart);
							WORD nBuild = LOWORD(driver_version.LowPart);
							ret = StringUtil::StdStringFromFormat("%u.%u.%u.%u", nProduct, nVersion, nSubVersion, nBuild);
						}
					}
				}
			}
		}

		RegCloseKey(hKey);
	}

	return ret;
}

std::string D3D11HostDisplay::GetDriverInfo() const
{
	std::string ret = "Unknown Feature Level";

	static constexpr std::array<std::tuple<D3D_FEATURE_LEVEL, const char*>, 4> feature_level_names = {{
		{D3D_FEATURE_LEVEL_10_0, "D3D_FEATURE_LEVEL_10_0"},
		{D3D_FEATURE_LEVEL_10_0, "D3D_FEATURE_LEVEL_10_1"},
		{D3D_FEATURE_LEVEL_11_0, "D3D_FEATURE_LEVEL_11_0"},
		{D3D_FEATURE_LEVEL_11_1, "D3D_FEATURE_LEVEL_11_1"},
	}};

	const D3D_FEATURE_LEVEL fl = m_device->GetFeatureLevel();
	for (size_t i = 0; i < std::size(feature_level_names); i++)
	{
		if (fl == std::get<0>(feature_level_names[i]))
		{
			ret = std::get<1>(feature_level_names[i]);
			break;
		}
	}

	ret += "\n";

	ComPtr<IDXGIDevice> dxgi_dev;
	if (m_device.try_query_to(&dxgi_dev))
	{
		ComPtr<IDXGIAdapter> dxgi_adapter;
		if (SUCCEEDED(dxgi_dev->GetAdapter(dxgi_adapter.put())))
		{
			DXGI_ADAPTER_DESC desc;
			if (SUCCEEDED(dxgi_adapter->GetDesc(&desc)))
			{
				ret += StringUtil::StdStringFromFormat("VID: 0x%04X PID: 0x%04X\n", desc.VendorId, desc.DeviceId);
				ret += StringUtil::WideStringToUTF8String(desc.Description);
				ret += "\n";

				const std::string driver_version(GetDriverVersionFromLUID(desc.AdapterLuid));
				if (!driver_version.empty())
				{
					ret += "Driver Version: ";
					ret += driver_version;
				}
			}
		}
	}

	return ret;
}

void D3D11HostDisplay::ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	if (!m_swap_chain)
		return;

	m_window_info.surface_scale = new_window_scale;

	if (m_window_info.surface_width == new_window_width && m_window_info.surface_height == new_window_height)
		return;

	m_swap_chain_rtv.reset();

	HRESULT hr = m_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN,
		m_using_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
	if (FAILED(hr))
		Console.Error("ResizeBuffers() failed: 0x%08X", hr);

	if (!CreateSwapChainRTV())
		pxFailRel("Failed to recreate swap chain RTV after resize");
}

bool D3D11HostDisplay::SupportsFullscreen() const
{
	return true;
}

bool D3D11HostDisplay::IsFullscreen()
{
	BOOL is_fullscreen = FALSE;
	return (m_swap_chain && SUCCEEDED(m_swap_chain->GetFullscreenState(&is_fullscreen, nullptr)) && is_fullscreen);
}

bool D3D11HostDisplay::SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate)
{
	if (!m_swap_chain)
		return false;

	BOOL is_fullscreen = FALSE;
	HRESULT hr = m_swap_chain->GetFullscreenState(&is_fullscreen, nullptr);
	if (!fullscreen)
	{
		// leaving fullscreen
		if (is_fullscreen)
			return SUCCEEDED(m_swap_chain->SetFullscreenState(FALSE, nullptr));
		else
			return true;
	}

	IDXGIOutput* output;
	if (FAILED(hr = m_swap_chain->GetContainingOutput(&output)))
		return false;

	DXGI_SWAP_CHAIN_DESC current_desc;
	hr = m_swap_chain->GetDesc(&current_desc);
	if (FAILED(hr))
		return false;

	DXGI_MODE_DESC new_mode = current_desc.BufferDesc;
	new_mode.Width = width;
	new_mode.Height = height;
	new_mode.RefreshRate.Numerator = static_cast<UINT>(std::floor(refresh_rate * 1000.0f));
	new_mode.RefreshRate.Denominator = 1000u;

	DXGI_MODE_DESC closest_mode;
	if (FAILED(hr = output->FindClosestMatchingMode(&new_mode, &closest_mode, nullptr)) ||
		new_mode.Format != current_desc.BufferDesc.Format)
	{
		Console.Error("Failed to find closest matching mode, hr=%08X", hr);
		return false;
	}

	if (new_mode.Width == current_desc.BufferDesc.Width && new_mode.Height == current_desc.BufferDesc.Height &&
		new_mode.RefreshRate.Numerator == current_desc.BufferDesc.RefreshRate.Numerator &&
		new_mode.RefreshRate.Denominator == current_desc.BufferDesc.RefreshRate.Denominator)
	{
		DevCon.WriteLn("Fullscreen mode already set");
		return true;
	}

	m_swap_chain_rtv.reset();
	m_swap_chain.reset();

	if (!CreateSwapChain(&closest_mode))
	{
		Console.Error("Failed to create a fullscreen swap chain");
		if (!CreateSwapChain(nullptr))
			pxFailRel("Failed to recreate windowed swap chain");

		return false;
	}

	return true;
}

bool D3D11HostDisplay::CreateImGuiContext()
{
	return ImGui_ImplDX11_Init(m_device.get(), m_context.get());
}

void D3D11HostDisplay::DestroyImGuiContext()
{
	ImGui_ImplDX11_Shutdown();
}

bool D3D11HostDisplay::UpdateImGuiFontTexture()
{
	ImGui_ImplDX11_CreateFontsTexture();
	return true;
}

bool D3D11HostDisplay::BeginPresent(bool frame_skip)
{
	if (frame_skip || !m_swap_chain)
	{
		ImGui::EndFrame();
		return false;
	}

	// When using vsync, the time here seems to include the time for the buffer to become available.
	// This blows our our GPU usage number considerably, so read the timestamp before the final blit
	// in this configuration. It does reduce accuracy a little, but better than seeing 100% all of
	// the time, when it's more like a couple of percent.
	if (m_vsync_mode != VsyncMode::Off && m_gpu_timing_enabled)
		PopTimestampQuery();

	static constexpr std::array<float, 4> clear_color = {};
	m_context->ClearRenderTargetView(m_swap_chain_rtv.get(), clear_color.data());
	m_context->OMSetRenderTargets(1, m_swap_chain_rtv.addressof(), nullptr);

	const CD3D11_VIEWPORT vp(0.0f, 0.0f, static_cast<float>(m_window_info.surface_width), static_cast<float>(m_window_info.surface_height));
	const CD3D11_RECT scissor(0, 0, m_window_info.surface_width, m_window_info.surface_height);
	m_context->RSSetViewports(1, &vp);
	m_context->RSSetScissorRects(1, &scissor);
	return true;
}

void D3D11HostDisplay::EndPresent()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// See note in BeginPresent() for why it's conditional on vsync-off.
	const bool vsync_on = m_vsync_mode != VsyncMode::Off;
	if (!vsync_on && m_gpu_timing_enabled)
		PopTimestampQuery();

	if (!vsync_on && m_using_allow_tearing)
		m_swap_chain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	else
		m_swap_chain->Present(static_cast<UINT>(vsync_on), 0);

	if (m_gpu_timing_enabled)
		KickTimestampQuery();
}

bool D3D11HostDisplay::CreateTimestampQueries()
{
	for (u32 i = 0; i < NUM_TIMESTAMP_QUERIES; i++)
	{
		for (u32 j = 0; j < 3; j++)
		{
			const CD3D11_QUERY_DESC qdesc((j == 0) ? D3D11_QUERY_TIMESTAMP_DISJOINT : D3D11_QUERY_TIMESTAMP);
			const HRESULT hr = m_device->CreateQuery(&qdesc, m_timestamp_queries[i][j].put());
			if (FAILED(hr))
			{
				m_timestamp_queries = {};
				return false;
			}
		}
	}

	KickTimestampQuery();
	return true;
}

void D3D11HostDisplay::DestroyTimestampQueries()
{
	if (!m_timestamp_queries[0][0])
		return;

	if (m_timestamp_query_started)
		m_context->End(m_timestamp_queries[m_write_timestamp_query][1].get());

	m_timestamp_queries = {};
	m_read_timestamp_query = 0;
	m_write_timestamp_query = 0;
	m_waiting_timestamp_queries = 0;
	m_timestamp_query_started = 0;
}

void D3D11HostDisplay::PopTimestampQuery()
{
	while (m_waiting_timestamp_queries > 0)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
		const HRESULT disjoint_hr = m_context->GetData(m_timestamp_queries[m_read_timestamp_query][0].get(), &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
		if (disjoint_hr != S_OK)
			break;

		if (disjoint.Disjoint)
		{
			DevCon.WriteLn("GPU timing disjoint, resetting.");
			m_read_timestamp_query = 0;
			m_write_timestamp_query = 0;
			m_waiting_timestamp_queries = 0;
			m_timestamp_query_started = 0;
		}
		else
		{
			u64 start = 0, end = 0;
			const HRESULT start_hr = m_context->GetData(m_timestamp_queries[m_read_timestamp_query][1].get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			const HRESULT end_hr = m_context->GetData(m_timestamp_queries[m_read_timestamp_query][2].get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
			if (start_hr == S_OK && end_hr == S_OK)
			{
				m_accumulated_gpu_time += static_cast<float>(static_cast<double>(end - start) / (static_cast<double>(disjoint.Frequency) / 1000.0));
				m_read_timestamp_query = (m_read_timestamp_query + 1) % NUM_TIMESTAMP_QUERIES;
				m_waiting_timestamp_queries--;
			}
		}
	}

	if (m_timestamp_query_started)
	{
		m_context->End(m_timestamp_queries[m_write_timestamp_query][2].get());
		m_context->End(m_timestamp_queries[m_write_timestamp_query][0].get());
		m_write_timestamp_query = (m_write_timestamp_query + 1) % NUM_TIMESTAMP_QUERIES;
		m_timestamp_query_started = false;
		m_waiting_timestamp_queries++;
	}
}

void D3D11HostDisplay::KickTimestampQuery()
{
	if (m_timestamp_query_started || !m_timestamp_queries[0][0] || m_waiting_timestamp_queries == NUM_TIMESTAMP_QUERIES)
		return;

	m_context->Begin(m_timestamp_queries[m_write_timestamp_query][0].get());
	m_context->End(m_timestamp_queries[m_write_timestamp_query][1].get());
	m_timestamp_query_started = true;
}

bool D3D11HostDisplay::SetGPUTimingEnabled(bool enabled)
{
	if (m_gpu_timing_enabled == enabled)
		return true;

	m_gpu_timing_enabled = enabled;
	if (m_gpu_timing_enabled)
	{
		return CreateTimestampQueries();
	}
	else
	{
		DestroyTimestampQueries();
		return true;
	}
}

float D3D11HostDisplay::GetAndResetAccumulatedGPUTime()
{
	const float value = m_accumulated_gpu_time;
	m_accumulated_gpu_time = 0.0f;
	return value;
}

HostDisplay::AdapterAndModeList D3D11HostDisplay::StaticGetAdapterAndModeList()
{
	ComPtr<IDXGIFactory> dxgi_factory;
	const HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgi_factory.put()));
	if (FAILED(hr))
		return {};

	return GetAdapterAndModeList(dxgi_factory.get());
}

HostDisplay::AdapterAndModeList D3D11HostDisplay::GetAdapterAndModeList(IDXGIFactory* dxgi_factory)
{
	AdapterAndModeList adapter_info;
	ComPtr<IDXGIAdapter> current_adapter;
	while (SUCCEEDED(dxgi_factory->EnumAdapters(static_cast<UINT>(adapter_info.adapter_names.size()),
		current_adapter.put())))
	{
		DXGI_ADAPTER_DESC adapter_desc;
		std::string adapter_name;
		if (SUCCEEDED(current_adapter->GetDesc(&adapter_desc)))
		{
			char adapter_name_buffer[128];
			const int name_length = WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description,
				static_cast<int>(std::wcslen(adapter_desc.Description)),
				adapter_name_buffer, sizeof(adapter_name_buffer), 0, nullptr);
			if (name_length >= 0)
				adapter_name.assign(adapter_name_buffer, static_cast<size_t>(name_length));
			else
				adapter_name.assign("(Unknown)");
		}
		else
		{
			adapter_name.assign("(Unknown)");
		}

		if (adapter_info.fullscreen_modes.empty())
		{
			ComPtr<IDXGIOutput> output;
			if (SUCCEEDED(current_adapter->EnumOutputs(0, &output)))
			{
				UINT num_modes = 0;
				if (SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, nullptr)))
				{
					std::vector<DXGI_MODE_DESC> modes(num_modes);
					if (SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 0, &num_modes, modes.data())))
					{
						for (const DXGI_MODE_DESC& mode : modes)
						{
							adapter_info.fullscreen_modes.push_back(GetFullscreenModeString(
								mode.Width, mode.Height,
								static_cast<float>(mode.RefreshRate.Numerator) / static_cast<float>(mode.RefreshRate.Denominator)));
						}
					}
				}
			}
		}

		// handle duplicate adapter names
		if (std::any_of(adapter_info.adapter_names.begin(), adapter_info.adapter_names.end(),
				[&adapter_name](const std::string& other) { return (adapter_name == other); }))
		{
			std::string original_adapter_name = std::move(adapter_name);

			u32 current_extra = 2;
			do
			{
				adapter_name = StringUtil::StdStringFromFormat("%s (%u)", original_adapter_name.c_str(), current_extra);
				current_extra++;
			} while (std::any_of(adapter_info.adapter_names.begin(), adapter_info.adapter_names.end(),
				[&adapter_name](const std::string& other) { return (adapter_name == other); }));
		}

		adapter_info.adapter_names.push_back(std::move(adapter_name));
	}

	return adapter_info;
}

HostDisplay::AdapterAndModeList D3D11HostDisplay::GetAdapterAndModeList()
{
	return GetAdapterAndModeList(m_dxgi_factory.get());
}

