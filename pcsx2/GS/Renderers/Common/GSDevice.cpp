// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/GSGL.h"
#include "GS/GS.h"
#include "GS/GSUtil.h"
#include "Host.h"

#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/FileSystem.h"
#include "common/HostSys.h"
#include "common/Path.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include "imgui.h"

#include <algorithm>
#include <ostream>
#include <fstream>

int SetDATMShader(SetDATM datm)
{
	switch (datm)
	{
		case SetDATM::DATM1_RTA_CORRECTION:
			return static_cast<int>(ShaderConvert::DATM_1_RTA_CORRECTION);
		case SetDATM::DATM0_RTA_CORRECTION:
			return static_cast<int>(ShaderConvert::DATM_0_RTA_CORRECTION);
		case SetDATM::DATM1:
			return static_cast<int>(ShaderConvert::DATM_1);
		case SetDATM::DATM0:
		default:
			return static_cast<int>(ShaderConvert::DATM_0);
	}
}

const char* shaderName(ShaderConvert value)
{
	switch (value)
	{
			// clang-format off
		case ShaderConvert::COPY:                   return "ps_copy";
		case ShaderConvert::RGBA8_TO_16_BITS:       return "ps_convert_rgba8_16bits";
		case ShaderConvert::DATM_1:                 return "ps_datm1";
		case ShaderConvert::DATM_0:                 return "ps_datm0";
		case ShaderConvert::DATM_1_RTA_CORRECTION:  return "ps_datm1_rta_correction";
		case ShaderConvert::DATM_0_RTA_CORRECTION:  return "ps_datm0_rta_correction";
		case ShaderConvert::COLCLIP_INIT:           return "ps_colclip_init";
		case ShaderConvert::COLCLIP_RESOLVE:        return "ps_colclip_resolve";
		case ShaderConvert::RTA_CORRECTION:         return "ps_rta_correction";
		case ShaderConvert::RTA_DECORRECTION:       return "ps_rta_decorrection";
		case ShaderConvert::TRANSPARENCY_FILTER:    return "ps_filter_transparency";
		case ShaderConvert::FLOAT32_TO_16_BITS:     return "ps_convert_float32_32bits";
		case ShaderConvert::FLOAT32_TO_32_BITS:     return "ps_convert_float32_32bits";
		case ShaderConvert::FLOAT32_TO_RGBA8:       return "ps_convert_float32_rgba8";
		case ShaderConvert::FLOAT32_TO_RGB8:        return "ps_convert_float32_rgba8";
		case ShaderConvert::FLOAT16_TO_RGB5A1:      return "ps_convert_float16_rgb5a1";
		case ShaderConvert::RGBA8_TO_FLOAT32:       return "ps_convert_rgba8_float32";
		case ShaderConvert::RGBA8_TO_FLOAT24:       return "ps_convert_rgba8_float24";
		case ShaderConvert::RGBA8_TO_FLOAT16:       return "ps_convert_rgba8_float16";
		case ShaderConvert::RGB5A1_TO_FLOAT16:      return "ps_convert_rgb5a1_float16";
		case ShaderConvert::RGBA8_TO_FLOAT32_BILN:  return "ps_convert_rgba8_float32_biln";
		case ShaderConvert::RGBA8_TO_FLOAT24_BILN:  return "ps_convert_rgba8_float24_biln";
		case ShaderConvert::RGBA8_TO_FLOAT16_BILN:  return "ps_convert_rgba8_float16_biln";
		case ShaderConvert::RGB5A1_TO_FLOAT16_BILN: return "ps_convert_rgb5a1_float16_biln";
		case ShaderConvert::FLOAT32_TO_FLOAT24:     return "ps_convert_float32_float24";
		case ShaderConvert::DEPTH_COPY:             return "ps_depth_copy";
		case ShaderConvert::DOWNSAMPLE_COPY:        return "ps_downsample_copy";
		case ShaderConvert::RGBA_TO_8I:             return "ps_convert_rgba_8i";
		case ShaderConvert::RGB5A1_TO_8I:           return "ps_convert_rgb5a1_8i";
		case ShaderConvert::CLUT_4:                 return "ps_convert_clut_4";
		case ShaderConvert::CLUT_8:                 return "ps_convert_clut_8";
		case ShaderConvert::YUV:                    return "ps_yuv";
			// clang-format on
		default:
			pxAssert(0);
			return "ShaderConvertUnknownShader";
	}
}

const char* shaderName(PresentShader value)
{
	switch (value)
	{
			// clang-format off
		case PresentShader::COPY:               return "ps_copy";
		case PresentShader::SCANLINE:           return "ps_filter_scanlines";
		case PresentShader::DIAGONAL_FILTER:    return "ps_filter_diagonal";
		case PresentShader::TRIANGULAR_FILTER:  return "ps_filter_triangular";
		case PresentShader::COMPLEX_FILTER:     return "ps_filter_complex";
		case PresentShader::LOTTES_FILTER:      return "ps_filter_lottes";
		case PresentShader::SUPERSAMPLE_4xRGSS: return "ps_4x_rgss";
		case PresentShader::SUPERSAMPLE_AUTO:   return "ps_automagical_supersampling";
			// clang-format on
		default:
			pxAssert(0);
			return "DisplayShaderUnknownShader";
	}
}

#ifdef PCSX2_DEVBUILD

enum class TextureLabel
{
	ColorRT,
	ColorHQRT,
	ColorHDRRT,
	ColorClipRT,
	U16RT,
	U32RT,
	DepthStencil,
	PrimIDTexture,
	RWTexture,
	CLUTTexture,
	Texture,
	ReplacementTexture,
	Other,
	Last = Other,
};

static std::array<u32, static_cast<u32>(TextureLabel::Last) + 1> s_texture_counts;

static TextureLabel GetTextureLabel(GSTexture::Type type, GSTexture::Format format)
{
	switch (type)
	{
		case GSTexture::Type::RenderTarget:
			switch (format)
			{
				case GSTexture::Format::Color:
					return TextureLabel::ColorRT;
				case GSTexture::Format::ColorHQ:
					return TextureLabel::ColorHQRT;
				case GSTexture::Format::ColorHDR:
					return TextureLabel::ColorHDRRT;
				case GSTexture::Format::ColorClip:
					return TextureLabel::ColorClipRT;
				case GSTexture::Format::UInt16:
					return TextureLabel::U16RT;
				case GSTexture::Format::UInt32:
					return TextureLabel::U32RT;
				case GSTexture::Format::PrimID:
					return TextureLabel::PrimIDTexture;
				default:
					return TextureLabel::Other;
			}
		case GSTexture::Type::Texture:
			switch (format)
			{
				case GSTexture::Format::Color:
					return TextureLabel::Texture;
				case GSTexture::Format::UNorm8:
					return TextureLabel::CLUTTexture;
				case GSTexture::Format::BC1:
				case GSTexture::Format::BC2:
				case GSTexture::Format::BC3:
				case GSTexture::Format::BC7:
				case GSTexture::Format::ColorHDR:
					return TextureLabel::ReplacementTexture;
				default:
					return TextureLabel::Other;
			}
		case GSTexture::Type::DepthStencil:
			return TextureLabel::DepthStencil;
		case GSTexture::Type::RWTexture:
			return TextureLabel::RWTexture;
		case GSTexture::Type::Invalid:
		default:
			return TextureLabel::Other;
	}
}

// Debug names
static const char* TextureLabelString(TextureLabel label)
{
	switch (label)
	{
		case TextureLabel::ColorRT:
			return "Color RT";
		case TextureLabel::ColorHQRT:
			return "Color HQ RT";
		case TextureLabel::ColorHDRRT:
			return "Color HDR RT";
		case TextureLabel::ColorClipRT:
			return "Color Clip RT";
		case TextureLabel::U16RT:
			return "U16 RT";
		case TextureLabel::U32RT:
			return "U32 RT";
		case TextureLabel::DepthStencil:
			return "Depth Stencil";
		case TextureLabel::PrimIDTexture:
			return "PrimID";
		case TextureLabel::RWTexture:
			return "RW Texture";
		case TextureLabel::CLUTTexture:
			return "CLUT Texture";
		case TextureLabel::Texture:
			return "Texture";
		case TextureLabel::ReplacementTexture:
			return "Replacement Texture";
		case TextureLabel::Other:
		default:
			return "Unknown Texture";
	}
}

#endif

std::unique_ptr<GSDevice> g_gs_device;

GSDevice::GSDevice()
{
#ifdef PCSX2_DEVBUILD
	s_texture_counts.fill(0);
#endif
}

GSDevice::~GSDevice()
{
	// should've been cleaned up in Destroy()
	pxAssert(m_pool[0].empty() && m_pool[1].empty() && !m_merge && !m_weavebob && !m_blend && !m_mad && !m_target_tmp && !m_cas);
}

