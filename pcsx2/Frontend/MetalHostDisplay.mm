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
#include "MetalHostDisplay.h"
#include "GS/Renderers/Metal/GSMetalCPPAccessible.h"
#include "GS/Renderers/Metal/GSDeviceMTL.h"
#include <imgui.h>

#ifdef __APPLE__

class MetalHostDisplayTexture final : public HostDisplayTexture
{
	MRCOwned<id<MTLTexture>> m_tex;
	u32 m_width, m_height;
public:
	MetalHostDisplayTexture(MRCOwned<id<MTLTexture>> tex, u32 width, u32 height)
		: m_tex(std::move(tex))
		, m_width(width)
		, m_height(height)
	{
	}

	void* GetHandle() const override { return (__bridge void*)m_tex; };
	u32 GetWidth()  const override { return m_width; }
	u32 GetHeight() const override { return m_height; }
};

HostDisplay* MakeMetalHostDisplay()
{
	return new MetalHostDisplay();
}

MetalHostDisplay::MetalHostDisplay()
{
}

MetalHostDisplay::~MetalHostDisplay()
{
}

HostDisplay::AdapterAndModeList GetMetalAdapterAndModeList()
{ @autoreleasepool {
	HostDisplay::AdapterAndModeList list;
	auto devs = MRCTransfer(MTLCopyAllDevices());
	for (id<MTLDevice> dev in devs.Get())
		list.adapter_names.push_back([[dev name] UTF8String]);
	return list;
}}

template <typename Fn>
static void OnMainThread(Fn&& fn)
{
	if ([NSThread isMainThread])
		fn();
	else
		dispatch_sync(dispatch_get_main_queue(), fn);
}

HostDisplay::RenderAPI MetalHostDisplay::GetRenderAPI() const
{
	return RenderAPI::Metal;
}

void* MetalHostDisplay::GetRenderDevice()  const { return const_cast<void*>(static_cast<const void*>(&m_dev)); }
void* MetalHostDisplay::GetRenderContext() const { return (__bridge void*)m_queue; }
void* MetalHostDisplay::GetRenderSurface() const { return (__bridge void*)m_layer; }
bool MetalHostDisplay::HasRenderDevice()   const { return m_dev.IsOk(); }
bool MetalHostDisplay::HasRenderSurface()  const { return static_cast<bool>(m_layer);}

void MetalHostDisplay::AttachSurfaceOnMainThread()
{
	ASSERT([NSThread isMainThread]);
	m_layer = MRCRetain([CAMetalLayer layer]);
	[m_layer setDrawableSize:CGSizeMake(m_window_info.surface_width, m_window_info.surface_height)];
	[m_layer setDevice:m_dev.dev];
	m_view = MRCRetain((__bridge NSView*)m_window_info.window_handle);
	[m_view setWantsLayer:YES];
	[m_view setLayer:m_layer];
}

void MetalHostDisplay::DetachSurfaceOnMainThread()
{
	ASSERT([NSThread isMainThread]);
	[m_view setLayer:nullptr];
	[m_view setWantsLayer:NO];
	m_view = nullptr;
	m_layer = nullptr;
}

