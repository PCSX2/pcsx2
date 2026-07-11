// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/HashCombine.h"
#include "common/WindowInfo.h"
#include "GS/GS.h"
#include "GS/Renderers/Common/GSFastList.h"
#include "GS/Renderers/Common/GSShaderEnums.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "GS/Renderers/Common/GSVertex.h"
#include "GS/GSAlignedClass.h"
#include "GS/GSExtra.h"
#include <array>
#include <span>

enum class Filter
{
	Nearest = 0,
	Biln    = 1,
};

static inline constexpr Filter Nearest = Filter::Nearest;
static inline constexpr Filter Biln    = Filter::Biln;

static inline constexpr Filter BilnIf(bool biln)
{
	return biln ? Biln : Nearest;
}

struct GPUPipelineStatistics
{
	u64 vs_invocations;
	u64 ps_invocations;
};

enum class ShaderConvert
{
	COPY = 0,
	DEPTH_COPY,
	RGB5A1_TO_16_BITS,
	DATM_1,
	DATM_0,
	DATM_1_RTA_CORRECTION,
	DATM_0_RTA_CORRECTION,
	COLCLIP_INIT,
	COLCLIP_RESOLVE,
	RTA_CORRECTION,
	RTA_DECORRECTION,
	TRANSPARENCY_FILTER,
	DEPTH32_TO_16_BITS,
	DEPTH32_TO_32_BITS,
	DEPTH32_TO_RGBA8,
	DEPTH32_TO_RGB8,
	DEPTH16_TO_RGB5A1,
	RGBA8_TO_DEPTH32,
	RGBA8_TO_DEPTH24,
	RGBA8_TO_DEPTH16,
	RGB5A1_TO_DEPTH16,
	DEPTH32_TO_DEPTH24,
	DOWNSAMPLE_COPY,
	RGBA_TO_8I,
	RGB5A1_TO_8I,
	CLUT_4,
	CLUT_8,
	YUV,
	Count
};

enum class PresentShader
{
	COPY = 0,
	SCANLINE,
	DIAGONAL_FILTER,
	TRIANGULAR_FILTER,
	COMPLEX_FILTER,
	LOTTES_FILTER,
	SUPERSAMPLE_4xRGSS,
	SUPERSAMPLE_AUTO,
	Count
};

enum class SetDATM : u8
{
	DATM0 = 0U,
	DATM1,
	DATM0_RTA_CORRECTION,
	DATM1_RTA_CORRECTION
};

enum class ShaderInterlace
{
	WEAVE = 0,
	BOB = 1,
	BLEND = 2,
	MAD_BUFFER = 3,
	MAD_RECONSTRUCT = 4,
	Count
};

static inline constexpr bool HasVariableWriteMask(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::COPY:
		case ShaderConvert::RTA_CORRECTION:
			return true;
		default:
			return false;
	}
}

static inline constexpr bool HasColorOutput(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::COPY:
		case ShaderConvert::RTA_CORRECTION:
		case ShaderConvert::RTA_DECORRECTION:
		case ShaderConvert::TRANSPARENCY_FILTER:
		case ShaderConvert::DEPTH32_TO_RGBA8:
		case ShaderConvert::DEPTH32_TO_RGB8:
		case ShaderConvert::DEPTH16_TO_RGB5A1:
		case ShaderConvert::DOWNSAMPLE_COPY:
		case ShaderConvert::RGBA_TO_8I:
		case ShaderConvert::RGB5A1_TO_8I:
		case ShaderConvert::CLUT_4:
		case ShaderConvert::CLUT_8:
		case ShaderConvert::YUV:
		case ShaderConvert::COLCLIP_RESOLVE:
			return true;
		default:
			return false;
	}
}

static inline constexpr bool HasFloat32Output(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::RGBA8_TO_DEPTH32:
		case ShaderConvert::RGBA8_TO_DEPTH24:
		case ShaderConvert::RGBA8_TO_DEPTH16:
		case ShaderConvert::RGB5A1_TO_DEPTH16:
		case ShaderConvert::DEPTH_COPY:
		case ShaderConvert::DEPTH32_TO_DEPTH24:
			return true;
		default:
			return false;
	}
}

static inline constexpr bool HasFloat32Input(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::DEPTH_COPY:
		case ShaderConvert::DEPTH32_TO_16_BITS:
		case ShaderConvert::DEPTH32_TO_32_BITS:
		case ShaderConvert::DEPTH32_TO_RGBA8:
		case ShaderConvert::DEPTH32_TO_RGB8:
		case ShaderConvert::DEPTH16_TO_RGB5A1:
		case ShaderConvert::DEPTH32_TO_DEPTH24:
			return true;
		default:
			return false;
	}
}

static inline constexpr bool IsDATMConvertShader(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::DATM_0:
		case ShaderConvert::DATM_1:
		case ShaderConvert::DATM_0_RTA_CORRECTION:
		case ShaderConvert::DATM_1_RTA_CORRECTION:
			return true;
		default:
			return false;
	}
}

static inline constexpr bool HasStencilOutput(ShaderConvert shader)
{
	return IsDATMConvertShader(shader);
}

static inline constexpr int IntegerOutputBpp(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::DEPTH32_TO_32_BITS:
			return 32;
		case ShaderConvert::DEPTH32_TO_16_BITS:
		case ShaderConvert::RGB5A1_TO_16_BITS:
			return 16;
		default:
			return 0;
	}
}

static inline constexpr bool HasColorClipOutput(ShaderConvert shader)
{
	return (shader == ShaderConvert::COLCLIP_INIT);
}

static inline constexpr bool SupportsBilinear(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::RGBA8_TO_DEPTH32:
		case ShaderConvert::RGBA8_TO_DEPTH24:
		case ShaderConvert::RGBA8_TO_DEPTH16:
		case ShaderConvert::RGB5A1_TO_DEPTH16:
			return true;
		default:
			return false;
	}
}

static inline constexpr u32 ShaderConvertWriteMask(ShaderConvert shader)
{
	switch (shader)
	{
		case ShaderConvert::DEPTH32_TO_RGB8:
			return 0x7;
		default:
			return 0xf;
	}
}

static inline constexpr int GetShaderIndexForMask(ShaderConvert shader, int mask)
{
	pxAssert(HasVariableWriteMask(shader));
	int index = mask;
	if (shader == ShaderConvert::RTA_CORRECTION)
		index |= 1 << 4;
	return index;
}

static inline constexpr ShaderConvert SetDATMShader(SetDATM datm)
{
	switch (datm)
	{
	case SetDATM::DATM1_RTA_CORRECTION:
		return ShaderConvert::DATM_1_RTA_CORRECTION;
	case SetDATM::DATM0_RTA_CORRECTION:
		return ShaderConvert::DATM_0_RTA_CORRECTION;
	case SetDATM::DATM1:
		return ShaderConvert::DATM_1;
	case SetDATM::DATM0:
	default:
		return ShaderConvert::DATM_0;
	}
}

const char* ShaderEntryPoint(ShaderConvert value);
const char* ShaderEntryPoint(PresentShader value);
const char* ShaderConvertName(ShaderConvert shader);

class ShaderConvertSelector
{
	union
	{
		struct
		{
			u32 shader : 8; // Main shader
			u32 mask : 8; // Variable color mask
			u32 depth_out : 1; // Depth texture output
			u32 filter : 1; // Shader filter (HW filter is specified separately)
		};

		u32 key;
	} fields;

	static_assert(sizeof(fields) == 4);

public:
	constexpr ShaderConvertSelector(ShaderConvert shader = ShaderConvert::COPY, u8 mask = 0xf,
 		bool depth_out = false, Filter filter = Filter::Nearest)
		: fields { static_cast<u32>(shader) }
	{
		*this = SetMask(mask).SetDepthOutput(depth_out).SetFilter(filter);
	}

	constexpr ShaderConvert Shader() const
	{
		return static_cast<ShaderConvert>(fields.shader);
	}

	constexpr u8 Mask() const
	{
		return fields.mask;
	}

	constexpr u8 DefaultMask() const
	{
		return ShaderConvertWriteMask(Shader());
	}

	constexpr Filter GetFilter() const
	{
		return static_cast<Filter>(fields.filter);
	}

	constexpr bool Biln() const
	{
		return GetFilter() == Filter::Biln;
	}

	constexpr bool Nearest() const
	{
		return GetFilter() == Filter::Nearest;
	}

	constexpr bool SupportsBilinear() const
	{
		return ::SupportsBilinear(Shader());
	}

	constexpr bool ColorOutput() const
	{
		return HasColorOutput(Shader());
	}

	constexpr bool DepthOutput() const
	{
		return fields.depth_out;
	}

	constexpr bool StencilOutput() const
	{
		return HasStencilOutput(Shader());
	}

	constexpr bool DATMConvertShader() const
	{
		return IsDATMConvertShader(Shader());
	}

	constexpr bool Float32Output() const
	{
		return HasFloat32Output(Shader());
	}

	constexpr bool Float32Input() const
	{
		return HasFloat32Input(Shader());
	}

	constexpr int IntegerOutputBpp() const
	{
		return ::IntegerOutputBpp(Shader());
	}

	constexpr bool VariableWriteMask() const
	{
		return HasVariableWriteMask(Shader());
	}

	constexpr bool ColorClipOutput() const
	{
		return HasColorClipOutput(Shader());
	}

	const char* Name() const
	{
		return ShaderConvertName(Shader());
	}