const char* GSDevice::RenderAPIToString(RenderAPI api)
{
	switch (api)
	{
		// clang-format off
#define CASE(x) case RenderAPI::x: return #x
		CASE(None);
		CASE(D3D11);
		CASE(D3D12);
		CASE(Metal);
		CASE(Vulkan);
		CASE(OpenGL);
#undef CASE
		// clang-format on
	default:
		return "Unknown";
	}
}

bool GSDevice::GetRequestedExclusiveFullscreenMode(u32* width, u32* height, float* refresh_rate)
{
	const std::string mode = Host::GetStringSettingValue("EmuCore/GS", "FullscreenMode", "");
	if (!mode.empty())
	{
		const std::string_view mode_view = mode;
		std::string_view::size_type sep1 = mode.find('x');
		if (sep1 != std::string_view::npos)
		{
			std::optional<u32> owidth = StringUtil::FromChars<u32>(mode_view.substr(0, sep1));
			sep1++;

			while (sep1 < mode.length() && std::isspace(mode[sep1]))
				sep1++;

			if (owidth.has_value() && sep1 < mode.length())
			{
				std::string_view::size_type sep2 = mode.find('@', sep1);
				if (sep2 != std::string_view::npos)
				{
					std::optional<u32> oheight = StringUtil::FromChars<u32>(mode_view.substr(sep1, sep2 - sep1));
					sep2++;

					while (sep2 < mode.length() && std::isspace(mode[sep2]))
						sep2++;

					if (oheight.has_value() && sep2 < mode.length())
					{
						std::optional<float> orefresh_rate = StringUtil::FromChars<float>(mode_view.substr(sep2));
						if (orefresh_rate.has_value())
						{
							*width = owidth.value();
							*height = oheight.value();
							*refresh_rate = orefresh_rate.value();
							return true;
						}
					}
				}
			}
		}
	}

	*width = 0;
	*height = 0;
	*refresh_rate = 0;
	return false;
}

std::string GSDevice::GetFullscreenModeString(u32 width, u32 height, float refresh_rate)
{
	return StringUtil::StdStringFromFormat("%u x %u @ %f hz", width, height, refresh_rate);
}

void GSDevice::GenerateExpansionIndexBuffer(void* buffer)
{
	static constexpr u32 MAX_INDEX = EXPAND_BUFFER_SIZE / 6 / sizeof(u16);

	u16* idx_buffer = static_cast<u16*>(buffer);
	for (u32 i = 0; i < MAX_INDEX; i++)
	{
		const u32 base = i * 4;
		*(idx_buffer++) = base + 0;
		*(idx_buffer++) = base + 1;
		*(idx_buffer++) = base + 2;
		*(idx_buffer++) = base + 1;
		*(idx_buffer++) = base + 2;
		*(idx_buffer++) = base + 3;
	}
}

GSVector4i GSDevice::ProcessCopyArea(const GSVector4i& rtsize, const GSVector4i& drawarea)
{
	GSVector4i snapped_drawarea(drawarea);
	// We don't want the snapped box adjustments when the rect is empty as it might make the copy to pass.
	// The empty rect itself needs to be handled in renderer properly.
	if (snapped_drawarea.rempty())
		return snapped_drawarea;

	// If copy area exceeds 95% coverage then we can do a full copy instead which should be faster.
	const float rt_area = static_cast<float>(rtsize.width() * rtsize.height());
	const float copy_area = static_cast<float>(drawarea.width() * drawarea.height());
	constexpr float coverage = 0.95f;
	if ((copy_area / rt_area) >= coverage)
	{
		snapped_drawarea = rtsize;
		return snapped_drawarea;
	}

	// Aligning bbox to 4 pixel boundaries so copies will be faster using Direct Memory Access,
	// otherwise it may stall as more commands need to be issued.
	snapped_drawarea = snapped_drawarea.ralign<Align_Outside>(GSVector2i(4, 4)).rintersect(rtsize);

	return snapped_drawarea;
}

std::optional<std::string> GSDevice::ReadShaderSource(const char* filename)
{
	return FileSystem::ReadFileToString(Path::Combine(EmuFolders::Resources, filename).c_str());
}

int GSDevice::GetMipmapLevelsForSize(int width, int height)
{
	return std::min(static_cast<int>(std::log2(std::max(width, height))) + 1, MAXIMUM_TEXTURE_MIPMAP_LEVELS);
}

bool GSDevice::Create(GSVSyncMode vsync_mode, bool allow_present_throttle)
{
	m_vsync_mode = vsync_mode;
	m_allow_present_throttle = allow_present_throttle;
	return true;
}

void GSDevice::Destroy()
{
	ClearCurrent();
	PurgePool();
}

bool GSDevice::AcquireWindow(bool recreate_window)
{
	std::optional<WindowInfo> wi = Host::AcquireRenderWindow(recreate_window);
	if (!wi.has_value())
	{
		Console.Error("Failed to acquire render window.");
		Host::ReportErrorAsync("Error", "Failed to acquire render window. The log may have more information.");
		return false;
	}

	m_window_info = std::move(wi.value());
	return true;
}

bool GSDevice::ShouldSkipPresentingFrame()
{
	// Only needed with FIFO.
	if (!m_allow_present_throttle || m_vsync_mode != GSVSyncMode::FIFO)
		return false;

	const float throttle_rate = (m_window_info.surface_refresh_rate > 0.0f) ? m_window_info.surface_refresh_rate : 60.0f;
	const u64 throttle_period = static_cast<u64>(static_cast<double>(GetTickFrequency()) / static_cast<double>(throttle_rate));

	const u64 now = GetCPUTicks();
	const double diff = now - m_last_frame_displayed_time;
	if (diff < throttle_period)
		return true;

	m_last_frame_displayed_time = now;
	return false;
}

void GSDevice::ThrottlePresentation()
{
	// Manually throttle presentation when vsync isn't enabled, so we don't try to render the
	// fullscreen UI at thousands of FPS and make the gpu go brrrrrrrr.
	const float throttle_rate = (m_window_info.surface_refresh_rate > 0.0f) ? m_window_info.surface_refresh_rate : 60.0f;

	const u64 sleep_period = static_cast<u64>(static_cast<double>(GetTickFrequency()) / static_cast<double>(throttle_rate));
	const u64 current_ts = GetCPUTicks();

	// Allow it to fall behind/run ahead up to 2*period. Sleep isn't that precise, plus we need to
	// allow time for the actual rendering.
	const u64 max_variance = sleep_period * 2;
	if (static_cast<u64>(std::abs(static_cast<s64>(current_ts - m_last_frame_displayed_time))) > max_variance)
		m_last_frame_displayed_time = current_ts + sleep_period;
	else
		m_last_frame_displayed_time += sleep_period;

	Threading::SleepUntil(m_last_frame_displayed_time);
}

void GSDevice::ClearRenderTarget(GSTexture* t, u32 c)
{
	t->SetClearColor(c);
}

void GSDevice::ClearDepth(GSTexture* t, float d)
{
	t->SetClearDepth(d);
}

bool GSDevice::ProcessClearsBeforeCopy(GSTexture* sTex, GSTexture* dTex, const bool full_copy)
{
	pxAssert(sTex->GetState() == GSTexture::State::Cleared && dTex->IsRenderTargetOrDepthStencil());

	// Pass it forward if we're clearing the whole thing.
	if (full_copy)
	{
		if (dTex->IsDepthStencil())
			dTex->SetClearDepth(sTex->GetClearDepth());
		else
			dTex->SetClearColor(sTex->GetClearColor());

		dTex->SetState(GSTexture::State::Cleared);

		return true;
	}

	// Destination is cleared, if it's the same colour and rect, we can just avoid this entirely.
	if (dTex->GetState() == GSTexture::State::Cleared)
	{
		if (dTex->IsDepthStencil())
		{
			if (dTex->GetClearDepth() == sTex->GetClearDepth())
				return true;
		}
		else
		{
			if (dTex->GetClearColor() == sTex->GetClearColor())
				return true;
		}
	}

	return false;
}

void GSDevice::InvalidateRenderTarget(GSTexture* t)
{
	t->SetState(GSTexture::State::Invalidated);
}

