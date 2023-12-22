// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/GSVector.h"
#include "GS/Renderers/Vulkan/GSTextureVK.h"
#include "GS/Renderers/Vulkan/VKLoader.h"
#include "GS/Renderers/Vulkan/VKStreamBuffer.h"

#include "common/HashCombine.h"
#include "common/ReadbackSpinManager.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class VKSwapChain;

class GSDeviceVK final : public GSDevice
{
public:
	enum : u32
	{
		NUM_COMMAND_BUFFERS = 3,
	};

	struct OptionalExtensions
	{
		bool vk_ext_provoking_vertex : 1;
		bool vk_ext_memory_budget : 1;
		bool vk_ext_calibrated_timestamps : 1;
		bool vk_ext_rasterization_order_attachment_access : 1;
		bool vk_ext_attachment_feedback_loop_layout : 1;
		bool vk_ext_full_screen_exclusive : 1;
		bool vk_ext_line_rasterization : 1;
		bool vk_khr_driver_properties : 1;
	};

	// Global state accessors
	__fi VkInstance GetVulkanInstance() const { return m_instance; }
	__fi VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
	__fi VkDevice GetDevice() const { return m_device; }
	__fi VmaAllocator GetAllocator() const { return m_allocator; }
	__fi u32 GetGraphicsQueueFamilyIndex() const { return m_graphics_queue_family_index; }
	__fi u32 GetPresentQueueFamilyIndex() const { return m_present_queue_family_index; }
	__fi const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_device_properties; }
	__fi const OptionalExtensions& GetOptionalExtensions() const { return m_optional_extensions; }

	// The interaction between raster order attachment access and fbfetch is unclear.
	__fi bool UseFeedbackLoopLayout() const
	{
		return (m_optional_extensions.vk_ext_attachment_feedback_loop_layout &&
				!m_optional_extensions.vk_ext_rasterization_order_attachment_access);
	}

	// Helpers for getting constants
	__fi u32 GetBufferCopyOffsetAlignment() const
	{
		return static_cast<u32>(m_device_properties.limits.optimalBufferCopyOffsetAlignment);
	}
	__fi u32 GetBufferCopyRowPitchAlignment() const
	{
		return static_cast<u32>(m_device_properties.limits.optimalBufferCopyRowPitchAlignment);
	}

	/// Returns true if running on an NVIDIA GPU.
	__fi bool IsDeviceNVIDIA() const { return (m_device_properties.vendorID == 0x10DE); }

	// Creates a simple render pass.
	VkRenderPass GetRenderPass(VkFormat color_format, VkFormat depth_format,
		VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
		VkAttachmentStoreOp color_store_op = VK_ATTACHMENT_STORE_OP_STORE,
		VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
		VkAttachmentStoreOp depth_store_op = VK_ATTACHMENT_STORE_OP_STORE,
		VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE, bool color_feedback_loop = false,
		bool depth_sampling = false);

	// Gets a non-clearing version of the specified render pass. Slow, don't call in hot path.
	VkRenderPass GetRenderPassForRestarting(VkRenderPass pass);

	// These command buffers are allocated per-frame. They are valid until the command buffer
	// is submitted, after that you should call these functions again.
	__fi VkCommandBuffer GetCurrentCommandBuffer() const { return m_current_command_buffer; }
	__fi VKStreamBuffer& GetTextureUploadBuffer() { return m_texture_stream_buffer; }
	VkCommandBuffer GetCurrentInitCommandBuffer();

	/// Allocates a descriptor set from the pool reserved for the current frame.
	VkDescriptorSet AllocatePersistentDescriptorSet(VkDescriptorSetLayout set_layout);

	/// Frees a descriptor set allocated from the global pool.
	void FreePersistentDescriptorSet(VkDescriptorSet set);

	// Gets the fence that will be signaled when the currently executing command buffer is
	// queued and executed. Do not wait for this fence before the buffer is executed.
	__fi VkFence GetCurrentCommandBufferFence() const { return m_frame_resources[m_current_frame].fence; }

	// Fence "counters" are used to track which commands have been completed by the GPU.
	// If the last completed fence counter is greater or equal to N, it means that the work
	// associated counter N has been completed by the GPU. The value of N to associate with
	// commands can be retreived by calling GetCurrentFenceCounter().
	u64 GetCompletedFenceCounter() const { return m_completed_fence_counter; }

	// Gets the fence that will be signaled when the currently executing command buffer is
	// queued and executed. Do not wait for this fence before the buffer is executed.
	u64 GetCurrentFenceCounter() const { return m_frame_resources[m_current_frame].fence_counter; }

	// Schedule a vulkan resource for destruction later on. This will occur when the command buffer
	// is next re-used, and the GPU has finished working with the specified resource.
	void DeferBufferDestruction(VkBuffer object, VmaAllocation allocation);
	void DeferFramebufferDestruction(VkFramebuffer object);
	void DeferImageDestruction(VkImage object, VmaAllocation allocation);
	void DeferImageViewDestruction(VkImageView object);

	// Wait for a fence to be completed.
	// Also invokes callbacks for completion.
	void WaitForFenceCounter(u64 fence_counter);

	void WaitForGPUIdle();