	const char* EntryPoint() const
	{
		return ShaderEntryPoint(Shader());
	}

	constexpr ShaderConvertSelector SetMask(u8 mask = 0xf) const
	{
		ShaderConvertSelector tmp = *this;
		tmp.fields.mask = VariableWriteMask() ? (mask & 0xf) : DefaultMask();
		return tmp;
	}

	constexpr ShaderConvertSelector SetMask(bool wr, bool wg, bool wb, bool wa) const
	{
		return SetMask((wr ? 1 : 0) | (wg ? 2 : 0) | (wb ? 4 : 0) | (wa ? 8 : 0));
	}

	constexpr ShaderConvertSelector SetDepthOutput(bool depth_out) const
	{
		ShaderConvertSelector tmp = *this;
		tmp.fields.depth_out = Float32Output() && depth_out;
		return tmp;
	}

	constexpr ShaderConvertSelector SetFilter(Filter filter) const
	{
		ShaderConvertSelector tmp = *this;
		tmp.fields.filter = static_cast<u32>(SupportsBilinear() ? filter : Filter::Nearest);
		return tmp;
	}

	GSTexture::Format OutputFormat() const
	{
		if (DepthOutput())
			return GSTexture::Format::DepthStencil;
		else if (int bpp = IntegerOutputBpp())
			return bpp == 16 ? GSTexture::Format::UInt16 : GSTexture::Format::UInt32;
		else if (Float32Output())
			return GSTexture::Format::DepthColor;
		else if (ColorOutput())
			return GSTexture::Format::Color;
		else if (ColorClipOutput())
			return GSTexture::Format::ColorClip;
		else
			return GSTexture::Format::Invalid;
	}

private:
	// Helper variables for packing valid shaders into a contiguous range.
	static const std::span<const ShaderConvertSelector> SHADERS;
	static const std::array<u8, static_cast<u32>(ShaderConvert::Count) * 4> INDEX_REMAP;
	static const u32 NUM_REMAPPED_SHADERS;

public:
	static constexpr u32 NUM_VARIABLE_WRITE_MASK_SHADERS = 2;
	static const u32 NUM_TOTAL_SHADERS;

	u32 Index() const
	{
		if (VariableWriteMask() && !fields.depth_out && Nearest())
			return GetShaderIndexForMask(Shader(), fields.mask) + NUM_REMAPPED_SHADERS;
		u32 remapped = INDEX_REMAP[(fields.depth_out << 0) +
		                           (fields.filter    << 1) +
		                           (fields.shader    << 2)];
		pxAssert(remapped < NUM_REMAPPED_SHADERS);
		return remapped;
	}

	// Inverse of Index()
	static ShaderConvertSelector Get(u32 index)
	{
		return SHADERS[index];
	}
};

static inline ShaderConvertSelector GetConvertShader(GSTexture::Format src, GSTexture::Format dst,
	u32 src_bpp = 32, u32 dst_bpp = 32, u8 mask = 0xf)
{
	ShaderConvert shader = static_cast<ShaderConvert>(-1);
	switch (src)
	{
		case GSTexture::Format::Color:
			switch (dst)
			{
				case GSTexture::Format::Color:
					pxAssert(src_bpp == 32 && dst_bpp == 32);
					shader = ShaderConvert::COPY; // bpp is handled by mask
					break;
				case GSTexture::Format::DepthColor:
				case GSTexture::Format::DepthStencil:
					switch (dst_bpp)
					{
						case 32:
							shader = ShaderConvert::RGBA8_TO_DEPTH32;
							break;
						case 24:
							shader = ShaderConvert::RGBA8_TO_DEPTH24;
							break;
						case 16:
							pxAssert(src_bpp == 16 || src_bpp == 32);
							shader = src_bpp == 16 ? ShaderConvert::RGB5A1_TO_DEPTH16 :
							                         ShaderConvert::RGBA8_TO_DEPTH16;
							break;
						default:
							pxAssert(false);
							break;
					}
					break;
				default:
					pxAssert(false);
					break;
			}
			break;
		case GSTexture::Format::DepthColor:
		case GSTexture::Format::DepthStencil:
			switch (dst)
			{
				case GSTexture::Format::Color:
					switch (dst_bpp)
					{
						case 32:
							shader = ShaderConvert::DEPTH32_TO_RGBA8;
							break;
						case 24:
							shader = ShaderConvert::DEPTH32_TO_RGB8;
							break;
						case 16:
							pxAssert(src_bpp == 16);
							shader = ShaderConvert::DEPTH16_TO_RGB5A1;
							break;
						default:
							pxAssert(false);
							break;
					}
					break;
				case GSTexture::Format::DepthColor:
				case GSTexture::Format::DepthStencil:
					switch (dst_bpp)
					{
						case 32:
							pxAssert(src_bpp == 32);
							shader = ShaderConvert::DEPTH_COPY;
							break;
						case 24:
							pxAssert(src_bpp == 32);
							shader = ShaderConvert::DEPTH32_TO_DEPTH24;
							break;
						default:
							pxAssert(false);
							break;
					}
					break;
				default:
					pxAssert(false);
			}
			break;
		default:
			pxAssert(false);
			break;
	}

	return ShaderConvertSelector(shader, mask, dst == GSTexture::Format::DepthStencil);
}

static inline ShaderConvertSelector GetConvertShader(const GSTexture* src, const GSTexture* dst, u32 src_bpp, u32 dst_bpp, u8 mask = 0xf)
{
	return GetConvertShader(src->GetFormat(), dst->GetFormat(), src_bpp, dst_bpp, mask);
}

static inline ShaderConvertSelector GetConvertShaderMask(GSTexture::Format src, GSTexture::Format dst,
	u32 src_bpp, u32 dst_bpp, bool red = true, bool green = true, bool blue = true, bool alpha = true)
{
	const u8 mask = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
	return GetConvertShader(src, dst, src_bpp, dst_bpp, mask);
}

static inline ShaderConvertSelector GetConvertShaderMask(const GSTexture* src, const GSTexture* dst,
	u32 src_bpp, u32 dst_bpp, bool red = true, bool green = true, bool blue = true, bool alpha = true)
{
	return GetConvertShaderMask(src->GetFormat(), dst->GetFormat(), src_bpp, dst_bpp, red, green, blue, alpha);
}

enum ChannelFetch
{
	ChannelFetch_NONE  = 0,
	ChannelFetch_RED   = 1,
	ChannelFetch_GREEN = 2,
	ChannelFetch_BLUE  = 3,
	ChannelFetch_ALPHA = 4,
	ChannelFetch_RGB   = 5,
	ChannelFetch_GXBY  = 6,
};

enum class HWBlendType
{
	SRC_ONE_DST_FACTOR      = 1, // Use the dest color as blend factor, Cs is set to 1.
	SRC_ALPHA_DST_FACTOR    = 2, // Use the dest color as blend factor, Cs is set to (Alpha - 1).
	SRC_DOUBLE              = 3, // Double source color.
	SRC_HALF_ONE_DST_FACTOR = 4, // Use the dest color as blend factor, Cs is set to 0.5, additionally divide As or Af by 2.
	SRC_INV_DST_BLEND_HALF  = 5, // Halve the alpha then double the final result.
	INV_SRC_DST_BLEND_HALF  = 6, // Halve the alpha then double the final result.

	BMIX1_ALPHA_HIGH_ONE    = 1, // Blend formula is replaced when alpha is higher than 1.
	BMIX1_SRC_HALF          = 2, // Impossible blend will always be wrong on hw, divide Cs by 2.
	BMIX2_OVERFLOW          = 3, // Blending Cs might overflow, try to compensate.
};

struct alignas(16) DisplayConstantBuffer
{
	GSVector4 SourceRect; // +0,xyzw
	GSVector4 TargetRect; // +16,xyzw
	GSVector2 SourceSize; // +32,xy
	GSVector2 TargetSize; // +40,zw
	GSVector2 TargetResolution; // +48,xy
	GSVector2 RcpTargetResolution; // +56,zw
	GSVector2 SourceResolution; // +64,xy
	GSVector2 RcpSourceResolution; // +72,zw
	GSVector4 TimeAndPad; // seconds since GS init +76,xyzw
	// +96

	// assumes that sRect is normalized
	void SetSource(const GSVector4& sRect, const GSVector2i& sSize)
	{
		SourceRect = sRect;
		SourceResolution = GSVector2(static_cast<float>(sSize.x), static_cast<float>(sSize.y));
		RcpSourceResolution = GSVector2(1.0f) / SourceResolution;
		SourceSize = GSVector2((sRect.z - sRect.x) * SourceResolution.x, (sRect.w - sRect.y) * SourceResolution.y);
	}
	void SetTarget(const GSVector4& dRect, const GSVector2i& dSize)
	{
		TargetRect = dRect;
		TargetResolution = GSVector2(static_cast<float>(dSize.x), static_cast<float>(dSize.y));
		RcpTargetResolution = GSVector2(1.0f) / TargetResolution;
		TargetSize = GSVector2(dRect.z - dRect.x, dRect.w - dRect.y);
	}
	void SetTime(float time)
	{
		TimeAndPad = GSVector4(time);
	}
};
static_assert(sizeof(DisplayConstantBuffer) == 96, "DisplayConstantBuffer is correct size");