void GSDevice::UpdateImGuiTextures()
{
	// TODO, use ImDrawData https://github.com/ocornut/imgui/issues/8597#issuecomment-2871835598
	for (ImTextureData* im_tex : ImGui::GetPlatformIO().Textures)
	{
		switch (im_tex->Status)
		{
			case ImTextureStatus_OK:
			case ImTextureStatus_Destroyed:
				continue;
			case ImTextureStatus_WantCreate:
			{
				GSTexture* gs_tex = g_gs_device->CreateTexture(im_tex->Width, im_tex->Height, 1, GSTexture::Format::Color);
				if (!gs_tex)
					pxFailRel("Failed to create ImGui texture");

				im_tex->SetTexID(reinterpret_cast<ImTextureID>(gs_tex->GetNativeHandle()));
				im_tex->BackendUserData = gs_tex;
				[[fallthrough]];
			}
			case ImTextureStatus_WantUpdates:
			{
				// If we fell through from WantCreate, then we are uploading the full size
				// Otherwise, we are just updating the specified region
				// clange-format off
				const int upload_x = (im_tex->Status == ImTextureStatus_WantCreate) ? 0 : im_tex->UpdateRect.x;
				const int upload_y = (im_tex->Status == ImTextureStatus_WantCreate) ? 0 : im_tex->UpdateRect.y;
				const int upload_w = (im_tex->Status == ImTextureStatus_WantCreate) ? im_tex->Width : im_tex->UpdateRect.w;
				const int upload_h = (im_tex->Status == ImTextureStatus_WantCreate) ? im_tex->Height : im_tex->UpdateRect.h;
				const int upload_pitch = upload_w * im_tex->BytesPerPixel;
				// clange-format on

				const GSVector4i rect{
					upload_x,
					upload_y,
					upload_x + upload_w,
					upload_y + upload_h,
				};

				GSTexture* gs_tex = static_cast<GSTexture*>(im_tex->BackendUserData);
				GSTexture::GSMap map;
				if (gs_tex->Map(map, &rect))
				{
					for (int y = 0; y < upload_h; y++)
						std::memcpy(map.bits + map.pitch * y, im_tex->GetPixelsAt(rect.x, rect.y + y), upload_pitch);

					gs_tex->Unmap();
				}
				else
				{
					for (int y = 0; y < upload_h; y++)
						gs_tex->Update({rect.left, rect.top + y, rect.right, rect.top + y + 1},
							im_tex->GetPixelsAt(rect.x, rect.y + y), upload_pitch);
				}

				im_tex->Status = ImTextureStatus_OK;
				break;
			}
			case ImTextureStatus_WantDestroy:
			{
				GSTexture* gs_tex = static_cast<GSTexture*>(im_tex->BackendUserData);
				if (gs_tex == nullptr)
					break;

				// While it's unlikely we're going to reuse the same size as imgui for rendering,
				// imgui may request a new atlas of the same size if old font sizes are evicted.
				Recycle(gs_tex);

				im_tex->SetTexID(ImTextureID_Invalid);
				im_tex->BackendUserData = nullptr;
				im_tex->Status = ImTextureStatus_Destroyed;
				break;
			}
			default:
				pxAssert(false);
				break;
		}
	}
}

void GSDevice::DestroyImGuiTextures()
{
	if (!ImGui::GetCurrentContext())
		return;

	for (ImTextureData* im_tex : ImGui::GetPlatformIO().Textures)
	{
		if (im_tex->Status != ImTextureStatus_Destroyed)
		{
			GSTexture* gs_tex = static_cast<GSTexture*>(im_tex->BackendUserData);
			if (gs_tex == nullptr)
				continue;

			Recycle(gs_tex);

			pxAssert(im_tex->RefCount == 1);

			im_tex->SetTexID(ImTextureID_Invalid);
			im_tex->BackendUserData = nullptr;
			im_tex->Status = ImTextureStatus_Destroyed;
		}
	}
}

void GSDevice::TextureRecycleDeleter::operator()(GSTexture* const tex)
{
	g_gs_device->Recycle(tex);
}

GSTexture* GSDevice::FetchSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format, bool clear, bool prefer_unused_texture)
{
	const GSVector2i size(std::clamp(width, 1, static_cast<int>(g_gs_device->GetMaxTextureSize())),
		std::clamp(height, 1, static_cast<int>(g_gs_device->GetMaxTextureSize())));
	FastList<GSTexture*>& pool = m_pool[type != GSTexture::Type::Texture];

	GSTexture* t = nullptr;
	auto fallback = pool.end();

	for (auto i = pool.begin(); i != pool.end(); ++i)
	{
		t = *i;

		pxAssert(t);

		if (t->GetType() == type && t->GetFormat() == format && t->GetSize() == size && t->GetMipmapLevels() == levels)
		{
			if (!prefer_unused_texture || t->GetLastFrameUsed() != m_frame)
			{
				m_pool_memory_usage -= t->GetMemUsage();
				pool.erase(i);
				break;
			}
			else if (fallback == pool.end())
			{
				fallback = i;
			}
		}

		t = nullptr;
	}

	if (!t)
	{
		if (pool.size() >= ((type == GSTexture::Type::Texture) ? MAX_POOLED_TEXTURES : MAX_POOLED_TARGETS) &&
			fallback != pool.end())
		{
			t = *fallback;
			m_pool_memory_usage -= t->GetMemUsage();
			pool.erase(fallback);
		}
		else
		{
			t = CreateSurface(type, size.x, size.y, levels, format);
			if (!t)
			{
				ERROR_LOG("GS: Memory allocation failure for {}x{} texture. Purging pool and retrying.", size.x, size.y);
				PurgePool();
				t = CreateSurface(type, size.x, size.y, levels, format);
				if (!t)
				{
					ERROR_LOG("GS: Memory allocation failure for {}x{} texture after purging pool.", size.x, size.y);
					return nullptr;
				}
			}

#ifdef PCSX2_DEVBUILD
			if (GSConfig.UseDebugDevice)
			{
				const TextureLabel label = GetTextureLabel(type, format);
				const u32 id = ++s_texture_counts[static_cast<u32>(label)];
				t->SetDebugName(TinyString::from_format("{} {}", TextureLabelString(label), id));
			}
#endif
		}
	}

	switch (type)
	{
	case GSTexture::Type::RenderTarget:
		{
			if (clear)
				ClearRenderTarget(t, 0);
			else
				InvalidateRenderTarget(t);
		}
		break;
	case GSTexture::Type::DepthStencil:
		{
			if (clear)
				ClearDepth(t, 0.0f);
			else
				InvalidateRenderTarget(t);
		}
		break;
	default:
		break;
	}

	return t;
}

void GSDevice::Recycle(GSTexture* t)
{
	if (!t)
		return;

	t->SetLastFrameUsed(m_frame);

	FastList<GSTexture*>& pool = m_pool[!t->IsTexture()];
	pool.push_front(t);
	m_pool_memory_usage += t->GetMemUsage();

	const u32 max_size = t->IsTexture() ? MAX_POOLED_TEXTURES : MAX_POOLED_TARGETS;
	const u32 max_age = t->IsTexture() ? MAX_TEXTURE_AGE : MAX_TARGET_AGE;
	while (pool.size() > max_size)
	{
		// Don't toss when the texture was last used in this frame.
		// Because we're going to need to keep it alive anyway.
		GSTexture* back = pool.back();
		if ((m_frame - back->GetLastFrameUsed()) < max_age)
			break;

		m_pool_memory_usage -= back->GetMemUsage();
		delete back;

		pool.pop_back();
	}
}

bool GSDevice::UsesLowerLeftOrigin() const
{
	const RenderAPI api = GetRenderAPI();
	return (api == RenderAPI::OpenGL);
}

void GSDevice::AgePool()
{
	m_frame++;

	// Toss out textures when they're not too-recently used.
	for (u32 pool_idx = 0; pool_idx < m_pool.size(); pool_idx++)
	{
		const u32 max_age = (pool_idx == 0) ? MAX_TEXTURE_AGE : MAX_TARGET_AGE;
		FastList<GSTexture*>& pool = m_pool[pool_idx];
		while (!pool.empty())
		{
			GSTexture* back = pool.back();
			if ((m_frame - back->GetLastFrameUsed()) < max_age)
				break;

			m_pool_memory_usage -= back->GetMemUsage();
			delete back;

			pool.pop_back();
		}
	}
}

void GSDevice::PurgePool()
{
	for (FastList<GSTexture*>& pool : m_pool)
	{
		for (GSTexture* t : pool)
			delete t;
		pool.clear();
	}
	m_pool_memory_usage = 0;
}

GSTexture* GSDevice::CreateRenderTarget(int w, int h, GSTexture::Format format, bool clear, bool prefer_reuse)
{
	return FetchSurface(GSTexture::Type::RenderTarget, w, h, 1, format, clear, !prefer_reuse);
}

GSTexture* GSDevice::CreateDepthStencil(int w, int h, GSTexture::Format format, bool clear, bool prefer_reuse)
{
	return FetchSurface(GSTexture::Type::DepthStencil, w, h, 1, format, clear, !prefer_reuse);
}

GSTexture* GSDevice::CreateTexture(int w, int h, int mipmap_levels, GSTexture::Format format, bool prefer_reuse /* = false */)
{
	pxAssert(mipmap_levels != 0 && (mipmap_levels < 0 || mipmap_levels <= GetMipmapLevelsForSize(w, h)));
	const int levels = mipmap_levels < 0 ? GetMipmapLevelsForSize(w, h) : mipmap_levels;
	return FetchSurface(GSTexture::Type::Texture, w, h, levels, format, false, m_features.prefer_new_textures && !prefer_reuse);
}

