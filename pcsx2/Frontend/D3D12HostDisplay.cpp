/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "Frontend/D3D12HostDisplay.h"
#include "GS/Renderers/DX11/D3D.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/D3D12/Context.h"
#include "common/D3D12/Util.h"
#include "common/StringUtil.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include <array>
#include <dxgi1_5.h>

class D3D12HostDisplayTexture : public HostDisplayTexture
{
public:
	explicit D3D12HostDisplayTexture(D3D12::Texture texture)
		: m_texture(std::move(texture))
	{
	}
	~D3D12HostDisplayTexture() override = default;

	void* GetHandle() const override { return const_cast<D3D12::Texture*>(&m_texture); }
	u32 GetWidth() const override { return m_texture.GetWidth(); }
	u32 GetHeight() const override { return m_texture.GetHeight(); }

	const D3D12::Texture& GetTexture() const { return m_texture; }
	D3D12::Texture& GetTexture() { return m_texture; }

private:
	D3D12::Texture m_texture;
};

D3D12HostDisplay::D3D12HostDisplay() = default;

D3D12HostDisplay::~D3D12HostDisplay()
{
	if (g_d3d12_context)
	{
		g_d3d12_context->WaitForGPUIdle();
		D3D12HostDisplay::DestroySurface();
		g_d3d12_context->Destroy();
	}
}

RenderAPI D3D12HostDisplay::GetRenderAPI() const
{
	return RenderAPI::D3D12;
}

void* D3D12HostDisplay::GetDevice() const
{
	return g_d3d12_context->GetDevice();
}

void* D3D12HostDisplay::GetContext() const
{
	return g_d3d12_context.get();
}

void* D3D12HostDisplay::GetSurface() const
{
	return m_swap_chain.get();
}

bool D3D12HostDisplay::HasDevice() const
{
	return static_cast<bool>(g_d3d12_context);
}

bool D3D12HostDisplay::HasSurface() const
{
	return static_cast<bool>(m_swap_chain);
}

std::unique_ptr<HostDisplayTexture> D3D12HostDisplay::CreateTexture(
	u32 width, u32 height, const void* data, u32 data_stride, bool dynamic /* = false */)
{
	D3D12::Texture tex;
	if (!tex.Create(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
			D3D12_RESOURCE_FLAG_NONE))
	{
		return {};
	}

	if (data && !tex.LoadData(g_d3d12_context->GetInitCommandList(), 0, 0, 0, width, height, data, data_stride))
		return {};

	return std::make_unique<D3D12HostDisplayTexture>(std::move(tex));
}

void D3D12HostDisplay::UpdateTexture(
	HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* texture_data, u32 texture_data_stride)
{
	static_cast<D3D12HostDisplayTexture*>(texture)->GetTexture().LoadData(
		g_d3d12_context->GetCommandList(), 0, x, y, width, height, texture_data, texture_data_stride);
}

bool D3D12HostDisplay::GetHostRefreshRate(float* refresh_rate)
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

void D3D12HostDisplay::SetVSync(VsyncMode mode)
{
	m_vsync_mode = mode;
}

bool D3D12HostDisplay::CreateDevice(const WindowInfo& wi, VsyncMode vsync)
{
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
			Console.Warning("Could not find adapter '%s', using first (%s)", EmuConfig.GS.Adapter.c_str(), adapter_info.adapter_names[0].c_str());
			adapter_index = 0;
		}
	}
	else
	{
		Console.WriteLn("No adapter selected, using first.");
		adapter_index = 0;
	}

	if (!D3D12::Context::Create(temp_dxgi_factory.get(), adapter_index, EmuConfig.GS.UseDebugDevice))
		return false;

	if (FAILED(hr))
	{
		Console.Error("Failed to create D3D device: 0x%08X", hr);
		return false;
	}

	m_dxgi_factory = std::move(temp_dxgi_factory);

	m_allow_tearing_supported = false;
	ComPtr<IDXGIFactory5> dxgi_factory5(m_dxgi_factory.try_query<IDXGIFactory5>());
	if (dxgi_factory5)
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

bool D3D12HostDisplay::SetupDevice()
{
	return true;
}

bool D3D12HostDisplay::MakeCurrent()
{
	return true;
}

bool D3D12HostDisplay::DoneCurrent()
{
	return true;
}

bool D3D12HostDisplay::CreateSwapChain(const DXGI_MODE_DESC* fullscreen_mode)
{
	if (m_window_info.type != WindowInfo::Type::Win32)
		return false;

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
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	m_using_allow_tearing = (m_allow_tearing_supported && !fullscreen_mode);
	if (m_using_allow_tearing)
		swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	if (fullscreen_mode)
	{
		swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		swap_chain_desc.Windowed = FALSE;
		swap_chain_desc.BufferDesc = *fullscreen_mode;
	}

	DevCon.WriteLn("Creating a %dx%d %s swap chain", swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height,
		swap_chain_desc.Windowed ? "windowed" : "full-screen");

	HRESULT hr =
		m_dxgi_factory->CreateSwapChain(g_d3d12_context->GetCommandQueue(), &swap_chain_desc, m_swap_chain.put());
	if (FAILED(hr))
	{
		Console.Error("CreateSwapChain failed: 0x%08X", hr);
		return false;
	}

	hr = m_dxgi_factory->MakeWindowAssociation(swap_chain_desc.OutputWindow, DXGI_MWA_NO_WINDOW_CHANGES);
	if (FAILED(hr))
		Console.Warning("MakeWindowAssociation() to disable ALT+ENTER failed");

	return CreateSwapChainRTV();
}

