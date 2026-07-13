// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Null/GSDeviceNone.h"

// -------------------------------------------------------------------------
// GSTextureNone
// -------------------------------------------------------------------------

GSTextureNone::GSTextureNone(Type type, int width, int height, int levels, Format format)
{
	m_type = type;
	m_size = GSVector2i(width, height);
	m_mipmap_levels = levels;
	m_format = format;
}

void* GSTextureNone::GetNativeHandle() const
{
	return nullptr;
}

bool GSTextureNone::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	return true;
}

bool GSTextureNone::Map(GSMap& m, const GSVector4i* r, int layer)
{
	// 8 bytes/texel covers the widest uncompressed format (RGBA16); callers get
	// scratch memory they can safely write through, contents are discarded.
	const GSVector4i rc = r ? *r : GSVector4i::loadh(m_size);
	const u32 pitch = static_cast<u32>(m_size.x) * 8;
	m_map_buffer.resize(static_cast<size_t>(pitch) * static_cast<size_t>(m_size.y));
	m.bits = m_map_buffer.data() + static_cast<size_t>(rc.top) * pitch + static_cast<size_t>(rc.left) * 8;
	m.pitch = static_cast<int>(pitch);
	return true;
}

void GSTextureNone::Unmap()
{
}

void GSTextureNone::GenerateMipmap()
{
}

#ifdef PCSX2_DEVBUILD
void GSTextureNone::SetDebugName(std::string_view name)
{
}
#endif

// -------------------------------------------------------------------------
// GSDownloadTextureNone
// -------------------------------------------------------------------------

GSDownloadTextureNone::GSDownloadTextureNone(u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
{
	m_buffer.resize(GetBufferSize(width, height, format));
}

void GSDownloadTextureNone::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	m_current_pitch = GetTransferPitch(use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width, 1);
	m_needs_flush = false;
}

bool GSDownloadTextureNone::Map(const GSVector4i& read_rc)
{
	m_map_pointer = m_buffer.data();
	return true;
}

void GSDownloadTextureNone::Unmap()
{
	m_map_pointer = nullptr;
}

void GSDownloadTextureNone::Flush()
{
}

#ifdef PCSX2_DEVBUILD
void GSDownloadTextureNone::SetDebugName(std::string_view name)
{
}
#endif

// -------------------------------------------------------------------------
// GSDeviceNone
// -------------------------------------------------------------------------

bool GSDeviceNone::Create(GSVSyncMode vsync_mode, bool allow_present_throttle)
{
	if (!GSDevice::Create(vsync_mode, allow_present_throttle))
		return false;

	m_name = "None";
	m_max_texture_size = 8192;

	// Nominal surfaceless "window" so layout consumers (ImGui display size, OSD
	// scale) see sane nonzero dimensions.
	m_window_info = WindowInfo();
	m_window_info.surface_width = 640;
	m_window_info.surface_height = 480;
	m_window_info.surface_scale = 1.0f;
	return true;
}

RenderAPI GSDeviceNone::GetRenderAPI() const
{
	return RenderAPI::None;
}

bool GSDeviceNone::HasSurface() const
{
	return true;
}

void GSDeviceNone::DestroySurface()
{
}

bool GSDeviceNone::UpdateWindow()
{
	return true;
}

void GSDeviceNone::ResizeWindow(u32 new_window_width, u32 new_window_height, float new_window_scale)
{
	m_window_info.surface_width = new_window_width;
	m_window_info.surface_height = new_window_height;
	m_window_info.surface_scale = new_window_scale;
}

bool GSDeviceNone::SupportsExclusiveFullscreen() const
{
	return false;
}

// Always FrameSkipped: the caller skips the whole present/ImGui draw path, so
// none of the drawing stubs below are ever reached with real work.
GSDevice::PresentResult GSDeviceNone::BeginPresent(bool frame_skip)
{
	return PresentResult::FrameSkipped;
}

void GSDeviceNone::EndPresent()
{
}

void GSDeviceNone::SetVSyncMode(GSVSyncMode mode, bool allow_present_throttle)
{
	m_vsync_mode = mode;
	m_allow_present_throttle = allow_present_throttle;
}

std::string GSDeviceNone::GetDriverInfo() const
{
	return "Null host device (no graphics API)";
}

bool GSDeviceNone::SetGPUTimingEnabled(bool enabled)
{
	return false;
}

float GSDeviceNone::GetAndResetAccumulatedGPUTime()
{
	return 0.0f;
}

void GSDeviceNone::PushDebugGroup(const char* fmt, ...)
{
}

void GSDeviceNone::PopDebugGroup()
{
}

void GSDeviceNone::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
}

std::unique_ptr<GSDownloadTexture> GSDeviceNone::CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format)
{
	return std::make_unique<GSDownloadTextureNone>(width, height, format);
}

void GSDeviceNone::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
}

void GSDeviceNone::PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	PresentShader shader, float shaderTime, Filter filter)
{
}

void GSDeviceNone::UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex,
	u32 dOffset, u32 dSize)
{
}

void GSDeviceNone::ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM,
	GSTexture* dTex, u32 DBW, u32 DPSM)
{
}

void GSDeviceNone::FilteredDownsampleTexture(GSTexture* sTex, GSTexture* dTex, u32 downsample_factor,
	const GSVector2i& clamp_min, const GSVector4& dRect)
{
}

void GSDeviceNone::RenderHW(GSHWDrawConfig& config)
{
}

void GSDeviceNone::ClearSamplerCache()
{
}

GSTexture* GSDeviceNone::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{
	return new GSTextureNone(type, width, height, levels, format);
}

void GSDeviceNone::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect,
	const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const Filter filter)
{
}

void GSDeviceNone::DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderInterlace shader, Filter filter, const InterlaceConstantBuffer& cb)
{
}

void GSDeviceNone::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
}

void GSDeviceNone::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
}

bool GSDeviceNone::DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants)
{
	return true;
}

void GSDeviceNone::DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderConvertSelector shader, Filter filter)
{
}

void GSDeviceNone::DoStretchRect(GSTexture* sTex, const GSVector4& sRect, const GSVector4& dRect,
	PresentShader shader, Filter filter)
{
}