void GSDevice::DoStretchRectWithAssertions(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	GSHWDrawConfig::ColorMaskSelector cms, ShaderConvert shader, bool linear)
{
	pxAssert((dTex && dTex->IsDepthStencil()) == HasDepthOutput(shader));
	pxAssert(linear ? SupportsBilinear(shader) : SupportsNearest(shader));
	GL_INS("StretchRect(%d) {%d,%d} %dx%d -> {%d,%d) %dx%d", shader, int(sRect.left), int(sRect.top),
		int(sRect.right - sRect.left), int(sRect.bottom - sRect.top), int(dRect.left), int(dRect.top),
		int(dRect.right - dRect.left), int(dRect.bottom - dRect.top));
	DoStretchRect(sTex, sRect, dTex, dRect, cms, shader, linear);
}

void GSDevice::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	bool red, bool green, bool blue, bool alpha, ShaderConvert shader)
{
	GSHWDrawConfig::ColorMaskSelector cms;

	cms.wr = red;
	cms.wg = green;
	cms.wb = blue;
	cms.wa = alpha;

	pxAssert(HasVariableWriteMask(shader));
	GL_INS("ColorCopy Red:%d Green:%d Blue:%d Alpha:%d", cms.wr, cms.wg, cms.wb, cms.wa);

	DoStretchRectWithAssertions(sTex, sRect, dTex, dRect, cms, shader, false);
}

void GSDevice::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderConvert shader, bool linear)
{
	DoStretchRectWithAssertions(sTex, sRect, dTex, dRect, GSHWDrawConfig::ColorMaskSelector(ShaderConvertWriteMask(shader)), shader, linear);
}

void GSDevice::StretchRect(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader, bool linear)
{
	StretchRect(sTex, GSVector4(0, 0, 1, 1), dTex, dRect, shader, linear);
}

void GSDevice::DrawMultiStretchRects(
	const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader)
{
	for (u32 i = 0; i < num_rects; i++)
	{
		const MultiStretchRect& sr = rects[i];
		pxAssert(HasVariableWriteMask(shader) || rects[0].wmask.wrgba == 0xf);
		if (rects[0].wmask.wrgba != 0xf)
		{
			g_gs_device->StretchRect(sr.src, sr.src_rect, dTex, sr.dst_rect, rects[0].wmask.wr,
				rects[0].wmask.wg, rects[0].wmask.wb, rects[0].wmask.wa, shader);
		}
		else
		{
			g_gs_device->StretchRect(sr.src, sr.src_rect, dTex, sr.dst_rect, shader, sr.linear);
		}
	}
}

void GSDevice::SortMultiStretchRects(MultiStretchRect* rects, u32 num_rects)
{
	// Depending on num_rects, insertion sort may be better here.
	std::sort(rects, rects + num_rects, [](const MultiStretchRect& lhs, const MultiStretchRect& rhs) {
		return lhs.src < rhs.src || lhs.linear < rhs.linear;
	});
}

void GSDevice::ClearCurrent()
{
	m_current = nullptr;

	delete m_merge;
	delete m_weavebob;
	delete m_blend;
	delete m_mad;
	delete m_target_tmp;
	delete m_cas;

	m_merge = nullptr;
	m_weavebob = nullptr;
	m_blend = nullptr;
	m_mad = nullptr;
	m_target_tmp = nullptr;
	m_cas = nullptr;
}

void GSDevice::Merge(GSTexture* sTex[3], GSVector4* sRect, GSVector4* dRect, const GSVector2i& fs, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c)
{
	if (ResizeRenderTarget(&m_merge, fs.x, fs.y, false, false))
		DoMerge(sTex, sRect, m_merge, dRect, PMODE, EXTBUF, c, GSConfig.PCRTCOffsets);

	m_current = m_merge;
}

void GSDevice::Interlace(const GSVector2i& ds, int field, int mode, float yoffset)
{
	static int bufIdx = 0;
	float offset = yoffset * static_cast<float>(field);
	offset = GSConfig.DisableInterlaceOffset ? 0.0f : offset;

	auto do_interlace = [this](GSTexture* sTex, GSTexture* dTex, ShaderInterlace shader, bool linear, float yoffset, int bufIdx) {
		const GSVector2i ds_i = dTex->GetSize();
		const GSVector2 ds = GSVector2(static_cast<float>(ds_i.x), static_cast<float>(ds_i.y));

		GSVector4 sRect = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
		GSVector4 dRect = GSVector4(0.0f, yoffset, ds.x, ds.y + yoffset);

		// Select the top or bottom half for MAD buffering.
		if (shader == ShaderInterlace::MAD_BUFFER)
		{
			const float half_size = ds.y * 0.5f;
			if ((bufIdx >> 1) == 1)
				dRect.y += half_size;
			else
				dRect.w -= half_size;
		}

		const InterlaceConstantBuffer cb = {
			GSVector4(static_cast<float>(bufIdx), 1.0f / ds.y, ds.y, MAD_SENSITIVITY)
		};

		GL_PUSH("DoInterlace %dx%d Shader:%d Linear:%d", ds_i.x, ds_i.y, static_cast<int>(shader), linear);
		DoInterlace(sTex, sRect, dTex, dRect, shader, linear, cb);
	};

	switch (mode)
	{
		case 0: // Weave
			ResizeRenderTarget(&m_weavebob, ds.x, ds.y, true, false);
			do_interlace(m_merge, m_weavebob, ShaderInterlace::WEAVE, false, offset, field);
			m_current = m_weavebob;
			break;
		case 1: // Bob
			// Field is reversed here as we are countering the bounce.
			ResizeRenderTarget(&m_weavebob, ds.x, ds.y, true, false);
			do_interlace(m_merge, m_weavebob, ShaderInterlace::BOB, true, yoffset * (1 - field), 0);
			m_current = m_weavebob;
			break;
		case 2: // Blend
			ResizeRenderTarget(&m_weavebob, ds.x, ds.y, true, false);
			do_interlace(m_merge, m_weavebob, ShaderInterlace::WEAVE, false, offset, field);
			ResizeRenderTarget(&m_blend, ds.x, ds.y, true, false);
			do_interlace(m_weavebob, m_blend, ShaderInterlace::BLEND, false, 0, 0);
			m_current = m_blend;
			break;
		case 3: // FastMAD Motion Adaptive Deinterlacing
			bufIdx++;
			bufIdx &= ~1;
			bufIdx |= field;
			bufIdx &= 3;
			ResizeRenderTarget(&m_mad, ds.x, ds.y * 2.0f, true, false);
			do_interlace(m_merge, m_mad, ShaderInterlace::MAD_BUFFER, false, offset, bufIdx);
			ResizeRenderTarget(&m_weavebob, ds.x, ds.y, true, false);
			do_interlace(m_mad, m_weavebob, ShaderInterlace::MAD_RECONSTRUCT, false, 0, bufIdx);
			m_current = m_weavebob;
			break;
		default:
			m_current = m_merge;
			break;
	}
}

void GSDevice::FXAA()
{
	// Combining FXAA+ShadeBoost can't share the same target.
	GSTexture*& dTex = (m_current == m_target_tmp) ? m_merge : m_target_tmp;
	if (ResizeRenderTarget(&dTex, m_current->GetWidth(), m_current->GetHeight(), false, false))
	{
		DoFXAA(m_current, dTex);
		m_current = dTex;
	}
}

void GSDevice::ShadeBoost()
{
	if (ResizeRenderTarget(&m_target_tmp, m_current->GetWidth(), m_current->GetHeight(), false, false))
	{
		// predivide to avoid the divide (multiply) in the shader
		const GSVector4 params(
			static_cast<float>(GSConfig.ShadeBoost_Brightness) * (1.0f / 50.0f),
			static_cast<float>(GSConfig.ShadeBoost_Contrast) * (1.0f / 50.0f),
			static_cast<float>(GSConfig.ShadeBoost_Saturation) * (1.0f / 50.0f),
			static_cast<float>(GSConfig.ShadeBoost_Gamma) * (1.0f / 50.0f));

		DoShadeBoost(m_current, m_target_tmp, params.v);

		m_current = m_target_tmp;
	}
}

void GSDevice::Resize(int width, int height)
{
	GSTexture*& dTex = (m_current == m_target_tmp) ? m_merge : m_target_tmp;
	GSVector2i s = m_current->GetSize();
	int multiplier = 1;

	if ((width > s.x || height > s.y))
	{
		while (width > s.x || height > s.y)
		{
			s = m_current->GetSize() * GSVector2i(++multiplier);
		}
	}
	else
		s = GSVector2i(width, height);

	if (ResizeRenderTarget(&dTex, s.x, s.y, false, false))
	{
		const GSVector4 sRect(0, 0, 1, 1);
		const GSVector4 dRect(0, 0, s.x, s.y);
		StretchRect(m_current, sRect, dTex, dRect, ShaderConvert::COPY, false);
		m_current = dTex;
	}
}