bool D3D12HostDisplay::CreateSwapChainRTV()
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	HRESULT hr = m_swap_chain->GetDesc(&swap_chain_desc);
	if (FAILED(hr))
		return false;

	for (u32 i = 0; i < swap_chain_desc.BufferCount; i++)
	{
		ComPtr<ID3D12Resource> backbuffer;
		hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(backbuffer.put()));
		if (FAILED(hr))
		{
			Console.Error("GetBuffer for RTV failed: 0x%08X", hr);
			return false;
		}

		D3D12::Texture tex;
		if (!tex.Adopt(std::move(backbuffer), DXGI_FORMAT_UNKNOWN, swap_chain_desc.BufferDesc.Format, DXGI_FORMAT_UNKNOWN,
				D3D12_RESOURCE_STATE_PRESENT))
		{
			return false;
		}

		m_swap_chain_buffers.push_back(std::move(tex));
	}

	m_window_info.surface_width = swap_chain_desc.BufferDesc.Width;
	m_window_info.surface_height = swap_chain_desc.BufferDesc.Height;
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

	m_current_swap_chain_buffer = 0;
	return true;
}

void D3D12HostDisplay::DestroySwapChainRTVs()
{
	for (D3D12::Texture& buffer : m_swap_chain_buffers)
		buffer.Destroy(false);
	m_swap_chain_buffers.clear();
	m_current_swap_chain_buffer = 0;
}

bool D3D12HostDisplay::ChangeWindow(const WindowInfo& new_wi)
{
	DestroySurface();

	m_window_info = new_wi;
	return CreateSwapChain(nullptr);
}

void D3D12HostDisplay::DestroySurface()
{
	// For some reason if we don't execute the command list here, the swap chain is in use.. not sure where.
	g_d3d12_context->ExecuteCommandList(D3D12::Context::WaitType::Sleep);

	if (IsFullscreen())
		SetFullscreen(false, 0, 0, 0.0f);

	DestroySwapChainRTVs();
	m_swap_chain.reset();
}

std::string D3D12HostDisplay::GetDriverInfo() const
{
	std::string ret = "Unknown Feature Level";

	static constexpr std::array<std::tuple<D3D_FEATURE_LEVEL, const char*>, 4> feature_level_names = {{
		{D3D_FEATURE_LEVEL_10_0, "D3D_FEATURE_LEVEL_10_0"},
		{D3D_FEATURE_LEVEL_10_0, "D3D_FEATURE_LEVEL_10_1"},
		{D3D_FEATURE_LEVEL_11_0, "D3D_FEATURE_LEVEL_11_0"},
		{D3D_FEATURE_LEVEL_11_1, "D3D_FEATURE_LEVEL_11_1"},
	}};

	const D3D_FEATURE_LEVEL fl = g_d3d12_context->GetFeatureLevel();
	for (size_t i = 0; i < std::size(feature_level_names); i++)
	{
		if (fl == std::get<0>(feature_level_names[i]))
		{
			ret = std::get<1>(feature_level_names[i]);
			break;
		}
	}

	ret += "\n";

	IDXGIAdapter* adapter = g_d3d12_context->GetAdapter();
	DXGI_ADAPTER_DESC desc;
	if (adapter && SUCCEEDED(adapter->GetDesc(&desc)))
	{
		ret += StringUtil::StdStringFromFormat("VID: 0x%04X PID: 0x%04X\n", desc.VendorId, desc.DeviceId);
		ret += StringUtil::WideStringToUTF8String(desc.Description);
		ret += "\n";

		const std::string driver_version(D3D::GetDriverVersionFromLUID(desc.AdapterLuid));
		if (!driver_version.empty())
		{
			ret += "Driver Version: ";
			ret += driver_version;
		}
	}

	return ret;
}

void D3D12HostDisplay::ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	if (!m_swap_chain)
		return;

	m_window_info.surface_scale = new_window_scale;

	if (m_window_info.surface_width == new_window_width && m_window_info.surface_height == new_window_height)
		return;

	// For some reason if we don't execute the command list here, the swap chain is in use.. not sure where.
	g_d3d12_context->ExecuteCommandList(D3D12::Context::WaitType::Sleep);

	DestroySwapChainRTVs();

	HRESULT hr = m_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN,
		m_using_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
	if (FAILED(hr))
		Console.Error("ResizeBuffers() failed: 0x%08X", hr);

	if (!CreateSwapChainRTV())
		pxFailRel("Failed to recreate swap chain RTV after resize");
}