bool MetalHostDisplay::CreateRenderDevice(const WindowInfo& wi, std::string_view adapter_name, VsyncMode vsync, bool threaded_presentation, bool debug_device)
{ @autoreleasepool {
	m_window_info = wi;
	pxAssertRel(!m_dev.dev, "Device already created!");
	std::string null_terminated_adapter_name(adapter_name);
	NSString* ns_adapter_name = [NSString stringWithUTF8String:null_terminated_adapter_name.c_str()];
	auto devs = MRCTransfer(MTLCopyAllDevices());
	for (id<MTLDevice> dev in devs.Get())
	{
		if ([[dev name] isEqualToString:ns_adapter_name])
			m_dev = GSMTLDevice(MRCRetain(dev));
	}
	if (!m_dev.dev)
	{
		if (!adapter_name.empty())
			Console.Warning("Metal: Couldn't find adapter %s, using default", null_terminated_adapter_name.c_str());
		m_dev = GSMTLDevice(MRCTransfer(MTLCreateSystemDefaultDevice()));
		if (!m_dev.dev)
			Host::ReportErrorAsync("No Metal Devices Available", "No Metal-supporting GPUs were found.  PCSX2 requires a Metal GPU (available on all macs from 2012 onwards).");
	}
	m_queue = MRCTransfer([m_dev.dev newCommandQueue]);

	m_pass_desc = MRCTransfer([MTLRenderPassDescriptor new]);
	[m_pass_desc colorAttachments][0].loadAction = MTLLoadActionClear;
	[m_pass_desc colorAttachments][0].clearColor = MTLClearColorMake(0, 0, 0, 0);
	[m_pass_desc colorAttachments][0].storeAction = MTLStoreActionStore;

	m_capture_start_frame = 0;
	if (char* env = getenv("MTL_CAPTURE"))
	{
		m_capture_start_frame = atoi(env);
	}
	if (m_capture_start_frame)
	{
		Console.WriteLn("Metal will capture frame %u", m_capture_start_frame);
	}

	if (m_dev.IsOk() && m_queue)
	{
		OnMainThread([this]
		{
			AttachSurfaceOnMainThread();
		});
		SetVSync(vsync);
		return true;
	}
	else
		return false;
}}

bool MetalHostDisplay::InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device)
{
	return true;
}

bool MetalHostDisplay::MakeRenderContextCurrent() { return true; }
bool MetalHostDisplay::DoneRenderContextCurrent() { return true; }

void MetalHostDisplay::DestroyRenderDevice()
{
	DestroyRenderSurface();
	m_queue = nullptr;
	m_dev.Reset();
}

void MetalHostDisplay::DestroyRenderSurface()
{
	if (!m_layer)
		return;
	OnMainThread([this]{ DetachSurfaceOnMainThread(); });
	m_layer = nullptr;
}

bool MetalHostDisplay::ChangeRenderWindow(const WindowInfo& wi)
{
	OnMainThread([this, &wi]
	{
		DetachSurfaceOnMainThread();
		m_window_info = wi;
		AttachSurfaceOnMainThread();
	});
	return true;
}

bool MetalHostDisplay::SupportsFullscreen() const { return false; }
bool MetalHostDisplay::IsFullscreen() { return false; }
bool MetalHostDisplay::SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) { return false; }

HostDisplay::AdapterAndModeList MetalHostDisplay::GetAdapterAndModeList()
{
	return GetMetalAdapterAndModeList();
}

std::string MetalHostDisplay::GetDriverInfo() const
{ @autoreleasepool {
	std::string desc([[m_dev.dev description] UTF8String]);
	desc += "\n    Texture Swizzle:   " + std::string(m_dev.features.texture_swizzle   ? "Supported" : "Unsupported");
	desc += "\n    Unified Memory:    " + std::string(m_dev.features.unified_memory    ? "Supported" : "Unsupported");
	desc += "\n    Framebuffer Fetch: " + std::string(m_dev.features.framebuffer_fetch ? "Supported" : "Unsupported");
	desc += "\n    Primitive ID:      " + std::string(m_dev.features.primid            ? "Supported" : "Unsupported");
	desc += "\n    Shader Version:    " + std::string(to_string(m_dev.features.shader_version));
	desc += "\n    Max Texture Size:  " + std::to_string(m_dev.features.max_texsize);
	return desc;
}}

void MetalHostDisplay::ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	m_window_info.surface_scale = new_window_scale;
	if (m_window_info.surface_width == static_cast<u32>(new_window_width) && m_window_info.surface_height == static_cast<u32>(new_window_height))
		return;
	m_window_info.surface_width = new_window_width;
	m_window_info.surface_height = new_window_height;
	@autoreleasepool
	{
		[m_layer setDrawableSize:CGSizeMake(new_window_width, new_window_height)];
	}
}