bool GSDevice::ResizeRenderTarget(GSTexture** t, int w, int h, bool preserve_contents, bool recycle)
{
	pxAssert(t);

	GSTexture* orig_tex = *t;
	if (orig_tex && orig_tex->GetWidth() == w && orig_tex->GetHeight() == h)
	{
		if (!preserve_contents)
			InvalidateRenderTarget(orig_tex);

		return true;
	}

	const GSTexture::Format fmt = orig_tex ? orig_tex->GetFormat() : GSTexture::Format::Color;
	const bool really_preserve_contents = (preserve_contents && orig_tex);
	GSTexture* new_tex = FetchSurface(GSTexture::Type::RenderTarget, w, h, 1, fmt, !really_preserve_contents, true);
	if (!new_tex)
	{
		Console.WriteLn("%dx%d texture allocation failed in ResizeTexture()", w, h);
		return false;
	}

	if (really_preserve_contents)
	{
		constexpr GSVector4 sRect = GSVector4::cxpr(0, 0, 1, 1);
		const GSVector4 dRect = GSVector4(orig_tex->GetRect());
		StretchRect(orig_tex, sRect, new_tex, dRect, ShaderConvert::COPY, true);
	}

	if (orig_tex)
	{
		if (recycle)
			Recycle(orig_tex);
		else
			delete orig_tex;
	}

	*t = new_tex;
	return true;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wignored-qualifiers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif

// Kinda grotty, but better than copy/pasting the relevant bits in..
#define A_CPU 1
#include "bin/resources/shaders/common/ffx_a.h"
#include "bin/resources/shaders/common/ffx_cas.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

bool GSDevice::GetCASShaderSource(std::string* source)
{
	std::optional<std::string> ffx_a_source = ReadShaderSource("shaders/common/ffx_a.h");
	std::optional<std::string> ffx_cas_source = ReadShaderSource("shaders/common/ffx_cas.h");
	if (!ffx_a_source.has_value() || !ffx_cas_source.has_value())
		return false;

	// Since our shader compilers don't support includes, and OpenGL doesn't at all... we'll do a really cheeky string replace.
	StringUtil::ReplaceAll(source, "#include \"ffx_a.h\"", ffx_a_source.value());
	StringUtil::ReplaceAll(source, "#include \"ffx_cas.h\"", ffx_cas_source.value());
	return true;
}

void GSDevice::CAS(GSTexture*& tex, GSVector4i& src_rect, GSVector4& src_uv, const GSVector4& draw_rect, bool sharpen_only)
{
	const int dst_width = sharpen_only ? src_rect.width() : static_cast<int>(std::ceil(draw_rect.z - draw_rect.x));
	const int dst_height = sharpen_only ? src_rect.height() : static_cast<int>(std::ceil(draw_rect.w - draw_rect.y));
	const int src_offset_x = static_cast<int>(src_rect.x);
	const int src_offset_y = static_cast<int>(src_rect.y);

	GSTexture* src_tex = tex;
	if (!m_cas || m_cas->GetWidth() != dst_width || m_cas->GetHeight() != dst_height)
	{
		delete m_cas;
		m_cas = CreateSurface(GSTexture::Type::RWTexture, dst_width, dst_height, 1, GSTexture::Format::Color);
		if (!m_cas)
		{
			Console.Error("Failed to allocate CAS RW texture.");
			return;
		}
	}

	std::array<u32, NUM_CAS_CONSTANTS> consts;
	CasSetup(&consts[0], &consts[4], static_cast<float>(GSConfig.CAS_Sharpness) * 0.01f,
		static_cast<AF1>(src_rect.width()), static_cast<AF1>(src_rect.height()),
		static_cast<AF1>(dst_width), static_cast<AF1>(dst_height));
	consts[8] = static_cast<u32>(src_offset_x);
	consts[9] = static_cast<u32>(src_offset_y);

	if (!DoCAS(src_tex, m_cas, sharpen_only, consts))
	{
		// leave textures intact if we failed
		Console.Warning("Applying CAS failed.");
		return;
	}

	tex = m_cas;
	src_rect = GSVector4i(0, 0, dst_width, dst_height);
	src_uv = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
}

bool GSHWDrawConfig::BlendState::IsEffective(ColorMaskSelector colormask) const
{
	return enable && (((colormask.key & 7u) && (src_factor != GSDevice::CONST_ONE || dst_factor != GSDevice::CONST_ZERO)) ||
						 ((colormask.key & 8u) && (src_factor_alpha != GSDevice::CONST_ONE || dst_factor_alpha != GSDevice::CONST_ZERO)));
}

std::string GSHWDrawConfig::GetTopologyName(u32 topology)
{
	switch (static_cast<GSHWDrawConfig::Topology>(topology))
	{
		case GSHWDrawConfig::Topology::Point: return "Point";
		case GSHWDrawConfig::Topology::Line: return "Line";
		case GSHWDrawConfig::Topology::Triangle: return "Triangle";
		default: return std::to_string(topology);
	}
}

std::string GSHWDrawConfig::GetVSExpandName(u32 vsexpand)
{
	switch (static_cast<GSHWDrawConfig::VSExpand>(vsexpand))
	{
		case GSHWDrawConfig::VSExpand::None: return "None";
		case GSHWDrawConfig::VSExpand::Point: return "Point";
		case GSHWDrawConfig::VSExpand::Line: return "Line";
		case GSHWDrawConfig::VSExpand::Sprite: return "Sprite";
		default: return std::to_string(vsexpand);
	}
}

std::string GSHWDrawConfig::GetPSDateName(u32 date)
{
	switch (date)
	{
		case 0: return "None";
		case 1: return "0_pass_PrimID_init";
		case 2: return "1_pass_PrimID_init";
		case 3: return "PrimID_main";
		case 5: return "0_pass_main";
		case 6: return "1_pass_main";
		default: return std::to_string(date);
	}
}

std::string GSHWDrawConfig::GetPSAlphaTestName(u32 atst)
{
	switch (static_cast<GSHWDrawConfig::PSAlphaTest>(atst))
	{
		case GSHWDrawConfig::PSAlphaTest::PS_ATST_NONE: return "NONE";
		case GSHWDrawConfig::PSAlphaTest::PS_ATST_LEQUAL: return "LEQUAL";
		case GSHWDrawConfig::PSAlphaTest::PS_ATST_GEQUAL: return "GEQUAL";
		case GSHWDrawConfig::PSAlphaTest::PS_ATST_EQUAL: return "EQUAL";
		case GSHWDrawConfig::PSAlphaTest::PS_ATST_NOTEQUAL: return "NOTEQUAL";
		default: return std::to_string(atst);
	};
}

std::string GSHWDrawConfig::GetPSDstFmtName(u32 dstfmt)
{
	switch (dstfmt)
	{
		case 0: return "32bit";
		case 1: return "24bit";
		case 2: return "16bit";
		default: return std::to_string(dstfmt);
	}
}

std::string GSHWDrawConfig::GetPSDepthFmtName(u32 depthfmt)
{
	switch (depthfmt)
	{
		case 0: return "None";
		case 1: return "32bit";
		case 2: return "16bit";
		case 3: return "RGBA";
		default: return std::to_string(depthfmt);
	}
}

std::string GSHWDrawConfig::GetPSBlendABDName(u32 abd)
{
	switch (abd)
	{
		case 0: return "Cs";
		case 1: return "Cd";
		case 2: return "Zero";
		default: return std::to_string(abd);
	}
}

std::string GSHWDrawConfig::GetPSBlendCName(u32 c)
{
	switch (c)
	{
		case 0: return "As";
		case 1: return "Ad";
		case 2: return "Af";
		default: return std::to_string(c);
	}
}

std::string GSHWDrawConfig::GetPSBlendHWName(u32 blendhw)
{
	switch (static_cast<HWBlendType>(blendhw))
	{
		case HWBlendType::SRC_ONE_DST_FACTOR: return "SRC_ONE_DST_FACTOR";
		case HWBlendType::SRC_ALPHA_DST_FACTOR: return "SRC_ALPHA_DST_FACTOR";
		case HWBlendType::SRC_DOUBLE: return "SRC_DOUBLE";
		case HWBlendType::SRC_HALF_ONE_DST_FACTOR: return "SRC_HALF_ONE_DST_FACTOR";
		case HWBlendType::SRC_INV_DST_BLEND_HALF: return "SRC_INV_DST_BLEND_HALF";
		case HWBlendType::INV_SRC_DST_BLEND_HALF: return "INV_SRC_DST_BLEND_HALF";
		default: return std::to_string(blendhw);
	}
}

std::string GSHWDrawConfig::GetPSBlendMixName(u32 blendmix)
{
	switch (static_cast<HWBlendType>(blendmix))
	{
		case HWBlendType::BMIX1_ALPHA_HIGH_ONE: return "BMIX1_ALPHA_HIGH_ONE";
		case HWBlendType::BMIX1_SRC_HALF: return "BMIX1_SRC_HALF";
		case HWBlendType::BMIX2_OVERFLOW: return "BMIX2_OVERFLOW";
		default: return std::to_string(blendmix);
	}
}

std::string GSHWDrawConfig::GetPSChannelName(u32 channel)
{
	switch (channel)
	{
		case 0: return "None";
		case 1: return "FetchRed";
		case 2: return "FetchGreen";
		case 3: return "FetchBlue";
		case 4: return "FetchAlpha";
		case 5: return "FetchRGB";
		case 6: return "FetchGXBY";
		default: return std::to_string(channel);
	}
}

std::string GSHWDrawConfig::GetPSDitherName(u32 dither)
{
	switch (dither)
	{
		case 0: return "None";
		case 1: return "Standard";
		case 2: return "ReciprocalScaled";
		default: return std::to_string(dither);
	}
}

std::string GSHWDrawConfig::GetSSTrilnName(u32 triln)
{
	switch (static_cast<GS_MIN_FILTER>(triln))
	{
		case GS_MIN_FILTER::Nearest: return "Nearest";
		case GS_MIN_FILTER::Linear: return "Linear";
		case GS_MIN_FILTER::Nearest_Mipmap_Nearest: return "Nearest_Mipmap_Nearest";
		case GS_MIN_FILTER::Nearest_Mipmap_Linear: return "Nearest_Mipmap_Linear";
		case GS_MIN_FILTER::Linear_Mipmap_Nearest: return "Linear_Mipmap_Nearest";
		case GS_MIN_FILTER::Linear_Mipmap_Linear: return "Linear_Mipmap_Linear";
		default: return std::to_string(triln);
	}
}

std::string GSHWDrawConfig::GetBlendOpName(u32 blendop)
{
	switch (blendop)
	{
		case 0: return "ADD";
		case 1: return "SUBTRACT";
		case 2: return "REVERSE_SUBTRACT";
		default: return std::to_string(blendop);
	}
}

std::string GSHWDrawConfig::GetBlendFactorName(u32 blendfactor)
{
	switch (blendfactor)
	{
		case 0: return "SRC_COLOR";
		case 1: return "ONE_MINUS_SRC_COLOR";
		case 2: return "DST_COLOR";
		case 3: return "ONE_MINUS_DST_COLOR";
		case 4: return "SRC1_COLOR";
		case 5: return "ONE_MINUS_SRC1_COLOR";
		case 6: return "SRC_ALPHA";
		case 7: return "ONE_MINUS_SRC_ALPHA";
		case 8: return "DST_ALPHA";
		case 9: return "ONE_MINUS_DST_ALPHA";
		case 10: return "SRC1_ALPHA";
		case 11: return "ONE_MINUS_SRC1_ALPHA";
		case 12: return "CONSTANT_COLOR";
		case 13: return "ONE_MINUS_CONSTANT_COLOR";
		case 14: return "ONE";
		case 15: return "ZERO";
		default: return std::to_string(blendfactor);
	}
}

std::string GSHWDrawConfig::GetDestinationAlphaModeName(u32 datm)
{
	switch (static_cast<GSHWDrawConfig::DestinationAlphaMode>(datm))
	{
		case GSHWDrawConfig::DestinationAlphaMode::Off: return "Off";
		case GSHWDrawConfig::DestinationAlphaMode::Stencil: return "Stencil";
		case GSHWDrawConfig::DestinationAlphaMode::StencilOne: return "StencilOne";
		case GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking: return "PrimIDTracking";
		case GSHWDrawConfig::DestinationAlphaMode::Full: return "Full";
		default: return std::to_string(datm);
	}
}

std::string GSHWDrawConfig::GetColClipModeName(u32 ccmode)
{
	switch (static_cast<GSHWDrawConfig::ColClipMode>(ccmode))
	{
		case GSHWDrawConfig::ColClipMode::NoModify: return "NoModify";
		case GSHWDrawConfig::ColClipMode::ConvertOnly: return "ConvertOnly";
		case GSHWDrawConfig::ColClipMode::ResolveOnly: return "ResolveOnly";
		case GSHWDrawConfig::ColClipMode::ConvertAndResolve: return "ConvertAndResolve";
		case GSHWDrawConfig::ColClipMode::EarlyResolve: return "EarlyResolve";
		default: return std::to_string(ccmode);
	}
}

std::string GSHWDrawConfig::GetSetDATMName(u32 datm)
{
	switch (static_cast<SetDATM>(datm))
	{
		case SetDATM::DATM0: return "DATM0";
		case SetDATM::DATM1: return "DATM1";
		case SetDATM::DATM0_RTA_CORRECTION: return "DATM0_RTA_CORRECTION";
		case SetDATM::DATM1_RTA_CORRECTION: return "DATM1_RTA_CORRECTION";
		default: return std::to_string(datm);
	};
}

static constexpr const char* INDENT = "  ";

void GSHWDrawConfig::DumpPSSelector(std::ostream& out, const PSSelector& ps, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "aem_fmt: " << static_cast<u32>(ps.aem_fmt) << std::endl;
	out << indent << "pal_fmt: " << static_cast<u32>(ps.pal_fmt) << std::endl;
	out << indent << "dst_fmt: " << static_cast<u32>(ps.dst_fmt) << " # " << GetPSDstFmtName(static_cast<u32>(ps.dst_fmt)) << std::endl;
	out << indent << "depth_fmt: " << static_cast<u32>(ps.depth_fmt) << " # " << GetPSDepthFmtName(static_cast<u32>(ps.depth_fmt)) << std::endl;
	out << indent << "aem: " << static_cast<u32>(ps.aem) << std::endl;
	out << indent << "fba: " << static_cast<u32>(ps.fba) << std::endl;
	out << indent << "fog: " << static_cast<u32>(ps.fog) << std::endl;
	out << indent << "iip: " << static_cast<u32>(ps.iip) << std::endl;
	out << indent << "date: " << static_cast<u32>(ps.date) << " # " << GetPSDateName(static_cast<u32>(ps.date)) << std::endl;
	out << indent << "atst: " << static_cast<u32>(ps.atst) << " # " << GetPSAlphaTestName(static_cast<u32>(ps.atst)) << std::endl;
	out << indent << "afail: " << static_cast<u32>(ps.afail) << " # " << GSUtil::GetAFAILName(static_cast<u32>(ps.afail)) << std::endl;
	out << indent << "fst: " << static_cast<u32>(ps.fst) << std::endl;
	out << indent << "tfx: " << static_cast<u32>(ps.tfx) << std::endl;
	out << indent << "tcc: " << GSUtil::GetTCCName(static_cast<u32>(ps.tcc)) << std::endl;
	out << indent << "wms: " << static_cast<u32>(ps.tcc) << " # " << static_cast<u32>(ps.wms) << std::endl;
	out << indent << "wmt: " << static_cast<u32>(ps.wmt) << std::endl;
	out << indent << "adjs: " << static_cast<u32>(ps.adjs) << std::endl;
	out << indent << "adjt: " << static_cast<u32>(ps.adjt) << std::endl;
	out << indent << "ltf: " << static_cast<u32>(ps.ltf) << std::endl;
	out << indent << "shuffle: " << static_cast<u32>(ps.shuffle) << std::endl;
	out << indent << "shuffle_same: " << static_cast<u32>(ps.shuffle_same) << std::endl;
	out << indent << "real16src: " << static_cast<u32>(ps.real16src) << std::endl;
	out << indent << "process_ba: " << static_cast<u32>(ps.process_ba) << std::endl;
	out << indent << "process_rg: " << static_cast<u32>(ps.process_rg) << std::endl;
	out << indent << "shuffle_across: " << static_cast<u32>(ps.shuffle_across) << std::endl;
	out << indent << "write_rg: " << static_cast<u32>(ps.write_rg) << std::endl;
	out << indent << "fbmask: " << static_cast<u32>(ps.fbmask) << std::endl;
	out << indent << "blend_a: " << static_cast<u32>(ps.blend_a) << " # " << GetPSBlendABDName(static_cast<u32>(ps.blend_a)) << std::endl;
	out << indent << "blend_b: " << static_cast<u32>(ps.blend_b) << " # " << GetPSBlendABDName(static_cast<u32>(ps.blend_b)) << std::endl;
	out << indent << "blend_c: " << static_cast<u32>(ps.blend_c) << " # " << GetPSBlendCName(static_cast<u32>(ps.blend_c)) << std::endl;
	out << indent << "blend_d: " << static_cast<u32>(ps.blend_d) << " # " << GetPSBlendABDName(static_cast<u32>(ps.blend_d)) << std::endl;
	out << indent << "fixed_one_a: " << static_cast<u32>(ps.fixed_one_a) << std::endl;
	out << indent << "blend_hw: " << static_cast<u32>(ps.blend_hw) << " # " << GetPSBlendHWName(static_cast<u32>(ps.blend_hw)) << std::endl;
	out << indent << "a_masked: " << static_cast<u32>(ps.a_masked) << std::endl;
	out << indent << "colclip_hw: " << static_cast<u32>(ps.colclip_hw) << std::endl;
	out << indent << "rta_correction: " << static_cast<u32>(ps.rta_correction) << std::endl;
	out << indent << "rta_source_correction: " << static_cast<u32>(ps.rta_source_correction) << std::endl;
	out << indent << "colclip: " << static_cast<u32>(ps.colclip) << std::endl;
	out << indent << "blend_mix: " << static_cast<u32>(ps.blend_mix) << " # " << GetPSBlendMixName(static_cast<u32>(ps.blend_mix)) << std::endl;
	out << indent << "round_inv: " << static_cast<u32>(ps.round_inv) << std::endl;
	out << indent << "pabe: " << static_cast<u32>(ps.pabe) << std::endl;
	out << indent << "no_color: " << static_cast<u32>(ps.no_color) << std::endl;
	out << indent << "no_color1: " << static_cast<u32>(ps.no_color1) << std::endl;
	out << indent << "channel: " << static_cast<u32>(ps.channel) << " # " << GetPSChannelName(static_cast<u32>(ps.channel)) << std::endl;
	out << indent << "channel_fb: " << static_cast<u32>(ps.channel_fb) << std::endl;
	out << indent << "dither: " << static_cast<u32>(ps.dither) << " # " << GetPSDitherName(static_cast<u32>(ps.dither)) << std::endl;
	out << indent << "dither_adjust: " << static_cast<u32>(ps.dither_adjust) << std::endl;
	out << indent << "zclamp: " << static_cast<u32>(ps.zclamp) << std::endl;
	out << indent << "tcoffsethack: " << static_cast<u32>(ps.tcoffsethack) << std::endl;
	out << indent << "urban_chaos_hle: " << static_cast<u32>(ps.urban_chaos_hle) << std::endl;
	out << indent << "tales_of_abyss_hle: " << static_cast<u32>(ps.tales_of_abyss_hle) << std::endl;
	out << indent << "tex_is_fb: " << static_cast<u32>(ps.tex_is_fb) << std::endl;
	out << indent << "automatic_lod: " << static_cast<u32>(ps.automatic_lod) << std::endl;
	out << indent << "manual_lod: " << static_cast<u32>(ps.manual_lod) << std::endl;
	out << indent << "point_sampler: " << static_cast<u32>(ps.point_sampler) << std::endl;
	out << indent << "region_rect: " << static_cast<u32>(ps.region_rect) << std::endl;
	out << indent << "scanmsk: " << static_cast<u32>(ps.scanmsk) << " # " << GSUtil::GetSCANMSKName(static_cast<u32>(ps.scanmsk)) << std::endl;
}

void GSHWDrawConfig::DumpVSSelector(std::ostream& out, const VSSelector& vs, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "fst: " << static_cast<u32>(vs.fst) << std::endl;
	out << indent << "tme: " << static_cast<u32>(vs.tme) << std::endl;
	out << indent << "iip: " << static_cast<u32>(vs.iip) << std::endl;
	out << indent << "point_size: " << static_cast<u32>(vs.point_size) << std::endl;
	out << indent << "expand: " << static_cast<u32>(vs.expand) << " # " << GetVSExpandName(static_cast<u32>(vs.expand)) << std::endl;
}

void GSHWDrawConfig::DumpBlendState(std::ostream& out, const BlendState& bs, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "enable: " << static_cast<u32>(bs.enable) << std::endl;
	out << indent << "constant_enable: " << static_cast<u32>(bs.constant_enable) << std::endl;
	out << indent << "op: " << static_cast<u32>(bs.op) << " # " << GetBlendOpName(static_cast<u32>(bs.op)) << std::endl;
	out << indent << "src_factor: " << static_cast<u32>(bs.src_factor) << " # " << GetBlendFactorName(static_cast<u32>(bs.src_factor)) << std::endl;
	out << indent << "dst_factor: " << static_cast<u32>(bs.dst_factor) << " # " << GetBlendFactorName(static_cast<u32>(bs.dst_factor)) << std::endl;
	out << indent << "src_factor_alpha: " << static_cast<u32>(bs.src_factor_alpha) << " # " << GetBlendFactorName(static_cast<u32>(bs.src_factor_alpha)) << std::endl;
	out << indent << "dst_factor_alpha: " << static_cast<u32>(bs.dst_factor_alpha) << " # " << GetBlendFactorName(static_cast<u32>(bs.dst_factor_alpha)) << std::endl;
	out << indent << "constant: " << static_cast<u32>(bs.constant) << std::endl;
}

void GSHWDrawConfig::DumpDepthStencilSelctor(std::ostream& out, const DepthStencilSelector& dss, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "ztst: " << static_cast<u32>(dss.ztst) << " # " << GSUtil::GetZTSTName(static_cast<u32>(dss.ztst)) << std::endl;
	out << indent << "zwe: " << static_cast<u32>(dss.zwe) << std::endl;
	out << indent << "date: " << static_cast<u32>(dss.date) << std::endl;
	out << indent << "date_one: " << static_cast<u32>(dss.date_one) << std::endl;
}

void GSHWDrawConfig::DumpSamplerSelector(std::ostream& out, const SamplerSelector& ss, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "tau: " << static_cast<u32>(ss.tau) << std::endl;
	out << indent << "tav: " << static_cast<u32>(ss.tav) << std::endl;
	out << indent << "biln: " << static_cast<u32>(ss.biln) << std::endl;
	out << indent << "triln: " << static_cast<u32>(ss.triln) << " # " << GetSSTrilnName(static_cast<u32>(ss.triln)) << std::endl;
	out << indent << "aniso: " << static_cast<u32>(ss.aniso) << std::endl;
	out << indent << "lodclamp: " << static_cast<u32>(ss.lodclamp) << std::endl;
}

void GSHWDrawConfig::DumpAlphaPass(std::ostream& out, const AlphaPass& ap, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "enable: " << static_cast<u32>(ap.enable) << std::endl;
	out << indent << "require_one_barrier: " << static_cast<u32>(ap.require_one_barrier) << std::endl;
	out << indent << "require_full_barrier: " << static_cast<u32>(ap.require_full_barrier) << std::endl;
	out << indent << "colormask: " << std::showbase << std::hex << static_cast<u32>(ap.colormask.wrgba) << std::dec << std::endl;
	out << indent << "ps_aref: " << static_cast<u32>(ap.ps_aref) << std::endl;

	out << indent << "ps:" << std::endl;
	DumpPSSelector(out, ap.ps, indent + INDENT);

	out << indent << "dss:" << std::endl;
	DumpDepthStencilSelctor(out, ap.depth, indent + INDENT);
}

void GSHWDrawConfig::DumpBlendMultipass(std::ostream& out, const BlendMultiPass& bmp, const std::string& indent)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << indent << "enable: " << static_cast<u32>(bmp.enable) << std::endl;
	out << indent << "no_color1: " << static_cast<u32>(bmp.no_color1) << std::endl;
	out << indent << "blend_hw: " << static_cast<u32>(bmp.blend_hw) << " # " << GetPSBlendHWName(static_cast<u32>(bmp.blend_hw)) << std::endl;
	out << indent << "dither: " << static_cast<u32>(bmp.dither) << std::endl;

	out << indent << "blend:" << std::endl;
	DumpBlendState(out, bmp.blend, indent + INDENT);
}