private:
	// Helper method to create a Vulkan instance.
	static VkInstance CreateVulkanInstance(const WindowInfo& wi, bool enable_debug_utils, bool enable_validation_layer);

	// Returns a list of Vulkan-compatible GPUs.
	using GPUList = std::vector<std::pair<VkPhysicalDevice, std::string>>;
	static GPUList EnumerateGPUs(VkInstance instance);
	static void GPUListToAdapterNames(std::vector<std::string>* dest, VkInstance instance);

	// Enable/disable debug message runtime.
	bool EnableDebugUtils();
	void DisableDebugUtils();

	void SubmitCommandBuffer(VKSwapChain* present_swap_chain = nullptr, bool submit_on_thread = false);
	void MoveToNextCommandBuffer();

	enum class WaitType
	{
		None,
		Sleep,
		Spin,
	};

	static WaitType GetWaitType(bool wait, bool spin);
	void ExecuteCommandBuffer(WaitType wait_for_completion);
	void WaitForPresentComplete();

	// Was the last present submitted to the queue a failure? If so, we must recreate our swapchain.
	bool CheckLastPresentFail();
	bool CheckLastSubmitFail();

	// Allocates a temporary CPU staging buffer, fires the callback with it to populate, then copies to a GPU buffer.
	bool AllocatePreinitializedGPUBuffer(u32 size, VkBuffer* gpu_buffer, VmaAllocation* gpu_allocation,
		VkBufferUsageFlags gpu_usage, const std::function<void(void*)>& fill_callback);

	union RenderPassCacheKey
	{
		struct
		{
			u32 color_format : 8;
			u32 depth_format : 8;
			u32 color_load_op : 2;
			u32 color_store_op : 1;
			u32 depth_load_op : 2;
			u32 depth_store_op : 1;
			u32 stencil_load_op : 2;
			u32 stencil_store_op : 1;
			u32 color_feedback_loop : 1;
			u32 depth_sampling : 1;
		};

		u32 key;
	};

	using ExtensionList = std::vector<const char*>;
	static bool SelectInstanceExtensions(ExtensionList* extension_list, const WindowInfo& wi, bool enable_debug_utils);
	bool SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface);
	bool SelectDeviceFeatures();
	bool CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer);
	bool ProcessDeviceExtensions();

	bool CreateAllocator();
	bool CreateCommandBuffers();
	bool CreateGlobalDescriptorPool();

	VkRenderPass CreateCachedRenderPass(RenderPassCacheKey key);

	void CommandBufferCompleted(u32 index);
	void ActivateCommandBuffer(u32 index);
	void ScanForCommandBufferCompletion();
	void WaitForCommandBufferCompletion(u32 index);

	void DoSubmitCommandBuffer(u32 index, VKSwapChain* present_swap_chain, u32 spin_cycles);
	void DoPresent(VKSwapChain* present_swap_chain);
	void WaitForPresentComplete(std::unique_lock<std::mutex>& lock);
	void PresentThread();
	void StartPresentThread();
	void StopPresentThread();

	bool InitSpinResources();
	void DestroySpinResources();
	void WaitForSpinCompletion(u32 index);
	void SpinCommandCompleted(u32 index);
	void SubmitSpinCommand(u32 index, u32 cycles);
	void CalibrateSpinTimestamp();
	u64 GetCPUTimestamp();

	struct FrameResources
	{
		// [0] - Init (upload) command buffer, [1] - draw command buffer
		VkCommandPool command_pool = VK_NULL_HANDLE;
		std::array<VkCommandBuffer, 2> command_buffers{VK_NULL_HANDLE, VK_NULL_HANDLE};
		VkFence fence = VK_NULL_HANDLE;
		u64 fence_counter = 0;
		s32 spin_id = -1;
		u32 submit_timestamp = 0;
		bool init_buffer_used = false;
		bool needs_fence_wait = false;
		bool timestamp_written = false;

		std::vector<std::function<void()>> cleanup_resources;
	};

	struct SpinResources
	{
		VkCommandPool command_pool = VK_NULL_HANDLE;
		VkCommandBuffer command_buffer = VK_NULL_HANDLE;
		VkSemaphore semaphore = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		u32 cycles = 0;
		bool in_progress = false;
	};

	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VmaAllocator m_allocator = VK_NULL_HANDLE;

	VkCommandBuffer m_current_command_buffer = VK_NULL_HANDLE;

	VkDescriptorPool m_global_descriptor_pool = VK_NULL_HANDLE;

	VkQueue m_graphics_queue = VK_NULL_HANDLE;
	VkQueue m_present_queue = VK_NULL_HANDLE;
	u32 m_graphics_queue_family_index = 0;
	u32 m_present_queue_family_index = 0;

	ReadbackSpinManager m_spin_manager;
	VkQueue m_spin_queue = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_spin_descriptor_set_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_spin_pipeline_layout = VK_NULL_HANDLE;
	VkPipeline m_spin_pipeline = VK_NULL_HANDLE;
	VkBuffer m_spin_buffer = VK_NULL_HANDLE;
	VmaAllocation m_spin_buffer_allocation = VK_NULL_HANDLE;
	VkDescriptorSet m_spin_descriptor_set = VK_NULL_HANDLE;
	std::array<SpinResources, NUM_COMMAND_BUFFERS> m_spin_resources;