struct alignas(16) MergeConstantBuffer
{
	GSVector4 BGColor;
	u32 EMODA;
	u32 EMODC;
	u32 DOFFSET;
	float ScaleFactor;
};
static_assert(sizeof(MergeConstantBuffer) == 32, "MergeConstantBuffer is correct size");

struct alignas(16) InterlaceConstantBuffer
{
	GSVector4 ZrH; // data passed to the shader
};
static_assert(sizeof(InterlaceConstantBuffer) == 16, "InterlaceConstantBuffer is correct size");

enum HWBlendFlags
{
	// Flags to determine blending behavior
	BLEND_CD     = 0x1,    // Output is Cd, hw blend can handle it
	BLEND_HW1    = 0x2,    // Clear color blending (use directly the destination color as blending factor)
	BLEND_HW2    = 0x4,    // Clear color blending (use directly the destination color as blending factor)
	BLEND_HW3    = 0x8,    // Multiply Cs by (255/128) to compensate for wrong Ad/255 value, should be Ad/128
	BLEND_HW4    = 0x10,   // HW rendering is split in 2 passes
	BLEND_HW5    = 0x20,   // HW rendering is split in 2 passes
	BLEND_HW6    = 0x40,   // HW rendering is split in 2 passes
	BLEND_HW7    = 0x80,   // HW rendering is split in 2 passes
	BLEND_HW8    = 0x100,  // HW rendering is split in 2 passes
	BLEND_HW9    = 0x200,  // HW rendering is split in 2 passes
	BLEND_MIX1   = 0x400,  // Mix of hw and sw, do Cs*F or Cs*As in shader
	BLEND_MIX2   = 0x800,  // Mix of hw and sw, do Cs*(As + 1) or Cs*(F + 1) in shader
	BLEND_MIX3   = 0x1000, // Mix of hw and sw, do Cs*(1 - As) or Cs*(1 - F) in shader
	BLEND_ACCU   = 0x2000, // Allow to use a mix of SW and HW blending to keep the best of the 2 worlds
	BLEND_NO_REC = 0x4000, // Doesn't require sampling of the RT as a texture
	BLEND_A_MAX  = 0x8000, // Impossible blending uses coeff bigger than 1
};

// Determines the HW blend function for the video backend
struct HWBlend
{
	typedef u8 BlendOp; /*GSDevice::BlendOp*/
	typedef u8 BlendFactor; /*GSDevice::BlendFactor*/

	u16 flags;
	BlendOp op;
	BlendFactor src, dst;
};

// The expansion macro (EXPAND) must take the arguments:
// 1. Index - index of the field.
// 2. Dynamic - whether the field can be used dynamically.
// 3. Type - type of the field.
// 4. Name - name of the field.
// 5. Bits - number of bits in the field.
// 6. Shader name - name of the shader macro.

#define PSSEL_FIELDS_EXPAND \
	EXPAND(0, true, u32, aem_fmt, 2, "PS_AEM_FMT") \
	EXPAND(1, true, u32, pal_fmt, 2, "PS_PAL_FMT") \
	EXPAND(2, true, u32, dst_fmt, 2, "PS_DST_FMT") \
	EXPAND(3, true, u32, depth_fmt, 2, "PS_DEPTH_FMT") \
	EXPAND(4, true, u32, aem, 1, "PS_AEM") \
	EXPAND(5, true, u32, fba, 1, "PS_FBA") \
	EXPAND(6, true, u32, fog, 1, "PS_FOG") \
	EXPAND(7, false, u32, iip, 1, "PS_IIP") \
	EXPAND(8, true, u32, date, 3, "PS_DATE") \
	EXPAND(9, true, PS_ATST, atst, 3, "PS_ATST") \
	EXPAND(10, true, PS_AFAIL, afail, 3, "PS_AFAIL") \
	EXPAND(11, true, u32, ztst, 2, "PS_ZTST") \
	EXPAND(12, true, u32, fst, 1, "PS_FST") \
	EXPAND(13, true, u32, tfx, 3, "PS_TFX") \
	EXPAND(14, true, u32, tcc, 1, "PS_TCC") \
	EXPAND(15, true, u32, wms, 2, "PS_WMS") \
	EXPAND(16, true, u32, wmt, 2, "PS_WMT") \
	EXPAND(17, true, u32, adjs, 1, "PS_ADJS") \
	EXPAND(18, true, u32, adjt, 1, "PS_ADJT") \
	EXPAND(19, true, u32, ltf, 1, "PS_LTF") \
	EXPAND(20, true, u32, shuffle, 1, "PS_SHUFFLE") \
	EXPAND(21, true, u32, shuffle_same, 1, "PS_SHUFFLE_SAME") \
	EXPAND(22, true, u32, real16src, 1, "PS_READ16_SRC") \
	EXPAND(23, true, u32, process_ba, 2, "PS_PROCESS_BA") \
	EXPAND(24, true, u32, process_rg, 2, "PS_PROCESS_RG") \
	EXPAND(25, true, u32, shuffle_across, 1, "PS_SHUFFLE_ACROSS") \
	EXPAND(26, true, u32, write_rg, 1, "PS_WRITE_RG") \
	EXPAND(27, true, u32, fbmask, 1, "PS_FBMASK") \
	EXPAND(28, true, u32, blend_a, 2, "PS_BLEND_A") \
	EXPAND(29, true, u32, blend_b, 2, "PS_BLEND_B") \
	EXPAND(30, true, u32, blend_c, 2, "PS_BLEND_C") \
	EXPAND(31, true, u32, blend_d, 2, "PS_BLEND_D") \
	EXPAND(32, true, u32, fixed_one_a, 1, "PS_FIXED_ONE_A") \
	EXPAND(33, true, u32, blend_hw, 3, "PS_BLEND_HW") \
	EXPAND(34, true, u32, a_masked, 1, "PS_A_MASKED") \
	EXPAND(35, true, u32, colclip_hw, 1, "PS_COLCLIP_HW") \
	EXPAND(36, true, u32, rta_correction, 1, "PS_RTA_CORRECTION") \
	EXPAND(37, true, u32, rta_source_correction, 1, "PS_RTA_SRC_CORRECTION") \
	EXPAND(38, true, u32, colclip, 1, "PS_COLCLIP") \
	EXPAND(39, true, u32, blend_mix, 2, "PS_BLEND_MIX") \
	EXPAND(40, true, u32, round_inv, 1, "PS_ROUND_INV") \
	EXPAND(41, true, u32, pabe, 1, "PS_PABE") \
	EXPAND(42, false, u32, no_color, 1, "PS_NO_COLOR") \
	EXPAND(43, false, u32, no_color1, 1, "PS_NO_COLOR1") \
	EXPAND(44, true, u32, channel, 3, "PS_CHANNEL_FETCH") \
	EXPAND(45, true, u32, dither, 2, "PS_DITHER") \
	EXPAND(46, true, u32, dither_adjust, 1, "PS_DITHER_ADJUST") \
	EXPAND(47, true, u32, zclamp, 1, "PS_ZCLAMP") \
	EXPAND(48, true, u32, zfloor, 1, "PS_ZFLOOR") \
	EXPAND(49, true, u32, tcoffsethack, 1, "PS_TCOFFSETHACK") \
	EXPAND(50, true, u32, urban_chaos_hle, 1, "PS_URBAN_CHAOS_HLE") \
	EXPAND(51, true, u32, tales_of_abyss_hle, 1, "PS_TALES_OF_ABYSS_HLE") \
	EXPAND(52, true, u32, tex_is_fb, 1, "PS_TEX_IS_FB") \
	EXPAND(53, true, u32, automatic_lod, 1, "PS_AUTOMATIC_LOD") \
	EXPAND(54, true, u32, manual_lod, 1, "PS_MANUAL_LOD") \
	EXPAND(55, true, u32, point_sampler, 1, "PS_POINT_SAMPLER") \
	EXPAND(56, true, u32, region_rect, 1, "PS_REGION_RECT") \
	EXPAND(57, true, u32, scanmsk, 2, "PS_SCANMSK") \
	EXPAND(58, true, PS_AA1, aa1, 2, "PS_AA1") \
	EXPAND(59, true, u32, abe, 1, "PS_ABE") \
	EXPAND(60, true, u32, sw_aniso, 5, "PS_ANISOTROPIC_FILTERING") \
	EXPAND(61, true, u32, zmask, 1, "PS_ZMASK") \
	EXPAND(62, true, u32, cmask, 1, "PS_CMASK") \
	EXPAND(63, false, u32, rov_color, 1, "PS_ROV_COLOR") \
	EXPAND(64, false, PS_ROV_DEPTH, rov_depth, 2, "PS_ROV_DEPTH")


#define VSSEL_FIELDS_EXPAND \
	EXPAND(0, true, u8, fst, 1, "VS_FST") \
	EXPAND(1, true, u8, tme, 1, "VS_TME") \
	EXPAND(2, false, u8, iip, 1, "VS_IIP") \
	EXPAND(3, false, u8, point_size, 1, "VS_POINT_SIZE") \
	EXPAND(4, true, VSExpand, expand, 3, "VS_EXPAND")

struct alignas(16) GSHWDrawConfig
{
	enum class Topology: u8
	{
		Point,
		Line,
		Triangle,
	};
	using VSExpand = GSShader::VSExpand;
	using PS_ATST  = GSShader::PS_ATST;
	using PS_AFAIL = GSShader::PS_AFAIL;
	using PS_AA1   = GSShader::PS_AA1;
	using PS_ROV_DEPTH = GSShader::PS_ROV_DEPTH;