bool D3D12HostDisplay::SupportsFullscreen() const
{
	return true;
}

bool D3D12HostDisplay::IsFullscreen()
{
	BOOL is_fullscreen = FALSE;
	return (m_swap_chain && SUCCEEDED(m_swap_chain->GetFullscreenState(&is_fullscreen, nullptr)) && is_fullscreen);
}

bool D3D12HostDisplay::SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate)
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

	if (new_mode.Width == current_desc.BufferDesc.Width && new_mode.Height == current_desc.BufferDesc.Width &&
		new_mode.RefreshRate.Numerator == current_desc.BufferDesc.RefreshRate.Numerator &&
		new_mode.RefreshRate.Denominator == current_desc.BufferDesc.RefreshRate.Denominator)
	{
		DevCon.WriteLn("Fullscreen mode already set");
		return true;
	}

	g_d3d12_context->ExecuteCommandList(D3D12::Context::WaitType::Sleep);
	DestroySwapChainRTVs();
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

HostDisplay::AdapterAndModeList D3D12HostDisplay::GetAdapterAndModeList()
{
	return GetAdapterAndModeList(m_dxgi_factory.get());
}

bool D3D12HostDisplay::CreateImGuiContext()
{
	ImGui::GetIO().DisplaySize.x = static_cast<float>(m_window_info.surface_width);
	ImGui::GetIO().DisplaySize.y = static_cast<float>(m_window_info.surface_height);

	if (!ImGui_ImplDX12_Init(DXGI_FORMAT_R8G8B8A8_UNORM))
		return false;

	return true;
}

void D3D12HostDisplay::DestroyImGuiContext()
{
	g_d3d12_context->WaitForGPUIdle();

	ImGui_ImplDX12_Shutdown();
}

bool D3D12HostDisplay::UpdateImGuiFontTexture()
{
	return ImGui_ImplDX12_CreateFontsTexture();
}

HostDisplay::PresentResult D3D12HostDisplay::BeginPresent(bool frame_skip)
{
	if (m_device_lost)
		return HostDisplay::PresentResult::DeviceLost;

	if (frame_skip || !m_swap_chain)
		return PresentResult::FrameSkipped;

	static constexpr std::array<float, 4> clear_color = {};
	D3D12::Texture& swap_chain_buf = m_swap_chain_buffers[m_current_swap_chain_buffer];

	ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();
	swap_chain_buf.TransitionToState(cmdlist, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdlist->ClearRenderTargetView(swap_chain_buf.GetWriteDescriptor(), clear_color.data(), 0, nullptr);
	cmdlist->OMSetRenderTargets(1, &swap_chain_buf.GetWriteDescriptor().cpu_handle, FALSE, nullptr);

	const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(m_window_info.surface_width), static_cast<float>(m_window_info.surface_height), 0.0f, 1.0f};
	const D3D12_RECT scissor{0, 0, static_cast<LONG>(m_window_info.surface_width), static_cast<LONG>(m_window_info.surface_height)};
	cmdlist->RSSetViewports(1, &vp);
	cmdlist->RSSetScissorRects(1, &scissor);
	return PresentResult::OK;
}

void D3D12HostDisplay::EndPresent()
{
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData());

	D3D12::Texture& swap_chain_buf = m_swap_chain_buffers[m_current_swap_chain_buffer];
	m_current_swap_chain_buffer = ((m_current_swap_chain_buffer + 1) % static_cast<u32>(m_swap_chain_buffers.size()));

	swap_chain_buf.TransitionToState(g_d3d12_context->GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
	if (!g_d3d12_context->ExecuteCommandList(D3D12::Context::WaitType::None))
	{
		m_device_lost = true;
		return;
	}

	const bool vsync = static_cast<UINT>(m_vsync_mode != VsyncMode::Off);
	if (!vsync && m_using_allow_tearing)
		m_swap_chain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
	else
		m_swap_chain->Present(static_cast<UINT>(vsync), 0);
}

bool D3D12HostDisplay::SetGPUTimingEnabled(bool enabled)
{
	g_d3d12_context->SetEnableGPUTiming(enabled);
	return true;
}

float D3D12HostDisplay::GetAndResetAccumulatedGPUTime()
{
	return g_d3d12_context->GetAndResetAccumulatedGPUTime();
}

HostDisplay::AdapterAndModeList D3D12HostDisplay::StaticGetAdapterAndModeList()
{
	ComPtr<IDXGIFactory> dxgi_factory;
	const HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(dxgi_factory.put()));
	if (FAILED(hr))
		return {};

	return GetAdapterAndModeList(dxgi_factory.get());
}

HostDisplay::AdapterAndModeList D3D12HostDisplay::GetAdapterAndModeList(IDXGIFactory* dxgi_factory)
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
				adapter_name_buffer, std::size(adapter_name_buffer), 0, nullptr);
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
							adapter_info.fullscreen_modes.push_back(StringUtil::StdStringFromFormat(
								"%u x %u @ %f hz", mode.Width, mode.Height,
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