#ifdef _WIN32
	double m_queryperfcounter_to_ns = 0;
#endif
	double m_spin_timestamp_scale = 0;
	double m_spin_timestamp_offset = 0;
	u32 m_spin_queue_family_index = 0;
	u32 m_command_buffer_render_passes = 0;
	u32 m_spin_timer = 0;
	bool m_spinning_supported = false;
	bool m_spin_queue_is_graphics_queue = false;
	bool m_spin_buffer_initialized = false;

	VkQueryPool m_timestamp_query_pool = VK_NULL_HANDLE;
	float m_accumulated_gpu_time = 0.0f;
	bool m_gpu_timing_enabled = false;
	bool m_gpu_timing_supported = false;
	bool m_wants_new_timestamp_calibration = false;
	VkTimeDomainEXT m_calibrated_timestamp_type = VK_TIME_DOMAIN_DEVICE_EXT;

	std::array<FrameResources, NUM_COMMAND_BUFFERS> m_frame_resources;
	u64 m_next_fence_counter = 1;
	u64 m_completed_fence_counter = 0;
	u32 m_current_frame = 0;

	std::atomic_bool m_last_submit_failed{false};
	std::atomic_bool m_last_present_failed{false};
	std::atomic_bool m_present_done{true};
	std::mutex m_present_mutex;
	std::condition_variable m_present_queued_cv;
	std::condition_variable m_present_done_cv;
	std::thread m_present_thread;
	std::atomic_bool m_present_thread_done{false};

	struct QueuedPresent
	{
		VKSwapChain* swap_chain;
		u32 command_buffer_index;
		u32 spin_cycles;
	};

	QueuedPresent m_queued_present = {};

	std::map<u32, VkRenderPass> m_render_pass_cache;

	VkDebugUtilsMessengerEXT m_debug_messenger_callback = VK_NULL_HANDLE;

	VkPhysicalDeviceFeatures m_device_features = {};
	VkPhysicalDeviceProperties m_device_properties = {};
	VkPhysicalDeviceDriverPropertiesKHR m_device_driver_properties = {};
	OptionalExtensions m_optional_extensions = {};