	struct PipelineSelectorFieldDesc
	{
		u32 index;
		bool dynamic;
		const char* name;
		u32 bits;
		const char* shader_name;
	};

#pragma pack(push, 1)
	struct VSSelector
	{
		union
		{
			struct
			{
				#define EXPAND(INDEX, DYNAMIC, TYPE, NAME, WIDTH, SHADER_NAME) TYPE NAME : WIDTH;
					VSSEL_FIELDS_EXPAND
				#undef EXPAND
			};
			u8 key;
		};
		VSSelector(): key(0) {}
		VSSelector(u8 k): key(k) {}

		u32 GetField(const std::string& name) const;
		u32 GetField(u32 index) const;

		/// Returns true if the fixed index buffer should be used.
		__fi bool UseFixedExpandIndexBuffer() const { return (expand == VSExpand::Point || expand == VSExpand::Sprite); }
		
		/// Return true if the index buffer should be bound as a vertex shader resource.
		__fi bool UseVSExpandIndexBuffer() const { return (expand == VSExpand::TriangleAA1); }

		__fi bool operator==(const VSSelector& rhs) const { return key == rhs.key; }
		__fi bool operator!=(const VSSelector& rhs) const { return key != rhs.key; }
		__fi bool operator<(const VSSelector& rhs) const { return key < rhs.key; }
	};
	static_assert(sizeof(VSSelector) == 1, "VSSelector is a single byte");

	struct PSSelector
	{
		// Performance note: there are too many shader combinations
		// It might hurt the performance due to frequent toggling worse it could consume
		// a lots of memory.
		union
		{
			struct
			{
				#define EXPAND(INDEX, DYNAMIC, TYPE, NAME, WIDTH, SHADER_NAME) TYPE NAME : WIDTH;
					PSSEL_FIELDS_EXPAND
				#undef EXPAND
			};

			struct
			{
				u64 key_lo;
				u64 key_hi;
			};
		};
		__fi PSSelector() : key_lo(0), key_hi(0) {}

		__fi bool operator==(const PSSelector& rhs) const { return (key_lo == rhs.key_lo && key_hi == rhs.key_hi); }
		__fi bool operator!=(const PSSelector& rhs) const { return (key_lo != rhs.key_lo || key_hi != rhs.key_hi); }
		__fi bool operator<(const PSSelector& rhs) const { return (key_lo < rhs.key_lo || key_hi < rhs.key_hi); }

		u32 GetField(const std::string& name) const;
		u32 GetField(u32 index) const;

		__fi bool IsSWBlending() const
		{
			return blend_a || blend_b || blend_d;
		}

		__fi bool IsZTesting() const
		{
			return ztst == ZTST_GEQUAL || ztst == ZTST_GREATER;
		}

		__fi bool IsAlphaTesting() const
		{
			return atst != PS_ATST::NONE;
		}

		__fi bool IsFeedbackLoopRT() const
		{
			const u32 sw_blend_bits = blend_a | blend_b | blend_d;
			const bool sw_blend_needs_rt = (sw_blend_bits != 0 && ((sw_blend_bits | blend_c) & 1u)) || ((a_masked & blend_c) != 0);
			const bool afail_needs_rt = afail == PS_AFAIL::ZB_ONLY || afail == PS_AFAIL::RGB_ONLY || afail == PS_AFAIL::RGB_ONLY_SW_Z;
			return tex_is_fb || fbmask || (date >= 5) || sw_blend_needs_rt || afail_needs_rt || cmask;
		}

		__fi bool IsFeedbackLoopDepth() const
		{
			const bool afail_needs_depth = afail == PS_AFAIL::FB_ONLY || afail == PS_AFAIL::RGB_ONLY_SW_Z;
			const bool ztst_needs_depth = ztst == ZTST_GEQUAL || ztst == ZTST_GREATER;
			const bool aa1_needs_depth = aa1 == PS_AA1::TRIANGLE_SW_Z;
			return afail_needs_depth || ztst_needs_depth || aa1_needs_depth || zmask;
		}

		__fi bool HasShaderDiscard() const
		{
			return (IsAlphaTesting() && afail == PS_AFAIL::KEEP) || scanmsk || date || IsZTesting();
		}

		/// Disables color output from the pixel shader, this is done when all channels are masked.
		__fi void DisableColorOutput()
		{
			// remove software blending, since this will cause the color to be declared inout with fbfetch.
			blend_a = blend_b = blend_c = blend_d = 0;

			// TEX_IS_FB relies on us having a color output to begin with.
			tex_is_fb = 0;

			// no point having fbmask, since we're not writing. DATE has to stay.
			fbmask = 0;

			// disable both outputs.
			no_color = no_color1 = 1;
		}

		/// Disables depth output from the pixel shader.
		__fi void DisableDepthOutput()
		{
			if (afail == PS_AFAIL::RGB_ONLY_SW_Z)
			{
				afail = PS_AFAIL::RGB_ONLY;
			}

			if (aa1 == PS_AA1::TRIANGLE_SW_Z)
			{
				aa1 = PS_AA1::TRIANGLE;
			}

			if (rov_depth == PS_ROV_DEPTH::READ_WRITE)
			{
				rov_depth = PS_ROV_DEPTH::READ_ONLY;
			}
		}

		__fi bool HasColorOutput() const
		{
			return !no_color;
		}

		__fi bool HasDepthOutput() const
		{
			return zfloor || zclamp || IsFeedbackLoopDepth() || (rov_depth == PS_ROV_DEPTH::READ_WRITE);
		}

		__fi bool HasColorROV() const
		{
			return rov_color != 0;
		}

		__fi bool HasDepthROV() const
		{
			return rov_depth != PS_ROV_DEPTH::NONE;
		}

		__fi bool HasDepthROVWrite() const
		{
			return rov_depth == PS_ROV_DEPTH::READ_WRITE;
		}

		__fi bool HasDATEInit() const
		{
			return date == 1 || date == 2;
		}
	};
	static_assert(sizeof(PSSelector) == 16, "PSSelector is 12 bytes");
#pragma pack(pop)

	struct PSSelectorHash
	{
		std::size_t operator()(const PSSelector& p) const
		{
			std::size_t h = 0;
			HashCombine(h, p.key_lo, p.key_hi);
			return h;
		}
	};

	enum class TFX_RT : u32
	{
		None,
		Color,
		ColclipHW,
		PrimID,
		Count,
	};

	enum class TFX_DS : u32
	{
		None,
		Depth,
		DepthStencil,
		Count,
	};

	enum class UberVSSelector
	{
		InputAssembly,
		VSExpand,
	};
#pragma pack(push, 1)
	template<bool allow_color_feedback>
	struct UberPSSelector
	{
		enum class Color : u8
		{
			None,
			Standard,
			Feedback,
			ROV,
			Count,
		};

		enum class Depth : u8
		{
			None,
			Standard,
			Feedback,
			ROV,
			Count,
		};

		union
		{
			struct
			{
				Color color : 2;
				u8 color1 : 1;
				Depth depth : 2;
				u8 date_init : 1; // DATE primid init.
			};

			u8 key;
		};

		static constexpr u32 MAX_NUM_SELECTORS = 64;

		__fi constexpr UberPSSelector() : key(0) {}

		__fi static constexpr UberPSSelector Decode(u8 key)
		{
			UberPSSelector ps;

			ps.color = static_cast<Color>((key >> 0) & 3);
			ps.color1 = (key >> 2) & 1;
			ps.depth = static_cast<Depth>((key >> 3) & 3);
			ps.date_init = (key >> 5) & 1;

			return ps;
		}

		__fi constexpr bool IsValid() const
		{
			// Make sure enums are valid.
			if (color >= Color::Count || depth >= Depth::Count)
				return false;
			const bool rov = color == Color::ROV || depth == Depth::ROV;
			const bool feedback = color == Color::Feedback || depth == Depth::Feedback;
			// These are mutually incompatible.
			if (rov && (feedback || color1))
				return false;
			// Only VK fragment shaders needs explicit color feedback.
			if (!allow_color_feedback && color == Color::Feedback)
				return false;
			// Depth ROV must imply color ROV (if color is used).
			if (depth == Depth::ROV && !(color == Color::None || color == Color::ROV))
				return false;
			// Color1 must imply color.
			if (color1 && color == Color::None)
				return false;
			// Must have either color or depth.
			if (color == Color::None && depth == Depth::None)
				return false;
			// There's only one uber date init shader and disallows DSB or special depth.
			if (date_init && (color != Color::Standard || color1 || depth > Depth::Standard))
				return false;
			return true;
		}

		__fi constexpr bool CompatibleWithAttachments(TFX_RT rt, TFX_DS ds) const
		{
			if ((rt == TFX_RT::None) != (color == Color::None || color == Color::ROV))
				return false;
			if ((ds == TFX_DS::None) != (depth == Depth::None || depth == Depth::ROV))
				return false;
			if (static_cast<bool>(date_init) != (rt == TFX_RT::PrimID))
				return false;
			if (date_init && (ds == TFX_DS::DepthStencil))
				return false;
			if (color1 && (rt == TFX_RT::PrimID))
				return false;
			return true;
		}

		__fi bool HasColor() const
		{
			return color != Color::None;
		}

		__fi bool HasDepth() const
		{
			return depth != Depth::None;
		}

		__fi bool HasColorFeedback() const
		{
			return color == Color::Feedback;
		}