void GSHWDrawConfig::DumpConfig(std::ostream& out, const GSHWDrawConfig& conf,
	bool ps, bool vs, bool bs, bool dss, bool ss, bool asp, bool bmp)
{
	out.imbue(std::locale::classic()); // Disable integer separators
	out << std::dec;

	out << "topology: " << static_cast<u32>(conf.topology) << " # " << GetTopologyName(static_cast<u32>(conf.topology)) << std::endl;
	out << "require_one_barrier: " << static_cast<u32>(conf.require_one_barrier) << std::endl;
	out << "require_full_barrier: " << static_cast<u32>(conf.require_full_barrier) << std::endl;

	out << "destination_alpha: " << static_cast<u32>(conf.destination_alpha) << " # " << GetDestinationAlphaModeName(static_cast<u32>(conf.destination_alpha)) << std::endl;
	out << "datm: " << static_cast<u32>(conf.datm) << " # " << GetSetDATMName(static_cast<u32>(conf.datm)) << std::endl;
	out << "line_expand: " << conf.line_expand << std::endl;
	out << "colormask: " << std::hex << std::showbase << static_cast<u32>(conf.colormask.wrgba) << std::dec << std::endl;

	if (ps)
	{
		out << "ps:" << std::endl;
		DumpPSSelector(out, conf.ps, INDENT);
	}

	if (vs)
	{
		out << "vs:" << std::endl;
		DumpVSSelector(out, conf.vs, INDENT);
	}

	if (bs)
	{
		out << "blend:" << std::endl;
		DumpBlendState(out, conf.blend, INDENT);
	}

	if (ss)
	{
		out << "sampler:" << std::endl;
		DumpSamplerSelector(out, conf.sampler, INDENT);
	}

	if (dss)
	{
		out << "depth:" << std::endl;
		DumpDepthStencilSelctor(out, conf.depth, INDENT);
	}
	
	if (asp)
	{
		out << "alpha_second_pass:" << std::endl;
		DumpAlphaPass(out, conf.alpha_second_pass, INDENT);
	}

	if (bmp)
	{
		out << "blend_multi_pass:" << std::endl;
		DumpBlendMultipass(out, conf.blend_multi_pass, INDENT);
	}
}