public:
	enum FeedbackLoopFlag : u8
	{
		FeedbackLoopFlag_None = 0,
		FeedbackLoopFlag_ReadAndWriteRT = 1,
		FeedbackLoopFlag_ReadDS = 2,
	};

	struct alignas(8) PipelineSelector
	{
		GSHWDrawConfig::PSSelector ps;

		union
		{
			struct
			{
				u32 topology : 2;
				u32 rt : 1;
				u32 ds : 1;
				u32 line_width : 1;
				u32 feedback_loop_flags : 2;
			};

			u32 key;
		};

		GSHWDrawConfig::BlendState bs;
		GSHWDrawConfig::VSSelector vs;
		GSHWDrawConfig::DepthStencilSelector dss;
		GSHWDrawConfig::ColorMaskSelector cms;
		u8 pad;

		__fi bool operator==(const PipelineSelector& p) const { return BitEqual(*this, p); }
		__fi bool operator!=(const PipelineSelector& p) const { return !BitEqual(*this, p); }

		__fi PipelineSelector() { std::memset(this, 0, sizeof(*this)); }

		__fi bool IsRTFeedbackLoop() const { return ((feedback_loop_flags & FeedbackLoopFlag_ReadAndWriteRT) != 0); }
		__fi bool IsTestingAndSamplingDepth() const { return ((feedback_loop_flags & FeedbackLoopFlag_ReadDS) != 0); }
	};
	static_assert(sizeof(PipelineSelector) == 24, "Pipeline selector is 24 bytes");

	struct PipelineSelectorHash
	{
		std::size_t operator()(const PipelineSelector& e) const noexcept
		{
			std::size_t hash = 0;
			HashCombine(hash, e.vs.key, e.ps.key_hi, e.ps.key_lo, e.dss.key, e.cms.key, e.bs.key, e.key);
			return hash;
		}
	};

	enum : u32
	{
		NUM_TFX_DYNAMIC_OFFSETS = 2,
		NUM_UTILITY_SAMPLERS = 1,
		CONVERT_PUSH_CONSTANTS_SIZE = 96,

		NUM_CAS_PIPELINES = 2,
	};
	enum TFX_DESCRIPTOR_SET : u32
	{
		TFX_DESCRIPTOR_SET_UBO,
		TFX_DESCRIPTOR_SET_TEXTURES,

		NUM_TFX_DESCRIPTOR_SETS,
	};
	enum TFX_TEXTURES : u32
	{
		TFX_TEXTURE_TEXTURE,
		TFX_TEXTURE_PALETTE,
		TFX_TEXTURE_RT,
		TFX_TEXTURE_PRIMID,

		NUM_TFX_TEXTURES
	};
	enum DATE_RENDER_PASS : u32
	{
		DATE_RENDER_PASS_NONE = 0,
		DATE_RENDER_PASS_STENCIL = 1,
		DATE_RENDER_PASS_STENCIL_ONE = 2,
	};