		__fi bool HasDepthFeedback() const
		{
			return depth == Depth::Feedback;
		}

		__fi bool HasColorROV() const
		{
			return color == Color::ROV;
		}

		__fi bool HasDepthROV() const
		{
			return depth == Depth::ROV;
		}

		static std::span<const UberPSSelector> GetValidSelectors();
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct SamplerSelector
	{
		union
		{
			struct
			{
				u8 tau      : 1;
				u8 tav      : 1;
				u8 biln     : 1;
				u8 triln    : 3;
				u8 lodclamp : 1;
			};
			u8 key;
		};
		SamplerSelector(): key(0) {}
		SamplerSelector(u8 k): key(k) {}
		static SamplerSelector Point() { return SamplerSelector(); }
		static SamplerSelector Linear()
		{
			SamplerSelector out;
			out.biln = 1;
			return out;
		}

		/// Returns true if the effective minification filter is linear.
		__fi bool IsMinFilterLinear() const
		{
			if (triln < static_cast<u8>(GS_MIN_FILTER::Nearest_Mipmap_Nearest))
			{
				// use the same filter as mag when mipmapping is off
				return biln;
			}
			else
			{
				// Linear_Mipmap_Nearest or Linear_Mipmap_Linear
				return (triln >= static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Nearest));
			}
		}

		/// Returns true if the effective magnification filter is linear.
		__fi bool IsMagFilterLinear() const
		{
			// magnification uses biln regardless of mip mode (they're only used for minification)
			return biln;
		}

		/// Returns true if the effective mipmap filter is linear.
		__fi bool IsMipFilterLinear() const
		{
			return (triln == static_cast<u8>(GS_MIN_FILTER::Nearest_Mipmap_Linear) ||
					triln == static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Linear));
		}

		/// Returns true if mipmaps should be used when filtering (i.e. LOD not clamped to zero).
		__fi bool UseMipmapFiltering() const
		{
			return (triln >= static_cast<u8>(GS_MIN_FILTER::Nearest_Mipmap_Nearest));
		}
	};
	struct DepthStencilSelector
	{
		union
		{
			struct
			{
				u8 ztst : 2;
				u8 zwe  : 1;
				u8 date : 1;
				u8 date_one : 1;

				u8 _free : 3;
			};
			u8 key;
		};
		constexpr DepthStencilSelector(): key(0) {}
		constexpr DepthStencilSelector(u8 k): key(k) {}
		static constexpr DepthStencilSelector NoDepth()
		{
			DepthStencilSelector out;
			out.ztst = ZTST_ALWAYS;
			return out;
		}
		static constexpr DepthStencilSelector DepthWriteAlways()
		{
			DepthStencilSelector out;
			out.zwe = true;
			out.ztst = ZTST_ALWAYS;
			return out;
		}
		static constexpr DepthStencilSelector ReducedUberDefault() { return DepthWriteAlways(); }
	};
	struct ColorMaskSelector
	{
		union
		{
			struct
			{
				u8 wr : 1;
				u8 wg : 1;
				u8 wb : 1;
				u8 wa : 1;

				u8 _free : 4;
			};
			struct
			{
				u8 wrgba : 4;
			};
			u8 key;
		};
		constexpr ColorMaskSelector(): key(0xF) {}
		constexpr ColorMaskSelector(u8 c): key(0) { wrgba = c; }
		static constexpr ColorMaskSelector ReducedUberDefault() { return ColorMaskSelector(); }
	};