std::unique_ptr<HostDisplayTexture> MetalHostDisplay::CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic)
{ @autoreleasepool {
	MTLTextureDescriptor* desc = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		                             width:width
		                            height:height
		                         mipmapped:false];
	[desc setUsage:MTLTextureUsageShaderRead];
	[desc setStorageMode:MTLStorageModePrivate];
	MRCOwned<id<MTLTexture>> tex = MRCTransfer([m_dev.dev newTextureWithDescriptor:desc]);
	if (!tex)
		return nullptr; // Something broke yay
	[tex setLabel:@"MetalHostDisplay Texture"];
	if (data)
		UpdateTexture(tex, 0, 0, width, height, data, data_stride);
	return std::make_unique<MetalHostDisplayTexture>(std::move(tex), width, height);
}}

void MetalHostDisplay::UpdateTexture(id<MTLTexture> texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride)
{
	id<MTLCommandBuffer> cmdbuf = [m_queue commandBuffer];
	id<MTLBlitCommandEncoder> enc = [cmdbuf blitCommandEncoder];
	size_t bytes = data_stride * height;
	MRCOwned<id<MTLBuffer>> buf = MRCTransfer([m_dev.dev newBufferWithLength:bytes options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
	memcpy([buf contents], data, bytes);
	[enc copyFromBuffer:buf
	       sourceOffset:0
	  sourceBytesPerRow:data_stride
	sourceBytesPerImage:bytes
	         sourceSize:MTLSizeMake(width, height, 1)
	          toTexture:texture
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	[enc endEncoding];
	[cmdbuf commit];
}

void MetalHostDisplay::UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride)
{ @autoreleasepool {
	UpdateTexture((__bridge id<MTLTexture>)texture->GetHandle(), x, y, width, height, data, data_stride);
}}

static bool s_capture_next = false;

bool MetalHostDisplay::BeginPresent(bool frame_skip)
{ @autoreleasepool {
	GSDeviceMTL* dev = static_cast<GSDeviceMTL*>(g_gs_device.get());
	if (dev && m_capture_start_frame && dev->FrameNo() == m_capture_start_frame)
		s_capture_next = true;
	if (frame_skip || m_window_info.type == WindowInfo::Type::Surfaceless || !g_gs_device)
	{
		ImGui::EndFrame();
		return false;
	}
	id<MTLCommandBuffer> buf = dev->GetRenderCmdBuf();
	m_current_drawable = MRCRetain([m_layer nextDrawable]);
	dev->EndRenderPass();
	if (!m_current_drawable)
	{
		[buf pushDebugGroup:@"Present Skipped"];
		[buf popDebugGroup];
		dev->FlushEncoders();
		ImGui::EndFrame();
		return false;
	}
	[m_pass_desc colorAttachments][0].texture = [m_current_drawable texture];
	id<MTLRenderCommandEncoder> enc = [buf renderCommandEncoderWithDescriptor:m_pass_desc];
	[enc setLabel:@"Present"];
	dev->m_current_render.encoder = MRCRetain(enc);
	return true;
}}

void MetalHostDisplay::EndPresent()
{ @autoreleasepool {
	GSDeviceMTL* dev = static_cast<GSDeviceMTL*>(g_gs_device.get());
	pxAssertDev(dev && dev->m_current_render.encoder && dev->m_current_render_cmdbuf, "BeginPresent cmdbuf was destroyed");
	ImGui::Render();
	dev->RenderImGui(ImGui::GetDrawData());
	dev->EndRenderPass();
	if (m_current_drawable)
		[dev->m_current_render_cmdbuf addScheduledHandler:[drawable = std::move(m_current_drawable)](id<MTLCommandBuffer>){
			[drawable present];
		}];
	dev->FlushEncoders();
	m_current_drawable = nullptr;
	if (m_capture_start_frame)
	{
		if (@available(macOS 10.15, iOS 13, *))
		{
			static NSString* const path = @"/tmp/PCSX2MTLCapture.gputrace";
			static u32 frames;
			if (frames)
			{
				--frames;
				if (!frames)
				{
					[[MTLCaptureManager sharedCaptureManager] stopCapture];
					Console.WriteLn("Metal Trace Capture to /tmp/PCSX2MTLCapture.gputrace finished");
					[[NSWorkspace sharedWorkspace] selectFile:path
					                 inFileViewerRootedAtPath:@"/tmp/"];
				}
			}
			else if (s_capture_next)
			{
				s_capture_next = false;
				MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
				if ([mgr supportsDestination:MTLCaptureDestinationGPUTraceDocument])
				{
					MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor new] autorelease];
					[desc setCaptureObject:m_dev.dev];
					if ([[NSFileManager defaultManager] fileExistsAtPath:path])
						[[NSFileManager defaultManager] removeItemAtPath:path error:nil];
					[desc setOutputURL:[NSURL fileURLWithPath:path]];
					[desc setDestination:MTLCaptureDestinationGPUTraceDocument];
					NSError* err = nullptr;
					[mgr startCaptureWithDescriptor:desc error:&err];
					if (err)
					{
						Console.Error("Metal Trace Capture failed: %s", [[err localizedDescription] UTF8String]);
					}
					else
					{
						Console.WriteLn("Metal Trace Capture to /tmp/PCSX2MTLCapture.gputrace started");
						frames = 2;
					}
				}
				else
				{
					Console.Error("Metal Trace Capture Failed: MTLCaptureManager doesn't support GPU trace documents! (Did you forget to run with METAL_CAPTURE_ENABLED=1?)");
				}
			}
		}
	}
}}