private:
	std::unique_ptr<VKSwapChain> m_swap_chain;

	VkDescriptorSetLayout m_utility_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_utility_pipeline_layout = VK_NULL_HANDLE;

	VkDescriptorSetLayout m_tfx_ubo_ds_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_tfx_texture_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_tfx_pipeline_layout = VK_NULL_HANDLE;

	VKStreamBuffer m_vertex_stream_buffer;
	VKStreamBuffer m_index_stream_buffer;
	VKStreamBuffer m_vertex_uniform_stream_buffer;
	VKStreamBuffer m_fragment_uniform_stream_buffer;
	VKStreamBuffer m_texture_stream_buffer;
	VkBuffer m_expand_index_buffer = VK_NULL_HANDLE;
	VmaAllocation m_expand_index_buffer_allocation = VK_NULL_HANDLE;

	VkSampler m_point_sampler = VK_NULL_HANDLE;
	VkSampler m_linear_sampler = VK_NULL_HANDLE;

	std::unordered_map<u32, VkSampler> m_samplers;

	std::array<VkPipeline, static_cast<int>(ShaderConvert::Count)> m_convert{};
	std::array<VkPipeline, static_cast<int>(PresentShader::Count)> m_present{};
	std::array<VkPipeline, 16> m_color_copy{};
	std::array<VkPipeline, 2> m_merge{};
	std::array<VkPipeline, NUM_INTERLACE_SHADERS> m_interlace{};
	VkPipeline m_hdr_setup_pipelines[2][2] = {}; // [depth][feedback_loop]
	VkPipeline m_hdr_finish_pipelines[2][2] = {}; // [depth][feedback_loop]
	VkRenderPass m_date_image_setup_render_passes[2][2] = {}; // [depth][clear]
	VkPipeline m_date_image_setup_pipelines[2][2] = {}; // [depth][datm]
	VkPipeline m_fxaa_pipeline = {};
	VkPipeline m_shadeboost_pipeline = {};

	std::unordered_map<u32, VkShaderModule> m_tfx_vertex_shaders;
	std::unordered_map<GSHWDrawConfig::PSSelector, VkShaderModule, GSHWDrawConfig::PSSelectorHash>
		m_tfx_fragment_shaders;
	std::unordered_map<PipelineSelector, VkPipeline, PipelineSelectorHash> m_tfx_pipelines;

	VkRenderPass m_utility_color_render_pass_load = VK_NULL_HANDLE;
	VkRenderPass m_utility_color_render_pass_clear = VK_NULL_HANDLE;
	VkRenderPass m_utility_color_render_pass_discard = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_load = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_clear = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_discard = VK_NULL_HANDLE;
	VkRenderPass m_date_setup_render_pass = VK_NULL_HANDLE;
	VkRenderPass m_swap_chain_render_pass = VK_NULL_HANDLE;

	VkRenderPass m_tfx_render_pass[2][2][2][3][2][2][3][3] = {}; // [rt][ds][hdr][date][fbl][dsp][rt_op][ds_op]

	VkDescriptorSetLayout m_cas_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_cas_pipeline_layout = VK_NULL_HANDLE;
	std::array<VkPipeline, NUM_CAS_PIPELINES> m_cas_pipelines = {};
	VkPipeline m_imgui_pipeline = VK_NULL_HANDLE;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	std::string m_tfx_source;

	GSTexture* CreateSurface(
		GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE,
		const GSRegEXTBUF& EXTBUF, u32 c, const bool linear) final;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;

	bool DoCAS(
		GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) final;

	VkSampler GetSampler(GSHWDrawConfig::SamplerSelector ss);
	void ClearSamplerCache() final;

	VkShaderModule GetTFXVertexShader(GSHWDrawConfig::VSSelector sel);
	VkShaderModule GetTFXFragmentShader(const GSHWDrawConfig::PSSelector& sel);
	VkPipeline CreateTFXPipeline(const PipelineSelector& p);
	VkPipeline GetTFXPipeline(const PipelineSelector& p);

	VkShaderModule GetUtilityVertexShader(const std::string& source, const char* replace_main);
	VkShaderModule GetUtilityFragmentShader(const std::string& source, const char* replace_main);

	bool CreateDeviceAndSwapChain();
	bool CheckFeatures();
	bool CreateNullTexture();
	bool CreateBuffers();
	bool CreatePipelineLayouts();
	bool CreateRenderPasses();

	bool CompileConvertPipelines();
	bool CompilePresentPipelines();
	bool CompileInterlacePipelines();
	bool CompileMergePipelines();
	bool CompilePostProcessingPipelines();
	bool CompileCASPipelines();

	bool CompileImGuiPipeline();
	void RenderImGui();
	void RenderBlankFrame();

	void DestroyResources();