void GSHWDrawConfig::DumpConfig(const std::string& fn, const GSHWDrawConfig& conf,
	bool ps, bool vs, bool bs, bool dss, bool ss, bool asp, bool bmp)
{
	std::ofstream file(fn);

	if (!file.is_open())
		return;
	
	DumpConfig(file, conf, ps, vs, bs, dss, ss, asp, bmp);
}

// clang-format off

// Maps PS2 blend modes to our best approximation of them with PC hardware
const std::array<HWBlend, 3*3*3*3> GSDevice::m_blendMap =
{{
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0000: (Cs - Cs)*As + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0001: (Cs - Cs)*As + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0002: (Cs - Cs)*As +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0012: (Cs - Cs)*Ad +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0020: (Cs - Cs)*F  + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 0021: (Cs - Cs)*F  + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 0022: (Cs - Cs)*F  +  0 ==> 0
	{ BLEND_A_MAX | BLEND_MIX2 , OP_SUBTRACT     , CONST_ONE       , SRC1_COLOR}      , // 0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{ BLEND_MIX1               , OP_ADD          , SRC1_COLOR      , INV_SRC1_COLOR}  , // 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{ BLEND_MIX1               , OP_SUBTRACT     , SRC1_COLOR      , SRC1_COLOR}      , // 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{ BLEND_A_MAX              , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , // 0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{ 0                        , OP_ADD          , DST_ALPHA       , INV_DST_ALPHA}   , // 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{ BLEND_HW5                , OP_SUBTRACT     , DST_ALPHA       , DST_ALPHA}       , // 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{ BLEND_A_MAX | BLEND_MIX2 , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , // 0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{ BLEND_MIX1               , OP_ADD          , CONST_COLOR     , INV_CONST_COLOR} , // 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{ BLEND_MIX1               , OP_SUBTRACT     , CONST_COLOR     , CONST_COLOR}     , // 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{ BLEND_ACCU               , OP_ADD          , SRC1_COLOR      , CONST_ONE}       , // 0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{ BLEND_NO_REC             , OP_ADD          , SRC1_COLOR      , CONST_ZERO}      , // 0202: (Cs -  0)*As +  0 ==> Cs*As
	{ BLEND_A_MAX | BLEND_HW8  , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{ BLEND_HW3                , OP_ADD          , DST_ALPHA       , CONST_ONE}       , // 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{ BLEND_HW3                , OP_ADD          , DST_ALPHA       , CONST_ZERO}      , // 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{ BLEND_ACCU               , OP_ADD          , CONST_COLOR     , CONST_ONE}       , // 0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_COLOR     , CONST_ZERO}      , // 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{ BLEND_MIX3               , OP_ADD          , INV_SRC1_COLOR  , SRC1_COLOR}      , // 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{ BLEND_A_MAX | BLEND_MIX1 , OP_REV_SUBTRACT , SRC1_COLOR      , CONST_ONE}       , // 1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{ BLEND_MIX1               , OP_REV_SUBTRACT , SRC1_COLOR      , SRC1_COLOR}      , // 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{ 0                        , OP_ADD          , INV_DST_ALPHA   , DST_ALPHA}       , // 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{ BLEND_A_MAX              , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , // 1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{ BLEND_HW5                , OP_REV_SUBTRACT , DST_ALPHA       , DST_ALPHA}       , // 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{ BLEND_MIX3               , OP_ADD          , INV_CONST_COLOR , CONST_COLOR}     , // 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{ BLEND_A_MAX | BLEND_MIX1 , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ONE}       , // 1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{ BLEND_MIX1               , OP_REV_SUBTRACT , CONST_COLOR     , CONST_COLOR}     , // 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1100: (Cd - Cd)*As + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1101: (Cd - Cd)*As + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1102: (Cd - Cd)*As +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1112: (Cd - Cd)*Ad +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 1120: (Cd - Cd)*F  + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 1121: (Cd - Cd)*F  + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 1122: (Cd - Cd)*F  +  0 ==> 0
	{ BLEND_HW4                , OP_ADD          , CONST_ONE       , SRC1_COLOR}      , // 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{ BLEND_HW1                , OP_ADD          , DST_COLOR       , SRC1_COLOR}      , // 1201: (Cd -  0)*As + Cd ==> Cd*(1 + As)
	{ BLEND_HW2                , OP_ADD          , DST_COLOR       , SRC1_COLOR}      , // 1202: (Cd -  0)*As +  0 ==> Cd*As
	{ BLEND_HW6                , OP_ADD          , CONST_ONE       , DST_ALPHA}       , // 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{ BLEND_HW1                , OP_ADD          , DST_COLOR       , DST_ALPHA}       , // 1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{ BLEND_HW5                , OP_ADD          , CONST_ZERO      , DST_ALPHA}       , // 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{ BLEND_HW4                , OP_ADD          , CONST_ONE       , CONST_COLOR}     , // 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{ BLEND_HW1                , OP_ADD          , DST_COLOR       , CONST_COLOR}     , // 1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{ BLEND_HW2                , OP_ADD          , DST_COLOR       , CONST_COLOR}     , // 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{ BLEND_NO_REC             , OP_ADD          , INV_SRC1_COLOR  , CONST_ZERO}      , // 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{ BLEND_ACCU               , OP_REV_SUBTRACT , SRC1_COLOR      , CONST_ONE}       , // 2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{ BLEND_NO_REC             , OP_REV_SUBTRACT , SRC1_COLOR      , CONST_ZERO}      , // 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{ BLEND_HW9                , OP_ADD          , INV_DST_ALPHA   , CONST_ZERO}      , // 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{ BLEND_HW3                , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ONE}       , // 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	{ 0                        , OP_REV_SUBTRACT , DST_ALPHA       , CONST_ZERO}      , // 2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	{ BLEND_NO_REC             , OP_ADD          , INV_CONST_COLOR , CONST_ZERO}      , // 2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	{ BLEND_ACCU               , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ONE}       , // 2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	{ BLEND_NO_REC             , OP_REV_SUBTRACT , CONST_COLOR     , CONST_ZERO}      , // 2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	{ BLEND_HW4                , OP_SUBTRACT     , CONST_ONE       , SRC1_COLOR}      , // 2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	{ 0                        , OP_ADD          , CONST_ZERO      , INV_SRC1_COLOR}  , // 2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	{ 0                        , OP_SUBTRACT     , CONST_ZERO      , SRC1_COLOR}      , // 2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	{ BLEND_HW6                , OP_SUBTRACT     , CONST_ONE       , DST_ALPHA}       , // 2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	{ BLEND_HW7                , OP_ADD          , CONST_ZERO      , INV_DST_ALPHA}   , // 2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	{ 0                        , OP_SUBTRACT     , CONST_ZERO      , DST_ALPHA}       , // 2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	{ BLEND_HW4                , OP_SUBTRACT     , CONST_ONE       , CONST_COLOR}     , // 2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	{ 0                        , OP_ADD          , CONST_ZERO      , INV_CONST_COLOR} , // 2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	{ 0                        , OP_SUBTRACT     , CONST_ZERO      , CONST_COLOR}     , // 2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2200: (0  -  0)*As + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2201: (0  -  0)*As + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2202: (0  -  0)*As +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2210: (0  -  0)*Ad + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2211: (0  -  0)*Ad + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2212: (0  -  0)*Ad +  0 ==> 0
	{ BLEND_NO_REC             , OP_ADD          , CONST_ONE       , CONST_ZERO}      , // 2220: (0  -  0)*F  + Cs ==> Cs
	{ BLEND_CD                 , OP_ADD          , CONST_ZERO      , CONST_ONE}       , // 2221: (0  -  0)*F  + Cd ==> Cd
	{ BLEND_NO_REC             , OP_ADD          , CONST_ZERO      , CONST_ZERO}      , // 2222: (0  -  0)*F  +  0 ==> 0
}};