void MetalHostDisplay::SetVSync(VsyncMode mode)
{
	[m_layer setDisplaySyncEnabled:mode != VsyncMode::Off];
	m_vsync_mode = mode;
}

bool MetalHostDisplay::CreateImGuiContext()
{
	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = "pcsx2_imgui_metal";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
	return true;
}

void MetalHostDisplay::DestroyImGuiContext()
{
	ImGui::GetIO().Fonts->SetTexID(nullptr);
}

bool MetalHostDisplay::UpdateImGuiFontTexture()
{ @autoreleasepool {
	u8* data;
	int width, height;
	ImFontAtlas* fonts = ImGui::GetIO().Fonts;
	fonts->GetTexDataAsAlpha8(&data, &width, &height);
	MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatA8Unorm width:width height:height mipmapped:false];
	[desc setUsage:MTLTextureUsageShaderRead];
	[desc setStorageMode:MTLStorageModePrivate];
	if (@available(macOS 10.15, *))
		if (m_dev.features.texture_swizzle)
			[desc setSwizzle:MTLTextureSwizzleChannelsMake(MTLTextureSwizzleOne, MTLTextureSwizzleOne, MTLTextureSwizzleOne, MTLTextureSwizzleAlpha)];
	m_font_tex = MRCTransfer([m_dev.dev newTextureWithDescriptor:desc]);
	[m_font_tex setLabel:@"ImGui Font"];
	UpdateTexture(m_font_tex, 0, 0, width, height, data, width);
	fonts->SetTexID((__bridge void*)m_font_tex);
	return static_cast<bool>(m_font_tex);
}}

bool MetalHostDisplay::GetHostRefreshRate(float* refresh_rate)
{
	OnMainThread([this, refresh_rate]
	{
		u32 did = [[[[[m_view window] screen] deviceDescription] valueForKey:@"NSScreenNumber"] unsignedIntValue];
		if (CGDisplayModeRef mode = CGDisplayCopyDisplayMode(did))
		{
			*refresh_rate = CGDisplayModeGetRefreshRate(mode);
			CGDisplayModeRelease(mode);
		}
		else
		{
			*refresh_rate = 0;
		}
	});
	return *refresh_rate != 0;
}

#endif // __APPLE__