public:
	GSDeviceVK();
	~GSDeviceVK() override;

	__fi static GSDeviceVK* GetInstance() { return static_cast<GSDeviceVK*>(g_gs_device.get()); }

	static void GetAdaptersAndFullscreenModes(
		std::vector<std::string>* adapters, std::vector<std::string>* fullscreen_modes);

	/// Returns true if Vulkan is suitable as a default for the devices in the system.
	static bool IsSuitableDefaultRenderer();

	__fi VkRenderPass GetTFXRenderPass(bool rt, bool ds, bool hdr, DATE_RENDER_PASS date, bool fbl, bool dsp,
		VkAttachmentLoadOp rt_op, VkAttachmentLoadOp ds_op) const
	{
		return m_tfx_render_pass[rt][ds][hdr][date][fbl][dsp][rt_op][ds_op];
	}
	__fi VkSampler GetPointSampler() const { return m_point_sampler; }
	__fi VkSampler GetLinearSampler() const { return m_linear_sampler; }

	RenderAPI GetRenderAPI() const override;
	bool HasSurface() const override;

	bool Create() override;
	void Destroy() override;

	bool UpdateWindow() override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsExclusiveFullscreen() const override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	void SetVSync(VsyncMode mode) override;

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
		bool green, bool blue, bool alpha) override;
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		PresentShader shader, float shaderTime, bool linear) override;
	void DrawMultiStretchRects(
		const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader) override;
	void DoMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTextureVK* dTex, ShaderConvert shader);

	void BeginRenderPassForStretchRect(
		GSTextureVK* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc, bool allow_discard = true);
	void DoStretchRect(GSTextureVK* sTex, const GSVector4& sRect, GSTextureVK* dTex, const GSVector4& dRect,
		VkPipeline pipeline, bool linear, bool allow_discard);
	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

	void BlitRect(GSTexture* sTex, const GSVector4i& sRect, u32 sLevel, GSTexture* dTex, const GSVector4i& dRect,
		u32 dLevel, bool linear);

	void UpdateCLUTTexture(
		GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM,
		GSTexture* dTex, u32 DBW, u32 DPSM) override;

	void SetupDATE(GSTexture* rt, GSTexture* ds, bool datm, const GSVector4i& bbox);
	GSTextureVK* SetupPrimitiveTrackingDATE(GSHWDrawConfig& config);

	void IASetVertexBuffer(const void* vertex, size_t stride, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr, bool check_state);
	void PSSetSampler(GSHWDrawConfig::SamplerSelector sel);

	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor,
		FeedbackLoopFlag feedback_loop = FeedbackLoopFlag_None);

	void SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb);
	void SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb);
	bool BindDrawPipeline(const PipelineSelector& p);

	void RenderHW(GSHWDrawConfig& config) override;
	void UpdateHWPipelineSelector(GSHWDrawConfig& config, PipelineSelector& pipe);
	void UploadHWDrawVerticesAndIndices(const GSHWDrawConfig& config);
	VkImageMemoryBarrier GetColorBufferBarrier(GSTextureVK* rt) const;
	VkDependencyFlags GetColorBufferBarrierFlags() const;
	void SendHWDraw(const GSHWDrawConfig& config, GSTextureVK* draw_rt, bool skip_first_barrier);

	//////////////////////////////////////////////////////////////////////////
	// Vulkan State
	//////////////////////////////////////////////////////////////////////////

public:
	VkFormat LookupNativeFormat(GSTexture::Format format) const;

	__fi VkFramebuffer GetCurrentFramebuffer() const { return m_current_framebuffer; }

	/// Ends any render pass, executes the command buffer, and invalidates cached state.
	void ExecuteCommandBuffer(bool wait_for_completion);
	void ExecuteCommandBuffer(bool wait_for_completion, const char* reason, ...);
	void ExecuteCommandBufferAndRestartRenderPass(bool wait_for_completion, const char* reason);
	void ExecuteCommandBufferForReadback();

	/// Set dirty flags on everything to force re-bind at next draw time.
	void InvalidateCachedState();

	/// Binds all dirty state to the command buffer.
	bool ApplyUtilityState(bool already_execed = false);
	bool ApplyTFXState(bool already_execed = false);

	void SetIndexBuffer(VkBuffer buffer);
	void SetBlendConstants(u8 color);
	void SetLineWidth(float width);

	void SetUtilityTexture(GSTexture* tex, VkSampler sampler);
	void SetUtilityPushConstants(const void* data, u32 size);
	void UnbindTexture(GSTextureVK* tex);

	// Ends a render pass if we're currently in one.
	// When Bind() is next called, the pass will be restarted.
	// Calling this function is allowed even if a pass has not begun.
	bool InRenderPass();
	void BeginRenderPass(VkRenderPass rp, const GSVector4i& rect);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, const VkClearValue* cv, u32 cv_count);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, u32 clear_color);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, float depth, u8 stencil);
	void EndRenderPass();

	void SetViewport(const VkViewport& viewport);
	void SetScissor(const GSVector4i& scissor);
	void SetPipeline(VkPipeline pipeline);

