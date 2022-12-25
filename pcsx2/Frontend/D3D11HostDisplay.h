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
#include "HostDisplay.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"
#include "common/WindowInfo.h"
#include <array>
#include <d3d11.h>
#include <dxgi.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class D3D11HostDisplay final : public HostDisplay
{
public:
	template <typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	D3D11HostDisplay();
	~D3D11HostDisplay();

	RenderAPI GetRenderAPI() const override;
	void* GetDevice() const override;
	void* GetContext() const override;
	void* GetSurface() const override;

	bool HasDevice() const override;
	bool HasSurface() const override;

	bool CreateDevice(const WindowInfo& wi, VsyncMode vsync) override;
	bool SetupDevice() override;

	bool MakeCurrent() override;
	bool DoneCurrent() override;

	bool ChangeWindow(const WindowInfo& new_wi) override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsFullscreen() const override;
	bool IsFullscreen() override;
	bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) override;
	AdapterAndModeList GetAdapterAndModeList() override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic = false) override;
	void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* texture_data, u32 texture_data_stride) override;

	bool GetHostRefreshRate(float* refresh_rate) override;

	void SetVSync(VsyncMode mode) override;

	bool BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	static AdapterAndModeList StaticGetAdapterAndModeList();

protected:
	static constexpr u32 DISPLAY_CONSTANT_BUFFER_SIZE = 16;
	static constexpr u8 NUM_TIMESTAMP_QUERIES = 5;

	static AdapterAndModeList GetAdapterAndModeList(IDXGIFactory* dxgi_factory);

	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	bool CreateSwapChain(const DXGI_MODE_DESC* fullscreen_mode);
	bool CreateSwapChainRTV();

	bool CreateTimestampQueries();
	void DestroyTimestampQueries();
	void PopTimestampQuery();
	void KickTimestampQuery();

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_context;

	ComPtr<IDXGIFactory> m_dxgi_factory;
	ComPtr<IDXGISwapChain> m_swap_chain;
	ComPtr<ID3D11RenderTargetView> m_swap_chain_rtv;

	bool m_allow_tearing_supported = false;
	bool m_using_flip_model_swap_chain = true;
	bool m_using_allow_tearing = false;

	std::array<std::array<ComPtr<ID3D11Query>, 3>, NUM_TIMESTAMP_QUERIES> m_timestamp_queries = {};
	u8 m_read_timestamp_query = 0;
	u8 m_write_timestamp_query = 0;
	u8 m_waiting_timestamp_queries = 0;
	bool m_timestamp_query_started = false;
	float m_accumulated_gpu_time = 0.0f;
	bool m_gpu_timing_enabled = false;
};

