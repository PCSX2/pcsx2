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

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/RedtapeWindows.h"

#include "common/D3D12/DescriptorHeapManager.h"
#include "common/D3D12/Texture.h"
#include "common/WindowInfo.h"

#include "HostDisplay.h"

#include <d3d12.h>
#include <dxgi.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <wil/com.h>

class D3D12HostDisplay : public HostDisplay
{
public:
	template <typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

	D3D12HostDisplay();
	~D3D12HostDisplay();

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

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	static AdapterAndModeList StaticGetAdapterAndModeList();

protected:
	static AdapterAndModeList GetAdapterAndModeList(IDXGIFactory* dxgi_factory);

	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	bool CreateSwapChain(const DXGI_MODE_DESC* fullscreen_mode);
	bool CreateSwapChainRTV();
	void DestroySwapChainRTVs();

	ComPtr<IDXGIFactory> m_dxgi_factory;
	ComPtr<IDXGISwapChain> m_swap_chain;
	std::vector<D3D12::Texture> m_swap_chain_buffers;
	u32 m_current_swap_chain_buffer = 0;

	bool m_allow_tearing_supported = false;
	bool m_using_allow_tearing = false;
	bool m_device_lost = false;
};