#pragma pack(pop)
	struct alignas(16) VSConstantBuffer
	{
		GSVector2 vertex_scale;
		GSVector2 vertex_offset;
		GSVector2 texture_scale;
		GSVector2 texture_offset;
		GSVector2 point_size;
		u32 max_depth;
		float line_aa1_width;
		
		__fi VSConstantBuffer()
		{
			memset(static_cast<void*>(this), 0, sizeof(*this));
		}
		__fi VSConstantBuffer(const VSConstantBuffer& other)
		{
			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
		}
		__fi VSConstantBuffer& operator=(const VSConstantBuffer& other)
		{
			new (this) VSConstantBuffer(other);
			return *this;
		}
		__fi bool operator==(const VSConstantBuffer& other) const
		{
			return BitEqual(*this, other);
		}
		__fi bool operator!=(const VSConstantBuffer& other) const
		{
			return !(*this == other);
		}
		__fi bool Update(const VSConstantBuffer& other)
		{
			if (*this == other)
				return false;

			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
			return true;
		}
	};

	struct VSPushConstants
	{
		u32 base_vertex;
		u32 base_index;

		__fi VSPushConstants()
		{
			memset(static_cast<void*>(this), 0, sizeof(*this));
		}
		__fi VSPushConstants(const VSPushConstants& other)
		{
			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
		}
		__fi VSPushConstants& operator=(const VSPushConstants& other)
		{
			new (this) VSPushConstants(other);
			return *this;
		}
		__fi bool operator==(const VSPushConstants& other) const
		{
			return BitEqual(*this, other);
		}
		__fi bool operator!=(const VSPushConstants& other) const
		{
			return !(*this == other);
		}
		__fi bool Update(const VSPushConstants& other)
		{
			if (*this == other)
				return false;

			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
			return true;
		}
	};

	static_assert(sizeof(VSPushConstants) == 2 * sizeof(u32));

	struct UberDynamicSelector
	{
		union
		{
			struct
			{
				u32 vs_selector;
				u32 ps_selector[4];
			};
			u32 selector[5];
		};

		static constexpr u32 VS_OFFSET = 0;
		static constexpr u32 PS_OFFSET = 1;
		static constexpr u32 NUM_SELECTORS = 5;

		__fi UberDynamicSelector()
		{
			memset(static_cast<void*>(this), 0, sizeof(*this));
		}
		__fi UberDynamicSelector(const UberDynamicSelector& other)
		{
			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
		}
		__fi UberDynamicSelector& operator=(const UberDynamicSelector& other)
		{
			new (this) UberDynamicSelector(other);
			return *this;
		}
		__fi bool operator==(const UberDynamicSelector& other) const
		{
			return BitEqual(*this, other);
		}
		__fi bool operator!=(const UberDynamicSelector& other) const
		{
			return !(*this == other);
		}
		__fi bool Update(const UberDynamicSelector& other)
		{
			if (*this == other)
				return false;

			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
			return true;
		}
	};

	static_assert(sizeof(UberDynamicSelector) == UberDynamicSelector::NUM_SELECTORS * sizeof(u32));
	static_assert(offsetof(UberDynamicSelector, vs_selector) == UberDynamicSelector::VS_OFFSET * sizeof(u32));
	static_assert(offsetof(UberDynamicSelector, ps_selector) == UberDynamicSelector::PS_OFFSET * sizeof(u32));

	struct alignas(16) TFXPushConstants
	{
		VSPushConstants vs_pc;
		UberDynamicSelector uber_selector;
		u32 _pad_0;

		static constexpr u32 VS_PC_OFFSET = 0;
		static constexpr u32 VS_PC_NUM_CONSTANTS = sizeof(vs_pc) / sizeof(u32);

		__fi TFXPushConstants()
		{
			memset(static_cast<void*>(this), 0, sizeof(*this));
		}
		__fi TFXPushConstants(const TFXPushConstants& other)
		{
			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
		}
		__fi TFXPushConstants& operator=(const TFXPushConstants& other)
		{
			new (this) TFXPushConstants(other);
			return *this;
		}
		__fi bool operator==(const TFXPushConstants& other) const
		{
			return BitEqual(*this, other);
		}
		__fi bool operator!=(const TFXPushConstants& other) const
		{
			return !(*this == other);
		}
		__fi bool Update(const TFXPushConstants& other)
		{
			if (*this == other)
				return false;

			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
			return true;
		}
	};

	static_assert(sizeof(TFXPushConstants) == 32, "TFXPushConstants wrong size");
	static_assert(offsetof(TFXPushConstants, vs_pc) == TFXPushConstants::VS_PC_OFFSET * sizeof(u32));

	struct alignas(16) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 WH;
		GSVector4 TA_MaxDepth_Af;
		GSVector4i FbMask;

		GSVector4 HalfTexel;
		GSVector4 MinMax;
		GSVector4 LODParams;
		GSVector4 STRange;
		GSVector4i ChannelShuffle;
		GSVector2 ChannelShuffleOffset;
		GSVector2 TCOffsetHack;
		GSVector2 STScale;

		GSVector4 DitherMatrix[4];

		GSVector4 ScaleFactor;

		float LineCovScale;
		float _pad0;
		float _pad1;
		float _pad2;

		__fi PSConstantBuffer()
		{
			memset(static_cast<void*>(this), 0, sizeof(*this));
		}
		__fi PSConstantBuffer(const PSConstantBuffer& other)
		{
			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
		}
		__fi PSConstantBuffer& operator=(const PSConstantBuffer& other)
		{
			new (this) PSConstantBuffer(other);
			return *this;
		}
		__fi bool operator==(const PSConstantBuffer& other) const
		{
			return BitEqual(*this, other);
		}
		__fi bool operator!=(const PSConstantBuffer& other) const
		{
			return !(*this == other);
		}
		__fi bool Update(const PSConstantBuffer& other)
		{
			if (*this == other)
				return false;

			memcpy(static_cast<void*>(this), static_cast<const void*>(&other), sizeof(*this));
			return true;
		}
	};
	// For hardware rendering backends
	struct BlendState
	{
		typedef u8 BlendOp; /*GSDevice::BlendOp*/
		typedef u8 BlendFactor; /*GSDevice::BlendFactor*/

		union
		{
			struct
			{
				bool enable : 1;
				bool constant_enable : 1;
				BlendOp op : 6;
				BlendFactor src_factor : 4;
				BlendFactor dst_factor : 4;
				BlendFactor src_factor_alpha : 4;
				BlendFactor dst_factor_alpha : 4;
				u8 constant;
			};
			u32 key;
		};
		constexpr BlendState(): key(0) {}
		constexpr BlendState(bool enable_, BlendFactor src_factor_, BlendFactor dst_factor_, BlendOp op_,
			BlendFactor src_alpha_factor_, BlendFactor dst_alpha_factor_, bool constant_enable_, u8 constant_)
			: key(0)
		{
			enable = enable_;
			constant_enable = constant_enable_;
			src_factor = src_factor_;
			dst_factor = dst_factor_;
			op = op_;
			src_factor_alpha = src_alpha_factor_;
			dst_factor_alpha = dst_alpha_factor_;
			constant = constant_;
		}
		static constexpr BlendState ReducedUberDefault() { return BlendState(); }
		// Blending has no effect if RGB is masked.
		bool IsEffective(ColorMaskSelector colormask) const;
	};

	enum class AlphaTestMode
	{
		NONE,
		KEEP,
		FEEDBACK,
		SIMPLE_FB_ONLY,
		SIMPLE_RGB_ONLY,
		PASS_THEN_FAIL,
		NEVER,
		ABORT_DRAW
	};

	static bool HasAlphaTestSecondPass(AlphaTestMode method)
	{
		return method == AlphaTestMode::SIMPLE_FB_ONLY ||
		       method == AlphaTestMode::SIMPLE_RGB_ONLY ||
		       method == AlphaTestMode::PASS_THEN_FAIL ||
		       method == AlphaTestMode::NEVER;
	}

	enum class DestinationAlphaMode : u8
	{
		Off,            ///< No destination alpha test
		Stencil,        ///< Emulate using read-only stencil
		StencilOne,     ///< Emulate using read-write stencil (first write wins)
		PrimIDTracking, ///< Emulate by tracking the primitive ID of the last pixel allowed through
		Full,           ///< Full emulation (using barriers / ROV)
	};

	enum class ColClipMode : u8
	{
		NoModify = 0,
		ConvertOnly = 1,
		ResolveOnly = 2,
		ConvertAndResolve = 3,
		EarlyResolve = 4
	};

	GSTexture* rt;         ///< Render target
	GSTexture* ds;         ///< Depth stencil
	GSTexture* tex;        ///< Source texture
	GSTexture* pal;        ///< Palette texture
	const GSVertex* verts; ///< Vertices to draw
	const u16* indices;    ///< Indices to draw
	u32 nverts;            ///< Number of vertices
	u32 nindices;          ///< Number of indices
	u32 indices_per_prim;  ///< Number of indices that make up one primitive
	const std::vector<size_t>* drawlist;          ///< For reducing barriers on sprites
	const std::vector<GSVector4i>* drawlist_bbox; ///< For RT copy when barriers not available.
	GSVector4i scissor; ///< Scissor rect
	GSVector4i drawarea; ///< Area in the framebuffer which will be modified.
	GSVector4i samplearea; ///< Area in the texture which will be sampled.
	Topology topology;  ///< Draw topology

	alignas(8) PSSelector ps;
	VSSelector vs;
	bool uber_shader; ///< Use uber shader in the current draw.

	BlendState blend;
	SamplerSelector sampler;
	ColorMaskSelector colormask;
	DepthStencilSelector depth;

	bool require_one_barrier;  ///< Require texture barrier before draw (also used to requst an rt copy if texture barrier isn't supported)
	bool require_full_barrier; ///< Require texture barrier between all prims

	enum : u32
	{
		TEX_HAZARD_NONE,
		TEX_HAZARD_RT,
		TEX_HAZARD_DEPTH,
	} tex_hazard;

	AlphaTestMode alpha_test;

	DestinationAlphaMode destination_alpha;
	SetDATM datm;
	bool line_expand;

	struct AlphaPass
	{
		alignas(8) PSSelector ps;
		bool enable : 1;
		bool require_one_barrier : 1;
		bool require_full_barrier : 1;
		ColorMaskSelector colormask;
		DepthStencilSelector depth;
		float ps_aref;
	};
	static_assert(sizeof(AlphaPass) == 24, "alpha pass is 24 bytes");

	AlphaPass alpha_second_pass;

	struct BlendMultiPass
	{
		BlendState blend;
		bool enable : 1;
		u8 no_color1 : 1;
		u8 blend_hw : 3; // HWBlendType
		u8 dither : 2;
	};
	static_assert(sizeof(BlendMultiPass) == 8, "blend multi pass is 8 bytes");

	BlendMultiPass blend_multi_pass;
	
	// Place push constants before constant buffer so that they're zeroed out ever draw.
	TFXPushConstants pc;

	VSConstantBuffer cb_vs;
	PSConstantBuffer cb_ps;
	
	// These are here as they need to be preserved between draws, and the state clear only does up to the constant buffers.
	ColClipMode colclip_mode;
	GIFRegFRAME colclip_frame;
	GSVector4i colclip_update_area; ///< Area in the framebuffer which colclip will modify;

	__fi bool IsFeedbackLoopRT(const PSSelector& ps) const
	{
		return ps.IsFeedbackLoopRT() || (tex_hazard == TEX_HAZARD_RT);
	}

	__fi bool IsFeedbackLoopDepth(const PSSelector& ps) const
	{
		return ps.IsFeedbackLoopDepth() || (tex_hazard == TEX_HAZARD_DEPTH);
	}
	
	bool IsBlending()
	{
		return blend.enable || blend_multi_pass.enable || ps.IsSWBlending();
	}

	// Draw pass selectors
	enum class DrawPass
	{
		Main,
		AlphaSecond,
		PrimID,
		Blend,
	};

	bool GetFullBarrier(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return require_full_barrier;
			case DrawPass::AlphaSecond: return alpha_second_pass.require_full_barrier;
			case DrawPass::PrimID: return false;
			case DrawPass::Blend: return false;
		}
	}

	bool GetOneBarrier(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return require_one_barrier;
			case DrawPass::AlphaSecond: return alpha_second_pass.require_one_barrier;
			case DrawPass::PrimID: return false;
			case DrawPass::Blend: return false;
		}
	}

	const VSSelector& GetVS(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return vs;
			case DrawPass::AlphaSecond: return vs;
			case DrawPass::Blend: return vs;
			case DrawPass::PrimID: return vs;
		}
	}

	const PSSelector GetPS(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return ps;
			case DrawPass::AlphaSecond: return alpha_second_pass.ps;
			case DrawPass::Blend:
			{
				PSSelector ps_blend = ps;
				ps_blend.no_color1 = blend_multi_pass.no_color1;
				ps_blend.blend_hw = blend_multi_pass.blend_hw;
				ps_blend.dither = blend_multi_pass.dither;
				return ps_blend;
			}
			case DrawPass::PrimID: return ps;
		}
	}

	ColorMaskSelector GetColorMask(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return colormask;
			case DrawPass::AlphaSecond: return alpha_second_pass.colormask;
			case DrawPass::Blend: return colormask;
			case DrawPass::PrimID: return GSHWDrawConfig::ColorMaskSelector(1);
		}
	}

	DepthStencilSelector GetDepth(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::Main: return depth;
			case DrawPass::AlphaSecond: return alpha_second_pass.depth;
			case DrawPass::Blend: return depth;
			case DrawPass::PrimID:
			{
				DepthStencilSelector primid_depth = depth;
				primid_depth.zwe = false;
				return primid_depth;
			}
		}
	}

	BlendState GetBlend(DrawPass pass) const
	{
		switch (pass)
		{
			default:
			case DrawPass::AlphaSecond:
			case DrawPass::PrimID:
			case DrawPass::Main:
				return blend;
			case DrawPass::Blend:;
				return blend_multi_pass.blend;
		}
	}

	// Dumping
	static void DumpConfig(const std::string& path, const GSHWDrawConfig& conf,
		bool ps = true, bool vs = true, bool bs = true, bool dss = true, bool ss = true, bool asp = true, bool bmp = true,
		bool cbvs = true, bool cbps = true);

	// Uber shader
	struct ShaderDefine
	{
		u32 index;
		const char* name;
		const char* shader_name;
		std::string value;
	};

	static constexpr std::array<PipelineSelectorFieldDesc, 6> vs_selector_fields = {
		#define EXPAND(INDEX, DYNAMIC, TYPE, NAME, WIDTH, SHADER_NAME) \
				PipelineSelectorFieldDesc{ INDEX, DYNAMIC, #NAME, WIDTH, SHADER_NAME },
			VSSEL_FIELDS_EXPAND
		#undef EXPAND
	};

	static constexpr std::array<PipelineSelectorFieldDesc, 65> ps_selector_fields = {
		#define EXPAND(INDEX, DYNAMIC, TYPE, NAME, WIDTH, SHADER_NAME) \
				PipelineSelectorFieldDesc{ INDEX, DYNAMIC, #NAME, WIDTH, SHADER_NAME },
			PSSEL_FIELDS_EXPAND
		#undef EXPAND
	};

	static std::span<const ShaderDefine> GetUberShaderVSSelectorDefines();
	static std::span<const ShaderDefine> GetUberShaderPSSelectorDefines();

	static UberDynamicSelector GetUberDynamicSelector(const VSSelector& vs, const PSSelector& ps);
};

static inline u32 GetExpansionFactor(GSHWDrawConfig::VSExpand expand)
{
	switch (expand)
	{
		case GSHWDrawConfig::VSExpand::Point:
		case GSHWDrawConfig::VSExpand::Line:
		case GSHWDrawConfig::VSExpand::LineAA1:
			return 4;
		case GSHWDrawConfig::VSExpand::Sprite:
			return 2;
		case GSHWDrawConfig::VSExpand::TriangleAA1:
			return 13;
		default:
			return 1;
	}
}

static inline u32 GetVertexAlignment(GSHWDrawConfig::VSExpand expand)
{
	switch (expand)
	{
		case GSHWDrawConfig::VSExpand::Sprite:
			// Sprite expand does a 2-4 expansion, and relies on the low bit of the vertex ID to figure out if it's the first or second coordinate.
			return 2;
		default:
			return 1;
	}
}

class GSDevice : public GSAlignedClass<32>
{
public:
	using DrawPass = GSHWDrawConfig::DrawPass;
	using TFX_RT = GSHWDrawConfig::TFX_RT;
	using TFX_DS = GSHWDrawConfig::TFX_DS;

	enum class PresentResult
	{
		OK,
		FrameSkipped,
		DeviceLost
	};

	enum class DebugMessageCategory
	{
		Cache,
		Reg,
		Debug,
		Message,
		Performance
	};

	// clang-format off
	struct FeatureSupport
	{
		bool broken_point_sampler : 1; ///< Issue with AMD cards, see tfx shader for details
		bool vs_expand            : 1; ///< Supports expanding points/lines/sprites in the vertex shader
		bool primitive_id         : 1; ///< Supports primitive ID for use with prim tracking destination alpha algorithm
		bool texture_barrier      : 1; ///< Supports sampling rt and hopefully texture barrier
		bool multidraw_fb_copy    : 1; ///< Replacement for texture barrier.
		bool provoking_vertex_last: 1; ///< Supports using the last vertex in a primitive as the value for flat shading.
		bool point_expand         : 1; ///< Supports point expansion in hardware.
		bool line_expand          : 1; ///< Supports line expansion in hardware.
		bool prefer_new_textures  : 1; ///< Allocate textures up to the pool size before reusing them, to avoid render pass restarts.
		bool dxt_textures         : 1; ///< Supports DXTn texture compression, i.e. S3TC and BC1-3.
		bool bptc_textures        : 1; ///< Supports BC6/7 texture compression.
		bool framebuffer_fetch    : 1; ///< Can sample from the framebuffer without texture barriers.
		bool stencil_buffer       : 1; ///< Supports stencil buffer, and can use for DATE.
		bool cas_sharpening       : 1; ///< Supports sufficient functionality for contrast adaptive sharpening.
		bool test_and_sample_depth: 1; ///< Supports concurrently binding the depth-stencil buffer for sampling and depth testing.
		bool depth_feedback       : 1; ///< Depth feedback loops can be done with DS directly (otherwise need to copy to separate RT).  Implies `feedback_loops`.
		bool aa1                  : 1; ///< Supports the GS AA1 feature.
		bool rov                  : 1; ///< Supports rasterizer ordered views for both depth and color.
		bool uber_shader          : 1; ///< Supports uber shader.
		FeatureSupport()
		{
			memset(this, 0, sizeof(*this));
		}
		/// Supports feedback loops through either texture barriers or rt copies.
		bool feedback_loops() const { return texture_barrier || multidraw_fb_copy; }
		/// FB fetch can handle depth feedback via color copy.
		bool FBFetchDepthFeedback() const { return framebuffer_fetch && !depth_feedback; }
	};

	struct MultiStretchRect
	{
		GSVector4 src_rect;
		GSVector4 dst_rect;
		GSTexture* src;
		Filter filter;
		GSHWDrawConfig::ColorMaskSelector wmask; // 0xf for all channels by default
	};

	struct TextureRecycleDeleter
	{
		void operator()(GSTexture* const tex);
	};
	using RecycledTexture = std::unique_ptr<GSTexture, TextureRecycleDeleter>;

	enum BlendFactor : u8
	{
		// HW blend factors
		SRC_COLOR,   INV_SRC_COLOR,   DST_COLOR,  INV_DST_COLOR,
		SRC1_COLOR,  INV_SRC1_COLOR,  SRC_ALPHA,  INV_SRC_ALPHA,
		DST_ALPHA,   INV_DST_ALPHA,   SRC1_ALPHA, INV_SRC1_ALPHA,
		CONST_COLOR, INV_CONST_COLOR, CONST_ONE,  CONST_ZERO,
	};
	enum BlendOp : u8
	{
		// HW blend operations
		OP_ADD, OP_SUBTRACT, OP_REV_SUBTRACT
	};
	// clang-format on

protected:
	std::string m_name = "Unknown";
	FeatureSupport m_features;
	u32 m_max_texture_size = 0;

	struct
	{
		u32 start, count;
	} m_vertex = {};
	struct
	{
		u32 start, count;
	} m_index = {};

	u32 m_frame = 0; // for ageing the pool

private:
	std::array<FastList<GSTexture*>, 2> m_pool; // [texture, target]
	u64 m_pool_memory_usage = 0;

	static const std::array<HWBlend, 3*3*3*3> m_blendMap;

protected:
	static constexpr int NUM_INTERLACE_SHADERS = 5;
	static constexpr float MAD_SENSITIVITY = 0.08f;
	static constexpr u32 MAX_POOLED_TARGETS = 300;
	static constexpr u32 MAX_TARGET_AGE = 20;
	static constexpr u32 MAX_POOLED_TEXTURES = 300;
	static constexpr u32 MAX_TEXTURE_AGE = 10;
	static constexpr u32 NUM_CAS_CONSTANTS = 12; // 8 plus src offset x/y, 16 byte alignment
	static constexpr u32 EXPAND_BUFFER_SIZE = sizeof(u16) * 16383 * 6;

	WindowInfo m_window_info;
	GSVSyncMode m_vsync_mode = GSVSyncMode::Disabled;
	bool m_allow_present_throttle = false;
	u64 m_last_frame_displayed_time = 0;

	GSTexture* m_merge = nullptr;
	GSTexture* m_weavebob = nullptr;
	GSTexture* m_blend = nullptr;
	GSTexture* m_mad = nullptr;
	GSTexture* m_target_tmp = nullptr;
	GSTexture* m_current = nullptr;
	GSTexture* m_cas = nullptr;
	GSTexture* m_colclip_rt = nullptr; ///< Temp hw colclip texture
	GSTexture* m_ds_as_rt = nullptr; ///< Depth as color

	bool AcquireWindow(bool recreate_window);

	virtual GSTexture* CreateSurface(GSTexture::Usage usage, int width, int height, int levels, GSTexture::Format format) = 0;

	virtual void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const Filter filter) = 0;
	virtual void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, Filter filter, const InterlaceConstantBuffer& cb) = 0;
	virtual void DoFXAA(GSTexture* sTex, GSTexture* dTex) = 0;
	virtual void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) = 0;

	/// Resolves CAS shader includes for the specified source.
	static bool GetCASShaderSource(std::string* source);

	/// Applies CAS and writes to the destination texture, which should be a shader writeable texture.
	virtual bool DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) = 0;

	/// Perform texture operations for ImGui
	void UpdateImGuiTextures();