private:
	enum DIRTY_FLAG : u32
	{
		DIRTY_FLAG_TFX_TEXTURE_0 = (1 << 0), // 0, 1, 2, 3
		DIRTY_FLAG_TFX_UBO = (1 << 4),
		DIRTY_FLAG_UTILITY_TEXTURE = (1 << 5),
		DIRTY_FLAG_BLEND_CONSTANTS = (1 << 6),
		DIRTY_FLAG_LINE_WIDTH = (1 << 7),
		DIRTY_FLAG_INDEX_BUFFER = (1 << 8),
		DIRTY_FLAG_VIEWPORT = (1 << 9),
		DIRTY_FLAG_SCISSOR = (1 << 10),
		DIRTY_FLAG_PIPELINE = (1 << 11),
		DIRTY_FLAG_VS_CONSTANT_BUFFER = (1 << 12),
		DIRTY_FLAG_PS_CONSTANT_BUFFER = (1 << 13),

		DIRTY_FLAG_TFX_TEXTURE_TEX = (DIRTY_FLAG_TFX_TEXTURE_0 << 0),
		DIRTY_FLAG_TFX_TEXTURE_PALETTE = (DIRTY_FLAG_TFX_TEXTURE_0 << 1),
		DIRTY_FLAG_TFX_TEXTURE_RT = (DIRTY_FLAG_TFX_TEXTURE_0 << 2),
		DIRTY_FLAG_TFX_TEXTURE_PRIMID = (DIRTY_FLAG_TFX_TEXTURE_0 << 3),

		DIRTY_FLAG_TFX_TEXTURES = DIRTY_FLAG_TFX_TEXTURE_TEX | DIRTY_FLAG_TFX_TEXTURE_PALETTE |
								  DIRTY_FLAG_TFX_TEXTURE_RT | DIRTY_FLAG_TFX_TEXTURE_PRIMID,

		DIRTY_BASE_STATE = DIRTY_FLAG_INDEX_BUFFER | DIRTY_FLAG_PIPELINE | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR |
						   DIRTY_FLAG_BLEND_CONSTANTS | DIRTY_FLAG_LINE_WIDTH,
		DIRTY_TFX_STATE = DIRTY_BASE_STATE | DIRTY_FLAG_TFX_TEXTURES,
		DIRTY_UTILITY_STATE = DIRTY_BASE_STATE | DIRTY_FLAG_UTILITY_TEXTURE,
		DIRTY_CONSTANT_BUFFER_STATE = DIRTY_FLAG_VS_CONSTANT_BUFFER | DIRTY_FLAG_PS_CONSTANT_BUFFER,
		ALL_DIRTY_STATE = DIRTY_BASE_STATE | DIRTY_TFX_STATE | DIRTY_UTILITY_STATE | DIRTY_CONSTANT_BUFFER_STATE,
	};

	enum class PipelineLayout
	{
		Undefined,
		TFX,
		Utility
	};

	void InitializeState();
	bool CreatePersistentDescriptorSets();

	void SetInitialState(VkCommandBuffer cmdbuf);
	void ApplyBaseState(u32 flags, VkCommandBuffer cmdbuf);

	// Which bindings/state has to be updated before the next draw.
	u32 m_dirty_flags = 0;
	FeedbackLoopFlag m_current_framebuffer_feedback_loop = FeedbackLoopFlag_None;
	bool m_warned_slow_spin = false;

	VkBuffer m_index_buffer = VK_NULL_HANDLE;

	GSTextureVK* m_current_render_target = nullptr;
	GSTextureVK* m_current_depth_target = nullptr;
	VkFramebuffer m_current_framebuffer = VK_NULL_HANDLE;
	VkRenderPass m_current_render_pass = VK_NULL_HANDLE;
	GSVector4i m_current_render_pass_area = GSVector4i::zero();

	GSVector4i m_scissor = GSVector4i::zero();
	VkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
	float m_current_line_width = 1.0f;
	u8 m_blend_constant_color = 0;

	std::array<const GSTextureVK*, NUM_TFX_TEXTURES> m_tfx_textures{};
	VkSampler m_tfx_sampler = VK_NULL_HANDLE;
	u32 m_tfx_sampler_sel = 0;
	VkDescriptorSet m_tfx_ubo_descriptor_set = VK_NULL_HANDLE;
	VkDescriptorSet m_tfx_texture_descriptor_set = VK_NULL_HANDLE;
	VkDescriptorSet m_tfx_rt_descriptor_set = VK_NULL_HANDLE;
	std::array<u32, NUM_TFX_DYNAMIC_OFFSETS> m_tfx_dynamic_offsets{};

	const GSTextureVK* m_utility_texture = nullptr;
	VkSampler m_utility_sampler = VK_NULL_HANDLE;
	VkDescriptorSet m_utility_descriptor_set = VK_NULL_HANDLE;

	PipelineLayout m_current_pipeline_layout = PipelineLayout::Undefined;
	VkPipeline m_current_pipeline = VK_NULL_HANDLE;

	std::unique_ptr<GSTextureVK> m_null_texture;

	// current pipeline selector - we save this in the struct to avoid re-zeroing it every draw
	PipelineSelector m_pipeline_selector = {};
};
