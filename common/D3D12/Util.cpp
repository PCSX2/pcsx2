/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "common/PrecompiledHeader.h"

#include "common/D3D12/Util.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"

u32 D3D12::GetTexelSize(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC7_UNORM:
			return 16;

		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return 4;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			return 4;

		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SINT:
			return 2;

		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_R8_UNORM:
			return 1;

		default:
			pxFailRel("Unknown format");
			return 1;
	}
}

void D3D12::SetDefaultSampler(D3D12_SAMPLER_DESC* desc)
{
	desc->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	desc->AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc->AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc->AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	desc->MipLODBias = 0;
	desc->MaxAnisotropy = 1;
	desc->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc->BorderColor[0] = 1.0f;
	desc->BorderColor[1] = 1.0f;
	desc->BorderColor[2] = 1.0f;
	desc->BorderColor[3] = 1.0f;
	desc->MinLOD = -3.402823466e+38F; // -FLT_MAX
	desc->MaxLOD = 3.402823466e+38F; // FLT_MAX
}

#ifdef _DEBUG

void D3D12::SetObjectName(ID3D12Object* object, const char* name)
{
	object->SetName(StringUtil::UTF8StringToWideString(name).c_str());
}

void D3D12::SetObjectNameFormatted(ID3D12Object* object, const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	SetObjectName(object, StringUtil::StdStringFromFormatV(format, ap).c_str());
	va_end(ap);
}

#endif