protected:
	// Entry point to the renderer-specific StretchRect code.
	virtual void DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderConvertSelector shader, Filter filter) = 0;
	virtual void DoStretchRect(GSTexture* sTex, const GSVector4& sRect, const GSVector4& dRect,
		PresentShader shader, Filter filter)
	{
		pxFailRel("Not implemented");
	}
	void DoStretchRectWithAssertions(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, 
		ShaderConvertSelector shader, Filter filter);
public:
	GSDevice();
	virtual ~GSDevice();

	/// Returns a string containing current adapter in use.
	const std::string& GetName() const { return m_name; }

	GSTexture* GetColorClipTexture() const { return m_colclip_rt; }
		
	void SetColorClipTexture(GSTexture* tex) { m_colclip_rt = tex; }

	bool IsDSInRTActive() const { return m_ds_as_rt; }
	/// Create a temporary color clone of depth for depth feedback
	virtual void BeginDSAsRT(GSTexture* ds, const GSVector4i& drawarea);
	void EndDSAsRT();

	/// Returns a string representing the specified API.
	static const char* RenderAPIToString(RenderAPI api);

	/// Parses the configured fullscreen mode into its components (width * height @ refresh Hz)
	static bool GetRequestedExclusiveFullscreenMode(u32* width, u32* height, float* refresh_rate);

	/// Converts a fullscreen mode to a string.
	static std::string GetFullscreenModeString(u32 width, u32 height, float refresh_rate);

	/// Generates a fixed index buffer for expanding points and sprites. Buffer is assumed to be at least EXPAND_BUFFER_SIZE in size.
	static void GenerateExpansionIndexBuffer(void* buffer);

	// Process copy area for sw blend copies.
	GSVector4i ProcessCopyArea(const GSVector4i& rtsize, const GSVector4i& drawarea);

	/// Reads the specified shader source file.
	static std::optional<std::string> ReadShaderSource(const char* filename);

	/// Returns the maximum number of mipmap levels for a given texture size.
	static int GetMipmapLevelsForSize(int width, int height);

	__fi u64 GetPoolMemoryUsage() const { return m_pool_memory_usage; }

	__fi FeatureSupport Features() const { return m_features; }
	__fi u32 GetMaxTextureSize() const { return m_max_texture_size; }

	__fi const WindowInfo& GetWindowInfo() const { return m_window_info; }
	__fi s32 GetWindowWidth() const { return static_cast<s32>(m_window_info.surface_width); }
	__fi s32 GetWindowHeight() const { return static_cast<s32>(m_window_info.surface_height); }
	__fi GSVector2i GetWindowSize() const { return GSVector2i(static_cast<s32>(m_window_info.surface_width), static_cast<s32>(m_window_info.surface_height)); }
	__fi float GetWindowScale() const { return m_window_info.surface_scale; }
	__fi GSVSyncMode GetVSyncMode() const { return m_vsync_mode; }
	__fi bool IsPresentThrottleAllowed() const { return m_allow_present_throttle; }

	__fi GSTexture* GetCurrent() const { return m_current; }
	__fi GSTexture* GetMAD() const { return m_mad; }
	
	void Recycle(GSTexture* t);

	/// Returns true if it's an OpenGL-based renderer.
	bool UsesLowerLeftOrigin() const;

	/// Free ImGui textures before shutdown
	void DestroyImGuiTextures();

	virtual bool Create(GSVSyncMode vsync_mode, bool allow_present_throttle);
	virtual void Destroy();

	/// Returns the graphics API used by this device.
	virtual RenderAPI GetRenderAPI() const = 0;

	/// Returns true if we have a window we're rendering into.
	virtual bool HasSurface() const = 0;

	/// Destroys the surface we're currently drawing to.
	virtual void DestroySurface() = 0;

	/// Switches to a new window/surface.
	virtual bool UpdateWindow() = 0;

	/// Call when the window size changes externally to recreate any resources.
	virtual void ResizeWindow(u32 new_window_width, u32 new_window_height, float new_window_scale) = 0;

	/// Returns true if exclusive fullscreen is supported.
	virtual bool SupportsExclusiveFullscreen() const = 0;

	/// Returns false if the window was completely occluded. If frame_skip is set, the frame won't be
	/// displayed, but the GPU command queue will still be flushed.
	virtual PresentResult BeginPresent(bool frame_skip) = 0;

	/// Presents the frame to the display.
	virtual void EndPresent() = 0;

	/// Changes vsync mode for this display.
	virtual void SetVSyncMode(GSVSyncMode mode, bool allow_present_throttle) = 0;

	/// Returns a string of information about the graphics driver being used.
	virtual std::string GetDriverInfo() const = 0;

	/// Enables/disables GPU frame timing.
	virtual bool SetGPUTimingEnabled(bool enabled) = 0;

	/// Returns the amount of GPU time utilized since the last time this method was called.
	virtual float GetAndResetAccumulatedGPUTime() = 0;

	/// Enables/disables GPU pipeline statistics.
	virtual bool SetGPUPipelineStatisticsEnabled(bool enabled) = 0;

	/// Get the pipeline statistics for the last frame.
	virtual GPUPipelineStatistics GetAndResetAccumulatedGPUPipelineStatistics() = 0;

	/// Returns true if not enough time has passed for present to not block.
	bool ShouldSkipPresentingFrame();

	/// Sleeps to the time the next frame can be displayed.
	void ThrottlePresentation();

	void ClearRenderTarget(GSTexture* t, u32 c);
	void ClearDepth(GSTexture* t, float d);
	bool ProcessClearsBeforeCopy(GSTexture* sTex, GSTexture* dTex, const bool full_copy);
	void InvalidateRenderTarget(GSTexture* t);

	virtual void PushDebugGroup(const char* fmt, ...) = 0;
	virtual void PopDebugGroup() = 0;
	virtual void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) = 0;

	GSTexture* FetchSurface(GSTexture::Usage usage, int width, int height, int levels, GSTexture::Format format, bool clear, bool prefer_reuse);
	GSTexture* FetchSurface(GSTexture::Usage usage, const GSVector2i& size, int levels, GSTexture::Format format, bool clear, bool prefer_reuse);
	GSTexture* CreateRenderTarget(int w, int h, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateRenderTarget(const GSVector2i& size, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateFeedbackTarget(int w, int h, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateFeedbackTarget(const GSVector2i& size, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateShaderWriteTarget(int w, int h, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateShaderWriteTarget(const GSVector2i& size, GSTexture::Format format, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateDepthStencil(int w, int h, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateDepthStencil(const GSVector2i& size, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateTexture(int w, int h, int mipmap_levels, GSTexture::Format format, bool prefer_reuse = false);
	GSTexture* CreateTexture(const GSVector2i& size, int mipmap_levels, GSTexture::Format format, bool prefer_reuse = false);
	GSTexture* CreateCompatible(GSTexture* tex, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateCompatible(GSTexture* tex, const GSVector2i& size, bool clear = true, bool prefer_reuse = true);
	GSTexture* CreateCompatible(GSTexture* tex, int w, int h, bool clear = true, bool prefer_reuse = true);

	virtual std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) = 0;

	virtual void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) = 0;

	// StretchRect - all options
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvertSelector shader, Filter filter);
	void StretchRect(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, ShaderConvertSelector shader, Filter filter);
	void StretchRect(GSTexture* sTex, GSTexture* dTex, ShaderConvertSelector shader, Filter filter);
	
	// StretchRect - infer shader based on formats
	void StretchRectAuto(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, Filter filter,
		u32 src_bpp = 32, u32 dst_bpp = 32);
	void StretchRectAuto(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, Filter filter,
		u32 src_bpp = 32, u32 dst_bpp = 32);
	void StretchRectAuto(GSTexture* sTex, GSTexture* dTex, Filter filter, u32 src_bpp = 32, u32 dst_bpp = 32);

	// StretchRect - nearest filter, infer shader based on formats, specify channel mask
	void StretchRectAutoMask(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha, u32 src_bpp = 32, u32 dst_bpp = 32);
	void StretchRectAutoMask(GSTexture* sTex, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha, u32 src_bpp = 32, u32 dst_bpp = 32);
	void StretchRectAutoMask(GSTexture* sTex, GSTexture* dTex, bool red, bool green, bool blue, bool alpha, u32 src_bpp = 32, u32 dst_bpp = 32);

	/// Performs a screen blit for display. If dTex is null, it assumes you are writing to the system framebuffer/swap chain.
	virtual void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, PresentShader shader, float shaderTime, Filter filter) = 0;

	/// Same as doing StretchRect for each item, except tries to batch together rectangles in as few draws as possible.
	/// The provided list should be sorted by texture, the implementations only check if it's the same as the last.
	virtual void DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvertSelector shader = ShaderConvert::COPY);

	/// Sorts a MultiStretchRect list for optimal batching.
	static void SortMultiStretchRects(MultiStretchRect* rects, u32 num_rects);

	/// Updates a GPU CLUT texture from a source texture.
	virtual void UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) = 0;

	/// Converts a colour format to an indexed format texture.
	virtual void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM) = 0;

	/// Uses box downsampling to resize a texture.
	virtual void FilteredDownsampleTexture(GSTexture* sTex, GSTexture* dTex, u32 downsample_factor, const GSVector2i& clamp_min, const GSVector4& dRect) = 0;

	virtual void RenderHW(GSHWDrawConfig& config) = 0;

	virtual void ClearSamplerCache() = 0;

	void ClearCurrent();
	void Merge(GSTexture* sTex[3], GSVector4* sRect, GSVector4* dRect, const GSVector2i& fs, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c);
	void Interlace(const GSVector2i& ds, int field, int mode, float yoffset);
	void FXAA();
	void ShadeBoost();
	void Resize(int width, int height);

	void CAS(GSTexture*& tex, GSVector4i& src_rect, GSVector4& src_uv, const GSVector4& draw_rect, bool sharpen_only);

	bool ResizeRenderTarget(GSTexture** t, int w, int h, bool preserve_contents, bool recycle);

	void AgePool();
	void PurgePool();

	__fi static constexpr bool IsDualSourceBlendFactor(u8 factor)
	{
		return (factor == SRC1_ALPHA || factor == INV_SRC1_ALPHA || factor == SRC1_COLOR || factor == INV_SRC1_COLOR);
	}
	__fi static constexpr bool IsConstantBlendFactor(u16 factor)
	{
		return (factor == CONST_COLOR || factor == INV_CONST_COLOR);
	}

	// Convert the GS blend equations to HW blend factors/ops
	// Index is computed as ((((A * 3 + B) * 3) + C) * 3) + D. A, B, C, D taken from ALPHA register.
	__ri static HWBlend GetBlend(u32 index) { return m_blendMap[index]; }
	__ri static u16 GetBlendFlags(u32 index) { return m_blendMap[index].flags; }

	virtual bool StartPipelineCompilationAsync(const GSHWDrawConfig& conf);
};

template <>
struct std::hash<GSHWDrawConfig::PSSelector> : public GSHWDrawConfig::PSSelectorHash {};

extern std::unique_ptr<GSDevice> g_gs_device;
