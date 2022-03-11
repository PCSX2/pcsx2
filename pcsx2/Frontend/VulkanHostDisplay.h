#pragma once
#include "common/Vulkan/Loader.h"
#include "common/Vulkan/StreamBuffer.h"
#include "common/Vulkan/SwapChain.h"
#include "common/WindowInfo.h"
#include "pcsx2/HostDisplay.h"
#include <memory>
#include <string_view>

namespace Vulkan
{
	class StreamBuffer;
	class SwapChain;
} // namespace Vulkan

class VulkanHostDisplay final : public HostDisplay
{
public:
	VulkanHostDisplay();
	~VulkanHostDisplay();

	RenderAPI GetRenderAPI() const override;
	void* GetRenderDevice() const override;
	void* GetRenderContext() const override;
	void* GetRenderSurface() const override;

	bool HasRenderDevice() const override;
	bool HasRenderSurface() const override;

	bool CreateRenderDevice(const WindowInfo& wi, std::string_view adapter_name, VsyncMode vsync, bool threaded_presentation, bool debug_device) override;
	bool InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device) override;
	void DestroyRenderDevice() override;

	bool MakeRenderContextCurrent() override;
	bool DoneRenderContextCurrent() override;

	bool ChangeRenderWindow(const WindowInfo& new_wi) override;
	void ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsFullscreen() const override;
	bool IsFullscreen() override;
	bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) override;
	AdapterAndModeList GetAdapterAndModeList() override;
	void DestroyRenderSurface() override;
	std::string GetDriverInfo() const override;

	std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic = false) override;
	void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* texture_data, u32 texture_data_stride) override;

	void SetVSync(VsyncMode mode) override;

	bool BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	void SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	static AdapterAndModeList StaticGetAdapterAndModeList(const WindowInfo* wi);

protected:
	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	std::unique_ptr<Vulkan::SwapChain> m_swap_chain;
};
