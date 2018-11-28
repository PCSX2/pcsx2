/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GSVector.h"
#include "Renderers/Common/GSDevice.h"
#include "GSAlignedClass.h"

class GSDeviceDX : public GSDevice
{
protected:
	int m_upscale_multiplier;

public:
	#pragma pack(push, 1)

	struct alignas(32) VSConstantBuffer
	{
		GSVector4 VertexScale;
		GSVector4 VertexOffset;
		GSVector4 Texture_Scale_Offset;

		struct VSConstantBuffer()
		{
			VertexScale = GSVector4::zero();
			VertexOffset = GSVector4::zero();
			Texture_Scale_Offset = GSVector4::zero();
		}

		__forceinline bool Update(const VSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			if(!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2]) & (a[3] == b[3])).alltrue())
			{
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];

				return true;
			}

			return false;
		}
	};

	struct VSSelector
	{
		union
		{
			struct
			{
				uint32 bppz:2;
				uint32 tme:1;
				uint32 fst:1;
				uint32 logz:1;
				uint32 rtcopy:1;
			};

			uint32 key;
		};

		operator uint32() {return key & 0xff;}

		VSSelector() : key(0) {}
	};

	struct alignas(32) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 HalfTexel;
		GSVector4 WH;
		GSVector4 MinMax;
		GSVector4 MinF_TA;
		GSVector4i MskFix;

		GSVector4 TC_OffsetHack;

		struct PSConstantBuffer()
		{
			FogColor_AREF = GSVector4::zero();
			HalfTexel = GSVector4::zero();
			WH = GSVector4::zero();
			MinMax = GSVector4::zero();
			MinF_TA = GSVector4::zero();
			MskFix = GSVector4i::zero();
		}

		__forceinline bool Update(const PSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			if(!((a[0] == b[0]) /*& (a[1] == b1)*/ & (a[2] == b[2]) & (a[3] == b[3]) & (a[4] == b[4]) & (a[5] == b[5])).alltrue()) // if WH matches HalfTexel does too
			{
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];
				a[4] = b[4];
				a[5] = b[5];

				return true;
			}

			return false;
		}
	};

	struct alignas(32) GSConstantBuffer
	{
		GSVector2 PointSize;

		struct GSConstantBuffer()
		{
			PointSize = GSVector2(0);
		}

		__forceinline bool Update(const GSConstantBuffer* cb)
		{
			return true;
		}
	};

	struct GSSelector
	{
		union
		{
			struct
			{
				uint32 iip:1;
				uint32 prim:2;
				uint32 point:1;
				uint32 line:1;

				uint32 _free:27;
			};

			uint32 key;
		};

		operator uint32() {return key;}

		GSSelector() : key(0) {}
		GSSelector(uint32 k) : key(k) {}
	};

	struct PSSelector
	{
		union
		{
			struct
			{
				// *** Word 1
				// Format
				uint32 fmt:4;
				uint32 dfmt:2;
				// Alpha extension/Correction
				uint32 aem:1;
				uint32 fba:1;
				// Fog
				uint32 fog:1;
				// Pixel test
				uint32 date:2;
				uint32 atst:3;
				// Color sampling
				uint32 fst:1;
				uint32 tfx:3;
				uint32 tcc:1;
				uint32 wms:2;
				uint32 wmt:2;
				uint32 ltf:1;
				// Shuffle and fbmask effect
				uint32 shuffle:1;
				uint32 read_ba:1;

				// *** Word 2
				// Blend and Colclip
				uint32 clr1:1;
				uint32 rt:1;
				uint32 colclip:2;

				// Hack
				uint32 aout:1;
				uint32 spritehack:1;
				uint32 tcoffsethack:1;
				uint32 point_sampler:1;

				uint32 _free:30;
			};

			uint64 key;
		};

		operator uint64() {return key;}

		PSSelector() : key(0) {}
	};

	struct PSSamplerSelector
	{
		union
		{
			struct
			{
				uint32 tau:1;
				uint32 tav:1;
				uint32 ltf:1;
			};

			uint32 key;
		};

		operator uint32() {return key & 0x7;}

		PSSamplerSelector() : key(0) {}
	};

	struct OMDepthStencilSelector
	{
		union
		{
			struct
			{
				uint32 ztst:2;
				uint32 zwe:1;
				uint32 date:1;
				uint32 fba:1;
				uint32 date_one:1;
			};

			uint32 key;
		};

		operator uint32() {return key & 0x3f;}

		OMDepthStencilSelector() : key(0) {}
	};

	struct OMBlendSelector
	{
		union
		{
			struct
			{
				uint32 abe:1;
				uint32 a:2;
				uint32 b:2;
				uint32 c:2;
				uint32 d:2;
				uint32 wr:1;
				uint32 wg:1;
				uint32 wb:1;
				uint32 wa:1;
				uint32 negative:1;
			};

			struct
			{
				uint32 _pad:1;
				uint32 abcd:8;
				uint32 wrgba:4;
			};

			uint32 key;
		};

		operator uint32() {return key & 0x3fff;}

		OMBlendSelector() : key(0) {}

		bool IsCLR1() const
		{
			return (key & 0x19f) == 0x93; // abe == 1 && a == 1 && b == 2 && d == 1
		}
	};

	struct D3D9Blend {int bogus, op, src, dst;};
	static const D3D9Blend m_blendMapD3D9[3*3*3*3];

	#pragma pack(pop)

protected:
	struct {D3D_FEATURE_LEVEL level; std::string model, vs, gs, ps, cs;} m_shader;
	uint32 m_msaa;
	DXGI_SAMPLE_DESC m_msaa_desc;

	static HMODULE s_d3d_compiler_dll;
	static decltype(&D3DCompile) s_pD3DCompile;
	// Older version doesn't support D3D_COMPILE_STANDARD_FILE_INCLUDE, which
	// could be useful for external shaders.
	static bool s_old_d3d_compiler_dll;

	GSTexture* FetchSurface(int type, int w, int h, bool msaa, int format);

public:
	GSDeviceDX();
	virtual ~GSDeviceDX();

	bool SetFeatureLevel(D3D_FEATURE_LEVEL level, bool compat_mode);
	void GetFeatureLevel(D3D_FEATURE_LEVEL& level) const {level = m_shader.level;}

	virtual void SetupVS(VSSelector sel, const VSConstantBuffer* cb) = 0;
	virtual void SetupGS(GSSelector sel, const GSConstantBuffer* cb) = 0;
	virtual void SetupPS(PSSelector sel, const PSConstantBuffer* cb, PSSamplerSelector ssel) = 0;
	virtual void SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, uint8 afix) = 0;

	virtual void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm) = 0;

	virtual bool HasStencil() = 0;
	virtual bool HasDepth32() = 0;

	static bool LoadD3DCompiler();
	static void FreeD3DCompiler();

	template<class T> void PrepareShaderMacro(std::vector<T>& dst, const T* src)
	{
		dst.clear();

		while(src && src->Definition && src->Name)
		{
			dst.push_back(*src++);
		}

		T m;

		m.Name = "SHADER_MODEL";
		m.Definition = m_shader.model.c_str();

		dst.push_back(m);

		m.Name = NULL;
		m.Definition = NULL;

		dst.push_back(m);
	}
};

